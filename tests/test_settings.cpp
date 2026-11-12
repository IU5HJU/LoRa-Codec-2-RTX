#include <cstring>
#include <string>

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

static void testVolumeMigration() {
  CHECK(migrateOldVolume(0) == 0);
  CHECK(migrateOldVolume(10) == 5 && migrateOldVolume(9) == 4 && migrateOldVolume(8) == 3);  // stesso livello di prima
  CHECK(migrateOldVolume(7) == 2 && migrateOldVolume(6) == 1);
  CHECK(migrateOldVolume(3) == 1 && migrateOldVolume(1) == 1);  // piu' bassi di prima non esistono piu': il minimo
  CHECK(kDefaultVolume == 5);
}

Settings make(const char* cs, uint32_t khz, int dbm, int vol = kDefaultVolume, int floorDb = kDefaultMicFloorDb) {
  Settings s;
  memcpy(s.callsign, cs, strlen(cs) < kMaxCallsign ? strlen(cs) : kMaxCallsign);
  s.freqKhz = khz;
  s.freqMsgKhz = khz;  // di partenza tutte le frequenze dei profili sono uguali
  s.powerDbm = dbm;
  s.volume = vol;
  s.micFloorDb = floorDb;
  return s;
}

const Settings kDefaults = make("N0CALL", 433900, 10);

struct Fixture {
  FakeStorage st;
  Settings active = kDefaults;
  SettingsConsole con{active, kDefaults, st, "T3", "v0.1.0-3-gabc1234"};
  char reply[300];
  ConsoleResult run(const char* line) { return con.handleLine(line, reply, sizeof(reply)); }
};

