#!/usr/bin/env python3
"""AI-owned evidence receiver for version-1 transport conformance tests."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import pathlib
import signal
import socket
import struct
from typing import BinaryIO


g_wire_magic = 0x31514556
g_command_kind = 1
g_receipt_kind = 2
g_protocol_major = 1
g_protocol_minor = 0
g_ready_state = 3
g_max_message_bytes = 64 * 1024
g_max_identifier_bytes = 128
g_max_field_value_bytes = 512
g_max_fields = 32
g_default_maximum_inbox_bytes = 64 * 1024 * 1024
g_running = True


def vqec_vision_ai_tools_evrcv_stop(_signal: int, _frame: object) -> None:
    del _signal, _frame
    global g_running
    g_running = False


def vqec_vision_ai_tools_evrcv_read_u32(payload: bytes, offset: int) -> tuple[int, int]:
    if offset + 4 > len(payload):
        raise ValueError("short uint32")
    return struct.unpack_from("<I", payload, offset)[0], offset + 4


def vqec_vision_ai_tools_evrcv_read_u64(payload: bytes, offset: int) -> tuple[int, int]:
    if offset + 8 > len(payload):
        raise ValueError("short uint64")
    return struct.unpack_from("<Q", payload, offset)[0], offset + 8


def vqec_vision_ai_tools_evrcv_read_text(
    payload: bytes, offset: int, maximum: int
) -> tuple[str, int]:
    length, offset = vqec_vision_ai_tools_evrcv_read_u32(payload, offset)
    if length > maximum or offset + length > len(payload):
        raise ValueError("invalid bounded string")
    raw = payload[offset : offset + length]
    if any(value < 0x20 or value > 0x7E for value in raw):
        raise ValueError("non-printable string")
    return raw.decode("ascii"), offset + length


def vqec_vision_ai_tools_evrcv_parse_command(payload: bytes) -> tuple[str, int, int]:
    if not payload or len(payload) > g_max_message_bytes:
        raise ValueError("invalid command size")
    offset = 0
    magic, offset = vqec_vision_ai_tools_evrcv_read_u32(payload, offset)
    kind, offset = vqec_vision_ai_tools_evrcv_read_u32(payload, offset)
    major, offset = vqec_vision_ai_tools_evrcv_read_u32(payload, offset)
    minor, offset = vqec_vision_ai_tools_evrcv_read_u32(payload, offset)
    if (magic, kind, major, minor) != (
        g_wire_magic,
        g_command_kind,
        g_protocol_major,
        g_protocol_minor,
    ):
        raise ValueError("unsupported command header")
    request_id, offset = vqec_vision_ai_tools_evrcv_read_text(
        payload, offset, g_max_identifier_bytes
    )
    _, offset = vqec_vision_ai_tools_evrcv_read_text(
        payload, offset, g_max_identifier_bytes
    )
    event_revision, offset = vqec_vision_ai_tools_evrcv_read_u64(payload, offset)
    for _index in range(4):
        _, offset = vqec_vision_ai_tools_evrcv_read_text(
            payload, offset, g_max_identifier_bytes
        )
    offset += 8
    if offset > len(payload):
        raise ValueError("short camera identity")
    for _index in range(3):
        _, offset = vqec_vision_ai_tools_evrcv_read_u64(payload, offset)
    occurred_at_ns, offset = vqec_vision_ai_tools_evrcv_read_u64(payload, offset)
    for _index in range(2):
        _, offset = vqec_vision_ai_tools_evrcv_read_u64(payload, offset)
    field_count, offset = vqec_vision_ai_tools_evrcv_read_u32(payload, offset)
    if not request_id or event_revision == 0 or field_count > g_max_fields:
        raise ValueError("invalid command identity")
    for _index in range(field_count):
        _, offset = vqec_vision_ai_tools_evrcv_read_text(
            payload, offset, g_max_identifier_bytes
        )
        _, offset = vqec_vision_ai_tools_evrcv_read_text(
            payload, offset, g_max_identifier_bytes
        )
        _, offset = vqec_vision_ai_tools_evrcv_read_text(
            payload, offset, g_max_field_value_bytes
        )
        offset += 8
        if offset > len(payload):
            raise ValueError("short command field")
    if offset != len(payload):
        raise ValueError("trailing command bytes")
    return request_id, event_revision, occurred_at_ns


def vqec_vision_ai_tools_evrcv_append_text(output: bytearray, value: str) -> None:
    encoded = value.encode("ascii")
    output.extend(struct.pack("<I", len(encoded)))
    output.extend(encoded)


def vqec_vision_ai_tools_evrcv_make_receipt(
    request_id: str, event_revision: int, occurred_at_ns: int
) -> bytes:
    media_digest = hashlib.sha256(request_id.encode("ascii")).hexdigest()[:24]
    output = bytearray(
        struct.pack(
            "<IIII",
            g_wire_magic,
            g_receipt_kind,
            g_protocol_major,
            g_protocol_minor,
        )
    )
    vqec_vision_ai_tools_evrcv_append_text(output, request_id)
    output.extend(struct.pack("<QI", event_revision, g_ready_state))
    vqec_vision_ai_tools_evrcv_append_text(output, f"reference.{media_digest}")
    output.extend(struct.pack("<QQ", occurred_at_ns, occurred_at_ns + 1))
    vqec_vision_ai_tools_evrcv_append_text(output, "reference_receiver")
    return bytes(output)


def vqec_vision_ai_tools_evrcv_decode_record(
    line: bytes,
) -> tuple[str, bytes, bytes]:
    document = json.loads(line.decode("ascii"))
    if not isinstance(document, dict) or set(document) != {
        "request_id",
        "command",
        "receipt",
    }:
        raise ValueError("invalid inbox record")
    request_id = document["request_id"]
    if not isinstance(request_id, str) or not request_id:
        raise ValueError("invalid inbox request identity")
    command = base64.b64decode(document["command"], validate=True)
    receipt = base64.b64decode(document["receipt"], validate=True)
    parsed_request_id, _revision, _occurred_at_ns = (
        vqec_vision_ai_tools_evrcv_parse_command(command)
    )
    if parsed_request_id != request_id or not receipt:
        raise ValueError("inbox record identity mismatch")
    return request_id, command, receipt


def vqec_vision_ai_tools_evrcv_open_inbox(
    path: pathlib.Path,
) -> tuple[BinaryIO, dict[str, tuple[bytes, bytes]]]:
    path.parent.mkdir(parents=True, exist_ok=True)
    os.chmod(path.parent, 0o700)
    descriptor = os.open(path, os.O_RDWR | os.O_CREAT | os.O_CLOEXEC, 0o600)
    os.chmod(path, 0o600)
    stream = os.fdopen(descriptor, "r+b", buffering=0)
    records: dict[str, tuple[bytes, bytes]] = {}
    valid_bytes = 0
    while True:
        line = stream.readline()
        if not line:
            break
        if not line.endswith(b"\n"):
            break
        try:
            request_id, command, receipt = vqec_vision_ai_tools_evrcv_decode_record(
                line[:-1]
            )
        except (UnicodeDecodeError, ValueError, json.JSONDecodeError):
            stream.close()
            raise SystemExit("evidence inbox contains an invalid committed record")
        prior = records.get(request_id)
        if prior is not None and prior != (command, receipt):
            stream.close()
            raise SystemExit("evidence inbox contains a conflicting request identity")
        records[request_id] = (command, receipt)
        valid_bytes += len(line)
    if stream.tell() != valid_bytes:
        os.ftruncate(stream.fileno(), valid_bytes)
        os.fsync(stream.fileno())
    stream.seek(0, os.SEEK_END)
    return stream, records


def vqec_vision_ai_tools_evrcv_commit_record(
    stream: BinaryIO,
    records: dict[str, tuple[bytes, bytes]],
    request_id: str,
    command: bytes,
    receipt: bytes,
    maximum_bytes: int,
) -> None:
    document = {
        "request_id": request_id,
        "command": base64.b64encode(command).decode("ascii"),
        "receipt": base64.b64encode(receipt).decode("ascii"),
    }
    record = json.dumps(document, sort_keys=True, separators=(",", ":")).encode(
        "ascii"
    ) + b"\n"
    current_bytes = stream.seek(0, os.SEEK_END)
    if len(record) > maximum_bytes - current_bytes:
        raise OSError("evidence inbox capacity exhausted")
    stream.write(record)
    os.fsync(stream.fileno())
    records[request_id] = (command, receipt)


def vqec_vision_ai_tools_evrcv_handle(
    peer: socket.socket,
    inbox_stream: BinaryIO,
    records: dict[str, tuple[bytes, bytes]],
    expected_uid: int,
    drop_first_ack: bool,
    maximum_inbox_bytes: int,
) -> None:
    credentials = peer.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12)
    _pid, uid, _gid = struct.unpack("3i", credentials)
    if uid != expected_uid:
        return
    while g_running:
        try:
            payload = peer.recv(g_max_message_bytes + 1)
        except (TimeoutError, socket.timeout):
            continue
        if not payload:
            return
        if len(payload) > g_max_message_bytes:
            return
        try:
            request_id, revision, occurred_at_ns = (
                vqec_vision_ai_tools_evrcv_parse_command(payload)
            )
        except (UnicodeDecodeError, ValueError):
            return
        row = records.get(request_id)
        if row is not None:
            if row[0] != payload:
                return
            if peer.send(row[1]) != len(row[1]):
                return
            continue
        receipt = vqec_vision_ai_tools_evrcv_make_receipt(
            request_id, revision, occurred_at_ns
        )
        vqec_vision_ai_tools_evrcv_commit_record(
            inbox_stream,
            records,
            request_id,
            payload,
            receipt,
            maximum_inbox_bytes,
        )
        if drop_first_ack:
            return
        if peer.send(receipt) != len(receipt):
            return


def vqec_vision_ai_tools_evrcv_parse() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--socket", type=pathlib.Path, required=True)
    parser.add_argument("--database", type=pathlib.Path, required=True)
    parser.add_argument("--expected-uid", type=int, required=True)
    parser.add_argument("--socket-mode", type=lambda value: int(value, 8), default=0o600)
    parser.add_argument("--drop-first-ack", action="store_true")
    parser.add_argument(
        "--maximum-inbox-bytes", type=int, default=g_default_maximum_inbox_bytes
    )
    return parser.parse_args()


def vqec_vision_ai_tools_evrcv_main() -> int:
    arguments = vqec_vision_ai_tools_evrcv_parse()
    if not arguments.socket.is_absolute() or not arguments.database.is_absolute():
        raise SystemExit("socket and database paths must be absolute")
    if arguments.maximum_inbox_bytes <= 0:
        raise SystemExit("maximum inbox bytes must be positive")
    arguments.socket.parent.mkdir(parents=True, exist_ok=True)
    arguments.socket.unlink(missing_ok=True)
    inbox_stream, records = vqec_vision_ai_tools_evrcv_open_inbox(arguments.database)
    server = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    server.bind(str(arguments.socket))
    os.chmod(arguments.socket, arguments.socket_mode)
    server.listen(4)
    server.settimeout(0.25)
    signal.signal(signal.SIGTERM, vqec_vision_ai_tools_evrcv_stop)
    signal.signal(signal.SIGINT, vqec_vision_ai_tools_evrcv_stop)
    try:
        while g_running:
            try:
                peer, _address = server.accept()
            except socket.timeout:
                continue
            peer.settimeout(0.25)
            with peer:
                vqec_vision_ai_tools_evrcv_handle(
                    peer,
                    inbox_stream,
                    records,
                    arguments.expected_uid,
                    arguments.drop_first_ack,
                    arguments.maximum_inbox_bytes,
                )
    finally:
        server.close()
        inbox_stream.close()
        arguments.socket.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(vqec_vision_ai_tools_evrcv_main())
