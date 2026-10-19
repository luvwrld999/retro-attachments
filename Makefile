PS5_PAYLOAD_SDK ?= $(CURDIR)/.toolchain/opt/ps5-payload-sdk
HOST_CXX ?= clang++
CPPFLAGS = -Ithird_party -Ibuild -Isrc
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
SOURCES = src/main.cpp src/model.hpp src/api.hpp src/render.hpp
.PHONY: all ps5 desktop test
all: ps5
build/assets.hpp: assets/Lato-Regular.ttf assets/cacert.pem scripts/embed.py
	python3 scripts/embed.py
ps5: dist/retro-attachments.elf
dist/retro-attachments.elf: $(SOURCES) build/assets.hpp
	mkdir -p dist
	$(PS5_PAYLOAD_SDK)/bin/prospero-clang++ $(CXXFLAGS) $(CPPFLAGS) -D__PROSPERO__ -I$(PS5_PAYLOAD_SDK)/target/user/homebrew/include/SDL2 -I$(PS5_PAYLOAD_SDK)/target/user/homebrew/include src/main.cpp -o $@ -lSDL2 -liconv -lSceSystemService -lSceUserService -lScePad -lSceAudioOut -lsamplerate -lSceVideoOut -lSceKeyboard -lSceImeDialog -lcurl -lpsl -lzstd -lssl -lcrypto -lz -lpthread

desktop: build/retro-attachments
build/retro-attachments: $(SOURCES) build/assets.hpp
	$(HOST_CXX) $(CXXFLAGS) $(CPPFLAGS) $$(sdl2-config --cflags) $$(curl-config --cflags) src/main.cpp -o $@ $$(sdl2-config --libs) $$(curl-config --libs)
test:
	mkdir -p build
	$(HOST_CXX) $(CXXFLAGS) $(CPPFLAGS) tests/model_test.cpp -o build/model_test
	./build/model_test
	$(HOST_CXX) $(CXXFLAGS) $(CPPFLAGS) $$(sdl2-config --cflags) $$(curl-config --cflags) tests/app_test.cpp -o build/app_test $$(sdl2-config --libs) $$(curl-config --libs)
	SDL_VIDEODRIVER=dummy ./build/app_test

NATIVE_DIR = third_party/ps5-native/tooling/native
NATIVE_HOST_SOURCES = $(NATIVE_DIR)/native_app_builder.cpp $(NATIVE_DIR)/elf_object.cpp $(NATIVE_DIR)/sce_module_writer.cpp $(NATIVE_DIR)/self_container.cpp
PS5_INCLUDES = -I$(PS5_PAYLOAD_SDK)/target/user/homebrew/include/SDL2 -I$(PS5_PAYLOAD_SDK)/target/user/homebrew/include
SDL_PS5_LIBS = -lSDL2 -liconv -lSceSystemService -lSceUserService -lScePad -lSceAudioOut -lsamplerate -lSceVideoOut -lSceKeyboard -lSceImeDialog
INSTALLER_FLAGS = -std=c++20 -O2 -Wall -Wextra $(CPPFLAGS) -Iinstaller -I$(NATIVE_DIR)
.PHONY: native-tools app installer installer-desktop test-installer
native-tools: build/ps5-native-tool build/libc-builder
build/ps5-native-tool: $(NATIVE_HOST_SOURCES) $(wildcard $(NATIVE_DIR)/*.hpp)
	mkdir -p build
	$(HOST_CXX) -std=c++20 -O2 $(NATIVE_HOST_SOURCES) -lz -o $@
build/libc-builder: $(NATIVE_DIR)/libc_builder.cpp $(wildcard $(NATIVE_DIR)/*.hpp)
	mkdir -p build
	$(HOST_CXX) -std=c++20 -O2 $(NATIVE_DIR)/libc_builder.cpp -o $@
launcher/sce_sys/icon0.png: scripts/make_icon.py
	python3 scripts/make_icon.py
app: ps5 native-tools launcher/sce_sys/icon0.png
	PS5_PAYLOAD_SDK="$(PS5_PAYLOAD_SDK)" python3 scripts/build_launcher.py
installer: app build/assets.hpp
	python3 scripts/embed_installer.py
	$(PS5_PAYLOAD_SDK)/bin/prospero-clang -c build/installer_bundle.S -o build/installer_bundle.o
	$(PS5_PAYLOAD_SDK)/bin/prospero-clang++ $(INSTALLER_FLAGS) $(PS5_INCLUDES) -D__PROSPERO__ installer/main.cpp build/installer_bundle.o -o dist/retro-attachments-installer.elf $(SDL_PS5_LIBS) -lpthread
installer-desktop: app build/assets.hpp
	python3 scripts/embed_installer.py
	python3 scripts/host_bundle.py
	$(HOST_CXX) $(INSTALLER_FLAGS) $$(sdl2-config --cflags) installer/main.cpp build/installer_bundle_host.o -o build/retro-attachments-installer $$(sdl2-config --libs)
test-installer:
	mkdir -p build
	$(HOST_CXX) $(INSTALLER_FLAGS) tests/installer_test.cpp -o build/installer_test
	./build/installer_test
	$(HOST_CXX) -std=c++20 -O2 -Wall -Wextra tests/launcher_test.cpp -o build/launcher_test
	./build/launcher_test

.PHONY: test-bundle
# Run after make installer (requires its generated bundle).
test-bundle:
	python3 scripts/host_bundle.py
	$(HOST_CXX) $(INSTALLER_FLAGS) tests/bundle_test.cpp build/installer_bundle_host.o -o build/bundle_test
	./build/bundle_test