static void testMeshcomSettings() {
  Fixture f;
  // valori di partenza: frequenza della rete, SSID 99, ascolto di 5 minuti, tre destinatari di esempio
  CHECK(f.run("CFG GET MC") == ConsoleResult::Ok && strcmp(f.reply, "CFG MC freqmc=433.175 ssid=99 window=300") == 0);
  CHECK(f.run("CFG GET MCDEST1") == ConsoleResult::Ok && strcmp(f.reply, "CFG MCDEST1=22251") == 0);
  CHECK(f.run("CFG GET MCDEST2") == ConsoleResult::Ok && strcmp(f.reply, "CFG MCDEST2=222") == 0);
  CHECK(f.run("CFG GET MCDEST3") == ConsoleResult::Ok && strcmp(f.reply, "CFG MCDEST3=*") == 0);
  CHECK(f.run("CFG GET MCDEST4") == ConsoleResult::Ok && strcmp(f.reply, "CFG MCDEST4=") == 0);
  // il profilo MeshCom fisso non c'e' piu': solo l'escursione
  CHECK(f.run("CFG SET PROFILE MC") == ConsoleResult::Error);
  // destinatari: gruppi, nominativi-SSID, tutti; maiuscole indifferenti; NONE svuota; valgono subito (nessun riavvio)
  CHECK(f.run("CFG SET MCDEST1 22253") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK mcdest1=22253") == 0 && strcmp(f.st.data.mcDests[0], "22253") == 0);
  CHECK(f.run("CFG SET MCDEST4 iu5abc-9") == ConsoleResult::Ok && strcmp(f.st.data.mcDests[3], "IU5ABC-9") == 0);
  CHECK(f.run("CFG SET mcdest5 IU5ABC/P") == ConsoleResult::Ok && strcmp(f.st.data.mcDests[4], "IU5ABC/P") == 0);
  CHECK(f.run("CFG SET MCDEST3 NONE") == ConsoleResult::Ok && f.st.data.mcDests[2][0] == 0 && strcmp(f.reply, "CFG OK mcdest3=") == 0);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=0"));
  CHECK(f.run("CFG SET MCDEST1 IU5ABC:9") == ConsoleResult::Error && strcmp(f.st.data.mcDests[0], "22253") == 0);
  CHECK(f.run("CFG SET MCDEST1 AAAAAAAAAAAAAAAAA") == ConsoleResult::Error);
  CHECK(f.run("CFG SET MCDEST1 IU5 ABC") == ConsoleResult::Error);
  CHECK(f.run("CFG SET MCDEST6 222") == ConsoleResult::Error && f.run("CFG SET MCDEST0 222") == ConsoleResult::Error && f.run("CFG SET MCDEST 222") == ConsoleResult::Error);
  CHECK(f.run("CFG GET MCDEST6") == ConsoleResult::Error);
  // SSID, frequenza, durata dell'ascolto
  CHECK(f.run("CFG SET MCSSID 12") == ConsoleResult::Ok && f.st.data.mcSsid == 12);
  CHECK(f.run("CFG SET MCSSID 100") == ConsoleResult::Error && f.st.data.mcSsid == 12);
  CHECK(f.run("CFG SET MCSSID 0") == ConsoleResult::Ok && f.st.data.mcSsid == 0);
  CHECK(f.run("CFG SET FREQMC 433.175") == ConsoleResult::Ok && f.st.data.freqMcKhz == 433175);
  CHECK(f.run("CFG SET FREQMC 500.000") == ConsoleResult::Error);
  CHECK(f.run("CFG SET MCWINDOW 600") == ConsoleResult::Ok && f.st.data.mcWindowS == 600 && strcmp(f.reply, "CFG OK mcwindow=600") == 0);
  CHECK(f.run("CFG SET MCWINDOW 29") == ConsoleResult::Error && f.run("CFG SET MCWINDOW 1801") == ConsoleResult::Error && f.st.data.mcWindowS == 600);
  CHECK(f.run("CFG GET MC") == ConsoleResult::Ok && strcmp(f.reply, "CFG MC freqmc=433.175 ssid=0 window=600") == 0);
  // reset: tornano i valori di partenza
  CHECK(f.run("CFG RESET") == ConsoleResult::Ok && f.run("CFG GET MCDEST1") == ConsoleResult::Ok && strcmp(f.reply, "CFG MCDEST1=22251") == 0);
  // nominativo con SSID
  Settings s;
  strcpy(s.callsign, "IU5HJU");
  char c[20];
  s.mcCallsign(c, sizeof(c));
  CHECK(strcmp(c, "IU5HJU-99") == 0);
  s.mcSsid = 0;
  s.mcCallsign(c, sizeof(c));
  CHECK(strcmp(c, "IU5HJU") == 0);
  // validazione
  CHECK(isValidMcDest("IU5ABC-9") && isValidMcDest("*") && isValidMcDest("22251") && isValidMcDest("IU5ABC/P") && !isValidMcDest("") &&
        !isValidMcDest("a b") && !isValidMcDest(nullptr));
  Settings bad = make("IU5HJU", 433900, 10);
  strcpy(bad.mcDests[2], "no no");
  CHECK(!isValidSettings(bad));
  bad = make("IU5HJU", 433900, 10);
  bad.mcSsid = 100;
  CHECK(!isValidSettings(bad));
  bad = make("IU5HJU", 433900, 10);
  bad.mcWindowS = 10;
  CHECK(!isValidSettings(bad));
  bad = make("IU5HJU", 433900, 10);
  bad.mcDests[1][0] = 0;  // un posto vuoto e' valido
  CHECK(isValidSettings(bad));
}

static void testMonitor() {
  Fixture f;
  CHECK(f.run("CFG GET MONITOR") == ConsoleResult::Ok && strcmp(f.reply, "CFG MONITOR=off") == 0);
  CHECK(f.run("CFG SET MONITOR ON") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK monitor=on") == 0 && f.st.data.rxMonitor);
  CHECK(f.run("CFG GET MONITOR") == ConsoleResult::Ok && strcmp(f.reply, "CFG MONITOR=on") == 0);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=1"));  // cambia i fili: vale dal riavvio
  CHECK(f.run("cfg set monitor off") == ConsoleResult::Ok && !f.st.data.rxMonitor);
  CHECK(f.run("CFG SET MONITOR MAYBE") == ConsoleResult::Error && !f.st.data.rxMonitor);
}

static void testRadioTestCommand() {
  Fixture f;
  CHECK(f.run("CFG RADIOTEST MORSE FM IR5XXX wpm=12") == ConsoleResult::RadioTest && strstr(f.reply, "CFG OK"));
  CHECK(f.con.radioTestRequest().kind == RadioTestKind::Morse && f.con.radioTestRequest().wpm == 12 && strcmp(f.con.radioTestRequest().text, "IR5XXX") == 0);
  CHECK(f.run("cfg radiotest tone 3 hz=900") == ConsoleResult::RadioTest && f.con.radioTestRequest().seconds == 3 && f.con.radioTestRequest().toneHz == 900);
  CHECK(f.run("CFG RADIOTEST MORSE SSB X") == ConsoleResult::Error && strstr(f.reply, "CFG ERR radiotest"));
  CHECK(f.run("CFG RADIOTEST") == ConsoleResult::Error);
  CHECK(f.run("CFG RADIOTESTX") == ConsoleResult::Error);  // comando sconosciuto, non una prova
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=0"));  // non cambia nessuna impostazione
}

static void testNodeSettings() {
  Fixture f;
  CHECK(f.run("CFG GET NODE") == ConsoleResult::Ok && strcmp(f.reply, "CFG NODE ssid=none pass=none ip=none port=5400 listen=5400 id=0 peerid=0 key=none") == 0);
  CHECK(f.run("CFG SET ROLE NODE") == ConsoleResult::Ok && f.st.data.role == kRoleNode && strcmp(f.reply, "CFG OK role=node") == 0);
  CHECK(f.run("CFG SET WIFISSID Casa Mia 2.4G") == ConsoleResult::Ok && strcmp(f.st.data.wifiSsid, "Casa Mia 2.4G") == 0 && strcmp(f.reply, "CFG OK wifissid=Casa Mia 2.4G") == 0);
  CHECK(f.run("CFG SET WIFIPASS la mia password") == ConsoleResult::Ok && strcmp(f.st.data.wifiPass, "la mia password") == 0 && strcmp(f.reply, "CFG OK wifipass=set") == 0);
  CHECK(f.run("CFG SET WIFIPASS corta") == ConsoleResult::Error && strcmp(f.st.data.wifiPass, "la mia password") == 0);  // meno di 8
  CHECK(f.run("CFG SET WIFISSID 12345678901234567890123456789012345") == ConsoleResult::Error);                      // oltre 32
  CHECK(f.run("CFG SET PEER 192.168.1.51:5401") == ConsoleResult::Ok && strcmp(f.st.data.peerIp, "192.168.1.51") == 0 && f.st.data.peerPort == 5401 && strcmp(f.reply, "CFG OK peer=192.168.1.51:5401") == 0);
  CHECK(f.run("CFG SET PEER 10.0.0.2") == ConsoleResult::Ok && f.st.data.peerPort == 5400);
  CHECK(f.run("CFG SET PEER 300.1.1.1") == ConsoleResult::Error && f.run("CFG SET PEER 1.2.3") == ConsoleResult::Error && f.run("CFG SET PEER 1.2.3.4:0") == ConsoleResult::Error);
  CHECK(f.run("CFG SET PEER 1.2.3.4:70000") == ConsoleResult::Error && f.run("CFG SET PEER 1.2.3.4:") == ConsoleResult::Error && f.run("CFG SET PEER ciao") == ConsoleResult::Error);
  CHECK(strcmp(f.st.data.peerIp, "10.0.0.2") == 0);
  CHECK(f.run("CFG SET NODEPORT 5555") == ConsoleResult::Ok && f.st.data.nodePort == 5555 && f.run("CFG SET NODEPORT 0") == ConsoleResult::Error);
  CHECK(f.run("CFG SET NODEID 1") == ConsoleResult::Ok && f.st.data.nodeId == 1 && f.run("CFG SET NODEID 0") == ConsoleResult::Error && f.run("CFG SET NODEID x") == ConsoleResult::Error);
  CHECK(f.run("CFG SET PEERID 2") == ConsoleResult::Ok && f.st.data.peerId == 2 && f.run("CFG SET PEERID 0") == ConsoleResult::Ok && f.st.data.peerId == 0);
  CHECK(f.run("CFG SET NODEID 4294967296") == ConsoleResult::Error);
  CHECK(f.run("CFG SET NODEKEY 000102030405060708090a0b0c0d0e0f") == ConsoleResult::Ok && f.st.data.nodeKeySet && f.st.data.nodeKey[15] == 15);
  CHECK(f.run("CFG SET NODEKEY 0001") == ConsoleResult::Error && f.st.data.nodeKeySet);
  CHECK(f.run("CFG GET NODE") == ConsoleResult::Ok && strcmp(f.reply, "CFG NODE ssid=Casa Mia 2.4G pass=set ip=10.0.0.2 port=5400 listen=5555 id=1 peerid=0 key=set") == 0);
  CHECK(!strstr(f.reply, "password") && !strstr(f.reply, "0102"));  // password e chiave non si mostrano
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "role=node") && strstr(f.reply, "reboot=1"));
  CHECK(f.run("CFG SET WIFIPASS NONE") == ConsoleResult::Ok && f.st.data.wifiPass[0] == 0 && f.run("CFG SET PEER NONE") == ConsoleResult::Ok && f.st.data.peerIp[0] == 0);
  CHECK(f.run("CFG SET NODEKEY NONE") == ConsoleResult::Ok && !f.st.data.nodeKeySet);
  CHECK(isValidIpv4("1.2.3.4") && isValidIpv4("255.255.255.255") && !isValidIpv4("1.2.3") && !isValidIpv4("1.2.3.4.5") && !isValidIpv4("a.b.c.d") && !isValidIpv4("") && !isValidIpv4("1..2.3"));
  // sulla T-Beam il nodo non c'e'
  FakeStorage st2;
  Settings active = kDefaults;
  SettingsConsole con{active, kDefaults, st2, "tbeam", "v0"};
  char reply[300];
  CHECK(con.handleLine("CFG SET ROLE NODE", reply, sizeof(reply)) == ConsoleResult::Error && st2.data.role == kRoleNormal);
}

