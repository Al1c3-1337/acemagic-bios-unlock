# Build and test

Requirements: Python 3.10+, Git, and LLVM with `clang` and `lld-link`. No Python packages or Windows SDK are required. Windows x64 and Linux x64 are the intended build hosts; the generated application always targets x64 UEFI.

On Ubuntu 24.04, install `clang`, `lld`, `python3`, and `git`. On Windows, install LLVM or Visual Studio's LLVM component. `CLANG` and `LLD_LINK` environment variables can select explicit executable paths. Otherwise tools are located through PATH or the usual Visual Studio LLVM directory.

From the repository root:

```text
python tools/fetch_edk2.py
python tools/build.py
python tests/test_core.py
python tools/package.py
```

`fetch_edk2.py` downloads only the pinned EDK II header tree into `.deps/edk2`. `build.py` can alternatively take `--edk2-include PATH` pointing to the pinned `MdePkg/Include` directory. Use the revision in `EDK2_COMMIT`; selecting unrelated headers is not an equivalent build.

Outputs:

- `build/RX16Toolkit.efi`: unsigned EFI application.
- `build/core.dll` or `.so`: native test library, never included in the USB archive.
- `build/mock.dll` or `.so`: simulated EFI services for private integration testing.
- `build/build.json` and `test-results.json`: build/test records.
- `dist/`: versioned USB and source ZIPs plus `SHA256SUMS.txt`.

The linker timestamp is fixed. Rebuilding with the same source, headers, compiler and linker should reproduce the EFI hash. Different LLVM versions may produce different binaries. ZIP metadata is fixed by the packaging script. Release manifests intentionally omit local usernames and absolute build paths.

## Optional reference firmware checks

Public tests construct their own small HII fixture. They exercise both mode transformations, malformed/truncated packages, targeted and random mutations, output bounds, and unsupported-image refusal. They require no ROM dump.

For private validation against the reference firmware, supply an extracted Setup PE body and AMITSE PE body:

```text
python tests/test_core.py --setup private-fixtures/Setup.bin --amitse private-fixtures/AMITSE.bin
```

These checks additionally verify the real HII package and loaded image fingerprints, table changes and restoration, and the actual application flow using mocked firmware protocols. Update failure, corrupted read-back and owner mismatch must restore the package without writing settings. Private fixtures are ignored by Git and excluded from source/release packaging.

No offline check replaces a hardware test. The core test report records whether private firmware checks were run. The release package includes the report for its exact binary hash.

## Packaging integrity

`package.py` refuses a failed/missing test report, a mismatched EFI hash, or changed build inputs. It uses an explicit public-file allowlist rather than archiving the working directory. It verifies each ZIP and its internal hashes. `python tools/check_release.py` checks the finished archives and public metadata again.
