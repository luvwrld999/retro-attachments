# Third-party components

- PS5 payload SDK and runtime: GPL-3.0-or-later, with BSD-licensed FreeBSD headers. https://github.com/ps5-payload-dev/sdk . Prebuilt toolchain package: https://github.com/ps5-payload-dev/pacbrew-repo/releases/tag/v0.41.1 ; package recipes identify the corresponding component sources.
- SDL2 PS5 port 2.30.12: zlib license. https://github.com/ps5-payload-dev/SDL . License copied into `third_party/SDL-LICENSE.txt`.
- nlohmann/json v3.12.0: MIT; notice embedded in `third_party/json.hpp`. https://github.com/nlohmann/json
- stb_image and stb_truetype, commit f0569113c93ad095470c54bf34a17b36646bbbb5: public domain or MIT; notices embedded in their headers. https://github.com/nothings/stb
- Lato: SIL Open Font License 1.1, `assets/Lato-OFL.txt`; https://github.com/google/fonts/tree/main/ofl/lato . Embedded font is unmodified Lato-Regular.ttf.
- Inter source asset (unused by build): SIL Open Font License 1.1, `assets/OFL.txt`; https://github.com/google/fonts/tree/main/ofl/inter . Retained as an optional design asset.
- Mozilla CA bundle from https://curl.se/ca/cacert.pem ; license information is in the PEM header (MPL 2.0). Embedded to verify HTTPS certificates.
- libcurl: curl license, https://curl.se/docs/copyright.html
- OpenSSL: Apache-2.0, https://github.com/openssl/openssl
- zlib: zlib license, https://zlib.net/zlib_license.html
- zstd: BSD-3-Clause/GPL-2.0 dual license, https://github.com/facebook/zstd
- libpsl: MIT, https://github.com/rockdaboot/libpsl
- libiconv: LGPL-2.1-or-later, https://www.gnu.org/software/libiconv/
- libsamplerate: BSD-2-Clause, https://github.com/libsndfile/libsamplerate
- LLVM libc++, libc++abi, libunwind: Apache-2.0 with LLVM exceptions, https://github.com/llvm/llvm-project

The PS5 build statically links these support libraries and dynamically imports console-provided SCE modules. The complete SDK package and its build recipes are needed to reproduce the link; they are not part of the small app archive. Preserve notices and satisfy the applicable source/relinking requirements before public redistribution.

- Native app tooling, startup, linker script, SHA implementation and clean-room runtime generator: Copyright (C) 2026 BlackBearReloaded, GPL-3.0-or-later. Vendored upstream revision and license are in `third_party/ps5-native/ORIGIN.md` and `third_party/ps5-native/LICENSE`. The shipped `libc.prx` is reproduced from those sources and checked against upstream SHA-256 `e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036`.
