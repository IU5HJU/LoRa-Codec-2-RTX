#include <cstring>

#include "settings.h"
#include "testing.h"

using namespace lcr;

namespace {

struct FakeStorage : ISettingsStorage {
  bool has = false;
  bool failSave = false;
  Settings data;
  int saves = 0;
  bool load(Settings& out) override {
    if (!has) return false;
    out = data;
    return true;
  }
  bool save(const Settings& s) override {
    if (failSave) return false;
    data = s;
    has = true;
    saves++;
    return true;
  }
  bool erase() override {
    has = false;
    return true;
  }
};

Settings make(const char* cs, uint32_t khz, int dbm, int vol = kDefaultVolume) {
  Settings s;
  memcpy(s.callsign, cs, strlen(cs) < kMaxCallsign ? strlen(cs) : kMaxCallsign);
  s.freqKhz = khz;
  s.powerDbm = dbm;
  s.volume = vol;
  return s;
}

const Settings kDefaults = make("N0CALL", 433900, 10);

struct Fixture {
  FakeStorage st;
  Settings active = kDefaults;
  SettingsConsole con{active, kDefaults, st, "T3"};
  char reply[160];
  ConsoleResult run(const char* line) { return con.handleLine(line, reply, sizeof(reply)); }
};

void testValidate() {
  CHECK(isValidSettings(kDefaults));
  CHECK(isValidSettings(make("IU5HJU", 430000, 2)));
  CHECK(isValidSettings(make("IU5HJU/P", 440000, 17)));
  CHECK(!isValidSettings(make("iu5hju", 433900, 10)));
  CHECK(!isValidSettings(make("", 433900, 10)));
  CHECK(!isValidSettings(make("N0CALL", 429999, 10)));
  CHECK(!isValidSettings(make("N0CALL", 440001, 10)));
  CHECK(!isValidSettings(make("N0CALL", 433900, 1)));
  CHECK(!isValidSettings(make("N0CALL", 433900, 18)));
  CHECK(isValidSettings(make("N0CALL", 433900, 10, 0)));
  CHECK(isValidSettings(make("N0CALL", 433900, 10, 10)));
  CHECK(!isValidSettings(make("N0CALL", 433900, 10, -1)));
  CHECK(!isValidSettings(make("N0CALL", 433900, 10, 11)));
}

void testLoad() {
  FakeStorage st;
  CHECK(loadSettings(st, kDefaults) == kDefaults);  // niente salvato
  st.save(make("IU5HJU", 434000, 12));
  CHECK(loadSettings(st, kDefaults) == make("IU5HJU", 434000, 12));
  st.data = make("IU5HJU", 999000, 12);  // dati corrotti: si torna ai predefiniti
  CHECK(loadSettings(st, kDefaults) == kDefaults);
}

void testNotCommand() {
  Fixture f;
  CHECK(f.run("") == ConsoleResult::NotCommand);
  CHECK(f.run("   ") == ConsoleResult::NotCommand);
  CHECK(f.run("[mic] livello -40 dBFS") == ConsoleResult::NotCommand);
  CHECK(f.run("CFGX GET") == ConsoleResult::NotCommand);
  CHECK(f.run("ciao CFG GET") == ConsoleResult::NotCommand);
  CHECK(f.run(nullptr) == ConsoleResult::NotCommand);
  CHECK(f.reply[0] == 0);
  CHECK(f.st.saves == 0);
}

void testGet() {
  Fixture f;
  CHECK(f.run("CFG GET") == ConsoleResult::Ok);
  CHECK(strcmp(f.reply, "CFG callsign=N0CALL freq=433.900 power=10 volume=7 board=T3 reboot=0") == 0);
  CHECK(f.run("  cfg   get \r\n") == ConsoleResult::Ok);  // maiuscole, spazi e fine riga indifferenti
  CHECK(strncmp(f.reply, "CFG callsign=N0CALL", 19) == 0);
  CHECK(f.run("CFG GET extra") == ConsoleResult::Error);
}

void testSetCallsign() {
  Fixture f;
  CHECK(f.run("CFG SET CALLSIGN iu5hju") == ConsoleResult::Ok);  // portato in maiuscolo
  CHECK(strcmp(f.reply, "CFG OK callsign=IU5HJU") == 0);
  CHECK(strcmp(f.st.data.callsign, "IU5HJU") == 0 && f.st.saves == 1);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok);
  CHECK(strstr(f.reply, "callsign=IU5HJU") && strstr(f.reply, "reboot=1"));  // vale dal riavvio
  CHECK(strcmp(f.active.callsign, "N0CALL") == 0);  // quello in uso non cambia
  CHECK(f.run("CFG SET CALLSIGN IU5HJU/P") == ConsoleResult::Ok);
  CHECK(f.run("CFG SET CALLSIGN I") == ConsoleResult::Ok);
  CHECK(f.run("CFG SET CALLSIGN") == ConsoleResult::Error);
  const int saves = f.st.saves;
  const char* bad[] = {"CFG SET CALLSIGN IU5HJUXYZ", "CFG SET CALLSIGN IU 5", "CFG SET CALLSIGN IU5-HJU",
                       "CFG SET CALLSIGN IU5HJ\xc3\xa0", "CFG SET CALLSIGN 123456789"};
  for (const char* b : bad) {
    CHECK(f.run(b) == ConsoleResult::Error);
    CHECK(strncmp(f.reply, "CFG ERR", 7) == 0);
  }
  CHECK(f.st.saves == saves);  // un valore non valido non viene salvato
  CHECK(strcmp(f.con.stored().callsign, "I") == 0);
}

