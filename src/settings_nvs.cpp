#include "settings_nvs.h"

#include <cstring>

#include "user_config.h"

namespace {
constexpr const char* kNamespace = "lcr";
}

lcr::Settings compileTimeSettings() {
  lcr::Settings s;
  strncpy(s.callsign, LCR_CALLSIGN, lcr::kMaxCallsign);
  s.freqKhz = static_cast<uint32_t>(LCR_FREQUENCY_MHZ * 1000.0f + 0.5f);
  s.freqRangeKhz = s.freqKhz;
  s.freqMsgKhz = s.freqKhz;  // tutte le frequenze dei profili partono uguali
  s.powerDbm = LCR_TX_POWER_DBM;
  s.volume = LCR_VOLUME_STEP;
  s.micFloorDb = LCR_MIC_FLOOR_DBFS;
  s.profile = lcr::kProfileStandard;
  s.gpsOn = false;  // il GPS consuma batteria: acceso solo se l'operatore lo sceglie
  s.posTx = false;  // la posizione e' un dato personale: non si trasmette finche' l'operatore non lo sceglie
  s.qthSet = false;
  return s;
}

bool NvsSettingsStorage::load(lcr::Settings& out) {
  Preferences p;
  if (!p.begin(kNamespace, true)) return false;
  const bool all = p.isKey("call") && p.isKey("freq") && p.isKey("pwr");
  if (all) {
    memset(out.callsign, 0, sizeof(out.callsign));
    p.getString("call", out.callsign, sizeof(out.callsign));
    // Dal 12/11/2026 "Standard" e' il Codec2 3200 (prima "Qualita'") e il 1200 si chiama "Range". Le impostazioni salvate prima di allora (senza
    // "pv") hanno in "freq" la frequenza del vecchio Standard (il Codec2 1200, oggi Range) e in "fqua" quella della vecchia Qualita' (oggi
    // Standard): si leggono scambiandole, cosi' una scheda gia' in uso resta sulla stessa frequenza e sullo stesso modo radio (l'id del
    // profilo non cambia: 0 = Codec2 1200, 2 = Codec2 3200). Al primo salvataggio passano al formato nuovo.
    const bool newFormat = p.isKey("pv");
    if (newFormat) {
      out.freqKhz = p.getUInt("freq", 0);
      out.freqRangeKhz = p.getUInt("frng", out.freqKhz);
    } else {
      out.freqRangeKhz = p.getUInt("freq", 0);
      out.freqKhz = p.getUInt("fqua", out.freqRangeKhz);
    }
    out.freqMsgKhz = p.getUInt("fmsg", out.freqKhz);  // assente nelle versioni precedenti: stessa frequenza dello Standard
    out.role = static_cast<uint8_t>(p.getUChar("role", lcr::kRoleNormal));  // assenti nelle versioni precedenti
    out.freqOutKhz = p.getUInt("fout", 0);
    out.shiftKhz = p.getInt("shf", 0);  // assente nelle versioni precedenti: simplex
    out.freqMcKhz = p.getUInt("fmc", 433175);  // assenti nelle versioni precedenti
    out.mcWindowS = p.getUShort("mcw", 300);
    for (size_t i = 0; i < lcr::kMcDestSlots; i++) {  // assenti nelle versioni precedenti: restano i destinatari di esempio
      char key[4] = {'m', 'd', static_cast<char>('1' + i), 0};
      if (p.isKey(key)) {
        memset(out.mcDests[i], 0, sizeof(out.mcDests[i]));
        p.getString(key, out.mcDests[i], sizeof(out.mcDests[i]));
      }
    }
    out.mcSsid = static_cast<uint8_t>(p.getUChar("mcs", 99));
    out.rxMonitor = p.getBool("mon", false);  // assente nelle versioni precedenti
    memset(out.sysopCall, 0, sizeof(out.sysopCall));
    if (p.isKey("sop")) p.getString("sop", out.sysopCall, sizeof(out.sysopCall));
    out.sysopMode = p.getBool("som", false);
    memset(out.wifiSsid, 0, sizeof(out.wifiSsid));
    memset(out.wifiPass, 0, sizeof(out.wifiPass));
    memset(out.peerIp, 0, sizeof(out.peerIp));
    if (p.isKey("wss")) p.getString("wss", out.wifiSsid, sizeof(out.wifiSsid));
    if (p.isKey("wsp")) p.getString("wsp", out.wifiPass, sizeof(out.wifiPass));
    if (p.isKey("pip")) p.getString("pip", out.peerIp, sizeof(out.peerIp));
    out.peerPort = p.getUShort("ppt", 5400);
    out.nodePort = p.getUShort("npt", 5400);
    out.nodeId = p.getUInt("nid", 0);
    out.peerId = p.getUInt("pid", 0);
    out.nodeKeySet = p.isKey("nky") && p.getBytesLength("nky") == sizeof(out.nodeKey);
    if (out.nodeKeySet) p.getBytes("nky", out.nodeKey, sizeof(out.nodeKey));
    out.idMorse = p.getBool("idm", true);  // assente nelle versioni precedenti
    out.idIntervalMin = static_cast<uint8_t>(p.getUChar("idi", 10));  // assente nelle versioni precedenti
    out.sysopKeySet = p.isKey("sky") && p.getBytesLength("sky") == sizeof(out.sysopKey);
    if (out.sysopKeySet) p.getBytes("sky", out.sysopKey, sizeof(out.sysopKey));
    out.powerDbm = p.getInt("pwr", 0);
    // "vol2" = scala nuova (5 = 0 dB, fino a +15 dB). Se manca c'e' al massimo il vecchio "vol" (10 = 0 dB): si converte.
    out.volume = p.isKey("vol2") ? p.getInt("vol2", lcr::kDefaultVolume)
                                 : (p.isKey("vol") ? lcr::migrateOldVolume(p.getInt("vol", 10)) : lcr::kDefaultVolume);
    out.micFloorDb = p.getInt("floor", lcr::kDefaultMicFloorDb);
    out.profile = static_cast<uint8_t>(p.getUChar("prof", newFormat ? lcr::kProfileStandard : lcr::kProfileRange));  // assente nelle versioni precedenti: erano sul Codec2 1200
    out.gpsOn = p.getBool("gps", false);  // assenti nelle versioni precedenti
    out.posTx = p.getBool("ptx", false);
    out.qthSet = p.getBool("qset", false);
    out.qthLatE7 = p.getInt("qlat", 0);
    out.qthLonE7 = p.getInt("qlon", 0);
    for (size_t i = 0; i < lcr::kTextSlots; i++) {  // assenti nelle versioni precedenti: restano i testi di esempio
      char key[4] = {'t', static_cast<char>('1' + i), 0, 0};
      if (p.isKey(key)) {
        memset(out.texts[i], 0, sizeof(out.texts[i]));
        p.getString(key, out.texts[i], sizeof(out.texts[i]));
      }
    }
  }
  p.end();
  return all;
}

