#pragma once
#include <cstdio>
#include <cstring>
#if defined(__PROSPERO__) && defined(RETRO_STARTUP_DIAGNOSTICS)
extern "C" int sceKernelSendNotificationRequest(int, void *, size_t, int);
#endif
inline void startupTrace(const char *message) {
#if defined(__PROSPERO__) && defined(RETRO_STARTUP_DIAGNOSTICS)
  struct { char reserved[45]; char message[3075]; } n{};
  std::snprintf(n.message, sizeof(n.message), "Retro Attachments diagnostic: %s", message);
  sceKernelSendNotificationRequest(0, &n, sizeof(n), 0);
  std::fprintf(stderr, "%s\n", n.message); std::fflush(stderr);
  if (FILE *f=std::fopen("/data/retro-attachments-startup.log", "a")) {
    std::fprintf(f, "%s\n", n.message); std::fclose(f);
  }
#else
  (void)message;
#endif
}
