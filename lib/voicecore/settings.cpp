#include "settings.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "packet.h"

namespace lcr {

bool operator==(const Settings& a, const Settings& b) {
  return strncmp(a.callsign, b.callsign, sizeof(a.callsign)) == 0 && a.freqKhz == b.freqKhz &&
         a.powerDbm == b.powerDbm && a.volume == b.volume;
}

bool isValidSettings(const Settings& s) {
  return isValidCallsign(s.callsign) && s.freqKhz >= kMinFreqKhz && s.freqKhz <= kMaxFreqKhz &&
         s.powerDbm >= kMinPowerDbm && s.powerDbm <= kMaxPowerDbm && s.volume >= kMinVolume &&
         s.volume <= kMaxVolume;
}

Settings loadSettings(ISettingsStorage& storage, const Settings& defaults) {
  Settings s;
  if (storage.load(s) && isValidSettings(s)) return s;
  return defaults;
}

namespace {

// Scrive nella risposta (sempre terminata).
void say(char* reply, size_t cap, const char* fmt, const char* a = "", const char* b = "") {
  if (!reply || cap == 0) return;
  snprintf(reply, cap, fmt, a, b);
}

bool equalsIgnoreCase(const char* a, const char* b) {
  for (; *a && *b; a++, b++) {
    if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b))) return false;
  }
  return *a == *b;
}

// "433.900" / "433.9" / "433" -> kHz. Solo cifre e al massimo un punto, massimo 3 decimali.
bool parseFreqKhz(const char* t, uint32_t& khz) {
  uint32_t whole = 0;
  size_t digits = 0;
  while (isdigit(static_cast<unsigned char>(*t))) {
    if (++digits > 4) return false;
    whole = whole * 10 + static_cast<uint32_t>(*t - '0');
    t++;
  }
  if (digits == 0) return false;
  uint32_t frac = 0;
  size_t fd = 0;
  if (*t == '.') {
    t++;
    while (isdigit(static_cast<unsigned char>(*t))) {
      if (++fd > 3) return false;
      frac = frac * 10 + static_cast<uint32_t>(*t - '0');
      t++;
    }
    if (fd == 0) return false;
  }
  if (*t != 0) return false;
  while (fd < 3) {
    frac *= 10;
    fd++;
  }
  khz = whole * 1000 + frac;
  return true;
}

bool parseInt(const char* t, int& v) {
  size_t n = 0;
  int sign = 1;
  if (*t == '-') {
    sign = -1;
    t++;
  }
  int acc = 0;
  while (isdigit(static_cast<unsigned char>(*t))) {
    if (++n > 4) return false;
    acc = acc * 10 + (*t - '0');
    t++;
  }
  if (n == 0 || *t != 0) return false;
  v = sign * acc;
  return true;
}

}  // namespace

SettingsConsole::SettingsConsole(const Settings& active, const Settings& defaults, ISettingsStorage& storage,
                                 const char* board)
    : active_(active), defaults_(defaults), stored_(active), storage_(storage), board_(board ? board : "") {}

ConsoleResult SettingsConsole::handleLine(const char* line, char* reply, size_t cap) {
  if (reply && cap > 0) reply[0] = 0;
  if (!line) return ConsoleResult::NotCommand;

  // Spezza in parole (massimo 4) su una copia: la riga originale non si tocca.
  char buf[96];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  char* tok[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
  size_t n = 0;
  bool tooMany = false;
  char* p = buf;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') *p++ = 0;
    if (!*p) break;
    if (n == 4) {
      tooMany = true;
      break;
    }
    tok[n++] = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
  }
  if (n == 0 || !equalsIgnoreCase(tok[0], "CFG")) return ConsoleResult::NotCommand;

  if (n == 1 || tooMany) {
    say(reply, cap, "CFG ERR comando sconosciuto");
    return ConsoleResult::Error;
  }
  if (equalsIgnoreCase(tok[1], "GET") && n == 2) return get(reply, cap);
  if (equalsIgnoreCase(tok[1], "SET") && n == 4) return set(tok[2], tok[3], reply, cap);
  if (equalsIgnoreCase(tok[1], "RESET") && n == 2) return reset(reply, cap);
  if (equalsIgnoreCase(tok[1], "TEST") && n == 3) return test(tok[2], reply, cap);
  if (equalsIgnoreCase(tok[1], "REBOOT") && n == 2) {
    say(reply, cap, "CFG OK reboot");
    return ConsoleResult::Reboot;
  }
  say(reply, cap, "CFG ERR comando sconosciuto");
  return ConsoleResult::Error;
}