bool NvsSettingsStorage::save(const lcr::Settings& s) {
  Preferences p;
  if (!p.begin(kNamespace, false)) return false;
  const bool ok = p.putString("call", s.callsign) > 0 && p.putUInt("freq", s.freqKhz) > 0 && p.putUInt("fmsg", s.freqMsgKhz) > 0 && p.putUInt("frng", s.rangeFreqKhz()) > 0 && p.putUChar("pv", 2) > 0 && p.putUChar("role", s.role) > 0 && p.putUInt("fout", s.freqOutKhz) > 0 && p.putBool("mon", s.rxMonitor) > 0 && p.putUInt("fmc", s.freqMcKhz) > 0 && p.putUChar("mcs", s.mcSsid) > 0 && p.putInt("pwr", s.powerDbm) > 0 &&
                    p.putInt("vol2", s.volume) > 0 &&
                    p.putInt("floor", s.micFloorDb) > 0 && p.putUChar("prof", s.profile) > 0 &&
                    p.putBool("gps", s.gpsOn) > 0 && p.putBool("ptx", s.posTx) > 0 && p.putBool("qset", s.qthSet) > 0 && p.putInt("qlat", s.qthLatE7) > 0 &&
                    p.putInt("qlon", s.qthLonE7) > 0;
  p.putUShort("mcw", s.mcWindowS);
  p.putInt("shf", s.shiftKhz);
  p.remove("fqua");  // vecchio formato (prima del 12/11/2026)
  p.putString("sop", s.sysopCall);
  p.putBool("som", s.sysopMode);
  p.putUChar("idi", s.idIntervalMin);
  p.putBool("idm", s.idMorse);
  p.putString("wss", s.wifiSsid);
  p.putString("wsp", s.wifiPass);
  p.putString("pip", s.peerIp);
  p.putUShort("ppt", s.peerPort);
  p.putUShort("npt", s.nodePort);
  p.putUInt("nid", s.nodeId);
  p.putUInt("pid", s.peerId);
  if (s.nodeKeySet) p.putBytes("nky", s.nodeKey, sizeof(s.nodeKey));
  else p.remove("nky");
  if (s.sysopKeySet) p.putBytes("sky", s.sysopKey, sizeof(s.sysopKey));
  else p.remove("sky");
  for (size_t i = 0; i < lcr::kMcDestSlots; i++) {  // un posto vuoto salva una stringa vuota
    char key[4] = {'m', 'd', static_cast<char>('1' + i), 0};
    p.putString(key, s.mcDests[i]);
  }
  for (size_t i = 0; i < lcr::kTextSlots; i++) {  // un posto vuoto salva una stringa vuota (put restituisce 0: non e' un errore)
    char key[4] = {'t', static_cast<char>('1' + i), 0, 0};
    p.putString(key, s.texts[i]);
  }
  p.end();
  return ok;
}

bool NvsSettingsStorage::erase() {
  Preferences p;
  if (!p.begin(kNamespace, false)) return false;
  const bool ok = p.clear();
  p.end();
  return ok;
}

uint32_t loadSysopCounter() {
  Preferences p;
  if (!p.begin(kNamespace, true)) return 0;
  const uint32_t c = p.getUInt("soc", 0);
  p.end();
  return c;
}

bool saveSysopCounter(uint32_t counter) {
  Preferences p;
  if (!p.begin(kNamespace, false)) return false;
  const bool ok = p.putUInt("soc", counter) > 0;
  p.end();
  return ok;
}
