# Real-device acceptance checklist

All items below are **pending**, not completed tests. Use two Flipper Zeros on official firmware 1.4.3, with microSD cards and the same build. Record firmware, region, frequency, distance, surroundings, and results. A third device is useful for collision tests.

1. **Launch / regional gating:** Start with no app data. App selects the first device-supported, firmware-permitted frequency in preference order 433.92, then 868.35 MHz and does not crash; if none qualifies, it stays off. Verify the selected frequency in status. Verify existing profiles preserve each saved band and explicit Off across restart. Set profiles and events. Try a firmware-blocked band: status says region-blocked and the app remains usable. Do not modify the region to force transmission.
2. **UI / editing:** Verify the passport name display; set 30-character statuses, all four avatars, and 24-character labels. Confirm rendering, wrapping, scrolling, and Back navigation. Save an empty status. Cancel an edit with Back and confirm the old value persists. Verify saved changes survive exit/relaunch.
3. **Basic exchange:** Verify A and B show their respective passport names in Exchange status, then give them different avatars and messages; set both local labels to DEF CON. Select a common permitted frequency. At 1–3 metres separation, leave both open for 60 seconds. Each should save the other, show the expected profile and DEF CON, and exclude itself. Record time to first card. If this fails, do not mark wireless exchange verified.
4. **Duplicate behavior:** Remain nearby for another 2 minutes and reopen both apps. Each collection should still contain one card for that sender at DEF CON. No counter is displayed. Change the sender's status: the existing card remains its original snapshot.
5. **Location provenance:** Change only A's local event to GrrCON. After another beacon from B, A should have a separate B card marked GrrCON while its original DEF CON card remains unchanged. B still tags incoming profiles with B's own label. Confirm labels are local even when the peer's event differs.
6. **Persistence:** Exit both, power-cycle, and browse cards with radio Off. Check avatars, full statuses, event labels, and identity-based duplicate behavior after restarting radio. Repeat while editing profile and browsing cards to confirm automatic reception continues.
7. **Failure handling:** With backups made, test missing/full SD, truncated card files, and damaged `profile.dat` with an intact `profile.bak`. Failed saves must not appear as saved cards. Damaged cards must not crash or be overwritten automatically. Failed configuration save must prevent radio start. Error state is latched until app restart.
8. **Collection limit:** On a separate test SD, prepare 100 distinct valid record slots (use the documented format and recompute CRCs). A new sender must not evict old cards or exceed 100. Status should say Collection full. Repeat with a damaged occupied slot and confirm it is reserved.
9. **Lifecycle:** Repeatedly switch bands and Off, edit profile/event while active, exit, and reopen. Confirm no crash and that the normal Sub-GHz app can use the radio afterward. With a separate receiver verify broadcasts stop on exit/Off. App status alone cannot establish this.
10. **Radio conditions:** Test differing bands (no exchange), interference, simultaneous launches, 3+ participants, increasing distance, and ordinary pocket carry. Measure delivery rate over several minutes. There is no ACK; beacons can be lost and one direction can succeed before the other. Do not advertise a range or crowded-event reliability before measuring it.
11. **Passport name migration:** Launch with an existing profile containing a custom nickname. Exchange status and outgoing cards must use the device passport name, with no Edit nickname menu item. Restart and verify the name remains correct; identity, band, event, avatar, status, and collected cards remain intact. Existing collected snapshots keep their original names.

## Development checks actually completed

- Official uFBT 0.2.6 build against firmware 1.4.3 / f7 / API 87.1.
- Strict FAP import validation (`APPCHK`).
- Host C protocol tests with UndefinedBehaviorSanitizer.
- uFBT source formatting/lint (see final build report).

Not completed: device launch, visual verification on a 128×64 screen, SD lifecycle/fault tests, RF transmission/reception, regional behavior on a provisioned Flipper, battery measurements, or field testing. AddressSanitizer was attempted but its host runtime failed to initialize; no ASan result is claimed.
