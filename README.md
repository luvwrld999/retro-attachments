# Retro Attachments — PS5 RetroAchievements viewer

Native SDL2 application targeting the PS5 payload SDK, distributed as `dist/retro-attachments.elf`.
Version 0.1.1 is an initial hardware-test build. It cross-compiles successfully; it has **not been run on a PS5**. Firmware 13.60 and your particular ELF loader still require testing. A native home-screen launcher and installer are now included; see [installation and recovery instructions](docs/INSTALL.md).

## Interface preview

Demo data shown below; no account credentials are included.

![Retro Attachments collection](docs/images/collection.png)

![Achievement details](docs/images/achievements.png)

![Built-in themes](docs/images/themes.png)

## Home-screen installer

Build with `make installer`, then launch `dist/retro-attachments-installer.elf`. Confirm with Cross, exit after the files are installed, and let ShadowMountPlus register `/data/homebrew/PPSA99841`. The tile runs a native launcher that streams the bundled viewer to the local ELF loader on port 9021. It still needs the jailbreak, ELF loader and compatible ShadowMountPlus active. This launch path is untested on PS5 hardware.

`dist/PPSA99841.zip` is the manual folder-install alternative. The installer verifies hashes, refuses an unowned destination, preserves the previous app on updates and leaves account settings untouched. Read [docs/INSTALL.md](docs/INSTALL.md) before testing.

## Launch the standalone ELF on PS5

1. Launch your existing jailbreak and an ELF loader capable of running graphical PS5 SDK payloads. The loader must expose the standard SCE modules and allow video output, controller access, networking and `/data` writes.
2. Send `dist/retro-attachments.elf` using your loader's normal method, or copy it to the loader's supported payload directory. The directory and network port depend on your loader. This is an ELF payload, not an installable PKG.
3. In **Settings**, enter your RetroAchievements username and **web API key** from https://retroachievements.org/settings. Select **Connect my collection**. Do not use your account password.
4. Open a game to view achievements and earned dates. Use **Explore games** to browse each console's games with achievement sets, including games you have not played.
5. **Appearance** offers Midnight Blue, OLED Black, Retro CRT, and Neon Purple. The choice persists across launches.

No credentials are bundled. Each installation uses its own API key. The key is masked in the interface but stored as plaintext in `/data/retroatlas/settings.json` with restrictive file permissions. Do not share that file. Theme, progress mode and username are stored there too. On a desktop, the default data directory is `userdata/`.

The font and CA certificate bundle are embedded in the ELF. On startup, the CA bundle is written to the data directory for libcurl. TLS certificate and hostname verification remain enabled. Ensure the console clock is correct. Settings and live API access require writable storage.

## Controls

| Action | DualSense | Desktop keyboard |
| --- | --- | --- |
| Navigate | D-pad / left stick | Arrow keys |
| Select / open | Cross | Enter |
| Back / focus sidebar | Circle | Escape |
| Previous / next tab | L1 / R1 | Shift+Tab / Tab |
| Search game titles | Triangle | S |
| All / earned / locked achievements | Square | F |
| Hardcore / Softcore (any unlock) | Touchpad click* | H |
| Refresh library / current game | Options | R |
| Exit | Settings → Exit Retro Attachments | F10 |

*Uses SDL's Back button mapping; verify this mapping on your loader/controller combination. Settings also offers a progress-mode toggle.

The on-screen keyboard supports controller entry. Select SAVE to apply or Circle to cancel. A USB keyboard can also enter text; Enter saves. Game search applies to the currently loaded collection or console catalogue.

## Included

- Paginated loading of the user's complete game-completion list.
- Console catalogue browser, cached on disk for 24 hours to reduce API traffic.
- Per-game achievement badges, descriptions, points and earned dates.
- Separate Hardcore and Softcore/any-unlock filtering. A Hardcore unlock counts as earned in the any-unlock view.
- Asynchronous HTTP and image requests, TLS verification, bounded downloads and a bounded texture cache.
- Four persistent themes, keyboard/controller account entry and distinct demo mode.
- Desktop test build and regression tests for parsing, state transitions and saved settings.

