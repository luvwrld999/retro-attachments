// Retro Attachments home-screen launcher. SPDX-License-Identifier: GPL-3.0-or-later
// Ask local websrv to launch the viewer as graphical homebrew (BigApp).
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <initializer_list>

extern "C" {
int sceKernelOpen(const char *, int, unsigned int);
int sceKernelClose(int);
std::int64_t sceKernelRead(int, void *, std::size_t);
int sceNetSocket(const char *, int, int, int);
int sceNetSocketClose(int);
int sceNetConnect(int, const void *, std::uint32_t);
int sceNetSend(int, const void *, std::size_t, int);
int sceNetSetsockopt(int, int, int, const void *, std::uint32_t);
int sceKernelSendNotificationRequest(int, void *, std::size_t, int);
}
struct Endpoint {
  std::uint8_t length, family;
  std::uint16_t port;
  std::uint32_t address;
  std::uint16_t virtual_port;
  std::uint8_t zero[6];
};
static_assert(sizeof(Endpoint) == 16);
static void notify(const char *message) {
  struct {
    char padding[45];
    char message[3075];
  } request{};
  std::size_t n = 0;
  while (message[n] && n < sizeof(request.message) - 1) {
    request.message[n] = message[n];
    ++n;
  }
  sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}
int main() {
  int file = sceKernelOpen("/data/homebrew/PPSA99841/retroatlas.elf", O_RDONLY, 0);
  if (file < 0) {
    notify("Retro Attachments: app payload is missing. Reinstall Retro Attachments.");
    return 1;
  }
  unsigned char buffer[32768];
  auto count = sceKernelRead(file, buffer, sizeof(buffer));
  if (count < 64 || buffer[0] != 0x7f || buffer[1] != 'E' || buffer[2] != 'L' ||
      buffer[3] != 'F') {
    sceKernelClose(file);
    notify("Retro Attachments: invalid app payload. Reinstall Retro Attachments.");
    return 1;
  }
  int socket = sceNetSocket("Retro Attachments", 2, 1, 6);
  if (socket < 0) {
    sceKernelClose(file);
    notify("Retro Attachments: network service is unavailable.");
    return 1;
  }
  int timeout = 10000000;
  bool ready = true;
  for (int option : {0x1105, 0x1106, 0x1109})
    ready = ready && sceNetSetsockopt(socket, 0xffff, option, &timeout,
                                      sizeof(timeout)) >= 0;
  constexpr std::uint16_t port = 8080;
  Endpoint endpoint{
      16,         2, static_cast<std::uint16_t>((port << 8) | (port >> 8)),
      0x0100007f, 0, {0}};
  ready = ready && sceNetConnect(socket, &endpoint, sizeof(endpoint)) >= 0;
  if (!ready) {
    sceNetSocketClose(socket);
    sceKernelClose(file);
    notify("Retro Attachments: start Homebrew Launcher / websrv on port 8080, then open this app "
           "again.");
    return 1;
  }
  // Use the stable installed path: hbldr can terminate this launcher before
  // reading the ELF, at which point /app0 may no longer refer to our title.
  constexpr char request[] =
      "GET /hbldr?path=%2Fdata%2Fhomebrew%2FPPSA99841%2Fretroatlas.elf&daemon=0 HTTP/1.1\r\n"
      "Host: 127.0.0.1:8080\r\nConnection: close\r\n\r\n";
  bool sent = true;
  std::size_t offset = 0;
  while (offset < sizeof(request) - 1) {
    int n = sceNetSend(socket, request + offset, sizeof(request) - 1 - offset, 0);
    if (n <= 0 || static_cast<std::size_t>(n) > sizeof(request) - 1 - offset) {
      sent = false;
      break;
    }
    offset += static_cast<std::size_t>(n);
  }
  sceKernelClose(file);
  sceNetSocketClose(socket);
  if (!sent)
    notify("Retro Attachments: launch request failed. Check Homebrew Launcher / websrv and "
           "try again.");
  // Request submission is not an acknowledgement of application startup.
  // hbldr may terminate this BigApp while launching the viewer, so do not
  // block waiting for an HTTP response that this process may never receive.
  return sent ? 0 : 1;
}
