// Frame di testo compatibili con le reti a pacchetto MeshCom: layout, FCS, ricodifica identica, campi non ammessi.
// I frame di prova sono costruiti con una implementazione indipendente (Python, stesso layout verificato su frame veri) e
// nominativi inventati.
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "meshcom_frame.h"
#include "testing.h"

using namespace lcr;

static std::vector<uint8_t> hex(const char* h) {
  std::vector<uint8_t> v;
  for (size_t i = 0; h[i] && h[i + 1]; i += 2) v.push_back(static_cast<uint8_t>(strtol(std::string(h + i, 2).c_str(), nullptr, 16)));
  return v;
}

static const char* kSend = "3A44332211144955354141412D39393E4955354242422D393A6369616F7B313233000C8808E5";
static const char* kBcast = "3AD4C3B2A1144955354141412D39393E2A3A5465737420415343494900038809C3";
static const char* kAck = "3A01000000144955354242422D393E4955354141412D39393A4955354141412D39393A61636B313233000C8809C3";
static const char* kRelay = "3A44332211924955354141412D39392C4955354343432D31323E4955354242422D393A6369616F7B313233000C880BBB2883617E";

static void testDecodeEncode() {
  mc::Frame f;
  auto b = hex(kSend);
  CHECK(b.size() == 38 && mc::decode(b.data(), b.size(), f) && f.fcsOk);
  CHECK(f.type == mc::kTypeText && f.msgId == 0x11223344 && f.maxHop == 4 && f.flags == mc::kFlagMesh);
  CHECK(strcmp(f.path, "IU5AAA-99") == 0 && strcmp(f.dest, "IU5BBB-9") == 0 && strcmp(f.payload, "ciao{123") == 0);
  CHECK(f.hw == 12 && f.mod == 0x88 && f.fcs == 0x08E5 && !f.hasFwTrailer);
  uint8_t out[256];
  size_t n = mc::encode(f, out, sizeof(out));
  CHECK(n == b.size() && memcmp(out, b.data(), n) == 0);

  b = hex(kBcast);
  CHECK(mc::decode(b.data(), b.size(), f) && f.fcsOk && strcmp(f.dest, "*") == 0 && f.msgId == 0xA1B2C3D4);
  n = mc::encode(f, out, sizeof(out));
  CHECK(n == b.size() && memcmp(out, b.data(), n) == 0);

  b = hex(kAck);
  CHECK(mc::decode(b.data(), b.size(), f) && f.fcsOk && strcmp(f.payload, "IU5AAA-99:ack123") == 0 && strcmp(f.dest, "IU5AAA-99") == 0);
  n = mc::encode(f, out, sizeof(out));
  CHECK(n == b.size() && memcmp(out, b.data(), n) == 0);

  // copia ripetuta da un nodo: percorso piu' lungo, salti e flag diversi, trailer del firmware
  b = hex(kRelay);
  CHECK(mc::decode(b.data(), b.size(), f) && f.fcsOk && f.maxHop == 2 && (f.flags & mc::kFlagServer) && (f.flags & mc::kFlagMesh));
  CHECK(strcmp(f.path, "IU5AAA-99,IU5CCC-12") == 0 && f.hasFwTrailer && f.fwVersion == 40 && f.lastHw == 0x83 && f.fwSub == 'a');
  n = mc::encode(f, out, sizeof(out));
  CHECK(n == b.size() && memcmp(out, b.data(), n) == 0);
}

static void testRejects() {
  mc::Frame f;
  auto b = hex(kSend);
  // un byte cambiato: la FCS non torna (il frame si legge ma e' da scartare)
  auto bad = b;
  bad[12] ^= 0x01;
  CHECK(mc::decode(bad.data(), bad.size(), f) && !f.fcsOk);
  CHECK(!mc::decode(b.data(), 10, f));                 // troncato
  CHECK(!mc::decode(b.data(), b.size() - 3, f));       // manca la FCS
  auto type = b;
  type[0] = 0x55;                                      // tipo sconosciuto
  CHECK(!mc::decode(type.data(), type.size(), f));
  auto old = hex(kRelay);
  old[old.size() - 4] = 30;                            // versione firmware 30 (< 35): scartato come fanno i nodi
  CHECK(!mc::decode(old.data(), old.size(), f));
  CHECK(!mc::decode(b.data(), 300, f));                // piu' del frame massimo
}

static void testEncodeLimits() {
  mc::Frame t;
  t.msgId = 7;
  strcpy(t.path, "IU5AAA-99");
  strcpy(t.dest, "IU5BBB-9");
  strcpy(t.payload, "ciao");
  t.hw = 12;
  uint8_t out[256];
  const size_t n = mc::encode(t, out, sizeof(out));
  CHECK(n > 0 && out[0] == ':' && out[5] == 0x15);  // 0x10 mesh + 5 salti predefiniti
  mc::Frame back;
  CHECK(mc::decode(out, n, back) && back.fcsOk && strcmp(back.payload, "ciao") == 0);
  // campi non ammessi
  mc::Frame x = t;
  strcpy(x.payload, "a\nb");
  CHECK(mc::encode(x, out, sizeof(out)) == 0);
  x = t;
  strcpy(x.dest, "a:b");
  CHECK(mc::encode(x, out, sizeof(out)) == 0);
  x = t;
  strcpy(x.path, "IU5>AAA");
  CHECK(mc::encode(x, out, sizeof(out)) == 0);
  x = t;
  x.path[0] = 0;
  CHECK(mc::encode(x, out, sizeof(out)) == 0);
  CHECK(mc::encode(t, out, 20) == 0);  // il buffer non basta
  // 150 caratteri di testo ci stanno con percorso e destinatario lunghi
  x = t;
  memset(x.payload, 'x', 150);
  x.payload[150] = 0;
  strcpy(x.path, "IU5AAA-99");
  strcpy(x.dest, "IU5BBB-99");
  const size_t big = mc::encode(x, out, sizeof(out));
  CHECK(big > 0 && big < 200 && mc::decode(out, big, back) && back.fcsOk && strlen(back.payload) == 150);
}

int main() {
  testDecodeEncode();
  testRejects();
  testEncodeLimits();
  TEST_MAIN_END();
}
