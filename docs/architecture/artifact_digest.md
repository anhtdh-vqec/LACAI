# Bounded artifact digest comparison

Source candidate, security/lead review pending. artifact_digest hashes bytes from a
caller-opened binary stream using SHA-256 and compares with a supplied lowercase digest.
The caller must supply an independently trusted expected digest, not blindly trust an
unauthenticated manifest. Success means byte integrity relative to that digest only.

API consumes from the current stream position through EOF; caller must position at
the beginning of the intended artifact. Maximum accepted size is explicit, 1 byte
through 4 GiB. Empty artifacts are rejected. A fixed 64 KiB scratch buffer is used;
at most limit+1 bytes are consumed to detect oversize. No seeking or full-file copy.
Read calls may block; the byte bound is not a time deadline. Use startup worker only.
Failure preserves the output receipt. Stream state/position is consumed, not restored.
The receipt contains matched digest and actual byte count, not a signed authorization.

No library/model load, file opening, path authorization, signature verification or
feature authorization is performed. Never accept a hash-match receipt as provenance.
This helper is deliberately not automatically connected to graph model_path loading:
the caller must guarantee the bytes later opened by QNN are the same immutable artifact.
Hash-then-reopen by path has a TOCTOU race; a retained FD does not prevent writes either.
Use a trusted immutable deployment store and atomic version activation, or add a platform
artifact handle contract before claiming secure loading. Backend/system libraries and
decoder/config artifacts also need independent verification in the eventual full kit.

Optional VQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST target uses OpenSSL Crypto >=3.0
from the local deployment/toolchain (patch version and provider policy pinned by BSP
release tooling). No dependency download. No OpenSSL types leak into the public header.
Implementation uses the official
[EVP digest API](https://docs.openssl.org/3.0/man3/EVP_DigestInit/).
Provider/init/update/final failures return errors; no alternate hash or insecure fallback.
This does not claim FIPS validation. No build, C++ test or board execution yet.
