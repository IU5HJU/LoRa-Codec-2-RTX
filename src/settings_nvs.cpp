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
  s.freqQualKhz = s.freqKhz;
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
    out.freqKhz = p.getUInt("freq", 0);
    out.freqMsgKhz = p.getUInt("fmsg", out.freqKhz);  // assente nelle versioni precedenti: stessa frequenza dello Standard
    out.freqQualKhz = p.getUInt("fqua", out.freqKhz);
    out.role = static_cast<uint8_t>(p.getUChar("role", lcr::kRoleNormal));  // assenti nelle versioni precedenti
    out.freqOutKhz = p.getUInt("fout", 0);
    out.freqMcKhz = p.getUInt("fmc", 433175);  // assenti nelle versioni precedenti
    memset(out.mcDest, 0, sizeof(out.mcDest));
    p.getString("mcd", out.mcDest, sizeof(out.mcDest));
    out.mcSsid = static_cast<uint8_t>(p.getUChar("mcs", 99));
    out.rxMonitor = p.getBool("mon", false);  // assente nelle versioni precedenti
    out.powerDbm = p.getInt("pwr", 0);
    // "vol2" = scala nuova (5 = 0 dB, fino a +15 dB). Se manca c'e' al massimo il vecchio "vol" (10 = 0 dB): si converte.
    out.volume = p.isKey("vol2") ? p.getInt("vol2", lcr::kDefaultVolume)
                                 : (p.isKey("vol") ? lcr::migrateOldVolume(p.getInt("vol", 10)) : lcr::kDefaultVolume);
    out.micFloorDb = p.getInt("floor", lcr::kDefaultMicFloorDb);
    out.profile = static_cast<uint8_t>(p.getUChar("prof", lcr::kProfileStandard));  // assente nelle versioni precedenti
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
  const bool ok = p.putString("call", s.callsign) > 0 && p.putUInt("freq", s.freqKhz) > 0 && p.putUInt("fmsg", s.freqMsgKhz) > 0 && p.putUInt("fqua", s.qualFreqKhz()) > 0 && p.putUChar("role", s.role) > 0 && p.putUInt("fout", s.freqOutKhz) > 0 && p.putBool("mon", s.rxMonitor) > 0 && p.putUInt("fmc", s.freqMcKhz) > 0 && p.putUChar("mcs", s.mcSsid) > 0 && p.putInt("pwr", s.powerDbm) > 0 &&
                    p.putInt("vol2", s.volume) > 0 &&
                    p.putInt("floor", s.micFloorDb) > 0 && p.putUChar("prof", s.profile) > 0 &&
                    p.putBool("gps", s.gpsOn) > 0 && p.putBool("ptx", s.posTx) > 0 && p.putBool("qset", s.qthSet) > 0 && p.putInt("qlat", s.qthLatE7) > 0 &&
                    p.putInt("qlon", s.qthLonE7) > 0;
  p.putString("mcd", s.mcDest);  // vuoto = nessun destinatario (put restituisce 0: non e' un errore)
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
