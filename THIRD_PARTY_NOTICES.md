# Third-party notices

## EDK II

Builds use UEFI interface declarations from [TianoCore EDK II](https://github.com/tianocore/edk2), pinned by `EDK2_COMMIT` to `b03a21a63e3bd001f52c527e5a57feddb53a690b`.

The upstream project license is reproduced in [docs/EDK2-LICENSE.txt](docs/EDK2-LICENSE.txt), **BSD-2-Clause-Patent**. Individual header notices also apply, including Intel Corporation's copyright notices (2006–2019 in the UEFI/processor interfaces used here). Downloaded headers retain their original notices. EDK II source is fetched as a build dependency rather than bundled into the source release.

## Development references

Offline research used UEFITool/UEFIExtract, IFRExtractor, pefile, and Capstone. These tools are not bundled or required to run the release. The public build and tests use Python's standard library, LLVM, and EDK II headers.