This is a viewer. It does not award achievements or integrate with an emulator. Current limitations: no full offline profile cache, no profile leaderboard/points dashboard, no automatic refresh, no global cross-console search, no PKG wrapper. Long text is clipped to the available panel space. The embedded font renderer covers basic Latin and Latin-1; other characters use placeholders. The CRT theme is a color palette, not a CRT shader. Console display/input behavior and authenticated live API responses remain to be verified on real hardware.

## Build

The build used the ps5-payload-dev package **v0.41.1**, LLVM **18**, and the supplied PS5 SDL **2.30.12** port. Source SDK and package recipes:

- https://github.com/ps5-payload-dev/sdk
- https://github.com/ps5-payload-dev/pacbrew-repo/releases/tag/v0.41.1
- https://github.com/ps5-payload-dev/SDL/tree/release-2.30.x-ps5

Download `ps5-payload-dev.tar.gz` from that release and extract it into `.toolchain/` (preserving `opt/ps5-payload-sdk`). Alternatively set `PS5_PAYLOAD_SDK` to an existing installation. Python 3 creates the embedded asset header. LLVM needs clang, lld and llvm-config.

```sh
# macOS with Homebrew LLVM 18
export LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config
make ps5
make installer
make test-installer test-bundle

# Desktop requires SDL2 and libcurl development files plus their *-config tools.
make desktop
./build/retro-attachments --demo
make test
```

`make test` uses SDL's dummy video driver; no display or API key is required. The tests cover fixture parsing and UI behavior, not PS5 hardware or live API availability.

```sh
# Render a preview without opening a desktop window
SDL_VIDEODRIVER=dummy ./build/retro-attachments --demo --screenshot build/library.bmp
SDL_VIDEODRIVER=dummy ./build/retro-attachments --demo --detail --screenshot build/detail.bmp
SDL_VIDEODRIVER=dummy ./build/retro-attachments --demo --themes --screenshot build/themes.bmp
```

API reference: https://api-docs.retroachievements.org/ . The app uses GetUserCompletionProgress, GetConsoleIDs, GetGameList and GetGameInfoAndUserProgress. API keys are URL-encoded and never printed in application logs. API requests are serialized; the worker pauses between requests. HTTP 429 errors require waiting before manual retry.

## Hardware acceptance checklist

- ELF loads and opens a readable display; returns cleanly through Settings → Exit.
- DualSense directions, Cross/Circle and shoulders work; reconnect works.
- Settings persist under `/data/retroatlas/` across restarts.
- Account connection returns real games; compare a known game and unlock date with the website.
- Hardcore and any-unlock counts match the account's data.
- Explore a console and a game not in the account library; check badge downloads.
- Disconnect networking, refresh, and confirm an actionable error with a responsive UI.

The former RetroAtlas internal title ID, ownership marker, bundled payload filename, and `/data/retroatlas` settings path remain unchanged for update compatibility. The visible name is Retro Attachments.

## Development status

This repository includes a GitHub Actions build workflow, contributor notes, a hardware-test report template and a source-archive script. Automated builds produce hardware-test artifacts. A passing build does not establish PS5 compatibility; on-console testing is still required.

## License

Application source: GPL-3.0-or-later. See `LICENSE` and `THIRD_PARTY.md`. Retro Attachments is an independent project and is not affiliated with Sony or RetroAchievements. Game art and achievement badges are fetched from RetroAchievements and are not bundled with the app.

## Interface polish in 0.1.1

The collection now uses four larger cards per page, an earned-achievement overview, clearer controller focus, progress rings and distinct theme previews. Artwork placeholders are original geometric cartridge designs; real game icons and badges still load from RetroAchievements. Summary figures describe the loaded collection or console, not global profile statistics.
