# Contributing

Build and test with `make desktop test test-installer`. After configuring the pinned PS5 SDK, run `make installer test-bundle`. See README.md for SDK installation and docs/INSTALL.md for deployment and recovery.

Keep the payload viewer, native launcher and installer separate. Never write outside the documented install/data paths or overwrite an unowned title. Add failure-path tests when changing file installation, rollback or payload streaming.

No credentials, user data, private console addresses or SDK caches belong in source control. Build outputs are ignored and can be uploaded as release artifacts with source and licenses.

Firmware support claims require recorded on-console results. The GitHub workflow builds hardware-test artifacts; it does not publish a release or assert console compatibility.
