# Compatibility and verification

## Reference target

- ACEMAGIC RX16 V1.0, board `AKN79C_H`.
- BIOS `CT_BI_AMI_RX16_AKN79C_A-004`, dated 2025-11-10.
- AMD Ryzen 7 255; observed DDR5-4800 SO-DIMM configuration: 16 GB plus 8 GB.

Reference ROM SHA-256:
`579160f5015887672b586c0bc5c3400735c4c957bf69ad972e83de6753f0ea62`.
The dump itself is deliberately excluded.

## Evidence as of 2026-10-02

| Check | Result |
| --- | --- |
| Separate advanced-menu tool | Owner confirmed unlock persists after reboot without USB |
| Separate OEM RAM tool and two-save workflow | Owner reported everything works |
| Exact combined binary on hardware | Pending |
| Combined core against reference Setup and AMITSE | Passed locally |
| Combined application's EFI-service flow | Passed in a host simulation, not firmware execution |
| Public synthetic parser/mutation tests | Passed locally; CI workflow included |
| GitHub-hosted workflow execution | Pending repository upload |
| Other RX16 revisions / DIMMs / BIOS versions | Unverified |

The owner's RAM confirmation is a user report, not an independently captured frequency or stress-test result. Do not describe this as validation across all RX16 systems.

The local simulation exercises both native form IDs, diagnostics, exit, undo, partial update failure, corrupted read-back, and changed configuration ownership. It checks that the application makes no direct settings writes. It cannot validate actual firmware protocol behavior, memory training, or save persistence.

Before promoting the combined release to stable, record its SHA-256 and test both modes on the reference machine, including another reboot for saved-setting persistence. Someone with already-unlocked menus can check U without changing unrelated settings. Preserve the original separate-tool packages while doing that check.
