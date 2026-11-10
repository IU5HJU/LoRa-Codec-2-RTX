#include "settings.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "packet.h"

namespace lcr {

bool operator==(const Settings& a, const Settings& b) {
  return strncmp(a.callsign, b.callsign, sizeof(a.callsign)) == 0 && a.freqKhz == b.freqKhz && a.freqMsgKhz == b.freqMsgKhz &&
         a.qualFreqKhz() == b.qualFreqKhz() &&
         a.powerDbm == b.powerDbm && a.volume == b.volume && a.micFloorDb == b.micFloorDb &&
         a.profile == b.profile && a.gpsOn == b.gpsOn && a.posTx == b.posTx && a.qthSet == b.qthSet &&
         (!a.qthSet || (a.qthLatE7 == b.qthLatE7 && a.qthLonE7 == b.qthLonE7));
}

bool isValidSettings(const Settings& s) {
  return isValidCallsign(s.callsign) && s.freqKhz >= kMinFreqKhz && s.freqKhz <= kMaxFreqKhz && s.freqMsgKhz >= kMinFreqKhz &&
         s.freqMsgKhz <= kMaxFreqKhz && (s.freqQualKhz == 0 || (s.freqQualKhz >= kMinFreqKhz && s.freqQualKhz <= kMaxFreqKhz)) &&
         s.powerDbm >= kMinPowerDbm && s.powerDbm <= kMaxPowerDbm && s.volume >= kMinVolume &&
         s.volume <= kMaxVolume &&
         s.micFloorDb >= kMinMicFloorDb && s.micFloorDb <= kMaxMicFloorDb && s.profile < kProfileCount &&
         (!s.qthSet || isValidCoordinate(s.qthLatE7, s.qthLonE7));
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

// "43.7696" / "-11.25" -> decimilionesimi di grado. Al massimo 7 decimali, nessun esponente.
bool parseDegreesE7(const char* t, int32_t& e7) {
  bool neg = false;
  if (*t == '-') {
    neg = true;
    t++;
  }
  int64_t whole = 0;
  size_t digits = 0;
  while (isdigit(static_cast<unsigned char>(*t))) {
    if (++digits > 3) return false;
    whole = whole * 10 + (*t - '0');
    t++;
  }
  if (digits == 0) return false;
  int64_t frac = 0;
  size_t fd = 0;
  if (*t == '.') {
    t++;
    while (isdigit(static_cast<unsigned char>(*t))) {
      if (++fd > 7) return false;
      frac = frac * 10 + (*t - '0');
      t++;
    }
    if (fd == 0) return false;
  }
  if (*t != 0) return false;
  while (fd < 7) {
    frac *= 10;
    fd++;
  }
  const int64_t v = whole * 10000000 + frac;
  e7 = static_cast<int32_t>(neg ? -v : v);
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

// "43.76960,11.25580" (5 decimali, circa 1 m).
void formatQth(char* out, size_t cap, int32_t latE7, int32_t lonE7) {
  auto part = [](char* o, size_t c, int32_t e7) {
    const int64_t a = e7 < 0 ? -static_cast<int64_t>(e7) : e7;
    const int64_t r = (a + 50) / 100;  // 5 decimali, arrotondato
    snprintf(o, c, "%s%lld.%05lld", (e7 < 0 && r != 0) ? "-" : "", static_cast<long long>(r / 100000), static_cast<long long>(r % 100000));
  };
  char la[16], lo[16];
  part(la, sizeof(la), latE7);
  part(lo, sizeof(lo), lonE7);
  snprintf(out, cap, "%s,%s", la, lo);
}

}  // namespace

SettingsConsole::SettingsConsole(const Settings& active, const Settings& defaults, ISettingsStorage& storage,
                                 const char* board, const char* version)
    : active_(active), defaults_(defaults), stored_(active), storage_(storage), board_(board ? board : ""), version_(version ? version : "") {}

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
  if (equalsIgnoreCase(tok[1], "VERSION") && n == 2) return versionInfo(reply, cap);
  if (equalsIgnoreCase(tok[1], "SET") && n == 4) return set(tok[2], tok[3], reply, cap);
  if (equalsIgnoreCase(tok[1], "RESET") && n == 2) return reset(reply, cap);
  if (equalsIgnoreCase(tok[1], "TEST") && n == 3) return test(tok[2], reply, cap);
  if (equalsIgnoreCase(tok[1], "REC") && n == 3) return rec(tok[2], reply, cap);
  if (equalsIgnoreCase(tok[1], "MIC") && n == 3) return mic(tok[2], reply, cap);
  if (equalsIgnoreCase(tok[1], "REBOOT") && n == 2) {
    say(reply, cap, "CFG OK reboot");
    return ConsoleResult::Reboot;
  }
  say(reply, cap, "CFG ERR comando sconosciuto");
  return ConsoleResult::Error;
}

ConsoleResult SettingsConsole::get(char* reply, size_t cap) {
  if (reply && cap > 0) {
    char qthText[32];
    if (stored_.qthSet) {
      formatQth(qthText, sizeof(qthText), stored_.qthLatE7, stored_.qthLonE7);
    } else {
      snprintf(qthText, sizeof(qthText), "none");
    }
    snprintf(reply, cap, "CFG callsign=%s freq=%u.%03u freqmsg=%u.%03u freqqual=%u.%03u power=%d volume=%d micfloor=%d profile=%s board=%s reboot=%d test=%d pos=%s qth=%s gps=%s",
             stored_.callsign, static_cast<unsigned>(stored_.freqKhz / 1000), static_cast<unsigned>(stored_.freqKhz % 1000),
             static_cast<unsigned>(stored_.freqMsgKhz / 1000), static_cast<unsigned>(stored_.freqMsgKhz % 1000),
             static_cast<unsigned>(stored_.qualFreqKhz() / 1000), static_cast<unsigned>(stored_.qualFreqKhz() % 1000),
             stored_.powerDbm, stored_.volume, stored_.micFloorDb, profileFor(stored_.profile).code, board_, stored_ != active_ ? 1 : 0, testMode_ ? 1 : 0, stored_.posTx ? "on" : "off", qthText, stored_.gpsOn ? "on" : "off");
  }
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::versionInfo(char* reply, size_t cap) {
  if (reply && cap > 0) snprintf(reply, cap, "CFG VERSION fw=%s board=%s proto=%d", version_, board_, static_cast<int>(kProtocolVersion));
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
  } else if (equalsIgnoreCase(key, "FREQMSG")) {
    // Come FREQ, per il profilo Messaggio. Nessun controllo che sia diversa da FREQ: puo' essere uguale (prove su una frequenza nota).
    uint32_t khz = 0;
    if (!parseFreqKhz(value, khz) || khz < kMinFreqKhz || khz > kMaxFreqKhz) {
      say(reply, cap, "CFG ERR freqmsg %s", "in MHz, da 430.000 a 440.000");
      return ConsoleResult::Error;
    }
    next.freqMsgKhz = khz;
    snprintf(shown, sizeof(shown), "freqmsg=%u.%03u", static_cast<unsigned>(khz / 1000), static_cast<unsigned>(khz % 1000));
  } else if (equalsIgnoreCase(key, "FREQQUAL")) {
    uint32_t khz = 0;
    if (!parseFreqKhz(value, khz) || khz < kMinFreqKhz || khz > kMaxFreqKhz) {
      say(reply, cap, "CFG ERR freqqual %s", "in MHz, da 430.000 a 440.000");
      return ConsoleResult::Error;
    }
    next.freqQualKhz = khz;
    snprintf(shown, sizeof(shown), "freqqual=%u.%03u", static_cast<unsigned>(khz / 1000), static_cast<unsigned>(khz % 1000));
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
  } else if (equalsIgnoreCase(key, "MICFLOOR")) {
    int fl = 0;
    if (!parseInt(value, fl) || fl < kMinMicFloorDb || fl > kMaxMicFloorDb) {
      say(reply, cap, "CFG ERR micfloor %s", "in dBFS, da -80 a -36");
      return ConsoleResult::Error;
    }
    next.micFloorDb = fl;
    snprintf(shown, sizeof(shown), "micfloor=%d", fl);
  } else if (equalsIgnoreCase(key, "PROFILE")) {
    uint8_t id = 0;
    if (!profileFromCode(value, id)) {
      say(reply, cap, "CFG ERR profile %s", "usa STD (Standard), MSG (Messaggio) oppure QUA (Qualita')");
      return ConsoleResult::Error;
    }
    next.profile = id;
    snprintf(shown, sizeof(shown), "profile=%s", profileFor(id).code);
  } else if (equalsIgnoreCase(key, "POS")) {
    if (equalsIgnoreCase(value, "ON")) {
      next.posTx = true;
    } else if (equalsIgnoreCase(value, "OFF")) {
      next.posTx = false;
    } else {
      say(reply, cap, "CFG ERR pos %s", "usa ON oppure OFF");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "pos=%s", next.posTx ? "on" : "off");
  } else if (equalsIgnoreCase(key, "GPS")) {
    // Il GPS c'e' solo sulla T-Beam: sulle altre schede un'impostazione che non fa niente confonderebbe.
    if (!equalsIgnoreCase(board_, "tbeam")) {
      say(reply, cap, "CFG ERR gps %s", "questa scheda non ha il GPS");
      return ConsoleResult::Error;
    }
    if (equalsIgnoreCase(value, "ON")) {
      next.gpsOn = true;
    } else if (equalsIgnoreCase(value, "OFF")) {
      next.gpsOn = false;
    } else {
      say(reply, cap, "CFG ERR gps %s", "usa ON oppure OFF");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "gps=%s", next.gpsOn ? "on" : "off");
  } else if (equalsIgnoreCase(key, "QTH")) {
    if (equalsIgnoreCase(value, "NONE")) {
      next.qthSet = false;
      next.qthLatE7 = next.qthLonE7 = 0;
      snprintf(shown, sizeof(shown), "qth=none");
    } else {
      char tmp[40];
      strncpy(tmp, value, sizeof(tmp) - 1);
      tmp[sizeof(tmp) - 1] = 0;
      char* comma = strchr(tmp, ',');
      int32_t la = 0, lo = 0;
      if (!comma) {
        say(reply, cap, "CFG ERR qth %s", "usa latitudine,longitudine in gradi, per esempio 43.7696,11.2558");
        return ConsoleResult::Error;
      }
      *comma = 0;
      if (!parseDegreesE7(tmp, la) || !parseDegreesE7(comma + 1, lo) || !isValidCoordinate(la, lo)) {
        say(reply, cap, "CFG ERR qth %s", "latitudine da -90 a 90, longitudine da -180 a 180, senza spazi");
        return ConsoleResult::Error;
      }
      next.qthSet = true;
      next.qthLatE7 = la;
      next.qthLonE7 = lo;
      char q[32];
      formatQth(q, sizeof(q), la, lo);
      snprintf(shown, sizeof(shown), "qth=%s", q);
    }
  } else {
    say(reply, cap, "CFG ERR %s parametro sconosciuto", key);
    return ConsoleResult::Error;
  }

  if (!storage_.save(next)) {
    say(reply, cap, "CFG ERR %s salvataggio non riuscito", key);
    return ConsoleResult::Error;
  }
  stored_ = next;
  active_.micFloorDb = next.micFloorDb;  // il fondo del microfono si applica subito: non richiede il riavvio
  say(reply, cap, "CFG OK %s", shown);
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::test(const char* value, char* reply, size_t cap) {
  if (equalsIgnoreCase(value, "ON")) {
    testMode_ = true;
  } else if (equalsIgnoreCase(value, "OFF")) {
    testMode_ = false;
  } else if (equalsIgnoreCase(value, "CODEC")) {
    say(reply, cap, "CFG OK codec: parla per 3 secondi dopo il bip, poi ascolta quattro volte (senza e con Codec2, poi a ritmo di ricezione)");
    return ConsoleResult::CodecTest;
  } else if (equalsIgnoreCase(value, "AUDIO")) {
    say(reply, cap, "CFG OK audio prova in corso: ascolta i toni due volte, senza e con la radio (circa 10 secondi)");
    return ConsoleResult::AudioTest;
  } else {
    say(reply, cap, "CFG ERR test usa ON, OFF, AUDIO oppure CODEC");
    return ConsoleResult::Error;
  }
  say(reply, cap, testMode_ ? "CFG OK test=on" : "CFG OK test=off");
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::rec(const char* value, char* reply, size_t cap) {
  if (equalsIgnoreCase(value, "ON")) {
    recMode_ = true;
  } else if (equalsIgnoreCase(value, "OFF")) {
    recMode_ = false;
  } else {
    say(reply, cap, "CFG ERR rec usa ON oppure OFF");
    return ConsoleResult::Error;
  }
  say(reply, cap, recMode_ ? "CFG OK rec=on" : "CFG OK rec=off");
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::mic(const char* value, char* reply, size_t cap) {
  if (equalsIgnoreCase(value, "MEASURE")) {
    say(reply, cap, "CFG OK mic misura in corso: stare in silenzio per 3 s");
    return ConsoleResult::MicMeasure;
  }
  say(reply, cap, "CFG ERR mic usa MEASURE");
  return ConsoleResult::Error;
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
