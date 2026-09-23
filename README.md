# FlipperPass

A small StreetPass-inspired Flipper Zero app: exchange nickname, pixel avatar, and a short status with other Flippers running FlipperPass and browse saved cards later. Each incoming card records **your manually selected current event/location**, such as DefCon or GrrCon. No GPS, repeat encounter counters, Nintendo interoperability, phone, or external radio board.

**Status: experimental hardware-test build.** 
Built against official firmware **1.4.3**, hardware target **f7**, SDK API **87.1**, with uFBT **0.2.6**. Compilation and exported-symbol checks pass. Host protocol tests pass. No Flipper hardware was available: wireless delivery, device UI, SD persistence, power use, and range have **not** been verified on a device. A compiled application does not prove over-the-air exchange works.
This flipper application was made with the help of Codex and GPt-6 Astra, because I (the human Ben) am garbage at writing C and organizing code.

## Install

1. Use a Flipper Zero running official firmware 1.4.3 with a working microSD card. Other releases require an SDK-compatible build; custom firmware is not required or tested.
2. Copy `dist/flipperpass.fap` to the SD card's `apps/Sub-GHz/` directory using qFlipper's file manager.
3. Open **Apps → Sub-GHz → FlipperPass**.
4. Press Back from the initial status screen to reach the menu. Set **Edit nickname**, **Edit status**, **Choose avatar**, and **Current event/location**.
5. In **Radio band / Off**, choose the same locally permitted frequency on both Flippers. Firmware regional restrictions are enforced. On first launch the radio is off; after selection, the saved band starts automatically on future launches.
6. Keep both apps open. First beacon is queued after 2–6 seconds; subsequent beacons every 12–18 seconds. Allow at least 60 seconds during testing. **Exchange status** displays the worker state and saved-card count, not confirmation that another device received your profile.
7. Open **Collected cards**, select a card, and read its avatar, status, and `At:` label. Back returns to the list; Back from the main menu exits and stops the radio. Editing/browsing keeps radio exchange active. **Radio band / Off → Off** stops exchange while preserving browsing.

An event label is local context, not a location claimed by the sender. Change it when you move events. The default is `Unlabeled`. The label is never broadcast. You may use different labels on the two devices; each stores its own label.

## Included behavior

- Nickname: 12 printable ASCII characters; status: 30; event label: 24.
- Four 12×12 pixel avatars: Smile, Cat, Robot, Ghost.
- Random, persisted 64-bit app identity, independent of the device serial number.
- Up to 100 saved cards. One snapshot per sender identity **and exact local event label**. Repeated beacons do not add cards or update the original snapshot. The same person at another event can have a separate card. No repeat counts.
- Incoming frames require recognized magic/version, valid avatar/text, and a CRC. Noise, incomplete frames, and unrelated traffic are discarded with stream resynchronization.
- Cards are only added to the visible collection after successful SD persistence. Full collections keep existing cards. Status displays a full/SD error condition; corrupt files are skipped and their slots reserved.
- Saved data lives in `apps_data/flipperpass/`. `profile.dat` holds profile, identity, event, and band; `profile.bak` is the previous configuration. `card000.dat` through `card099.dat` hold cards. Temporary files are staged before rename. Back up this directory before manual changes. To clear cards, exit the app and delete only `cardNNN.dat` files. Deleting the profile and backup creates a new identity on next launch.

Profiles are public, unauthenticated radio broadcasts. Do not put secrets in them. CRC detects corruption, not impersonation. Copying the profile data to another Flipper clones its identity. Labels are case-sensitive; no timestamp or encounter history is recorded.

## Why Sub-GHz

The stock SDK exposes `SubGhzTxRxWorker`, a byte-stream worker using the built-in CC1101 and the firmware GFSK preset. Its packet chunk size is 60 bytes, matching our complete frame. The worker handles receiving between transmissions. We verified the required symbols are exported by the installed 1.4.3 SDK and the resulting `.fap` passes uFBT's strict import check.

This is a half-duplex, best-effort broadcast protocol. There is no acknowledgement, pairing, encryption, carrier-sense scheduler, or guaranteed exchange; multiple senders can collide. Random timing reduces synchronized collisions. Sub-GHz range is not a measure of physical proximity; receiving a profile does not prove the person was in the same room. A crowded event needs field testing.

Available frequencies are 915.00, 433.92, and 868.35 MHz. Both devices must use the same frequency and their configured firmware regions must permit it. Firmware allowing a frequency is not a certification of this app or assurance that every use meets local power, airtime, and channel rules. Choose an appropriate frequency for the test location; do not bypass region restrictions. No radio transmission was performed during development.

An explicit region check runs **before** starting the SDK worker: release 1.4.3's worker starts its thread even when its own frequency check fails. The app also validates the internal device and frequency, and stops/joins the worker before reconfiguration or exit. This app operates only in the foreground and does not provide background scanning after exit.

Official sources checked:

- [uFBT installation and build instructions](https://github.com/flipperdevices/flipperzero-ufbt)
- [Firmware 1.4.3 packet worker implementation](https://github.com/flipperdevices/flipperzero-firmware/blob/1.4.3/lib/subghz/subghz_tx_rx_worker.c)
- [Firmware 1.4.3 packet worker API](https://github.com/flipperdevices/flipperzero-firmware/blob/1.4.3/lib/subghz/subghz_tx_rx_worker.h)
- [Firmware 1.4.3 exported SDK symbols](https://github.com/flipperdevices/flipperzero-firmware/blob/1.4.3/targets/f7/api_symbols.csv)

## Build and validate

The tools are kept inside this project; nothing needs a global Python installation or changes to the home uFBT cache.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install ufbt==0.2.6
export UFBT_HOME="$PWD/.ufbt"
.venv/bin/ufbt update --branch=1.4.3
.venv/bin/ufbt
./tests/run.sh
.venv/bin/ufbt lint
```

The output is `dist/flipperpass.fap`; `dist/debug/flipperpass_d.elf` includes debug symbols. `UFBT_HOME="$PWD/.ufbt" .venv/bin/ufbt launch` can install and launch via USB when a device is connected. It does not replace the firmware.

Host tests compile the actual protocol C module with strict warnings and UndefinedBehaviorSanitizer. They cover a known CRC vector, maximum text lengths, round-trip encoding, all 480 single-bit mutations, invalid versions/avatars/text, every stream split/truncation point, consecutive frames, and recovery after one million deterministic noise bytes. These are protocol tests, not an emulator or radio test. Optional `SANITIZERS=address,undefined ./tests/run.sh` enables ASan where supported; ASan failed during runtime initialization on the development Mac, before executing tests.

See [TESTING.md](TESTING.md) for the real-device acceptance checklist and [PROTOCOL.md](PROTOCOL.md) for the frame/storage formats.
