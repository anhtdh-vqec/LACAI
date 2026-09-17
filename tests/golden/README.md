# golden

Small privacy-safe fixtures or checked artifact references for preprocess/tensor/decode parity.

- **Status:** service-smoke config fixtures delivered; preprocess/tensor/decode golden parity still planned
- **Rule:** no biometric data or model binaries in the repository

Config fixtures consumed by the service smoke CTests: `service_smoke_deployment.json`,
`service_smoke_catalog.json` and `service_smoke_features.json`. Model golden parity fixtures
are not present yet.

## Responsibility

- Pin reproducible preprocess, tensor and decode parity fixtures.
- Keep fixtures small and free of personal data; reference checked artifacts instead.

## See also

- [Tensor output](../../docs/architecture/tensor_output.md)
