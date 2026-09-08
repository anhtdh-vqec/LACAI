# tools

Run `powershell -NoProfile -File tools/vqec_vision_check_source_layout.ps1` from the
repository root. Read-only filename, quoted-include existence and CMake source checks;
nonzero exit on violations. This is not an AST checker, target dependency validator,
compiler, ownership test or board test. AST naming enforcement and CI remain planned.
vqec_vision_manifest_check.cpp is the optional model metadata diagnostic executable source.
