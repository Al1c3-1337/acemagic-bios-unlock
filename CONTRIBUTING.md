# Contributing

Start with [the build instructions](docs/BUILDING.md). Keep changes small, preserve the firmware checks, and add tests for new parsing or mutation behavior.

New firmware support requires separate verified identifiers, menu/variable mappings, offline fixtures kept private where appropriate, a recovery plan, and a hardware test report. A matching model name or AMI version is not evidence of compatibility. Do not broaden this release's guards to accept an unverified BIOS.

Do not commit ROM dumps, full vendor firmware modules, personal CPU-Z reports, serial numbers, license keys, or private logs. Synthetic test inputs are preferred in public pull requests.

For behavior changes, include the checks run, any changed saved-variable bindings, and whether the exact produced binary has been tested on hardware. Keep README, release notes, VERSION, and compatibility records consistent.