ConsoleResult SettingsConsole::get(char* reply, size_t cap) {
  if (reply && cap > 0) {
    snprintf(reply, cap, "CFG callsign=%s freq=%u.%03u power=%d volume=%d board=%s reboot=%d test=%d", stored_.callsign,
             static_cast<unsigned>(stored_.freqKhz / 1000), static_cast<unsigned>(stored_.freqKhz % 1000),
             stored_.powerDbm, stored_.volume, board_, stored_ != active_ ? 1 : 0, testMode_ ? 1 : 0);
  }
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::set(const char* key, const char* value, char* reply, size_t cap) {
  Settings next = stored_;
  char shown[40];

  if (equalsIgnoreCase(key, "CALLSIGN")) {
    char up[kMaxCallsign + 2];
    size_t i = 0;
    for (; value[i] && i < sizeof(up) - 1; i++) up[i] = static_cast<char>(toupper(static_cast<unsigned char>(value[i])));
    up[i] = 0;
    if (!isValidCallsign(up)) {
      say(reply, cap, "CFG ERR callsign %s", "solo A-Z, 0-9 e '/', massimo 8 caratteri");
      return ConsoleResult::Error;
    }
    memset(next.callsign, 0, sizeof(next.callsign));
    memcpy(next.callsign, up, strlen(up));
    snprintf(shown, sizeof(shown), "callsign=%s", next.callsign);
  } else if (equalsIgnoreCase(key, "FREQ")) {
    uint32_t khz = 0;
    if (!parseFreqKhz(value, khz) || khz < kMinFreqKhz || khz > kMaxFreqKhz) {
      say(reply, cap, "CFG ERR freq %s", "in MHz, da 430.000 a 440.000");
      return ConsoleResult::Error;
    }
    next.freqKhz = khz;
    snprintf(shown, sizeof(shown), "freq=%u.%03u", static_cast<unsigned>(khz / 1000), static_cast<unsigned>(khz % 1000));
  } else if (equalsIgnoreCase(key, "POWER")) {
    int dbm = 0;
    if (!parseInt(value, dbm) || dbm < kMinPowerDbm || dbm > kMaxPowerDbm) {
      say(reply, cap, "CFG ERR power %s", "in dBm, da 2 a 17");
      return ConsoleResult::Error;
    }
    next.powerDbm = dbm;
    snprintf(shown, sizeof(shown), "power=%d", dbm);
  } else if (equalsIgnoreCase(key, "VOLUME")) {
    int vol = 0;
    if (!parseInt(value, vol) || vol < kMinVolume || vol > kMaxVolume) {
      say(reply, cap, "CFG ERR volume %s", "da 0 (muto) a 10");
      return ConsoleResult::Error;
    }
    next.volume = vol;
    snprintf(shown, sizeof(shown), "volume=%d", vol);
  } else {
    say(reply, cap, "CFG ERR %s parametro sconosciuto", key);
    return ConsoleResult::Error;
  }

  if (!storage_.save(next)) {
    say(reply, cap, "CFG ERR %s salvataggio non riuscito", key);
    return ConsoleResult::Error;
  }
  stored_ = next;
  say(reply, cap, "CFG OK %s", shown);
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::test(const char* value, char* reply, size_t cap) {
  if (equalsIgnoreCase(value, "ON")) {
    testMode_ = true;
  } else if (equalsIgnoreCase(value, "OFF")) {
    testMode_ = false;
  } else {
    say(reply, cap, "CFG ERR test usa ON oppure OFF");
    return ConsoleResult::Error;
  }
  say(reply, cap, testMode_ ? "CFG OK test=on" : "CFG OK test=off");
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::reset(char* reply, size_t cap) {
  if (!storage_.erase()) {
    say(reply, cap, "CFG ERR reset non riuscito");
    return ConsoleResult::Error;
  }
  stored_ = defaults_;
  say(reply, cap, "CFG OK reset");
  return ConsoleResult::Ok;
}

}  // namespace lcr
