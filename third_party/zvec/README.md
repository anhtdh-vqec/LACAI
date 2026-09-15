# Zvec dependency

Pinned upstream v0.7.0, source commit and SDK checksum in dependency.json.
Run `bash tools/vqec_vision_prepare_zvec.sh` from a fresh checkout.
The downloaded SDK lives in sdk/ and is gitignored. LACAI builds the adapter by default;
VQEC_VISION_AI_ZVEC_ROOT can explicitly select another reviewed installation.
No dependency download occurs implicitly during CMake configuration.

The public Linux ARM64 binary is used, not a locally cross-built Zvec library.
LACAI adapter and tests are compiled and linked with the approved eSDK.
The archive SHA-256 matches the GitHub release asset digest. LICENSE is the unmodified
Apache-2.0 upstream license at the pinned commit. No upstream implementation source is
copied into LACAI. Bundled dependency notices must be reviewed before redistribution;
this local development acquisition does not establish product license acceptance.

On 2026-09-15 the real C API integration passed under eSDK QEMU and natively on
QCS6490 .48: synthetic cosine threshold, model revision and delete visibility.
This evidence covers index execution, not camera FD-to-FR integration or restart recovery.
