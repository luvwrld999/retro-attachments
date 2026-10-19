// SPDX-License-Identifier: GPL-3.0-or-later
// Non-graphical entry point for Payload Manager; websrv owns graphical startup.
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include "payload_blob.hpp"
#include "startup_trace.hpp"
#ifndef RETRO_STAGE_DIR
#define RETRO_STAGE_DIR "/data/retro-attachments-payloads"
#endif
#ifndef RETRO_WEBSRV_URL
#define RETRO_WEBSRV_URL "http://127.0.0.1:8080"
#endif
static size_t discard(char*, size_t size, size_t count, void*) { return size * count; }
int main() {
  startupTrace("Payload Manager bridge v0.1.4-alpha.1: preparing graphical launch");
  struct stat st{};
  if (mkdir(RETRO_STAGE_DIR, 0755) && errno != EEXIST) {
    startupTrace("Cannot create /data staging directory"); return 1;
  }
  if (lstat(RETRO_STAGE_DIR, &st) || !S_ISDIR(st.st_mode)) {
    startupTrace("Staging path is not a directory"); return 1;
  }
  // Unique temporary file, followed by publication only after a full readback.
  std::string temporary = std::string(RETRO_STAGE_DIR) + "/.stage-XXXXXX";
  int fd = mkstemp(temporary.data());
  if (fd < 0) { startupTrace("Cannot create staged ELF"); return 1; }
  bool ok = true;
  size_t offset = 0;
  while (offset < payload_blob_size) {
    ssize_t n = write(fd, payload_blob + offset, payload_blob_size - offset);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { ok = false; break; }
    offset += static_cast<size_t>(n);
  }
  if (ok) ok = fchmod(fd, 0755) == 0 && fsync(fd) == 0 && lseek(fd, 0, SEEK_SET) == 0;
  unsigned char buffer[16384];
  offset = 0;
  while (ok && offset < payload_blob_size) {
    size_t want = payload_blob_size - offset;
    if (want > sizeof(buffer)) want = sizeof(buffer);
    ssize_t n = read(fd, buffer, want);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0 || std::memcmp(buffer, payload_blob + offset, static_cast<size_t>(n))) { ok = false; break; }
    offset += static_cast<size_t>(n);
  }
  if (close(fd)) ok = false;
  const std::string path = std::string(RETRO_STAGE_DIR) + "/" + payload_filename;
  if (ok) ok = rename(temporary.c_str(), path.c_str()) == 0;
  if (!ok) { unlink(temporary.c_str()); startupTrace("ELF staging or verification failed; launch cancelled"); return 1; }
  if (curl_global_init(CURL_GLOBAL_DEFAULT)) { startupTrace("Network initialization failed"); return 1; }
  CURL *curl = curl_easy_init();
  if (!curl) { curl_global_cleanup(); startupTrace("Cannot create launch request"); return 1; }
  char *escaped = curl_easy_escape(curl, path.c_str(), 0);
  if (!escaped) { curl_easy_cleanup(curl); curl_global_cleanup(); return 1; }
  const std::string url = std::string(RETRO_WEBSRV_URL) + "/hbldr?daemon=0&pipe=0&path=" + escaped;
  curl_free(escaped);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_PROXY, "");
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard);
  startupTrace("Requesting graphical app through local websrv on port 8080");
  CURLcode result = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  if (result == CURLE_COULDNT_CONNECT) {
    startupTrace("Start websrv on port 8080, then launch this ELF again from Payload Manager"); return 1;
  }
  if (result != CURLE_OK) {
    startupTrace("No launch acknowledgement from websrv; check console before retrying"); return 1;
  }
  if (status != 200) {
    char message[160];
    std::snprintf(message, sizeof(message), "websrv rejected graphical launch (HTTP %ld); close other apps and check websrv", status);
    startupTrace(message); return 1;
  }
  startupTrace("websrv accepted graphical launch; awaiting app display");
  return 0;
}