static void testIdMorse() {
  Fixture f;
  CHECK(f.run("CFG GET IDMORSE") == ConsoleResult::Ok && strcmp(f.reply, "CFG IDMORSE=on") == 0);  // di base acceso
  CHECK(f.run("CFG SET IDMORSE OFF") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK idmorse=off") == 0 && !f.st.data.idMorse);
  CHECK(f.run("CFG GET IDMORSE") == ConsoleResult::Ok && strcmp(f.reply, "CFG IDMORSE=off") == 0);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=1"));
  CHECK(f.run("cfg set idmorse on") == ConsoleResult::Ok && f.st.data.idMorse);
  CHECK(f.run("CFG SET IDMORSE FORSE") == ConsoleResult::Error && f.st.data.idMorse);
}

static void testIdInterval() {
  Fixture f;
  CHECK(f.run("CFG GET IDINT") == ConsoleResult::Ok && strcmp(f.reply, "CFG IDINT=10") == 0);
  CHECK(f.run("CFG SET IDINT 3") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK idint=3") == 0 && f.st.data.idIntervalMin == 3);
  CHECK(f.run("CFG GET IDINT") == ConsoleResult::Ok && strcmp(f.reply, "CFG IDINT=3") == 0);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=1"));
  CHECK(f.run("CFG SET IDINT 1") == ConsoleResult::Ok && f.run("CFG SET IDINT 10") == ConsoleResult::Ok && f.st.data.idIntervalMin == 10);
  CHECK(f.run("CFG SET IDINT 0") == ConsoleResult::Error && f.run("CFG SET IDINT 11") == ConsoleResult::Error && f.run("CFG SET IDINT x") == ConsoleResult::Error);
  CHECK(f.st.data.idIntervalMin == 10);
  Settings bad = make("IU5HJU", 433900, 10);
  bad.idIntervalMin = 0;
  CHECK(!isValidSettings(bad));
  bad.idIntervalMin = 11;
  CHECK(!isValidSettings(bad));
}

static void testSysop() {
  Fixture f;
  CHECK(f.run("CFG GET SYSOP") == ConsoleResult::Ok && strcmp(f.reply, "CFG SYSOP call=none key=none mode=off") == 0);
  CHECK(f.run("CFG SET SYSOP IZ0SOP") == ConsoleResult::Ok);
  CHECK(strcmp(f.st.data.sysopCall, "IZ0SOP") == 0 && strcmp(f.reply, "CFG OK sysop=IZ0SOP") == 0);
  CHECK(f.run("CFG SET SYSOP TROPPO-LUNGO-X") == ConsoleResult::Error && strcmp(f.st.data.sysopCall, "IZ0SOP") == 0);
  CHECK(f.run("CFG SET SYSOPKEY 000102030405060708090a0b0c0d0e0f") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK sysopkey=set") == 0);
  CHECK(f.st.data.sysopKeySet && f.st.data.sysopKey[0] == 0 && f.st.data.sysopKey[1] == 1 && f.st.data.sysopKey[15] == 15);
  CHECK(f.run("CFG GET SYSOP") == ConsoleResult::Ok && strcmp(f.reply, "CFG SYSOP call=IZ0SOP key=set mode=off") == 0);  // la chiave non si mostra
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && !strstr(f.reply, "0102") && strstr(f.reply, "reboot=1"));
  CHECK(f.run("CFG SET SYSOPKEY 0001") == ConsoleResult::Error && f.st.data.sysopKeySet);
  CHECK(f.run("CFG SET SYSOPKEY zz0102030405060708090a0b0c0d0e0f") == ConsoleResult::Error && f.st.data.sysopKey[1] == 1);
  CHECK(f.run("CFG SET SYSOPKEY NONE") == ConsoleResult::Ok && !f.st.data.sysopKeySet && f.st.data.sysopKey[1] == 0);
  CHECK(f.run("CFG SET SYSOP NONE") == ConsoleResult::Ok && f.st.data.sysopCall[0] == 0);
  CHECK(f.run("CFG SET SYSOPMODE ON") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK sysopmode=on") == 0 && f.st.data.sysopMode);
  CHECK(f.run("CFG GET SYSOP") == ConsoleResult::Ok && strstr(f.reply, "mode=on") && f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=1"));
  CHECK(f.run("CFG SET SYSOPMODE MAYBE") == ConsoleResult::Error && f.st.data.sysopMode);
  CHECK(f.run("cfg set sysopmode off") == ConsoleResult::Ok && !f.st.data.sysopMode);
}

static void testTexts() {
  Fixture f;
  // di partenza: i cinque testi di esempio, tutti validi
  CHECK(strcmp(kDefaults.texts[0], "%C QRV su VoLoRa modo %M %F") == 0 && isValidSettings(make("IU5HJU", 433900, 10)));
  CHECK(f.run("CFG GET TEXT2") == ConsoleResult::Ok && strcmp(f.reply, "CFG TEXT2=Arrivo tra 10 minuti") == 0);
  // il testo e' il resto della riga, con spazi; vale subito (nessun riavvio)
  CHECK(f.run("CFG SET TEXT1 Sono QRV su 433.775, chiamami") == ConsoleResult::Ok &&
        strcmp(f.reply, "CFG OK text1=Sono QRV su 433.775, chiamami") == 0);
  CHECK(strcmp(f.st.data.texts[0], "Sono QRV su 433.775, chiamami") == 0);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=0"));
  CHECK(f.run("cfg set text5   ciao   a tutti  \r\n") == ConsoleResult::Ok && strcmp(f.st.data.texts[4], "ciao   a tutti") == 0);
  CHECK(f.run("CFG GET TEXT5") == ConsoleResult::Ok && strcmp(f.reply, "CFG TEXT5=ciao   a tutti") == 0);
  // una sola parola
  CHECK(f.run("CFG SET TEXT3 OK") == ConsoleResult::Ok && strcmp(f.st.data.texts[2], "OK") == 0);
  // 150 caratteri vanno, 151 no
  std::string big(150, 'x');
  CHECK(f.run(("CFG SET TEXT4 " + big).c_str()) == ConsoleResult::Ok && strlen(f.st.data.texts[3]) == 150);
  CHECK(f.run(("CFG SET TEXT4 " + big + "y").c_str()) == ConsoleResult::Error && strlen(f.st.data.texts[3]) == 150);
  // posto inesistente, vuoto, caratteri non ASCII
  CHECK(f.run("CFG SET TEXT1 ciao {IU5ABC} testo") == ConsoleResult::Error && strcmp(f.st.data.texts[0], "Sono QRV su 433.775, chiamami") == 0);
  CHECK(f.run("CFG SET TEXT6 ciao") == ConsoleResult::Error);
  CHECK(f.run("CFG SET TEXT0 ciao") == ConsoleResult::Error);
  CHECK(f.run("CFG SET TEXT1") == ConsoleResult::Error);
  CHECK(f.run("CFG SET TEXT1 caff\xc3\xa8") == ConsoleResult::Error && strcmp(f.st.data.texts[0], "Sono QRV su 433.775, chiamami") == 0);
  CHECK(f.run("CFG GET TEXT9") == ConsoleResult::Error);
  // NONE svuota il posto
  CHECK(f.run("CFG SET TEXT2 NONE") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK text2=") == 0 && f.st.data.texts[1][0] == 0);
  CHECK(f.run("CFG GET TEXT2") == ConsoleResult::Ok && strcmp(f.reply, "CFG TEXT2=") == 0);
  // salvataggio non riuscito: errore e nulla cambia
  f.st.failSave = true;
  CHECK(f.run("CFG SET TEXT3 altro") == ConsoleResult::Error);
  f.st.failSave = false;
  CHECK(f.run("CFG GET TEXT3") == ConsoleResult::Ok && strcmp(f.reply, "CFG TEXT3=OK") == 0);
  // reset: tornano i testi di esempio
  CHECK(f.run("CFG RESET") == ConsoleResult::Ok);
  CHECK(f.run("CFG GET TEXT1") == ConsoleResult::Ok && strcmp(f.reply, "CFG TEXT1=%C QRV su VoLoRa modo %M %F") == 0);
  // impostazioni con un testo non valido non sono valide
  Settings bad = make("IU5HJU", 433900, 10);
  bad.texts[0][0] = '\n';
  CHECK(!isValidSettings(bad));
  Settings empty = make("IU5HJU", 433900, 10);
  empty.texts[0][0] = 0;
  CHECK(isValidSettings(empty));
}

static void testRoleNotOnTbeam() {
  FakeStorage st;
  Settings active = kDefaults;
  SettingsConsole con{active, kDefaults, st, "tbeam", "v0"};
  char reply[300];
  CHECK(con.handleLine("CFG SET ROLE RPTTX", reply, sizeof(reply)) == ConsoleResult::Error && strstr(reply, "solo la T3") && st.data.role == kRoleNormal);
  CHECK(con.handleLine("CFG SET ROLE RPTRX", reply, sizeof(reply)) == ConsoleResult::Error);
  CHECK(con.handleLine("CFG SET ROLE OFF", reply, sizeof(reply)) == ConsoleResult::Ok);  // tornare stazione si puo' sempre
}

static void testRepeaterRole() {
  Fixture f;
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "role=off") && strstr(f.reply, "freqout=433.900"));
  CHECK(f.run("CFG SET ROLE RPTRX") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK role=rptrx") == 0);
  CHECK(f.st.data.role == kRoleRepeaterRx);
  CHECK(f.run("cfg set role rpttx") == ConsoleResult::Ok && f.st.data.role == kRoleRepeaterTx);
  CHECK(f.run("CFG SET FREQOUT 434.400") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK freqout=434.400") == 0);
  CHECK(f.st.data.outFreqKhz() == 434400 && f.st.data.freqKhz == 433900);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "role=rpttx") && strstr(f.reply, "freqout=434.400") && strstr(f.reply, "reboot=1"));
  CHECK(f.run("CFG SET ROLE XYZ") == ConsoleResult::Error && f.st.data.role == kRoleRepeaterTx);
  CHECK(f.run("CFG SET FREQOUT 500.000") == ConsoleResult::Error);
  // La scheda TX usa la frequenza di uscita al posto di quella del profilo.
  const Settings out = f.st.data.withProfileFreqKhz(f.st.data.outFreqKhz());
  CHECK(out.profileFreqKhz() == 434400 && f.st.data.profileFreqKhz() == 433900);
  Settings m = f.st.data;
  m.profile = kProfileMessage;
  m.freqMsgKhz = 433100;
  CHECK(m.withProfileFreqKhz(434400).freqMsgKhz == 434400 && m.withProfileFreqKhz(434400).freqKhz == 433900);
  Settings bad = f.st.data;
  bad.role = 9;
  CHECK(!isValidSettings(bad));
}


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
  CHECK(isValidSettings(make("N0CALL", 433900, 10, 7, -80)));
  CHECK(isValidSettings(make("N0CALL", 433900, 10, 7, -36)));
  CHECK(!isValidSettings(make("N0CALL", 433900, 10, 7, -81)));
  CHECK(!isValidSettings(make("N0CALL", 433900, 10, 7, -35)));
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

void testPositionSettings() {
  Fixture f;
  // Di default la posizione non si trasmette e il QTH non c'e'.
  CHECK(!f.st.data.posTx && !f.st.data.qthSet);
  CHECK(f.run("CFG SET POS ON") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK pos=on") == 0 && f.st.data.posTx);
  CHECK(f.run("CFG SET pos off") == ConsoleResult::Ok && !f.st.data.posTx);
  CHECK(f.run("CFG SET POS MAYBE") == ConsoleResult::Error);
  CHECK(f.run("CFG SET QTH 43.7696,11.2558") == ConsoleResult::Ok);
  CHECK(strcmp(f.reply, "CFG OK qth=43.76960,11.25580") == 0);
  CHECK(f.st.data.qthSet && f.st.data.qthLatE7 == 437696000 && f.st.data.qthLonE7 == 112558000);
  CHECK(f.st.data.qth().valid && !f.st.data.qth().fromGps);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "pos=off qth=43.76960,11.25580"));
  CHECK(f.run("CFG SET QTH -33.8688,151.2093") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK qth=-33.86880,151.20930") == 0);
  CHECK(f.run("CFG SET QTH 0,-0.5") == ConsoleResult::Ok && f.st.data.qthLatE7 == 0 && f.st.data.qthLonE7 == -5000000);
  // Valori non validi: nessun salvataggio.
  const int saves = f.st.saves;
  const char* bad[] = {"CFG SET QTH 91,0", "CFG SET QTH 0,181", "CFG SET QTH 43.7696", "CFG SET QTH 43.7696;11.2558", "CFG SET QTH a,b",
                       "CFG SET QTH 43.12345678,11", "CFG SET QTH ,", "CFG SET QTH 43.,11", "CFG SET QTH 1e1,2"};
  for (const char* b : bad) {
    CHECK(f.run(b) == ConsoleResult::Error);
    CHECK(strncmp(f.reply, "CFG ERR", 7) == 0);
  }
  CHECK(f.st.saves == saves);
  CHECK(f.run("CFG SET QTH NONE") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK qth=none") == 0 && !f.st.data.qthSet);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "qth=none"));
}

void testGpsSetting() {
  // Sulla T3 il GPS non c'e': errore, niente salvato.
  Fixture f;
  CHECK(!f.st.data.gpsOn);
  const int saves = f.st.saves;
  CHECK(f.run("CFG SET GPS ON") == ConsoleResult::Error);
  CHECK(strncmp(f.reply, "CFG ERR gps", 11) == 0 && f.st.saves == saves);
  // Sulla T-Beam: ON e OFF, salvato, vale dal riavvio.
  FakeStorage st;
  Settings active = kDefaults;
  SettingsConsole con(active, kDefaults, st, "tbeam", "dev");
  char reply[200];
  CHECK(con.handleLine("CFG GET", reply, sizeof(reply)) == ConsoleResult::Ok && strstr(reply, "gps=off"));
  CHECK(con.handleLine("CFG SET GPS ON", reply, sizeof(reply)) == ConsoleResult::Ok);
  CHECK(strcmp(reply, "CFG OK gps=on") == 0 && st.data.gpsOn);
  CHECK(con.handleLine("CFG GET", reply, sizeof(reply)) == ConsoleResult::Ok && strstr(reply, "gps=on") && strstr(reply, "reboot=1"));
  CHECK(active.gpsOn == false);  // quello in uso non cambia
  CHECK(con.handleLine("CFG SET gps maybe", reply, sizeof(reply)) == ConsoleResult::Error);
  CHECK(con.handleLine("CFG SET GPS OFF", reply, sizeof(reply)) == ConsoleResult::Ok && !st.data.gpsOn);
  // La riga di CFG GET con QTH lungo e GPS acceso sta nel buffer da 260 (main.cpp).
  CHECK(con.handleLine("CFG SET QTH -33.8688,-151.2093", reply, sizeof(reply)) == ConsoleResult::Ok);
  CHECK(con.handleLine("CFG SET POS ON", reply, sizeof(reply)) == ConsoleResult::Ok);
  CHECK(con.handleLine("CFG SET GPS ON", reply, sizeof(reply)) == ConsoleResult::Ok);
  CHECK(con.handleLine("CFG GET", reply, sizeof(reply)) == ConsoleResult::Ok && strlen(reply) < 259 && strstr(reply, "gps=on"));
}

void testGet() {
  Fixture f;
  CHECK(f.run("CFG GET") == ConsoleResult::Ok);
  CHECK(strcmp(f.reply, "CFG callsign=N0CALL freq=433.900 freqmsg=433.900 freqqual=433.900 power=10 volume=5 micfloor=-60 profile=std board=T3 reboot=0 test=0 pos=off qth=none gps=off role=off freqout=433.900") == 0);
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

void testTestMode() {
  Fixture f;
  CHECK(!f.con.testMode());
  CHECK(f.run("CFG TEST CODEC") == ConsoleResult::CodecTest);
  CHECK(f.run("CFG REC ON") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK rec=on") == 0 && f.con.recordMode());
  CHECK(f.run("CFG REC OFF") == ConsoleResult::Ok && !f.con.recordMode());
  CHECK(f.run("CFG REC MAYBE") == ConsoleResult::Error);
  CHECK(f.run("CFG TEST ON") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK test=on") == 0 && f.con.testMode());
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "test=1"));
  CHECK(f.st.saves == 0 && !f.st.has);  // non si salva: all'accensione e' sempre spenta
  CHECK(f.run("cfg test off") == ConsoleResult::Ok && !f.con.testMode() && strcmp(f.reply, "CFG OK test=off") == 0);
  CHECK(f.run("CFG TEST") == ConsoleResult::Error);
  CHECK(f.run("CFG TEST MAYBE") == ConsoleResult::Error && strncmp(f.reply, "CFG ERR test", 12) == 0);
  CHECK(f.run("CFG TEST AUDIO") == ConsoleResult::AudioTest && strncmp(f.reply, "CFG OK audio", 12) == 0);
  CHECK(f.run("cfg test audio") == ConsoleResult::AudioTest);
  CHECK(!f.con.testMode());  // la prova audio non cambia il modo prova di collegamento
  CHECK(f.run("CFG TEST ON") == ConsoleResult::Ok);
  CHECK(f.run("CFG RESET") == ConsoleResult::Ok && f.con.testMode());  // il reset non tocca la prova
  Fixture g;  // un nuovo avvio parte spento
  CHECK(!g.con.testMode());
}

void testFreqMsg() {
  Fixture f;
  // Di partenza uguale a quella dello Standard
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "freq=433.900 freqmsg=433.900"));
  CHECK(f.run("CFG SET FREQMSG 434.125") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK freqmsg=434.125") == 0);
  CHECK(f.st.data.freqMsgKhz == 434125 && f.st.data.freqKhz == 433900);  // lo Standard non cambia
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "freq=433.900 freqmsg=434.125"));
  // Mai bloccante: uguale a quella dello Standard, anche dopo averla cambiata
  CHECK(f.run("CFG SET FREQMSG 433.900") == ConsoleResult::Ok && f.st.data.freqMsgKhz == f.st.data.freqKhz);
  CHECK(f.run("CFG SET FREQ 434.500") == ConsoleResult::Ok && f.run("CFG SET FREQMSG 434.500") == ConsoleResult::Ok);
  CHECK(f.st.data.freqKhz == 434500 && f.st.data.freqMsgKhz == 434500);
  CHECK(f.run("cfg set freqmsg 433.9") == ConsoleResult::Ok && f.st.data.freqMsgKhz == 433900);
  // Resta solo il limite della banda
  const char* bad[] = {"CFG SET FREQMSG 429.999", "CFG SET FREQMSG 440.001", "CFG SET FREQMSG x", "CFG SET FREQMSG", "CFG SET FREQMSG 433.9999"};
  for (const char* b : bad) CHECK(f.run(b) == ConsoleResult::Error);
  CHECK(strncmp((f.run("CFG SET FREQMSG 500"), f.reply), "CFG ERR freqmsg", 15) == 0);
  CHECK(f.st.data.freqMsgKhz == 433900);
  // Un valore salvato fuori banda non e' valido
  Settings s = f.st.data;
  s.freqMsgKhz = 441000;
  CHECK(!isValidSettings(s));
  // La radio lavora con la frequenza del profilo in uso
  Settings m = make("IU5HJU", 433900, 10);
  m.freqMsgKhz = 434200;
  m.profile = kProfileMessage;
  CHECK(m.profileFreqKhz() == 434200 && m.freqMhz() > 434.19f && m.freqMhz() < 434.21f);
  m.profile = kProfileStandard;
  CHECK(m.profileFreqKhz() == 433900);
}

void testProfile() {
  Fixture f;
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "profile=std"));  // predefinito: Standard
  CHECK(f.run("CFG SET PROFILE MSG") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK profile=msg") == 0);
  CHECK(f.st.data.profile == kProfileMessage);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "profile=msg") && strstr(f.reply, "reboot=1"));  // vale dal riavvio
  CHECK(f.run("cfg set profile std") == ConsoleResult::Ok && f.st.data.profile == kProfileStandard);
  CHECK(f.run("CFG SET PROFILE QUA") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK profile=qua") == 0);
  CHECK(f.st.data.profile == kProfileQuality);
  CHECK(f.run("CFG SET FREQQUAL 434.250") == ConsoleResult::Ok && strcmp(f.reply, "CFG OK freqqual=434.250") == 0);
  CHECK(f.st.data.profileFreqKhz() == 434250 && f.st.data.freqKhz == 433900);
  CHECK(f.run("CFG SET FREQQUAL 500.000") == ConsoleResult::Error);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "freqqual=434.250"));
  f.run("CFG SET FREQQUAL 433.900");
  f.run("CFG SET PROFILE STD");
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=0"));
  const char* bad[] = {"CFG SET PROFILE", "CFG SET PROFILE XYZ", "CFG SET PROFILE MESSAGGIO", "CFG SET PROFILE 1"};
  for (const char* b : bad) CHECK(f.run(b) == ConsoleResult::Error);
  CHECK(strncmp((f.run("CFG SET PROFILE XYZ"), f.reply), "CFG ERR profile", 15) == 0);
  CHECK(f.st.data.profile == kProfileStandard);
  // Un valore salvato fuori dalla tabella non e' valido: si torna ai predefiniti
  Settings s = f.st.data;
  s.profile = 7;
  CHECK(!isValidSettings(s));
}

