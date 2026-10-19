# Install and launch Retro Attachments

Version 0.1.3-alpha.1 changes graphical launch to PS5 Homebrew Launcher / websrv. Previous Payload Manager tests failed during direct display-memory allocation. This new path is not yet confirmed on the user's PS5.

## Test the viewer first

1. Download `Retro-Attachments-Homebrew-Launcher.zip`.
2. Extract the contained `homebrew` folder onto your USB root. The viewer must be at `homebrew/RetroAttachments/eboot.elf`; the installer is `homebrew/RetroAttachmentsInstaller/eboot.elf`. Alternatively copy these two app folders into `/data/homebrew/` over FTP. Do not overwrite unrelated folders.
3. Close any game or other graphical homebrew first; websrv's graphical loader can terminate a running BigApp when launching another.
4. Start your jailbreak and Homebrew Launcher / websrv. Refresh its homebrew library, normally at `http://YOUR-PS5-IP:8080/`.
5. Launch **RetroAttachments** from the library as graphical homebrew. Do not send this viewer through Payload Manager or the websrv `/elfldr` upload page. Do not select daemon/background execution.
6. Check display, controller input, demo, and clean exit before connecting your account.

The folder layout follows [websrv's homebrew instructions](https://github.com/ps5-payload-dev/websrv#installing-homebrew). No firmware compatibility is inferred from this layout alone. If startup still fails, report its last notification or `/data/retro-attachments-startup.log`.

## Optional home-screen shortcut

1. Once the viewer works, launch **RetroAttachmentsInstaller** from that same Homebrew Launcher library. It also needs graphical-app launch, not ordinary payload injection.
2. Review `/data/homebrew/PPSA99841`, then press Cross to install or Circle to cancel. The installer verifies and copies files; it does not register titles itself.
3. Exit and let compatible ShadowMountPlus folder scanning register the title.
4. Keep websrv running on port **8080**, then open the Retro Attachments home-screen tile.

The native shortcut verifies `/data/homebrew/PPSA99841/retroatlas.elf` and sends a local HTTP request to websrv's `/hbldr` endpoint with daemon mode disabled. It no longer streams the ELF to port 9021. The stable installed path is used because websrv may terminate the shortcut and unmount its `/app0` while starting the viewer. Sending the request is not proof that the viewer launched.

The installer requires writable `/data` and room for the app, staging files, and retained update backup. ShadowMountPlus must support folder scanning; original ShadowMount support is not established. See [upstream ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus).

## Manual shortcut installation

Extract `PPSA99841.zip` into `/data/homebrew/`, producing `/data/homebrew/PPSA99841/eboot.bin`. Preserve executable permissions on the binaries. Never overwrite an unrelated folder with this title ID. Let ShadowMountPlus register it and keep websrv on port 8080 running. For first testing, the USB Homebrew Launcher package is simpler and does not require installing this shortcut.

## Updates and recovery

- Before writing, the installer validates embedded SHA-256 hashes and checks storage availability.
- Files are staged under `/data/retroatlas-installer/stage-<pid>`, flushed, reread, and verified before publication.
- If the destination exists, it must have the exact `.retroatlas-owned` marker. Otherwise installation stops without replacing that destination.
- On an update, the old directory is moved to `/data/retroatlas-installer/previous-<pid>`. The complete staged directory is then renamed into place. The old backup is retained.
- A normal failure before publication restores the old directory. A power loss between the two renames can leave the app temporarily absent; the backup remains available for manual recovery. This is not a power-loss-atomic directory exchange.
- If an interrupted install leaves `install.lock`, first confirm **no installer is running**. Inspect the target, staging folder and backup before clearing the empty lock directory with your file manager. If the target is missing and a backup exists, restore that complete backup directory to `/data/homebrew/PPSA99841` before retrying. Preserve files you are uncertain about.
- If a backup or staging name already exists for the new process ID, installation stops instead of overwriting it. Resolve the previous interrupted attempt first.
- After replacing files, restart/rescan using your loader's documented process; it may still have an older app folder mounted. Avoid running an update while the viewer is open.

Account settings stay in `/data/retroatlas/settings.json`, outside the installed title. The installer never reads or changes that directory. It performs no downloads and contains no API key.

## Removing the app

Close the viewer, use ShadowMountPlus's documented unmount/unregister process, and remove only `/data/homebrew/PPSA99841` after confirming it is your Retro Attachments folder. Account data and installer backups are separate; keep them if you want to restore later. This installer does not offer automatic uninstallation.

## Before a public release

Record the exact console firmware, jailbreak, ShadowMountPlus version and Homebrew Launcher / websrv version. Verify fresh install, tile registration, Demo Preview, live account access, input, clean exit, relaunch, theme/settings persistence and an update. Confirm the development title ID `PPSA99841` does not conflict with another app in your intended distribution. Do not label the build firmware-compatible based on compilation alone.
