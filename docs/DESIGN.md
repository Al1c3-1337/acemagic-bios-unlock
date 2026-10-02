# Implementation notes

## Runtime sequence

The app reads a small set of Setup and AMD CBS values, locates the loaded firmware modules, validates their identities, and asks for a mode. It exports the existing Setup HII package, builds a separate modified copy, updates that same package, verifies read-back and the original configuration owner, and finally adjusts the relevant AMITSE menu table.

The app calls the existing `EFI_FORM_BROWSER2_PROTOCOL.SendForm`. It preserves the HII handle and `EFI_HII_CONFIG_ACCESS_PROTOCOL` owner so the original firmware performs user-requested saves. It does not create substitute storage, call `SetVariable`, or request a reboot.

Only one mode is active per launch. Running another mode requires a reboot. R restores the original menu definitions and verifies the restored HII package; it does not revert saved variables. Failed update/read-back triggers restoration before the app exits.

## Guarded identities

| Component | Identity/check |
| --- | --- |
| Setup module | `899407D7-99FE-43D8-9A21-79EC328CAC21`, loaded size `0x32500` |
| AMITSE module | `B1DA0ADF-4F77-4070-A88E-BFFE1C60529A`, loaded size `0x69C80` |
| Setup form set | `7B59104A-C00D-4158-87FF-F04D6396A915` |
| Setup variable | `EC87D643-EBA4-4BB5-A1E5-3F3E36B20DA9`, size `0xC9` |
| AMD variable | `AmdSetupPHX`, `3A997502-647A-4C82-998E-52EF9486A247`, size `0x67F` |

Exact code-section FNV-1a fingerprints, tables, and storage bindings are in the source. These fingerprints are compatibility checks, not cryptographic firmware authentication. The app relies on the platform's existing boot trust model.

## Unlock mode

The Setup HII parser identifies 22 gates comparing the hide question with 1 and changes the comparisons to 2. It changes the single suppression expression hiding the hide-control question from True to False. It changes the restricted AMITSE top-level entry from form `0x2712` to `0x2713`. The original browser opens Main (`0x2711`), where the user saves Setup byte `0x02` as 0.

## RAM mode

The exact OEM Memory Configuration form (`0x279C`) is replaced in the existing package. Its two conditional speed selectors become numeric questions with minimum 3200 MT/s and step 200. Existing maximums/defaults of 5600 and 4800 are retained; these are UI bounds, not proof that every value works. The active switch, permissions, question IDs, storage offsets, and other forms stay unchanged. Two AMITSE data bytes route the full/restricted Advanced tab to this page for the current boot.

The manufacturer and AMD menus use different variable fields:

| Control | OEM Setup | AMD AmdSetupPHX |
| --- | --- | --- |
| Active Memory Timing Settings | byte `0xAF` | byte `0x5E` |
| Memory Target Speed, UINT16 MT/s | `0xB0` | `0x5F` |

For the active switch, `0xFF` is Auto and `0x01` is Enabled.

In the reference `OemSyncCBSSetup` module, the callback at RVA `0x9B4` reads Setup and AmdSetupPHX. Instructions at `0xB9E–0xBB0` reset the AMD switch to Auto if the OEM switch is Auto. Instructions at `0xBB3–0xBC8` copy the OEM speed and enable AMD timing only if the OEM switch is Enabled **and the speeds differ**. This accounts for the two-save procedure: setting the master copy does not always re-enable AMD timing when the saved speeds already match.

## References

- [UEFI HII architecture and numeric questions](https://uefi.org/specs/UEFI/2.10/33_Human_Interface_Infrastructure.html#efi-ifr-numeric)
- [UEFI configuration processing and browser protocol](https://uefi.org/specs/UEFI/2.10/35_HII_Configuration_Processing_and_Browser_Protocol.html)
- [Pinned EDK II revision](https://github.com/tianocore/edk2/commit/b03a21a63e3bd001f52c527e5a57feddb53a690b)