void testMicFloor() {
  Fixture f;
  CHECK(f.run("CFG SET MICFLOOR -58") == ConsoleResult::Ok && f.st.data.micFloorDb == -58);
  CHECK(strcmp(f.reply, "CFG OK micfloor=-58") == 0);
  CHECK(f.run("CFG SET MICFLOOR -80") == ConsoleResult::Ok && f.st.data.micFloorDb == -80);
  CHECK(f.run("CFG SET MICFLOOR -36") == ConsoleResult::Ok && f.st.data.micFloorDb == -36);
  CHECK(f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "micfloor=-36") && strstr(f.reply, "reboot=0"));  // vale subito
  CHECK(f.con.stored().micFloorDb == -36);
  CHECK(f.run("CFG SET POWER 12") == ConsoleResult::Ok && f.run("CFG GET") == ConsoleResult::Ok && strstr(f.reply, "reboot=1"));  // il resto dal riavvio
  const char* bad[] = {"CFG SET MICFLOOR -81", "CFG SET MICFLOOR -35", "CFG SET MICFLOOR 0", "CFG SET MICFLOOR x",
                       "CFG SET MICFLOOR -60.5", "CFG SET MICFLOOR"};
  for (const char* b : bad) CHECK(f.run(b) == ConsoleResult::Error);
  CHECK(strncmp((f.run("CFG SET MICFLOOR -99"), f.reply), "CFG ERR micfloor", 16) == 0);
  CHECK(f.st.data.micFloorDb == -36);
  // la soglia dell'AGC e' il fondo + 6 dB
  CHECK(micGateFromFloor(-60) == -54.0f && micGateFromFloor(-80) == -74.0f && micGateFromFloor(-36) == -30.0f);
  // misura del fondo: risponde subito e chiede al chiamante di misurare
  CHECK(f.run("CFG MIC MEASURE") == ConsoleResult::MicMeasure && strncmp(f.reply, "CFG OK mic", 10) == 0);
  CHECK(f.run("cfg mic measure") == ConsoleResult::MicMeasure);
  CHECK(f.run("CFG MIC") == ConsoleResult::Error);
  CHECK(f.run("CFG MIC FOO") == ConsoleResult::Error);
  CHECK(f.st.data.micFloorDb == -36);  // la misura non salva nulla da sola
}

