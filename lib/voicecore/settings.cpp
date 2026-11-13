#include "settings.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "packet.h"

namespace lcr {

bool operator==(const Settings& a, const Settings& b) {
  return strncmp(a.callsign, b.callsign, sizeof(a.callsign)) == 0 && a.freqKhz == b.freqKhz && a.freqMsgKhz == b.freqMsgKhz && a.shiftKhz == b.shiftKhz &&
         a.rangeFreqKhz() == b.rangeFreqKhz() && a.role == b.role && a.outFreqKhz() == b.outFreqKhz() &&
         a.powerDbm == b.powerDbm && a.volume == b.volume && a.micFloorDb == b.micFloorDb &&
         strncmp(a.sysopCall, b.sysopCall, sizeof(a.sysopCall)) == 0 && a.sysopKeySet == b.sysopKeySet && a.sysopMode == b.sysopMode && a.idIntervalMin == b.idIntervalMin && strncmp(a.wifiSsid, b.wifiSsid, sizeof(a.wifiSsid)) == 0 && strncmp(a.wifiPass, b.wifiPass, sizeof(a.wifiPass)) == 0 &&
         strncmp(a.peerIp, b.peerIp, sizeof(a.peerIp)) == 0 && a.peerPort == b.peerPort && a.nodePort == b.nodePort && a.nodeId == b.nodeId && a.peerId == b.peerId &&
         a.nodeKeySet == b.nodeKeySet && (!a.nodeKeySet || memcmp(a.nodeKey, b.nodeKey, sizeof(a.nodeKey)) == 0) && a.idMorse == b.idMorse &&
         (!a.sysopKeySet || memcmp(a.sysopKey, b.sysopKey, sizeof(a.sysopKey)) == 0) &&
         a.profile == b.profile && a.rxMonitor == b.rxMonitor && a.gpsOn == b.gpsOn && a.posTx == b.posTx && a.qthSet == b.qthSet &&
         (!a.qthSet || (a.qthLatE7 == b.qthLatE7 && a.qthLonE7 == b.qthLonE7));
}

// Ogni posto dei testi e' vuoto oppure un testo valido, terminato.
static bool validTexts(const Settings& s) {
  for (size_t i = 0; i < kTextSlots; i++) {
    const size_t n = strnlen(s.texts[i], kMaxTextChars + 1);
    if (n > kMaxTextChars || (n > 0 && !isValidText(s.texts[i], n))) return false;
  }
  return true;
}

bool isValidMcDest(const char* d) {
  if (!d || !d[0]) return false;
  const size_t n = strnlen(d, 16);
  if (n > 15) return false;
  if (n == 1 && d[0] == '*') return true;
  for (size_t i = 0; i < n; i++) {
    const char c = d[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-')) return false;
  }
  return true;
}

static bool printableAscii(const char* t, size_t maxLen) {
  const size_t n = strnlen(t, maxLen + 1);
  if (n > maxLen) return false;
  for (size_t i = 0; i < n; i++)
    if (static_cast<unsigned char>(t[i]) < 0x20 || static_cast<unsigned char>(t[i]) > 0x7E) return false;
  return true;
}

// "a.b.c.d" con quattro numeri da 0 a 255
bool isValidHostName(const char* s) {
  if (!s) return false;
  const size_t n = strnlen(s, kMaxPeerHost + 2);
  if (n == 0 || n > kMaxPeerHost) return false;
  bool letter = false;
  size_t label = 0;
  for (size_t i = 0; i < n; i++) {
    const char c = s[i];
    if (c == '.') {
      if (label == 0 || s[i - 1] == '-') return false;
      label = 0;
      continue;
    }
    if (isalpha(static_cast<unsigned char>(c))) letter = true;
    else if (!isdigit(static_cast<unsigned char>(c)) && c != '-') return false;
    if (c == '-' && label == 0) return false;
    if (++label > 63) return false;
  }
  return label > 0 && s[n - 1] != '-' && letter;
}

bool isValidPeerHost(const char* s) { return isValidIpv4(s) || isValidHostName(s); }

bool isValidIpv4(const char* s) {
  if (!s || !s[0]) return false;
  int parts = 0;
  const char* p = s;
  while (*p) {
    if (!isdigit(static_cast<unsigned char>(*p))) return false;
    int v = 0, digits = 0;
    while (isdigit(static_cast<unsigned char>(*p))) {
      v = v * 10 + (*p - '0');
      if (++digits > 3 || v > 255) return false;
      p++;
    }
    parts++;
    if (*p == '.') {
      p++;
      if (!*p) return false;
    } else if (*p) {
      return false;
    }
  }
  return parts == 4;
}

static bool validNode(const Settings& s) {
  return printableAscii(s.wifiSsid, 32) && printableAscii(s.wifiPass, 64) && (s.peerIp[0] == 0 || isValidPeerHost(s.peerIp)) &&
         strnlen(s.peerIp, kMaxPeerHost + 1) <= kMaxPeerHost && s.peerPort >= 1 && s.nodePort >= 1;
}

static bool validMcDests(const Settings& s) {
  for (size_t i = 0; i < kMcDestSlots; i++) {
    if (strnlen(s.mcDests[i], 16) > 15 || (s.mcDests[i][0] && !isValidMcDest(s.mcDests[i]))) return false;
  }
  return true;
}

// Lo shift e' libero da -10000 a +10000 kHz (da 0 a 10 MHz, in positivo o negativo, come l'offset di un ripetitore sulle radio) e, se c'e', porta in banda (430-440 MHz) tutte le frequenze dei profili.
static bool validShift(const Settings& s) {
  if (s.shiftKhz == 0) return true;
  if (s.shiftKhz > kMaxShiftKhz || s.shiftKhz < -kMaxShiftKhz) return false;
  const int64_t f[3] = {s.freqKhz, s.freqMsgKhz, s.rangeFreqKhz()};
  for (int64_t v : f) {
    const int64_t t = v + s.shiftKhz;
    if (t < static_cast<int64_t>(kMinFreqKhz) || t > static_cast<int64_t>(kMaxFreqKhz)) return false;
  }
  return true;
}

bool isValidSettings(const Settings& s) {
  return validShift(s) && isValidCallsign(s.callsign) && (s.sysopCall[0] == 0 || isValidCallsign(s.sysopCall)) && s.idIntervalMin >= kMinIdIntervalMin && s.idIntervalMin <= kMaxIdIntervalMin && s.freqKhz >= kMinFreqKhz && s.freqKhz <= kMaxFreqKhz && s.freqMsgKhz >= kMinFreqKhz &&
         s.freqMsgKhz <= kMaxFreqKhz && s.freqMcKhz >= kMinFreqKhz && s.freqMcKhz <= kMaxFreqKhz && s.mcSsid <= 99 &&
         s.mcWindowS >= kMinMcWindowS && s.mcWindowS <= kMaxMcWindowS && validMcDests(s) && (s.freqRangeKhz == 0 || (s.freqRangeKhz >= kMinFreqKhz && s.freqRangeKhz <= kMaxFreqKhz)) &&
         (s.freqOutKhz == 0 || (s.freqOutKhz >= kMinFreqKhz && s.freqOutKhz <= kMaxFreqKhz)) && s.role <= kRoleNode && validNode(s) &&
         s.powerDbm >= kMinPowerDbm && s.powerDbm <= kMaxPowerDbm && s.volume >= kMinVolume &&
         s.volume <= kMaxVolume &&
         s.micFloorDb >= kMinMicFloorDb && s.micFloorDb <= kMaxMicFloorDb && s.profile < kProfileCount &&
         (!s.qthSet || isValidCoordinate(s.qthLatE7, s.qthLonE7)) && validTexts(s);
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

  // CFG RADIOTEST ...: prove radio del Morse (parametri liberi, letti da radio_test.cpp)
  if (strncasecmp(line, "CFG", 3) == 0 && (line[3] == ' ' || line[3] == '\t')) {
    const char* q = line + 3;
    while (*q == ' ' || *q == '\t') q++;
    if (strncasecmp(q, "RADIOTEST", 9) == 0 && (q[9] == 0 || q[9] == ' ' || q[9] == '\t')) {
      char err[80];
      if (!parseRadioTest(q + 9, radioTest_, err, sizeof(err))) {
        say(reply, cap, "CFG ERR radiotest %s", err);
        return ConsoleResult::Error;
      }
      say(reply, cap, "CFG OK radiotest in corso (un tasto in seriale la ferma)");
      return ConsoleResult::RadioTest;
    }
  }

  // Spezza in parole (massimo 4) su una copia: la riga originale non si tocca.
  char buf[96];  // solo per spezzare in parole: i testi (CFG SET TEXTn) si leggono dalla riga originale
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

  // CFG SET WIFISSID / WIFIPASS <resto della riga>: possono avere spazi.
  if (n >= 4 && equalsIgnoreCase(tok[1], "SET") && (equalsIgnoreCase(tok[2], "WIFISSID") || equalsIgnoreCase(tok[2], "WIFIPASS"))) {
    const bool isSsid = equalsIgnoreCase(tok[2], "WIFISSID");
    const char* rest = line + (tok[3] - buf);
    size_t len = strlen(rest);
    while (len > 0 && (rest[len - 1] == ' ' || rest[len - 1] == '\t' || rest[len - 1] == '\r' || rest[len - 1] == '\n')) len--;
    Settings next = stored_;
    char* dst = isSsid ? next.wifiSsid : next.wifiPass;
    const size_t maxLen = isSsid ? 32 : 64;
    if (len == 4 && strncasecmp(rest, "NONE", 4) == 0) {
      memset(dst, 0, maxLen + 1);
    } else {
      bool ok = len >= 1 && len <= maxLen && (isSsid || len >= 8);  // WPA2: password da 8 a 64 caratteri
      for (size_t i = 0; ok && i < len; i++) ok = static_cast<unsigned char>(rest[i]) >= 0x20 && static_cast<unsigned char>(rest[i]) <= 0x7E;
      if (!ok) {
        say(reply, cap, isSsid ? "CFG ERR wifissid %s" : "CFG ERR wifipass %s", isSsid ? "da 1 a 32 caratteri ASCII (NONE lo toglie)" : "da 8 a 64 caratteri ASCII (NONE per una rete aperta)");
        return ConsoleResult::Error;
      }
      memset(dst, 0, maxLen + 1);
      memcpy(dst, rest, len);
    }
    if (!storage_.save(next)) {
      say(reply, cap, isSsid ? "CFG ERR wifissid %s" : "CFG ERR wifipass %s", "salvataggio non riuscito");
      return ConsoleResult::Error;
    }
    stored_ = next;
    if (reply && cap > 0) {
      if (isSsid) snprintf(reply, cap, "CFG OK wifissid=%s", next.wifiSsid[0] ? next.wifiSsid : "none");
      else snprintf(reply, cap, "CFG OK wifipass=%s", next.wifiPass[0] ? "set" : "none");  // la password non si mostra
    }
    return ConsoleResult::Ok;
  }

  // CFG SET TEXTn <testo>: il testo e' il resto della riga (puo' avere spazi).
  if (n >= 4 && equalsIgnoreCase(tok[1], "SET") && strlen(tok[2]) == 5 && strncasecmp(tok[2], "TEXT", 4) == 0 &&
      tok[2][4] >= '1' && tok[2][4] < '1' + static_cast<int>(kTextSlots)) {
    const char* rest = line + (tok[3] - buf);
    size_t len = strlen(rest);
    while (len > 0 && (rest[len - 1] == ' ' || rest[len - 1] == '\t' || rest[len - 1] == '\r' || rest[len - 1] == '\n')) len--;
    return setText(tok[2][4] - '1', rest, len, reply, cap);
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && strlen(tok[2]) == 5 && strncasecmp(tok[2], "TEXT", 4) == 0 &&
      tok[2][4] >= '1' && tok[2][4] < '1' + static_cast<int>(kTextSlots)) {
    return getText(tok[2][4] - '1', reply, cap);
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && strlen(tok[2]) == 7 && strncasecmp(tok[2], "MCDEST", 6) == 0 && tok[2][6] >= '1' &&
      tok[2][6] < '1' + static_cast<int>(kMcDestSlots)) {
    if (reply && cap > 0) snprintf(reply, cap, "CFG MCDEST%c=%s", tok[2][6], stored_.mcDests[tok[2][6] - '1']);
    return ConsoleResult::Ok;
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && equalsIgnoreCase(tok[2], "MC")) {
    if (reply && cap > 0) {
      snprintf(reply, cap, "CFG MC freqmc=%u.%03u ssid=%u window=%u", static_cast<unsigned>(stored_.freqMcKhz / 1000),
               static_cast<unsigned>(stored_.freqMcKhz % 1000), static_cast<unsigned>(stored_.mcSsid), static_cast<unsigned>(stored_.mcWindowS));
    }
    return ConsoleResult::Ok;
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && equalsIgnoreCase(tok[2], "NODE")) {
    if (reply && cap > 0) {
      snprintf(reply, cap, "CFG NODE ssid=%s pass=%s ip=%s port=%u listen=%u id=%lu peerid=%lu key=%s", stored_.wifiSsid[0] ? stored_.wifiSsid : "none",
               stored_.wifiPass[0] ? "set" : "none", stored_.peerIp[0] ? stored_.peerIp : "none", static_cast<unsigned>(stored_.peerPort),
               static_cast<unsigned>(stored_.nodePort), static_cast<unsigned long>(stored_.nodeId), static_cast<unsigned long>(stored_.peerId),
               stored_.nodeKeySet ? "set" : "none");
    }
    return ConsoleResult::Ok;
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && equalsIgnoreCase(tok[2], "IDMORSE")) {
    say(reply, cap, stored_.idMorse ? "CFG IDMORSE=on" : "CFG IDMORSE=off");
    return ConsoleResult::Ok;
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && equalsIgnoreCase(tok[2], "SHIFT")) {
    if (reply && cap > 0) snprintf(reply, cap, "CFG SHIFT=%s%d", stored_.shiftKhz > 0 ? "+" : "", static_cast<int>(stored_.shiftKhz));
    return ConsoleResult::Ok;
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && equalsIgnoreCase(tok[2], "IDINT")) {
    if (reply && cap > 0) snprintf(reply, cap, "CFG IDINT=%u", static_cast<unsigned>(stored_.idIntervalMin));
    return ConsoleResult::Ok;
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && equalsIgnoreCase(tok[2], "SYSOP")) {
    if (reply && cap > 0) {
      snprintf(reply, cap, "CFG SYSOP call=%s key=%s mode=%s", stored_.sysopCall[0] ? stored_.sysopCall : "none", stored_.sysopKeySet ? "set" : "none",
               stored_.sysopMode ? "on" : "off");
    }
    return ConsoleResult::Ok;
  }
  if (n == 3 && equalsIgnoreCase(tok[1], "GET") && equalsIgnoreCase(tok[2], "MONITOR")) {
    say(reply, cap, stored_.rxMonitor ? "CFG MONITOR=on" : "CFG MONITOR=off");
    return ConsoleResult::Ok;
  }
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
    snprintf(reply, cap, "CFG callsign=%s freq=%u.%03u freqmsg=%u.%03u freqrange=%u.%03u power=%d volume=%d micfloor=%d profile=%s board=%s reboot=%d test=%d pos=%s qth=%s gps=%s role=%s freqout=%u.%03u",
             stored_.callsign, static_cast<unsigned>(stored_.freqKhz / 1000), static_cast<unsigned>(stored_.freqKhz % 1000),
             static_cast<unsigned>(stored_.freqMsgKhz / 1000), static_cast<unsigned>(stored_.freqMsgKhz % 1000),
             static_cast<unsigned>(stored_.rangeFreqKhz() / 1000), static_cast<unsigned>(stored_.rangeFreqKhz() % 1000),
             stored_.powerDbm, stored_.volume, stored_.micFloorDb, profileFor(stored_.profile).code, board_, stored_ != active_ ? 1 : 0, testMode_ ? 1 : 0, stored_.posTx ? "on" : "off", qthText, stored_.gpsOn ? "on" : "off", roleCode(stored_.role),
             static_cast<unsigned>(stored_.outFreqKhz() / 1000), static_cast<unsigned>(stored_.outFreqKhz() % 1000));
  }
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::versionInfo(char* reply, size_t cap) {
  if (reply && cap > 0) snprintf(reply, cap, "CFG VERSION fw=%s board=%s proto=%d", version_, board_, static_cast<int>(kProtocolVersion));
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::set(const char* key, const char* value, char* reply, size_t cap) {
  Settings next = stored_;
  char shown[kMaxPeerHost + 24];  // la riga "peer=nome:porta" e la piu' lunga

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
    // Le frequenze degli altri modi che erano uguali alla vecchia seguono la nuova (di base partono tutte uguali): chi ha una frequenza diversa per
    // una configurazione particolare la tiene, e la cambia con FREQMSG e FREQRANGE.
    const uint32_t old = next.freqKhz;
    next.freqKhz = khz;
    if (next.freqMsgKhz == old) next.freqMsgKhz = khz;
    if (next.freqRangeKhz != 0 && next.freqRangeKhz == old) next.freqRangeKhz = khz;
    if (!validShift(next)) {
      say(reply, cap, "CFG ERR freq %s", "con lo shift impostato la frequenza di trasmissione uscirebbe da 430-440 MHz (CFG SET SHIFT 0)");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "freq=%u.%03u", static_cast<unsigned>(khz / 1000), static_cast<unsigned>(khz % 1000));
  } else if (equalsIgnoreCase(key, "FREQMSG")) {
    // Come FREQ, per il profilo Messaggio. Nessun controllo che sia diversa da FREQ: puo' essere uguale (prove su una frequenza nota).
    uint32_t khz = 0;
    if (!parseFreqKhz(value, khz) || khz < kMinFreqKhz || khz > kMaxFreqKhz) {
      say(reply, cap, "CFG ERR freqmsg %s", "in MHz, da 430.000 a 440.000");
      return ConsoleResult::Error;
    }
    next.freqMsgKhz = khz;
    if (!validShift(next)) {
      say(reply, cap, "CFG ERR freqmsg %s", "con lo shift impostato la frequenza di trasmissione uscirebbe da 430-440 MHz (CFG SET SHIFT 0)");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "freqmsg=%u.%03u", static_cast<unsigned>(khz / 1000), static_cast<unsigned>(khz % 1000));
  } else if (equalsIgnoreCase(key, "FREQRANGE")) {
    uint32_t khz = 0;
    if (!parseFreqKhz(value, khz) || khz < kMinFreqKhz || khz > kMaxFreqKhz) {
      say(reply, cap, "CFG ERR freqrange %s", "in MHz, da 430.000 a 440.000");
      return ConsoleResult::Error;
    }
    next.freqRangeKhz = khz;
    if (!validShift(next)) {
      say(reply, cap, "CFG ERR freqrange %s", "con lo shift impostato la frequenza di trasmissione uscirebbe da 430-440 MHz (CFG SET SHIFT 0)");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "freqrange=%u.%03u", static_cast<unsigned>(khz / 1000), static_cast<unsigned>(khz % 1000));
  } else if (equalsIgnoreCase(key, "FREQMC")) {
    uint32_t khz = 0;
    if (!parseFreqKhz(value, khz) || khz < kMinFreqKhz || khz > kMaxFreqKhz) {
      say(reply, cap, "CFG ERR freqmc %s", "in MHz, da 430.000 a 440.000");
      return ConsoleResult::Error;
    }
    next.freqMcKhz = khz;
    snprintf(shown, sizeof(shown), "freqmc=%u.%03u", static_cast<unsigned>(khz / 1000), static_cast<unsigned>(khz % 1000));
  } else if (strlen(key) == 7 && strncasecmp(key, "MCDEST", 6) == 0 && key[6] >= '1' && key[6] < '1' + static_cast<int>(kMcDestSlots)) {
    const int slot = key[6] - '1';
    if (equalsIgnoreCase(value, "NONE")) {
      next.mcDests[slot][0] = 0;
    } else {
      char up[sizeof(next.mcDests[0]) + 1];
      size_t i = 0;
      for (; value[i] && i < sizeof(up) - 1; i++) up[i] = static_cast<char>(toupper(static_cast<unsigned char>(value[i])));
      up[i] = 0;
      if (!isValidMcDest(up)) {
        say(reply, cap, "CFG ERR mcdest %s", "gruppo (22251), nominativo-SSID (IU5ABC-9, A-Z 0-9 / -, massimo 15), * per tutti, NONE per svuotare");
        return ConsoleResult::Error;
      }
      memset(next.mcDests[slot], 0, sizeof(next.mcDests[slot]));
      memcpy(next.mcDests[slot], up, strlen(up));
    }
    snprintf(shown, sizeof(shown), "mcdest%d=%s", slot + 1, next.mcDests[slot]);
  } else if (equalsIgnoreCase(key, "MCWINDOW")) {
    int w = 0;
    if (!parseInt(value, w) || w < kMinMcWindowS || w > kMaxMcWindowS) {
      say(reply, cap, "CFG ERR mcwindow %s", "secondi, da 30 a 1800");
      return ConsoleResult::Error;
    }
    next.mcWindowS = static_cast<uint16_t>(w);
    snprintf(shown, sizeof(shown), "mcwindow=%d", w);
  } else if (equalsIgnoreCase(key, "MCSSID")) {
    int ssid = 0;
    if (!parseInt(value, ssid) || ssid < 0 || ssid > 99) {
      say(reply, cap, "CFG ERR mcssid %s", "da 0 (nessuno) a 99");
      return ConsoleResult::Error;
    }
    next.mcSsid = static_cast<uint8_t>(ssid);
    snprintf(shown, sizeof(shown), "mcssid=%d", ssid);
  } else if (equalsIgnoreCase(key, "FREQOUT")) {
    uint32_t khz = 0;
    if (!parseFreqKhz(value, khz) || khz < kMinFreqKhz || khz > kMaxFreqKhz) {
      say(reply, cap, "CFG ERR freqout %s", "in MHz, da 430.000 a 440.000");
      return ConsoleResult::Error;
    }
    next.freqOutKhz = khz;
    snprintf(shown, sizeof(shown), "freqout=%u.%03u", static_cast<unsigned>(khz / 1000), static_cast<unsigned>(khz % 1000));
  } else if (equalsIgnoreCase(key, "ROLE")) {
    // Il ripetitore (RX e TX) usa solo la T3: due T-Beam sarebbero uno spreco. Il nodo Internet va bene su T3 e su T-Beam.
    if ((equalsIgnoreCase(value, "RPTRX") || equalsIgnoreCase(value, "RPTTX")) && equalsIgnoreCase(board_, "tbeam")) {
      say(reply, cap, "CFG ERR role %s", "il ripetitore usa solo la T3 (due T-Beam sarebbero uno spreco); il nodo va bene anche sulla T-Beam");
      return ConsoleResult::Error;
    }
    if (equalsIgnoreCase(value, "OFF")) {
      next.role = kRoleNormal;
    } else if (equalsIgnoreCase(value, "RPTRX")) {
      next.role = kRoleRepeaterRx;
    } else if (equalsIgnoreCase(value, "RPTTX")) {
      next.role = kRoleRepeaterTx;
    } else if (equalsIgnoreCase(value, "NODE")) {
      next.role = kRoleNode;
    } else {
      say(reply, cap, "CFG ERR role %s", "usa OFF, RPTRX, RPTTX oppure NODE");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "role=%s", roleCode(next.role));
  } else if (equalsIgnoreCase(key, "SYSOP")) {
    if (equalsIgnoreCase(value, "NONE")) {
      next.sysopCall[0] = 0;
    } else if (isValidCallsign(value)) {
      memset(next.sysopCall, 0, sizeof(next.sysopCall));
      strncpy(next.sysopCall, value, kMaxCallsign);
    } else {
      say(reply, cap, "CFG ERR sysop %s", "nominativo non valido (oppure NONE)");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "sysop=%s", next.sysopCall[0] ? next.sysopCall : "none");
  } else if (equalsIgnoreCase(key, "PEER")) {
    if (equalsIgnoreCase(value, "NONE")) {
      next.peerIp[0] = 0;
      next.peerPort = 5400;
      snprintf(shown, sizeof(shown), "peer=none");
    } else {
      char ip[kMaxPeerHost + 1] = {0};
      const char* colon = strrchr(value, ':');
      const size_t il = colon ? static_cast<size_t>(colon - value) : strlen(value);
      int port = 5400;
      bool ok = il > 0 && il < sizeof(ip);
      if (ok) {
        memcpy(ip, value, il);
        ok = isValidPeerHost(ip);
      }
      if (ok && colon) {
        const char* q = colon + 1;
        port = 0;
        ok = *q != 0;
        for (; ok && *q; q++) {
          if (!isdigit(static_cast<unsigned char>(*q)) || port > 6553) ok = false;
          else port = port * 10 + (*q - '0');
        }
        ok = ok && port >= 1 && port <= 65535;
      }
      if (!ok) {
        say(reply, cap, "CFG ERR peer %s", "usa indirizzo o nome e porta, per esempio 192.168.1.51:5400 o reflector.example.org:5400 (oppure NONE)");
        return ConsoleResult::Error;
      }
      strncpy(next.peerIp, ip, sizeof(next.peerIp) - 1);
      next.peerPort = static_cast<uint16_t>(port);
      snprintf(shown, sizeof(shown), "peer=%s:%d", next.peerIp, port);
    }
  } else if (equalsIgnoreCase(key, "NODEPORT")) {
    int port = 0;
    if (!parseInt(value, port) || port < 1 || port > 65535) {
      say(reply, cap, "CFG ERR nodeport %s", "porta UDP da 1 a 65535");
      return ConsoleResult::Error;
    }
    next.nodePort = static_cast<uint16_t>(port);
    snprintf(shown, sizeof(shown), "nodeport=%d", port);
  } else if (equalsIgnoreCase(key, "NODEID") || equalsIgnoreCase(key, "PEERID")) {
    const bool isPeer = equalsIgnoreCase(key, "PEERID");
    char* end = nullptr;
    const unsigned long v = strtoul(value, &end, 10);
    if (end == value || *end != 0 || strlen(value) > 10 || v > 4294967295UL || (!isPeer && v == 0)) {
      say(reply, cap, isPeer ? "CFG ERR peerid %s" : "CFG ERR nodeid %s", isPeer ? "numero da 0 (qualunque nodo) a 4294967295" : "numero da 1 a 4294967295");
      return ConsoleResult::Error;
    }
    (isPeer ? next.peerId : next.nodeId) = static_cast<uint32_t>(v);
    snprintf(shown, sizeof(shown), "%s=%lu", isPeer ? "peerid" : "nodeid", v);
  } else if (equalsIgnoreCase(key, "NODEKEY")) {
    if (equalsIgnoreCase(value, "NONE")) {
      next.nodeKeySet = false;
      memset(next.nodeKey, 0, sizeof(next.nodeKey));
    } else {
      uint8_t k[16];
      bool okHex = strlen(value) == 32;
      for (size_t i = 0; okHex && i < 32; i++) {
        const char c = value[i];
        int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (v < 0) okHex = false;
        else if (i % 2 == 0) k[i / 2] = static_cast<uint8_t>(v << 4);
        else k[i / 2] = static_cast<uint8_t>(k[i / 2] | v);
      }
      if (!okHex) {
        say(reply, cap, "CFG ERR nodekey %s", "servono 32 cifre esadecimali (oppure NONE)");
        return ConsoleResult::Error;
      }
      memcpy(next.nodeKey, k, sizeof(k));
      next.nodeKeySet = true;
    }
    snprintf(shown, sizeof(shown), "nodekey=%s", next.nodeKeySet ? "set" : "none");
  } else if (equalsIgnoreCase(key, "IDMORSE")) {
    if (equalsIgnoreCase(value, "ON")) {
      next.idMorse = true;
    } else if (equalsIgnoreCase(value, "OFF")) {
      next.idMorse = false;
    } else {
      say(reply, cap, "CFG ERR idmorse %s", "usa ON oppure OFF");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "idmorse=%s", next.idMorse ? "on" : "off");
  } else if (equalsIgnoreCase(key, "SHIFT")) {
    // un numero di kHz con o senza segno, da -10000 a +10000 ("0", "+7600", "-5000", anche "10000"): parseInt() si ferma a 4 cifre e non vuole il segno +
    int v = 0;
    const char* d = value;
    const bool neg = *d == '-';
    if (*d == '+' || *d == '-') d++;
    size_t nd = 0;
    int acc = 0;
    for (; isdigit(static_cast<unsigned char>(d[nd])) && nd < 6; nd++) acc = acc * 10 + (d[nd] - '0');
    const bool okNum = nd > 0 && d[nd] == 0;
    v = neg ? -acc : acc;
    if (!okNum || v > kMaxShiftKhz || v < -kMaxShiftKhz) {
      say(reply, cap, "CFG ERR shift %s", "in kHz, da -10000 a +10000 (0 = nessuno)");
      return ConsoleResult::Error;
    }
    next.shiftKhz = v;
    if (!validShift(next)) {
      say(reply, cap, "CFG ERR shift %s", "la frequenza di trasmissione uscirebbe da 430-440 MHz");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "shift=%s%d", v > 0 ? "+" : "", v);
  } else if (equalsIgnoreCase(key, "IDINT")) {
    int mins = 0;
    if (!parseInt(value, mins) || mins < kMinIdIntervalMin || mins > kMaxIdIntervalMin) {
      say(reply, cap, "CFG ERR idint %s", "in minuti, da 1 a 10");
      return ConsoleResult::Error;
    }
    next.idIntervalMin = static_cast<uint8_t>(mins);
    snprintf(shown, sizeof(shown), "idint=%d", mins);
  } else if (equalsIgnoreCase(key, "SYSOPMODE")) {
    if (equalsIgnoreCase(value, "ON")) {
      next.sysopMode = true;
    } else if (equalsIgnoreCase(value, "OFF")) {
      next.sysopMode = false;
    } else {
      say(reply, cap, "CFG ERR sysopmode %s", "usa ON oppure OFF");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "sysopmode=%s", next.sysopMode ? "on" : "off");
  } else if (equalsIgnoreCase(key, "SYSOPKEY")) {
    if (equalsIgnoreCase(value, "NONE")) {
      next.sysopKeySet = false;
      memset(next.sysopKey, 0, sizeof(next.sysopKey));
    } else {
      uint8_t k[16];
      bool okHex = strlen(value) == 32;
      for (size_t i = 0; okHex && i < 32; i++) {
        const char c = value[i];
        int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (v < 0) okHex = false;
        else if (i % 2 == 0) k[i / 2] = static_cast<uint8_t>(v << 4);
        else k[i / 2] = static_cast<uint8_t>(k[i / 2] | v);
      }
      if (!okHex) {
        say(reply, cap, "CFG ERR sysopkey %s", "servono 32 cifre esadecimali (oppure NONE)");
        return ConsoleResult::Error;
      }
      memcpy(next.sysopKey, k, sizeof(k));
      next.sysopKeySet = true;
    }
    snprintf(shown, sizeof(shown), "sysopkey=%s", next.sysopKeySet ? "set" : "none");
  } else if (equalsIgnoreCase(key, "MONITOR")) {
    if (equalsIgnoreCase(value, "ON")) {
      next.rxMonitor = true;
    } else if (equalsIgnoreCase(value, "OFF")) {
      next.rxMonitor = false;
    } else {
      say(reply, cap, "CFG ERR monitor %s", "usa ON oppure OFF");
      return ConsoleResult::Error;
    }
    snprintf(shown, sizeof(shown), "monitor=%s", next.rxMonitor ? "on" : "off");
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
      say(reply, cap, "CFG ERR profile %s", "usa STD (Standard), RNG (Range) oppure MSG (Messaggio); QUA vale ancora come STD");
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

ConsoleResult SettingsConsole::setText(int slot, const char* text, size_t len, char* reply, size_t cap) {
  Settings next = stored_;
  char shown[kMaxTextChars + 16];
  if (len == 4 && strncasecmp(text, "NONE", 4) == 0) {
    next.texts[slot][0] = 0;
    snprintf(shown, sizeof(shown), "text%d=", slot + 1);
  } else {
    if (!isValidText(text, len)) {
      if (reply && cap > 0) snprintf(reply, cap, "CFG ERR text%d testo ASCII da 1 a %d caratteri, senza { e } (NONE per svuotare)", slot + 1, static_cast<int>(kMaxTextChars));
      return ConsoleResult::Error;
    }
    memset(next.texts[slot], 0, sizeof(next.texts[slot]));
    memcpy(next.texts[slot], text, len);
    snprintf(shown, sizeof(shown), "text%d=%s", slot + 1, next.texts[slot]);
  }
  if (!storage_.save(next)) {
    if (reply && cap > 0) snprintf(reply, cap, "CFG ERR text%d salvataggio non riuscito", slot + 1);
    return ConsoleResult::Error;
  }
  stored_ = next;
  memcpy(active_.texts, next.texts, sizeof(active_.texts));  // valgono subito
  if (reply && cap > 0) snprintf(reply, cap, "CFG OK %s", shown);
  return ConsoleResult::Ok;
}

ConsoleResult SettingsConsole::getText(int slot, char* reply, size_t cap) {
  if (reply && cap > 0) snprintf(reply, cap, "CFG TEXT%d=%s", slot + 1, stored_.texts[slot]);
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
  memcpy(active_.texts, defaults_.texts, sizeof(active_.texts));
  say(reply, cap, "CFG OK reset");
  return ConsoleResult::Ok;
}

}  // namespace lcr
