// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "hash.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace install {
inline constexpr const char *title = "PPSA99841";
inline constexpr const char *owner = "RetroAtlas:home-screen-launcher:v1\n";
struct Entry {
  const char *path;
  std::size_t offset, size;
  const char *sha256;
  unsigned mode;
};
struct Outcome {
  std::string backup;
};
struct Fd {
  int fd;
  explicit Fd(int n) : fd(n) {}
  ~Fd() {
    if (fd >= 0)
      ::close(fd);
  }
  Fd(const Fd &) = delete;
};
inline void fail(const std::string &message) {
  throw std::runtime_error(message);
}
inline std::string hash(std::span<const unsigned char> bytes) {
  auto digest = ps5::crypto::sha256(bytes);
  std::string out;
  const char *hex = "0123456789abcdef";
  for (auto b : digest) {
    out += hex[b >> 4];
    out += hex[b & 15];
  }
  return out;
}
inline bool exists(const std::string &path) {
  struct stat s {};
  if (lstat(path.c_str(), &s) == 0)
    return true;
  if (errno == ENOENT)
    return false;
  fail("Cannot inspect " + path);
  return false;
}
inline void directory(const std::string &path, bool create = false) {
  struct stat s {};
  if (lstat(path.c_str(), &s) != 0) {
    if (errno != ENOENT || !create || mkdir(path.c_str(), 0755) != 0)
      fail("Cannot access directory " + path);
    if (lstat(path.c_str(), &s) != 0)
      fail("Cannot inspect directory " + path);
  }
  if (!S_ISDIR(s.st_mode) || S_ISLNK(s.st_mode))
    fail("Refusing non-directory or symlink: " + path);
}
inline void rootCheck(const std::string &path) {
  if (path.empty() || path[0] != '/')
    fail("Install root must be absolute");
  std::size_t start = 1;
  for (;;) {
    auto end = path.find('/', start);
    auto part = path.substr(start, end - start);
    if (part.empty() || part == "." || part == "..")
      fail("Invalid install root");
    directory(path.substr(0, end));
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
}
inline bool safePath(const std::string &path) {
  if (path.empty() || path.front() == '/')
    return false;
  std::size_t start = 0;
  for (;;) {
    auto end = path.find('/', start);
    auto part = path.substr(start, end - start);
    if (part.empty() || part == "." || part == "..")
      return false;
    for (unsigned char c : part)
      if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-'))
        return false;
    if (end == std::string::npos)
      return true;
    start = end + 1;
  }
}
inline std::vector<unsigned char> read(const std::string &path,
                                       std::size_t limit) {
  Fd f(open(path.c_str(), O_RDONLY | O_NOFOLLOW));
  if (f.fd < 0)
    fail("Cannot read " + path);
  struct stat s {};
  if (fstat(f.fd, &s) != 0 || !S_ISREG(s.st_mode) || s.st_size < 0 ||
      static_cast<std::uint64_t>(s.st_size) > limit)
    fail("Invalid installed file " + path);
  std::vector<unsigned char> bytes(s.st_size);
  std::size_t off = 0;
  while (off < bytes.size()) {
    ssize_t n = ::read(f.fd, bytes.data() + off, bytes.size() - off);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      fail("Short read: " + path);
    off += n;
  }
  return bytes;
}
inline void syncDir(const std::string &path) {
  Fd fd(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW));
  if (fd.fd < 0)
    fail("Cannot open directory for sync: " + path);
  if (fsync(fd.fd) != 0 && errno != EINVAL && errno != ENOTSUP)
    fail("Directory sync failed: " + path);
}
inline void write(const std::string &path, std::span<const unsigned char> bytes,
                  unsigned mode) {
  Fd f(open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600));
  if (f.fd < 0)
    fail("Cannot create " + path);
  std::size_t off = 0;
  while (off < bytes.size()) {
    ssize_t n = ::write(f.fd, bytes.data() + off, bytes.size() - off);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      fail("Write failed; check free space: " + path);
    off += n;
  }
  if (fchmod(f.fd, mode) != 0 || fsync(f.fd) != 0)
    fail("Cannot persist " + path);
}
// The checkpoint hook is used by host tests to inject failures at transaction
// boundaries.
inline Outcome run(const std::string &root, std::span<const Entry> files,
                   std::span<const unsigned char> blob,
                   const std::function<void(int)> &progress = {},
                   const std::function<void(const char *)> &checkpoint = {}) {
  rootCheck(root);
  if (files.empty() || blob.empty())
    fail("Empty installation bundle");
  std::vector<std::string> names;
  bool owned = false, eboot = false, param = false, runtime = false,
       payload = false;
  std::size_t total = 0;
  for (auto &f : files) {
    std::string path = f.path;
    if (!safePath(path) ||
        std::find(names.begin(), names.end(), path) != names.end() ||
        f.offset > blob.size() || f.size > blob.size() - f.offset ||
        f.size == 0 || (f.mode != 0644 && f.mode != 0755))
      fail("Invalid installation manifest");
    if (hash(blob.subspan(f.offset, f.size)) != f.sha256)
      fail("Embedded bundle checksum failed");
    if (path == ".retroatlas-owned") {
      std::string value((const char *)blob.data() + f.offset, f.size);
      owned = value == owner;
    }
    eboot |= path == "eboot.bin";
    param |= path == "sce_sys/param.json";
    runtime |= path == "sce_module/libc.prx";
    payload |= path == "retroatlas.elf";
    names.push_back(path);
    total += f.size;
  }
  if (!(owned && eboot && param && runtime && payload))
    fail("Incomplete app bundle");
  const std::string home = root + "/homebrew",
                    work = root + "/retroatlas-installer";
  directory(home, true);
  directory(work, true);
  const std::string lock = work + "/install.lock";
  if (mkdir(lock.c_str(), 0700) != 0)
    fail("Installer lock exists or storage is not writable. See INSTALL.md "
         "before retrying.");
  struct Lock {
    std::string path;
    ~Lock() { rmdir(path.c_str()); }
  } guard{lock};
  const std::string target = home + "/" + title;
  bool update = exists(target);
  if (update) {
    directory(target);
    auto marker = read(target + "/.retroatlas-owned", 128);
    if (std::string(marker.begin(), marker.end()) != owner)
      fail("Title ID is occupied by another app. Nothing was replaced.");
  }
  struct statvfs space {};
  if (statvfs(root.c_str(), &space) != 0)
    fail("Cannot check free storage space");
  if (static_cast<std::uint64_t>(space.f_bavail) * space.f_frsize <
      total + 8 * 1024 * 1024)
    fail("Not enough free space (bundle plus 8 MiB required)");
  struct stat hs {
  }, ws{};
  if (stat(home.c_str(), &hs) != 0 || stat(work.c_str(), &ws) != 0 ||
      hs.st_dev != ws.st_dev)
    fail("Staging and app folder must share a filesystem");
  std::string stage = work + "/stage-" + std::to_string(getpid());
  if (mkdir(stage.c_str(), 0755) != 0)
    fail("Staging folder exists or cannot be created. No app was replaced.");
  std::vector<std::string> createdFiles, createdDirs;
  bool promoted = false, movedOld = false;
  std::string backup;
  try {
    for (std::size_t i = 0; i < files.size(); ++i) {
      auto &f = files[i];
      std::string relative = f.path;
      std::size_t off = 0;
      while ((off = relative.find('/', off)) != std::string::npos) {
        std::string dir = stage + "/" + relative.substr(0, off);
        if (!exists(dir)) {
          directory(dir, true);
          createdDirs.push_back(dir);
        } else
          directory(dir);
        ++off;
      }
      const auto dest = stage + "/" + relative;
      createdFiles.push_back(dest);
      write(dest, blob.subspan(f.offset, f.size), f.mode);
      if (hash(read(dest, f.size)) != f.sha256)
        fail("Written file checksum mismatch: " + relative);
      if (progress)
        progress(int((i + 1) * 85 / files.size()));
      if (checkpoint)
        checkpoint("file-written");
    }
    for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it)
      syncDir(*it);
    syncDir(stage);
    if (checkpoint)
      checkpoint("before-promotion");
    if (update) {
      backup = work + "/previous-" + std::to_string(getpid());
      if (exists(backup))
        fail("Backup folder already exists. No app was replaced.");
      if (rename(target.c_str(), backup.c_str()) != 0)
        fail("Cannot preserve existing app; close it and try again");
      movedOld = true;
    }
    if (checkpoint)
      checkpoint("old-preserved");
    if (rename(stage.c_str(), target.c_str()) != 0)
      fail("Could not publish app folder");
    promoted = true;
    syncDir(home);
    syncDir(work);
    if (progress)
      progress(100);
    return {backup};
  } catch (...) {
    if (!promoted) {
      bool restored = !movedOld || rename(backup.c_str(), target.c_str()) == 0;
      for (auto it = createdFiles.rbegin(); it != createdFiles.rend(); ++it)
        unlink(it->c_str());
      for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it)
        rmdir(it->c_str());
      rmdir(stage.c_str());
      if (!restored)
        fail("Update failed; previous app is preserved at " + backup +
             ". Restore it before retrying.");
    }
    throw;
  }
}
} // namespace install
