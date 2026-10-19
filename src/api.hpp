#pragma once
#include "model.hpp"
#include <atomic>
#include <ctime>
#include <curl/curl.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
struct Api {
  std::string key, user, dir, ca;
  std::atomic<bool> *stop = nullptr;
  static size_t write(char *p, size_t s, size_t n, void *v) {
    auto &out = *static_cast<std::string *>(v);
    if (s * n > 32 * 1024 * 1024 - out.size())
      return 0;
    out.append(p, s * n);
    return s * n;
  }
  static int progress(void *p, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return p && static_cast<std::atomic<bool> *>(p)->load() ? 1 : 0;
  }
  std::string fetch(const std::string &url) {
    CURL *h = curl_easy_init();
    if (!h)
      throw std::runtime_error("Network initialization failed");
    std::string out;
    curl_easy_setopt(h, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_USERAGENT,
                     "RetroAttachments/0.1 (PS5 achievement viewer)");
    curl_easy_setopt(h, CURLOPT_CAINFO, ca.c_str());
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(h, CURLOPT_XFERINFODATA, stop);
    curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
    auto code = curl_easy_perform(h);
    long status = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(h);
    if (code != CURLE_OK)
      throw std::runtime_error("Connection failed. Check internet, console "
                               "clock and TLS certificates.");
    if (status == 429)
      throw std::runtime_error(
          "Rate limited. Wait a minute before refreshing.");
    if (status == 401 || status == 403)
      throw std::runtime_error(
          "Access denied. Check your RetroAchievements web API key.");
    if (status != 200)
      throw std::runtime_error("Server returned HTTP " +
                               std::to_string(status) + ". Try again later.");
    return out;
  }
  static std::string encode(const std::string &s) {
    static const char *hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
      if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        o += c;
      else {
        o += '%';
        o += hex[c >> 4];
        o += hex[c & 15];
      }
    }
    return o;
  }
  Json get(std::string endpoint, std::string query = "", std::string cache = "",
           int ttl = 0) {
    std::string path = dir + "/" + cache;
    struct stat st {};
    if (!cache.empty() && stat(path.c_str(), &st) == 0 &&
        std::time(nullptr) - st.st_mtime < ttl) {
      try {
        std::ifstream f(path);
        Json j;
        f >> j;
        return j;
      } catch (...) {
      }
    }
    auto body = fetch("https://retroachievements.org/API/API_" + endpoint +
                      ".php?y=" + encode(key) + query);
    Json j;
    try {
      j = Json::parse(body);
    } catch (...) {
      throw std::runtime_error("Server returned an unreadable response.");
    }
    if (j.is_object() &&
        (j.contains("Error") || j.value("Success", true) == false))
      throw std::runtime_error("RetroAchievements rejected this request. Check "
                               "username and API key.");
    if (!cache.empty()) {
      std::ofstream f(path + ".tmp");
      f << j.dump();
      f.close();
      if (f)
        std::rename((path + ".tmp").c_str(), path.c_str());
    }
    return j;
  }
  std::vector<Game> library() {
    std::vector<Game> out;
    for (int offset = 0;;) {
      if (offset > 0) std::this_thread::sleep_for(std::chrono::milliseconds(1100));
      auto j = get("GetUserCompletionProgress",
                   "&u=" + encode(user) + "&c=500&o=" + std::to_string(offset));
      if (!j.contains("Results") || !j["Results"].is_array())
        throw std::runtime_error("Unexpected library response.");
      auto &rows = j["Results"];
      for (auto &r : rows)
        out.push_back(gameFrom(r, true));
      offset += rows.size();
      if (rows.empty() || offset >= num(j, "Total", offset))
        break;
      if (stop && *stop)
        break;
    }
    return out;
  }
};