void testVersion() {
  Fixture f;
  CHECK(f.run("CFG VERSION") == ConsoleResult::Ok && strcmp(f.reply, "CFG VERSION fw=v0.1.0-3-gabc1234 board=T3 proto=2") == 0);
  CHECK(f.run("cfg version") == ConsoleResult::Ok);
  CHECK(f.run("CFG VERSION x") == ConsoleResult::Error);
  FakeStorage st2;
  Settings a = kDefaults;
  SettingsConsole d(a, kDefaults, st2, "tbeam");  // senza versione: "dev"
  char r[80];
  CHECK(d.handleLine("CFG VERSION", r, sizeof(r)) == ConsoleResult::Ok && strcmp(r, "CFG VERSION fw=dev board=tbeam proto=2") == 0);
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
  testVolumeMigration();
  testRepeaterRole();
  testRoleNotOnTbeam();
  testTexts();
  testMonitor();
  testRadioTestCommand();
  testNodeSettings();
  testIdMorse();
  testIdInterval();
  testSysop();
  testMeshcomSettings();
  testValidate();
  testLoad();
  testNotCommand();
  testGet();
  testGpsSetting();
  testPositionSettings();
  testSetCallsign();
  testSetFreq();
  testSetPower();
  testSetVolume();
  testTestMode();
  testVersion();
  testFreqMsg();
  testProfile();
  testMicFloor();
  testOther();
  testSmallBuffer();
  TEST_MAIN_END();
}
