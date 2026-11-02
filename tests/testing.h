// Mini framework di test senza dipendenze.
#pragma once

#include <cstdio>

inline int g_failures = 0;
inline int g_checks = 0;

#define CHECK(cond)                                                  \
  do {                                                               \
    g_checks++;                                                      \
    if (!(cond)) {                                                   \
      g_failures++;                                                  \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
    }                                                                \
  } while (0)

#define TEST_MAIN_END()                                                         \
  std::printf("%s: %d controlli, %d falliti\n", __FILE__, g_checks, g_failures); \
  return g_failures ? 1 : 0
