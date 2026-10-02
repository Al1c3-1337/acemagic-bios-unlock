# RX16 Toolkit

One USB-bootable UEFI application for the **ACEMAGIC RX16 with A-004 firmware**. Unlock the hidden advanced BIOS menus, open the manufacturer's RAM controls, or collect a short diagnostic log.

The application edits menu definitions in memory and opens the original BIOS interface. You decide which settings to save using the BIOS's own save command.

**Status:** `1.0.0-rc.1`. The owner reported that both separate tools work on the target RX16. The combined application has passed offline firmware and simulated-service tests; this exact combined binary still needs a hardware smoke test. See [compatibility and testing](docs/COMPATIBILITY.md).

## Supported firmware

| Item | Supported target |
| --- | --- |
| Machine / board | ACEMAGIC RX16 / `AKN79C_H` |
| BIOS | `CT_BI_AMI_RX16_AKN79C_A-004` |
| BIOS date | 2025-11-10 |
| Architecture | x64 UEFI |
| Observed CPU | AMD Ryzen 7 255 |

The RX16 name alone is insufficient. The app checks exact loaded Setup and AMITSE image sizes and code fingerprints, original menu tables, and the expected form structure. A mismatch stops the operation. Do not remove these checks to try another BIOS.

## Start here

1. Download **`rx16-toolkit-1.0.0-rc.1-usb.zip`** from the release assets. GitHub's automatic source archives are for developers.
2. Extract its contents to the root of a FAT32 USB. Preserve existing USB files before replacing them. The boot file must be at `EFI/BOOT/BOOTX64.EFI`.
3. Boot the USB's **UEFI** entry. If you already use an EFI shell, run `RX16Toolkit.efi` directly. No `startup.nsh` or `echo` command is needed.
4. Select a mode, then press **O** to open the original BIOS.

| Key | At the initial menu |
| --- | --- |
| **U** | Prepare advanced-menu unlock |
| **M** | Prepare the manufacturer's RAM page; unlock menus first |
| **D** | Read and log saved settings again |
| **Q** | Exit without changing menus |

After a mode is prepared: **O** opens BIOS, **D** reads settings, **Q** returns while keeping temporary menus for that boot, and **R** restores the original menu definitions. Reboot before switching modes or running another BIOS-menu tool.

The EFI application is **unsigned**. Secure Boot may refuse to launch it. Use an already authorized boot method; this tool does not change Secure Boot settings or keys. A launch-time Security Violation is distinct from a BIOS-variable save error.

## Unlock the advanced menus

1. Select **U**, then **O**.
2. In **Main**, set **Setup Item Hide Control → Disabled**.
3. Use the BIOS's **Save and Exit** command, normally **F10**.
4. Re-enter regular BIOS without the USB and check that the advanced menus remain visible.

The temporary menu edits disappear at reboot. The setting you deliberately saved can persist. If the browser does not open, use **Q**, then choose **Enter Setup in the same boot**.

## Make a RAM underclock persist

The manufacturer's timing switch can reset the AMD CBS switch to Auto while leaving the target speed saved. Set both copies as follows; the two saves are intentional.

1. With advanced menus already unlocked, boot the tool, select **M**, then **O**.
2. In **Memory Configuration**, set **Active Memory Timing Settings → Enabled** and **Memory Target Speed → 3200**. The speed is in **MT/s**. Use Enter or the BIOS +/- controls to edit it. This page temporarily takes the place of the Advanced tab.
3. Save and reboot.
4. Enter regular BIOS and open **AMD CBS → UMC Common Options → DDR Options → DDR Timing Configuration → Accept**.
5. Set **Active Memory Timing Settings → Enabled again**, confirm **Memory Target Speed → 3200**, and save again.
6. Check CPU-Z's **Memory** tab. About **1600 MHz** corresponds to DDR5-3200; about **2400 MHz** corresponds to DDR5-4800. The SPD tab shows module specifications, not the current operating speed.
7. Reboot once more and verify both the setting and operating speed persist.

Leave RAM timings and voltages unchanged. Exposing a value does not establish that every DIMM/CPU combination supports it. The owner reported this workflow working; other configurations remain unverified.

## Risk and recovery

Firmware settings, including an underclock, can prevent memory training and boot. **Removing the USB or selecting R does not undo saved settings.** There is no validated RX16 recovery procedure included here, and a CMOS reset is not guaranteed to clear these UEFI settings.

If the machine still boots and you want automatic RAM settings again, use **M** to set the manufacturer's timing switch to **Auto**, save, and also return the AMD timing switch to **Auto**. Avoid loading all BIOS defaults merely to undo the RAM change: defaults may hide the menus again.

## Troubleshooting

- **A compatibility check fails:** stop and keep the log. Do not force the tool past a mismatch.
- **M opens Main:** select the temporary **Memory Configuration** tab.
- **O does not open BIOS:** press **Q**, then enter Setup without rebooting.
- **Values still reset:** boot the tool again, then exit with **Q**. It records both saved copies before any menu edit.
- **A save returns Security Violation:** keep the exact error and the log. This app retains the original BIOS save handler but does not guarantee every protected setting is writable.
- **Update/restore verification fails:** reboot before entering BIOS or trying another mode.

`RX16_TOOLKIT_LOG.txt` is written to the USB root and replaced at each launch. It records firmware-check results, the hide flag, and OEM/AMD RAM values. It does not dump the ROM or all UEFI variables. Copy a log before another run if you need to keep it, and review it before sharing.

Report problems using the issue template. Include the exact BIOS version and whether the setting reverts or remains selected while the measured speed is unchanged. Do not post full BIOS dumps or personal identifiers.

## Build, inspect, and release

- [Build and test](docs/BUILDING.md)
- [How the two operations work](docs/DESIGN.md)
- [Compatibility and verification status](docs/COMPATIBILITY.md)
- [Create a GitHub release](docs/RELEASING.md)
- [Release notes](RELEASE_NOTES.md) and [changelog](CHANGELOG.md)

The project contains no full ROM images, flashing utility, shell binary, or embedded private logs. It does not program flash, invoke `SetVariable`, save settings automatically, or disable firmware protection. The user-initiated save occurs inside the original firmware browser.

## License and credits

Project-authored code and documentation use the [MIT license](LICENSE). EDK II interface declarations are used under their upstream terms; see [third-party notices](THIRD_PARTY_NOTICES.md). Vendor names identify the target hardware and firmware. This is an independent project, without affiliation with ACEMAGIC, AMI, or AMD.
