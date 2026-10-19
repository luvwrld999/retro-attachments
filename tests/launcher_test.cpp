#define main launcher_main
#include "../launcher/main.cpp"
#undef main
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
static std::vector<unsigned char> data, stream;
static std::size_t position = 0;
static int sockets = 0, closed = 0, failConnect = 0, failSend = 0;
static std::string notification;
extern "C" {
int sceKernelOpen(const char *path, int, unsigned int) {
  assert(std::string(path) == "/data/homebrew/PPSA99841/retroatlas.elf");
  return 4;
}
int sceKernelClose(int fd) {
  assert(fd == 4);
  return 0;
}
std::int64_t sceKernelRead(int, void *out, std::size_t size) {
  auto n = std::min(size, data.size() - position);
  memcpy(out, data.data() + position, n);
  position += n;
  return n;
}
int sceNetSocket(const char *, int d, int t, int p) {
  assert(d == 2 && t == 1 && p == 6);
  ++sockets;
  return 5;
}
int sceNetSocketClose(int fd) {
  assert(fd == 5);
  ++closed;
  return 0;
}
int sceNetSetsockopt(int, int level, int option, const void *,
                     std::uint32_t size) {
  assert(level == 0xffff && size == sizeof(int));
  assert(option == 0x1105 || option == 0x1106 || option == 0x1109);
  return 0;
}
int sceNetConnect(int, const void *raw, std::uint32_t size) {
  assert(size == 16);
  auto &a = *static_cast<const Endpoint *>(raw);
  assert(a.address == 0x0100007f && a.port == 0x901f);
  return failConnect ? -1 : 0;
}
int sceNetSend(int, const void *bytes, std::size_t size, int) {
  if (failSend)
    return -1;
  auto n = std::min(size, std::size_t(7));
  auto p = static_cast<const unsigned char *>(bytes);
  stream.insert(stream.end(), p, p + n);
  return n;
}
int sceKernelSendNotificationRequest(int, void *raw, std::size_t, int) {
  notification = static_cast<char *>(raw) + 45;
  return 0;
}
}
void reset() {
  data.assign(75000, 42);
  data[0] = 0x7f;
  data[1] = 'E';
  data[2] = 'L';
  data[3] = 'F';
  stream.clear();
  position = 0;
  sockets = closed = failConnect = failSend = 0;
  notification.clear();
}
int main() {
  reset();
  assert(launcher_main() == 0 && std::string(stream.begin(), stream.end()) ==
      "GET /hbldr?path=%2Fdata%2Fhomebrew%2FPPSA99841%2Fretroatlas.elf&daemon=0 HTTP/1.1\r\n"
      "Host: 127.0.0.1:8080\r\nConnection: close\r\n\r\n" && closed == 1 &&
         notification.empty());
  reset();
  failConnect = 1;
  assert(launcher_main() == 1 && stream.empty() && closed == 1 &&
         notification.find("8080") != std::string::npos);
  reset();
  failSend = 1;
  assert(launcher_main() == 1 && closed == 1 &&
         notification.find("request failed") != std::string::npos);
  reset();
  data[0] = 0;
  assert(launcher_main() == 1 && sockets == 0 &&
         notification.find("invalid") != std::string::npos);
  reset();
  data.resize(10);
  assert(launcher_main() == 1 && sockets == 0);
  std::cout << "PASS: native launcher stable installed path, loopback endpoint, chunked "
               "HTTP homebrew request, timeout setup, close, connection/send/invalid-ELF "
               "failures\n";
}
