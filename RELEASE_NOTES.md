# RX16 Toolkit 1.0.0-rc.1

One USB application for advanced BIOS menu access and persistent RAM settings on the ACEMAGIC RX16 with `CT_BI_AMI_RX16_AKN79C_A-004` firmware.

Choose **U** for advanced-menu unlock, **M** for the manufacturer's RAM page, or **D** for diagnostics. Changes to menu definitions last until reboot; settings are saved only when you save them in the original BIOS.

## Downloads

- **`rx16-toolkit-1.0.0-rc.1-usb.zip`** — ready for a FAT32 USB; includes `EFI/BOOT/BOOTX64.EFI`, `RX16Toolkit.efi`, instructions, license notices, and checksums.
- **`rx16-toolkit-1.0.0-rc.1-source.zip`** — complete source, tests, documentation, and GitHub workflows.
- **`SHA256SUMS.txt`** — hashes of both release archives.

Extract the USB ZIP's contents to the USB root, boot its UEFI entry, select a mode, then press **O**. Follow the included instructions, particularly the two-save procedure for RAM settings.

## Compatibility and status

Only the A-004 firmware listed above is supported. Exact firmware checks stop on mismatches. The EFI binary is unsigned; Secure Boot may refuse it.

The owner confirmed that the separate menu-unlock and RAM tools work on the target machine. Both operations in the combined source passed offline checks against the original firmware, along with simulated BIOS-service tests. **This exact combined release candidate has not yet been boot-tested.** Broader hardware compatibility is not established.

RAM changes can prevent boot. Removing the USB does not undo saved settings, and no guaranteed recovery method is included. Leave timings and voltages unchanged during the documented underclock procedure.

No ROM image or flash utility is included. If something fails, provide the exact BIOS version and the USB's `RX16_TOOLKIT_LOG.txt` after reviewing it for privacy.
