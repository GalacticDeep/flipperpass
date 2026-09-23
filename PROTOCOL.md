# FlipperPass protocol v1

One profile is exactly 60 bytes, fitting the official SubGhzTxRxWorker packet chunk. No C struct layout or native endianness is serialized. The SDK presents received data as a stream, so the parser examines a sliding 60-byte window until it finds a valid frame. Invalid bytes consume bounded memory; every valid frame resets the window. Radio's own CRC is supplemented by the application CRC below.

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 4 | ASCII `FLPS` |
| 4 | 1 | Version `1` |
| 5 | 8 | Opaque random persisted identity |
| 13 | 1 | Avatar: 0 Smile, 1 Cat, 2 Robot, 3 Ghost |
| 14 | 13 | NUL-terminated nickname, max 12 printable ASCII characters, nonempty |
| 27 | 31 | NUL-terminated status, max 30 printable ASCII characters, may be empty |
| 58 | 2 | CRC-16/CCITT-FALSE over bytes 0–57, little-endian |

CRC parameters: polynomial 0x1021, initial 0xFFFF, no reflection, no final XOR. `123456789` gives 0x29B1. The encoder zero-fills the frame; nickname/status buffers must already be bounded, valid, and zero-initialized. Decoder rejects unterminated fields, control characters, non-ASCII text, unknown version/avatar, and invalid CRC. CRC is not authentication.

The first beacon is scheduled 2–6 seconds after starting the radio, then one every 12–18 seconds with new random jitter. No immediate response or retry loop. Each worker write queues one complete frame. Other Flippers' profiles are never rebroadcast. Sender ID plus the receiver's exact local label is the duplicate key; saved snapshots do not change on repeated encounters. No encounter count is encoded or maintained.

## Local file formats

All lengths are exact; unknown lengths and bad CRCs are rejected. These are v1 formats, not a general interchange guarantee.

**Card (87 bytes):** bytes 0–59 are the profile frame; bytes 60–84 are a NUL-terminated local event label (24 characters max, nonempty); bytes 85–86 are a little-endian CRC-16/CCITT-FALSE over bytes 0–84. Files use slots `card000.dat`–`card099.dat`. Staged writes use `card.tmp`, sync, then rename. Existing files, including damaged ones, reserve their slot.

**Configuration (88 bytes):** bytes 0–59 are the user's own profile; bytes 60–84 are current label; byte 85 is band index (0 Off, 1 915000000, 2 433920000, 3 868350000 Hz); bytes 86–87 are CRC over bytes 0–85. Write to `profile.tmp`, sync, retain the previous `profile.dat` as `profile.bak`, then rename the staged file. Startup tries primary then backup. If neither is valid, it creates a new identity with radio Off. Interrupted writes can lose the most recent edit; filesystem/power-loss behavior still requires hardware testing.

No location is sent over the air. Records have no timestamp, signal strength, encounter count, or peer-provided location.
