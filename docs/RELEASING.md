# Release on GitHub

The prepared source repository and release assets can be uploaded to a new GitHub repository named `rx16-toolkit`. Repository creation and publication are separate from local preparation.

## First upload

1. Create an empty GitHub repository with the visibility you want.
2. Put the source ZIP's contents in the repository root, including `.github`. Do not upload the outer ZIP as the source tree. Alternatively use the prepared source directory with Git.
3. Push the source to the default branch, normally `main`.
4. Wait for **Build and test** to pass. The job builds from the pinned EDK II revision, runs public tests, verifies release archives, and uploads them as an Actions artifact.

No usernames, remote URLs, secrets, or owner-specific settings are hard-coded. The included GitHub Actions are pinned to exact upstream commits.

## Draft release

Update `VERSION`, `CHANGELOG.md`, `RELEASE_NOTES.md`, and the compatibility record together when the release changes. For this prepared candidate, use tag **`v1.0.0-rc.1`**.

Pushing a `v*` tag runs **Draft release**. That workflow rebuilds and tests the tagged source, checks that the tag matches VERSION, and creates a **draft** GitHub release with the USB ZIP, source ZIP, and checksum file. A version containing a hyphen is marked as a prerelease. It will not replace an existing release or publish the draft automatically.

For a manual release, open GitHub's Releases page, create a draft for the same tag, paste `RELEASE_NOTES.md`, and attach these three files from `dist`:

```text
rx16-toolkit-1.0.0-rc.1-usb.zip
rx16-toolkit-1.0.0-rc.1-source.zip
SHA256SUMS.txt
```

The **USB ZIP** is the user download. GitHub's automatically generated source archives do not contain a built EFI binary.

Before publishing, review the supported BIOS version and test-status wording. Keep this candidate marked prerelease until the combined binary's hardware smoke test is recorded. The two precursor tools have owner confirmation; the newly combined binary is not covered by that earlier report.

## Stable promotion

Record the exact tested EFI SHA-256 and results for U, M, native saving, persistence across reboots, and diagnostic logging. Then update the compatibility record, remove the prerelease suffix in VERSION, rebuild/test/package, and publish from a matching tag. Retain relevant limitations for other firmware and RAM configurations.

References: [GitHub release creation](https://cli.github.com/manual/gh_release_create), [GitHub Actions artifacts](https://github.com/actions/upload-artifact).
