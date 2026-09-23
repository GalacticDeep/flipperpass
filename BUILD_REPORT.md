# Build report

Development validation on 2026-09-22 (macOS).

- uFBT: 0.2.6; official firmware SDK: 1.4.3; hardware target: f7; API: 87.1.
- Release artifact: `dist/flipperpass.fap` (14008 bytes).
- SHA-256: `643265ffe34580145094204bb80003c31354a192075539e76657eba060391070`.
- C compilation/link: passed, including `-Wall -Wextra -Werror` from official SDK.
- FAP strict import/symbol validation (`APPCHK`): passed.
- uFBT formatting/lint: passed.
- Actual protocol module compiled and tested on host with UndefinedBehaviorSanitizer: passed.
- AddressSanitizer: host runtime initialization failure; no ASan test result.
- No hardware connected or tested. Wireless exchange, on-device UI, storage failure handling, and regional behavior need the pending checks in TESTING.md.

This report describes a buildable experimental implementation, not a verified radio product.
