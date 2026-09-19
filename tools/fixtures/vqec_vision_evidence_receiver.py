#!/usr/bin/env python3
"""AI-owned evidence receiver for version-1 transport conformance tests."""

from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import signal
import socket
import sqlite3
import struct


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


def vqec_vision_ai_tools_evrcv_open_database(path: pathlib.Path) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    os.chmod(path.parent, 0o700)
    database = sqlite3.connect(path)
    database.execute("PRAGMA journal_mode=WAL")
    database.execute("PRAGMA synchronous=FULL")
    database.execute(
        "CREATE TABLE IF NOT EXISTS inbox("
        "request_id TEXT PRIMARY KEY, command BLOB NOT NULL, receipt BLOB NOT NULL)"
    )
    database.commit()
    os.chmod(path, 0o600)
    return database


def vqec_vision_ai_tools_evrcv_handle(
    peer: socket.socket,
    database: sqlite3.Connection,
    expected_uid: int,
    drop_first_ack: bool,
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
        row = database.execute(
            "SELECT command,receipt FROM inbox WHERE request_id=?", (request_id,)
        ).fetchone()
        if row is not None:
            if bytes(row[0]) != payload:
                return
            peer.sendall(bytes(row[1]))
            continue
        receipt = vqec_vision_ai_tools_evrcv_make_receipt(
            request_id, revision, occurred_at_ns
        )
        database.execute(
            "INSERT INTO inbox(request_id,command,receipt) VALUES(?,?,?)",
            (request_id, payload, receipt),
        )
        database.commit()
        if drop_first_ack:
            return
        peer.sendall(receipt)


def vqec_vision_ai_tools_evrcv_parse() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--socket", type=pathlib.Path, required=True)
    parser.add_argument("--database", type=pathlib.Path, required=True)
    parser.add_argument("--expected-uid", type=int, required=True)
    parser.add_argument("--socket-mode", type=lambda value: int(value, 8), default=0o600)
    parser.add_argument("--drop-first-ack", action="store_true")
    return parser.parse_args()


def vqec_vision_ai_tools_evrcv_main() -> int:
    arguments = vqec_vision_ai_tools_evrcv_parse()
    if not arguments.socket.is_absolute() or not arguments.database.is_absolute():
        raise SystemExit("socket and database paths must be absolute")
    arguments.socket.parent.mkdir(parents=True, exist_ok=True)
    arguments.socket.unlink(missing_ok=True)
    database = vqec_vision_ai_tools_evrcv_open_database(arguments.database)
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
                    database,
                    arguments.expected_uid,
                    arguments.drop_first_ack,
                )
    finally:
        server.close()
        database.close()
        arguments.socket.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(vqec_vision_ai_tools_evrcv_main())
