#include "installer_bundle.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
int main() {
  char path[] = "/tmp/retroatlas-bundle-test-XXXXXX";
  char *created = mkdtemp(path);
  assert(created);
  std::string root = std::filesystem::canonical(created).string();
  try {
    auto out = install::run(root, installer_files,
                            {installer_blob, installer_blob_size});
    assert(out.backup.empty());
    for (auto &f : installer_files) {
      auto bytes = install::read(
          root + "/homebrew/" + install::title + "/" + f.path, f.size);
      assert(install::hash(bytes) == f.sha256);
    }
    std::filesystem::remove_all(root);
  } catch (...) {
    std::filesystem::remove_all(root);
    throw;
  }
  std::cout << "PASS: complete production app bundle installed and all on-disk "
               "hashes verified in isolated host directory\n";
}
