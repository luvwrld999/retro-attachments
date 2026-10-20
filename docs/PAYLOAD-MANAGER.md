# Launch from Payload Manager

> **DO NOT RUN v0.1.4-alpha.1 on PS5.** A user reported a full-console crash after launching it through Payload Manager on firmware 13.60. The failing stage and root cause are not yet established. Testing is suspended; no replacement build is verified. Local tests did not validate the console launch handoff.

Version 0.1.4-alpha.1 adds dedicated Payload Manager entry points.

1. Start websrv on port 8080 using your existing setup. You do not need to open its library.
2. Close any running game or graphical homebrew; websrv may terminate it during launch.
3. Download `Retro-Attachments-Payload-Manager.zip` and select `RetroAttachments/RetroAttachments.elf` in Payload Manager.
4. Test the viewer first. For the optional home-screen installation, select `RetroAttachmentsInstaller/RetroAttachmentsInstaller.elf` in Payload Manager and confirm inside the installer.

These ELFs contain the graphical application. They stage and verify it under `/data/retro-attachments-payloads/`, then request websrv's graphical launch. No folder copying, manual library launch, SDL initialization, or display allocation is performed by the bridge. Websrv must be running; installation alone is insufficient. Allow writable `/data` and space for the viewer and installer copies (approximately 35 MB, plus older versions). Old versioned copies are retained; do not remove them while an app is running. Account settings remain under `/data/retroatlas/`.

Use these dedicated entry points, not the old lowercase raw graphical ELFs or the Homebrew Launcher ZIP. Names alone do not change how a raw graphical ELF is injected.

The bridge reports staging errors, unavailable websrv, rejected requests, and missing acknowledgements through notifications and `/data/retro-attachments-startup.log`. A successful websrv response is not proof that a frame appeared. PS5 13.60 compatibility remains pending hardware testing.