void testSetFreq() {
  Fixture f;
  CHECK(f.run("CFG SET FREQ 434.125") == ConsoleResult::Ok);
  CHECK(strcmp(f.reply, "CFG OK freq=434.125") == 0 && f.st.data.freqKhz == 434125);
  CHECK(f.run("CFG SET FREQ 433.9") == ConsoleResult::Ok && f.st.data.freqKhz == 433900);
  CHECK(f.run("CFG SET FREQ 433") == ConsoleResult::Ok && f.st.data.freqKhz == 433000);
  CHECK(f.run("CFG SET FREQ 430.000") == ConsoleResult::Ok);
  CHECK(f.run("CFG SET FREQ 440.000") == ConsoleResult::Ok && f.st.data.freqKhz == 440000);
  const int saves = f.st.saves;
  const char* bad[] = {"CFG SET FREQ 429.999", "CFG SET FREQ 440.001", "CFG SET FREQ 868.0", "CFG SET FREQ 433,9",
                       "CFG SET FREQ abc", "CFG SET FREQ 433.9001", "CFG SET FREQ 433.", "CFG SET FREQ .9",
                       "CFG SET FREQ -433.9", "CFG SET FREQ 99999999999"};
  for (const char* b : bad) {
    CHECK(f.run(b) == ConsoleResult::Error);
    CHECK(strncmp(f.reply, "CFG ERR freq", 12) == 0);
  }
  CHECK(f.st.saves == saves);
}

void testSetPower() {
  Fixture f;
  CHECK(f.run("CFG SET POWER 2") == ConsoleResult::Ok && f.st.data.powerDbm == 2);
  CHECK(f.run("CFG SET POWER 17") == ConsoleResult::Ok && f.st.data.powerDbm == 17);
  CHECK(strcmp(f.reply, "CFG OK power=17") == 0);
  const char* bad[] = {"CFG SET POWER 1", "CFG SET POWER 18", "CFG SET POWER 20", "CFG SET POWER -5",
                       "CFG SET POWER x", "CFG SET POWER 10.5", "CFG SET POWER 99999"};
  for (const char* b : bad) {
    CHECK(f.run(b) == ConsoleResult::Error);
    CHECK(strncmp(f.reply, "CFG ERR power", 13) == 0);
  }
  CHECK(f.st.data.powerDbm == 17);
}

void testSetVolume() {
  Fixture f;
  CHECK(f.run("CFG SET VOLUME 3") == ConsoleResult::Ok && f.st.data.volume == 3);
  CHECK(strcmp(f.reply, "CFG OK volume=3") == 0);
  CHECK(f.run("CFG SET VOLUME 0") == ConsoleResult::Ok && f.st.data.volume == 0);
  CHECK(f.run("CFG SET VOLUME 10") == ConsoleResult::Ok && f.st.data.volume == 10);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "volume=10") && strstr(f.reply, "reboot=1"));
  const char* bad[] = {"CFG SET VOLUME 11", "CFG SET VOLUME -1", "CFG SET VOLUME x", "CFG SET VOLUME 5.5"};
  for (const char* b : bad) {
    CHECK(f.run(b) == ConsoleResult::Error);
    CHECK(strncmp(f.reply, "CFG ERR volume", 14) == 0);
  }
  CHECK(f.st.data.volume == 10);
}

void testOther() {
  Fixture f;
  CHECK(f.run("CFG") == ConsoleResult::Error);
  CHECK(f.run("CFG FOO") == ConsoleResult::Error);
  CHECK(f.run("CFG SET BAUD 9600") == ConsoleResult::Error);
  CHECK(strstr(f.reply, "parametro sconosciuto") != nullptr);
  CHECK(f.run("CFG SET FREQ 434.0 extra") == ConsoleResult::Error);  // troppe parole
  CHECK(f.st.saves == 0);

  // Salvataggio fallito: errore e nessun cambiamento.
  f.st.failSave = true;
  CHECK(f.run("CFG SET POWER 12") == ConsoleResult::Error);
  CHECK(f.con.stored().powerDbm == 10);
  f.st.failSave = false;

  // Reset: si torna ai predefiniti e si cancella la memoria.
  CHECK(f.run("CFG SET CALLSIGN IU5HJU") == ConsoleResult::Ok);
  CHECK(f.run("CFG RESET") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK reset") == 0);
  CHECK(!f.st.has && f.con.stored() == kDefaults);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=0"));

  CHECK(f.run("CFG REBOOT") == ConsoleResult::Reboot);
  CHECK(strcmp(f.reply, "CFG OK reboot") == 0);
}

void testSmallBuffer() {
  Fixture f;
  char small[8];
  CHECK(f.con.handleLine("CFG GET", small, sizeof(small)) == ConsoleResult::Ok);
  CHECK(strlen(small) == 7);  // troncata ma terminata
  CHECK(f.con.handleLine("CFG SET POWER 12", nullptr, 0) == ConsoleResult::Ok);
  // Riga lunghissima: nessuna scrittura fuori dai limiti.
  char longLine[400];
  memset(longLine, 'A', sizeof(longLine));
  memcpy(longLine, "CFG SET CALLSIGN ", 17);
  longLine[399] = 0;
  CHECK(f.con.handleLine(longLine, f.reply, sizeof(f.reply)) == ConsoleResult::Error);
}

}  // namespace

int main() {
  testValidate();
  testLoad();
  testNotCommand();
  testGet();
  testSetCallsign();
  testSetFreq();
  testSetPower();
  testSetVolume();
  testOther();
  testSmallBuffer();
  TEST_MAIN_END();
}
