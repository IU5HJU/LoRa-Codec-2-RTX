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
  s.powerDbm = LCR_TX_POWER_DBM;
  s.volume = LCR_VOLUME_STEP;
  s.micFloorDb = LCR_MIC_FLOOR_DBFS;
  s.profile = lcr::kProfileStandard;
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
    out.powerDbm = p.getInt("pwr", 0);
    out.volume = p.getInt("vol", lcr::kDefaultVolume);  // assente nelle versioni precedenti
    out.micFloorDb = p.getInt("floor", lcr::kDefaultMicFloorDb);
    out.profile = static_cast<uint8_t>(p.getUChar("prof", lcr::kProfileStandard));  // assente nelle versioni precedenti
  }
  p.end();
  return all;
}

bool NvsSettingsStorage::save(const lcr::Settings& s) {
  Preferences p;
  if (!p.begin(kNamespace, false)) return false;
  const bool ok = p.putString("call", s.callsign) > 0 && p.putUInt("freq", s.freqKhz) > 0 && p.putInt("pwr", s.powerDbm) > 0 &&
                    p.putInt("vol", s.volume) > 0 &&
                    p.putInt("floor", s.micFloorDb) > 0 && p.putUChar("prof", s.profile) > 0;
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
