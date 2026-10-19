# Install the Retro Attachments home-screen launcher

This is an initial hardware-test build. **The installer, home-screen launch path and app have not been tested on a PS5, including firmware 13.60.** Host tests and format checks do not establish console compatibility.

## What you get

- `retro-attachments-installer.elf`: a graphical, offline installer containing every app file. Installation starts only after pressing Cross or Enter.
- `PPSA99841.zip`: the same app folder for manual FTP installation.
- `retro-attachments.elf`: the original standalone viewer payload.

The installed title is a native PS5 **launcher** (`eboot.bin`, `sce_module/libc.prx`, icon and metadata). It streams the bundled viewer ELF from `/app0/retroatlas.elf` to the console's own ELF loader at `127.0.0.1:9021`, then exits. The viewer runs as a payload. This is not a self-contained native port of the viewer or a signed retail PKG, and the home-screen tile cannot replace your jailbreak/ELF loader. No exploit or firmware patches are included.

## Console requirements

- Your working jailbreak and permission to execute graphical PS5 payloads.
- An ELF loader accepting raw ELF streams on **port 9021** for launching the installed tile. The installer itself may be launched by any compatible ELF loader.
- **ShadowMountPlus with folder scanning of `/data/homebrew`**. Target the modern 1.7beta4-or-newer folder workflow; this project has not verified it on hardware. Original ShadowMount compatibility is not established. See [ShadowMountPlus upstream](https://github.com/drakmor/ShadowMountPlus).
- Writable `/data` and sufficient free storage for the app plus an 8 MiB staging margin. Updates keep the previous app folder, so retain additional space for backups.

## Installer steps

1. Close Retro Attachments if it is running. Start your jailbreak, ShadowMountPlus and ELF loader as usual.
2. Send **`retro-attachments-installer.elf`** through your normal payload-loading method.
3. Review the destination shown on screen: `/data/homebrew/PPSA99841`.
4. Press **Cross** to install, or **Circle** to cancel. Keep the console on until installation finishes.
5. When the installer reports that the **files** are installed, press Circle to exit. Let ShadowMountPlus scan the folder. Its registration step is separate from the installer's file copy.
6. Open the **Retro Attachments** tile. Keep the ELF loader on port 9021 active. Try Demo Preview first, then configure your account if display/input/exit all work.

The native launcher reports missing-payload, connection and transfer errors through console notifications. A successful stream transfer is not proof that the viewer opened; confirm its interface appears.

If the tile does not appear, inspect ShadowMountPlus's own status and folder-scanning configuration. The installer does **not** modify the console's application database or directly register/unregister titles. Do not assume a successful copy means a title was registered.

## Manual alternative

Extract `PPSA99841.zip` and upload the complete `PPSA99841` folder to `/data/homebrew/`. The executable must be exactly:

```text
/data/homebrew/PPSA99841/eboot.bin
```

Avoid an extra nested `PPSA99841` directory. Preserve executable permissions (`0755`) on `eboot.bin`, `retroatlas.elf`, and `sce_module/libc.prx`, and allow directory traversal. Do not overwrite an existing unrelated folder with this development title ID. Let ShadowMountPlus register the folder, then follow step 6 above.

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

Record the exact console firmware, jailbreak, ShadowMountPlus version and ELF loader version. Verify fresh install, tile registration, Demo Preview, live account access, input, clean exit, relaunch, theme/settings persistence and an update. Confirm the development title ID `PPSA99841` does not conflict with another app in your intended distribution. Do not label the build firmware-compatible based on compilation alone.
