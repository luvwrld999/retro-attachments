#include "install.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;
struct Temp {
  std::string path;
  Temp() {
    char p[] = "/tmp/retroatlas-install-test-XXXXXX";
    auto *d = mkdtemp(p);
    assert(d);
    path = fs::canonical(d).string();
  }
  ~Temp() { fs::remove_all(path); }
};
struct Bundle {
  std::vector<unsigned char> bytes;
  std::vector<std::string> hashes;
  std::vector<install::Entry> entries;
  Bundle(std::string text = "version-one") {
    const char *paths[] = {"eboot.bin",         "sce_module/libc.prx",
                           "retroatlas.elf",    "sce_sys/param.json",
                           "sce_sys/icon0.png", ".retroatlas-owned"};
    hashes.reserve(6);
    for (auto path : paths) {
      std::string data =
          std::string(path) == ".retroatlas-owned" ? install::owner : text;
      auto offset = bytes.size();
      bytes.insert(bytes.end(), data.begin(), data.end());
      hashes.push_back(install::hash({bytes.data() + offset, data.size()}));
      entries.push_back({path, offset, data.size(), hashes.back().c_str(),
                         std::string(path).ends_with(".elf") ? 0755u : 0644u});
    }
  }
};
template <class F> void rejects(F fn) {
  bool failed = false;
  try {
    fn();
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed);
}
void put(const std::string &path, const std::string &text) {
  install::write(
      path, {reinterpret_cast<const unsigned char *>(text.data()), text.size()},
      0600);
}
int main() {
  const unsigned char abc[] = {'a', 'b', 'c'};
  assert(install::hash(abc) ==
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  Bundle first, second("version-two");
  {
    Temp tmp;
    auto target = tmp.path + "/homebrew/" + install::title;
    fs::create_directory(tmp.path + "/retroatlas");
    put(tmp.path + "/retroatlas/settings.json", "keep-account");
    int progress = 0;
    auto out = install::run(tmp.path, first.entries, first.bytes,
                            [&](int n) { progress = n; });
    assert(out.backup.empty() && progress == 100);
    for (auto &f : first.entries)
      assert(install::hash(install::read(target + "/" + f.path, f.size)) ==
             f.sha256);
    struct stat st {};
    assert(stat((target + "/retroatlas.elf").c_str(), &st) == 0 &&
           (st.st_mode & 0777) == 0755);
    auto outcome = install::run(tmp.path, second.entries, second.bytes);
    assert(!outcome.backup.empty());
    assert(install::hash(install::read(outcome.backup + "/eboot.bin", 100)) ==
           first.entries[0].sha256);
    assert(install::hash(install::read(target + "/eboot.bin", 100)) ==
           second.entries[0].sha256);
    auto settings = install::read(tmp.path + "/retroatlas/settings.json", 100);
    assert(std::string(settings.begin(), settings.end()) == "keep-account");
  }
  for (const char *fault :
       {"file-written", "before-promotion", "old-preserved"}) {
    Temp tmp;
    auto target = tmp.path + "/homebrew/" + install::title;
    install::run(tmp.path, first.entries, first.bytes);
    rejects([&] {
      install::run(tmp.path, second.entries, second.bytes, {},
                   [&](const char *point) {
                     if (std::string(point) == fault)
                       throw std::runtime_error("injected");
                   });
    });
    assert(install::hash(install::read(target + "/eboot.bin", 100)) ==
           first.entries[0].sha256);
    assert(!fs::exists(tmp.path + "/retroatlas-installer/install.lock"));
  }
  {
    Temp tmp;
    auto corrupt = first.bytes;
    corrupt[0] ^= 1;
    rejects([&] { install::run(tmp.path, first.entries, corrupt); });
    assert(!fs::exists(tmp.path + "/homebrew"));
  }
  {
    Temp tmp;
    auto entries = first.entries;
    entries[0].path = "../escape";
    rejects([&] { install::run(tmp.path, entries, first.bytes); });
    assert(!fs::exists(tmp.path + "/homebrew"));
  }
  {
    Temp tmp;
    auto target = tmp.path + "/homebrew/" + install::title;
    fs::create_directories(target);
    put(target + "/important.txt", "keep");
    rejects([&] { install::run(tmp.path, first.entries, first.bytes); });
    assert(fs::exists(target + "/important.txt"));
  }
  {
    Temp tmp, outside;
    fs::create_directory_symlink(outside.path, tmp.path + "/homebrew");
    rejects([&] { install::run(tmp.path, first.entries, first.bytes); });
    assert(fs::is_empty(outside.path));
  }
  {
    Temp tmp, outside;
    fs::create_directories(tmp.path + "/homebrew");
    fs::create_directory_symlink(outside.path,
                                 tmp.path + "/homebrew/" + install::title);
    rejects([&] { install::run(tmp.path, first.entries, first.bytes); });
    assert(fs::is_empty(outside.path));
  }
  {
    Temp tmp;
    fs::create_directories(tmp.path + "/retroatlas-installer/install.lock");
    rejects([&] { install::run(tmp.path, first.entries, first.bytes); });
    assert(fs::is_directory(tmp.path + "/retroatlas-installer/install.lock"));
  }
  {
    Temp tmp;
    rejects([&] {
      install::run(
          tmp.path, first.entries, first.bytes, {}, [&](const char *point) {
            if (std::string(point) == "before-promotion")
              throw std::runtime_error("power loss simulation before publish");
          });
    });
    assert(!fs::exists(tmp.path + "/homebrew/" + install::title));
  }
  std::cout
      << "PASS: SHA-256, fresh install, file verification, executable "
         "permissions, update backup, account preservation, rollback at three "
         "checkpoints, corruption/path/ownership/symlink/lock rejection\n";
}
