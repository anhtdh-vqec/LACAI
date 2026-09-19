#!/usr/bin/env python3
"""Create exact Ed25519 signatures for App Manager acceptance fixtures."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import struct
import subprocess
import tempfile


g_package_domain = b"VQEC-LACAI-VQAPP-1"
g_entitlement_domain = b"VQEC-LACAI-ENTITLEMENT-1"
g_max_document_bytes = 1024 * 1024


def vqec_vision_ai_tools_apsig_read(path: pathlib.Path) -> bytes:
    payload = path.read_bytes()
    if not payload or len(payload) > g_max_document_bytes:
        raise ValueError(f"invalid bounded fixture: {path}")
    return payload


def vqec_vision_ai_tools_apsig_frame_package(
    manifest: bytes, configuration: bytes
) -> bytes:
    return (
        g_package_domain
        + struct.pack(">Q", len(manifest))
        + manifest
        + struct.pack(">Q", len(configuration))
        + configuration
    )


def vqec_vision_ai_tools_apsig_frame_entitlement(grant: bytes) -> bytes:
    return g_entitlement_domain + struct.pack(">Q", len(grant)) + grant


def vqec_vision_ai_tools_apsig_sign(
    private_key: pathlib.Path, payload: bytes, output: pathlib.Path
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix="vqec-app-signing-", delete=True) as source:
        source.write(payload)
        source.flush()
        subprocess.run(
            [
                "openssl",
                "pkeyutl",
                "-sign",
                "-rawin",
                "-inkey",
                str(private_key),
                "-in",
                source.name,
                "-out",
                str(output),
            ],
            check=True,
        )
    if output.stat().st_size != 64:
        output.unlink(missing_ok=True)
        raise ValueError("OpenSSL did not create a raw 64-byte Ed25519 signature")


def vqec_vision_ai_tools_apsig_parse() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--private-key", type=pathlib.Path, required=True)
    parser.add_argument("--manifest", type=pathlib.Path, required=True)
    parser.add_argument("--configuration", type=pathlib.Path, required=True)
    parser.add_argument("--entitlement", type=pathlib.Path, required=True)
    parser.add_argument("--package-signature", type=pathlib.Path, required=True)
    parser.add_argument("--entitlement-signature", type=pathlib.Path, required=True)
    return parser.parse_args()


def vqec_vision_ai_tools_apsig_main() -> int:
    arguments = vqec_vision_ai_tools_apsig_parse()
    manifest = vqec_vision_ai_tools_apsig_read(arguments.manifest)
    configuration = vqec_vision_ai_tools_apsig_read(arguments.configuration)
    entitlement = vqec_vision_ai_tools_apsig_read(arguments.entitlement)
    vqec_vision_ai_tools_apsig_sign(
        arguments.private_key,
        vqec_vision_ai_tools_apsig_frame_package(manifest, configuration),
        arguments.package_signature,
    )
    vqec_vision_ai_tools_apsig_sign(
        arguments.private_key,
        vqec_vision_ai_tools_apsig_frame_entitlement(entitlement),
        arguments.entitlement_signature,
    )
    print(f"manifest_sha256={hashlib.sha256(manifest).hexdigest()}")
    print(f"configuration_sha256={hashlib.sha256(configuration).hexdigest()}")
    print(f"entitlement_sha256={hashlib.sha256(entitlement).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(vqec_vision_ai_tools_apsig_main())
