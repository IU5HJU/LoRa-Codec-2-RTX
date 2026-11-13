// Macchina a stati del dispositivo.
//
//   Idle  --PTT (canale libero)--> Transmitting --(pacchetti inviati)--> Idle
//   Idle  --pacchetto valido-----> Receiving    --(fine/timeout)------> Idle
//
// Il PTT e' rifiutato se si sta ricevendo, se il canale e' occupato o se la
// trasmissione precedente non e' ancora stata inviata del tutto.
// Non esiste una fase di "bianco": i 5-10 s tra i turni sono solo l'attesa
// dell'operatore.
#pragma once

#include <cstdint>

#include "inbox.h"
#include "interfaces.h"
#include "link_test.h"
#include "meshcom_frame.h"
#include "profile.h"
#include "protocol.h"
#include "rx_stream.h"
#include "tx_session.h"

namespace lcr {

struct NodeConfig {
  char callsign[kMaxCallsign + 1] = {0};
  bool fec = true;  // parita' XOR 1 su 4
  // Generatore di ID di sessione (uno per PTT). Se nullptr usa un contatore.
  uint16_t (*newSessionId)() = nullptr;
  // Profilo di trasmissione (profile.h): Standard, Range o Messaggio.
  uint8_t profile = kProfileStandard;
  // Posizione di questa stazione e se trasmetterla nel pacchetto START (disattivato di default: la posizione e' un dato
  // personale, si trasmette solo se l'operatore lo sceglie). Vedi position.h.
  bool sendPosition = false;
  Position position;
  // Escursione sulla rete di testo (MeshCom): nominativo-SSID, identificatore hardware del frame e generatore di ID a 32 bit
  // (uno per messaggio).
  char mcCall[16] = {0};
  uint8_t mcHw = 0;
  uint32_t (*newMsgId)() = nullptr;
  // Diagnostica dell'escursione: una riga per ogni frame ricevuto, con il motivo se non e' mostrato (di solito la seriale)
  void (*meshLog)(const char* msg) = nullptr;
  // Solo per i test: tempo aggiunto alla pausa tra i pacchetti per simulare il tempo in aria (la radio finta e' istantanea).
  uint32_t extraGapMs = 0;
  // Chiave del sysop (16 byte) se questa scheda e' la sua stazione: serve a verificare le risposte dei ripetitori. nullptr = non attiva.
  const uint8_t* sysopKey = nullptr;
  // Prova di potenza (link_test.h): con la prova accesa il PTT lancia la prova dalla potenza massima alla minima.
  // sweepRestoreDbm: potenza da rimettere alla fine.
  int sweepMaxDbm = 17, sweepMinDbm = 2, sweepRestoreDbm = 10;
};

class VoiceNode {
 public:
  enum class State { Idle, Receiving, Transmitting };

  VoiceNode(const NodeConfig& cfg, IRadio& radio, ICodec& codec, IAudioIn& in, IAudioOut& out, INotifier& notify);

  // PTT: una pressione breve (meno di kPttClickMaxMs) e' un clic; tre clic veloci attivano o disattivano la
  // prova di collegamento. La trasmissione parte se il PTT resta premuto, con il microfono gia' bufferizzato.
  void pttPressed(uint32_t nowMs);
  void pttReleased(uint32_t nowMs);
  // Da chiamare spesso (almeno ogni ~10 ms).
  void tick(uint32_t nowMs);

  // Aggiorna la posizione di questa stazione (per esempio dal GPS) e se trasmetterla. Vale dal turno successivo.
  void setOwnPosition(const Position& p, bool send) {
    cfg_.position = p;
    cfg_.sendPosition = send;
  }

  // Prova di collegamento (non persistente): con la prova attiva il PTT trasmette frame di prova
  // invece della voce, e dopo aver ricevuto una prova la scheda risponde con il risultato.
  // Nel profilo Messaggio la prova di collegamento non c'e' (12 s di frame a SF12 sarebbero circa 17 minuti in aria).
  void setTestMode(bool on) { testMode_ = on && !prof_.message && !guest_ && !isPlaceholderCallsign(cfg_.callsign); }  // senza nominativo la prova non si accende: non si trasmette
  bool testMode() const { return testMode_; }

  // Modo "registra e poi trasmetti" (prova, non persistente): il PTT registra il messaggio senza trasmettere; dopo il
  // rilascio e un secondo di attesa la scheda lo trasmette. Serve a separare, nelle prove, la registrazione dalla
  // radio (nessuna elaborazione audio mentre la radio trasmette). Ignorato con la prova di collegamento attiva.
  enum class RecPhase { None, Recording, Waiting, Sending };
  // Nel profilo Messaggio il modo e' sempre acceso (non si puo' spegnere).
  void setRecordMode(bool on) { recMode_ = (on && !guest_) || prof_.message; }
  bool recordMode() const { return recMode_; }
  RecPhase recPhase() const { return recPhase_; }
  uint32_t recFrames() const { return recCount_; }

  // Messaggi (casella con un solo messaggio): un clic breve sul PTT apre il messaggio in attesa, un clic mentre e' aperto lo chiude
  // (e lo cancella). La voce non si riproduce da sola nel profilo Messaggio.
  const Inbox& inbox() const { return inbox_; }
  // Secondi che restano all'annuncio di "ripetitore occupato" (0 = nessuno). Non blocca il PTT.
  int repeaterBusyRemainingSec(uint32_t nowMs) const {
    if (!busyActive_ || static_cast<int32_t>(busyUntilMs_ - nowMs) <= 0) return 0;
    return static_cast<int>((busyUntilMs_ - nowMs + 999) / 1000);
  }
  bool messageOpen() const { return playing_ || textOpen_; }
  // Trasmette un breve testo (ASCII stampabile senza graffe, 1-150 caratteri), in due copie. false (con pttDenied) se non si puo'.
  bool sendText(const char* text, uint32_t nowMs);
  // Trasmette un pacchetto singolo START|END gia' pronto, due volte come i testi (comandi del sysop, sysop.h). Stesse condizioni di
  // sendText: nominativo valido, canale libero, non in trasmissione o ricezione.
  bool sendSingle(const Packet& p, uint32_t nowMs);

  // Escursione sulla rete di testo (docs/meshcom-testo.md): porta la radio ai parametri MeshCom, invia 'text' a 'dest' (un gruppo, un
  // nominativo-SSID o "*") e ascolta per windowMs; poi la radio torna da sola al profilo in uso. text = nullptr: solo ascolto.
  // Con l'escursione gia' in corso invia un altro testo e allunga l'ascolto. Si parte solo a riposo (ne' ricezione, ne'
  // trasmissione, ne' registrazione). false (con pttDenied) se non si puo'; se l'invio e' rifiutato la radio torna subito com'era.
  bool startMeshcom(const char* text, const char* dest, uint32_t nowMs, uint32_t windowMs);
  // Gruppi (numeri) di cui mostrare i messaggi durante l'escursione, oltre a quelli diretti al nostro nominativo-SSID: di solito i
  // destinatari impostati, per vedere le risposte ai propri QRV. Si chiama prima di startMeshcom.
  void setMeshcomGroups(const char* const* groups, size_t n);
  // Fine anticipata (altrimenti alla scadenza della finestra), per esempio con il volume lungo: la radio torna subito al profilo
  // in uso. Se in quel momento sta trasmettendo, torna appena l'invio e' finito.
  void endMeshcom();
  bool meshcomActive() const { return guest_; }
  uint32_t meshcomRemainingMs(uint32_t nowMs) const;

  // Prova di potenza (link_test.h, PowerSweep): con la prova di collegamento attiva trasmette prove brevi a potenza decrescente da maxDbm a
  // minDbm, aspettando ogni volta la risposta dell'altra scheda; alla fine (o con un PTT) la potenza torna a restoreDbm. false se non si puo'
  // (prova non attiva, nominativo N0CALL, profilo Messaggio, scheda non a riposo).
  bool startPowerSweep(uint32_t nowMs, int maxDbm, int minDbm, int restoreDbm);
  void stopPowerSweep() { if (sweepPhase_ != SweepPhase::None) sweepFinish(true); }
  bool powerSweepActive() const { return sweepPhase_ != SweepPhase::None; }
  const PowerSweep& powerSweep() const { return sweep_; }

  State state() const { return state_; }
  const RxStream& rx() const { return rx_; }

 private:
  void startTx(uint32_t nowMs);
  void beginPtt(uint32_t nowMs);
  void stopMic();
  void finishCapture(bool limit);
  void txTick(uint32_t nowMs);
  void recTick(uint32_t nowMs);
  void finishRecording(uint32_t nowMs, bool limit);
  void setRecPhase(RecPhase p);
  void reportRxProgress();
  void rxTick(uint32_t nowMs);
  void handleMeshFrame(const uint8_t* b, size_t n, uint32_t nowMs);
  bool sendMeshText(const char* text, const char* dest, uint32_t nowMs);
  bool meshActive() const { return guest_; }
  void meshAckTick(uint32_t nowMs);
  uint32_t makeMsgId();
  // ID gia' visti (anello di 70 come i nodi della rete): true se e' una copia; l'indice nell'anello in *slot
  bool meshSeen(uint32_t id, size_t* slot);
  void meshRemember(uint32_t id, uint32_t nowMs);
  void drainToInbox(uint32_t nowMs);
  void handleText(const Packet& p);
  void handleSysopReply(const Packet& p);
  void handleBusy(const Packet& p, uint32_t nowMs);
  void handleBeacon(const Packet& p);
  void clearBusy();
  void openMessage(uint32_t nowMs);
  void closeMessage();
  void playTick(uint32_t nowMs);
  void finishTestRx(uint32_t nowMs);
  void sendTestReply(uint32_t nowMs);
  void sweepTick(uint32_t nowMs);
  void sweepRecord(uint32_t nowMs, bool answered, const TestReport& r);
  void sweepFinish(bool aborted);
  void sendSweepSummary(uint32_t nowMs);
  uint16_t makeSessionId();

  NodeConfig cfg_;
  const Profile& prof_;
  uint32_t maxTurnFrames_;
  IRadio& radio_;
  ICodec& codec_;
  IAudioIn& in_;
  IAudioOut& out_;
  INotifier& notify_;

  State state_ = State::Idle;
  TxSession tx_;
  RxStream rx_;

  bool releasing_ = false;
  bool limitHit_ = false;
  uint32_t framesCaptured_ = 0;
  uint16_t fallbackSession_ = 1;

  uint32_t nextPlayMs_ = 0;
  uint32_t nextSendMs_ = 0;  // non trasmettere prima di questo istante (pausa tra pacchetti)
  bool justSent_ = false;
  bool callsignNotified_ = false;
  bool positionNotified_ = false;
  size_t txTotal_ = 0;   // pacchetti dichiarati nel messaggio in invio (0 = nessuno)
  size_t txSent_ = 0;    // pacchetti gia' inviati
  int rxPercent_ = 0;    // ultima percentuale di ricezione notificata

  // PTT: clic e attesa prima di trasmettere
  bool pendingStart_ = false;  // PTT premuto, in attesa di capire se e' un clic o l'inizio di una trasmissione
  uint32_t pressMs_ = 0;
  uint8_t clickCount_ = 0;
  uint32_t lastClickMs_ = 0;
  bool micStarted_ = false;

  // Registra e poi trasmetti
  bool recMode_ = false;
  RecPhase recPhase_ = RecPhase::None;
  uint8_t recBuf_[kMaxTurnFrames * kMaxFrameBytes];
  uint32_t recCount_ = 0;
  uint32_t recSent_ = 0;
  uint32_t recReleaseMs_ = 0;

  // Prova di collegamento
  bool testMode_ = false;
  bool txTest_ = false;             // la trasmissione in corso e' una prova
  uint32_t testFramesSent_ = 0;
  uint32_t nextTestFrameMs_ = 0;
  uint16_t lastTestSession_ = 0;    // ultima prova trasmessa da qui (per riconoscere la risposta)
  bool lastTestValid_ = false;
  bool rxIsTest_ = false;           // la ricezione in corso e' una prova
  LinkTestRx linkRx_;
  bool replyPending_ = false;
  uint32_t replyAtMs_ = 0;
  uint8_t replyTries_ = 0;
  uint16_t replySession_ = 0;
  TestReport replyReport_;
  uint16_t notifiedSession_ = 0;
  // Prova di potenza
  enum class SweepPhase { None, Gap, Tx, Wait };
  SweepPhase sweepPhase_ = SweepPhase::None;
  PowerSweep sweep_;
  uint32_t sweepAtMs_ = 0, sweepGapStartMs_ = 0;
  uint8_t sweepBusy_ = 0;
  uint8_t summaryBusy_ = 0;
  uint32_t lastSweepNowMs_ = 0;  // ultimo istante visto da sweepTick/sweepRecord (per programmare il riepilogo)
  bool summaryPending_ = false;
  uint8_t summaryCopies_ = 0;
  uint32_t summaryAtMs_ = 0;
  uint16_t summarySession_ = 0;       // sessione dell'ultima prova trasmessa (per la copia sul riepilogo)
  uint16_t lastSummaryRx_ = 0;
  bool haveSummaryRx_ = false;
  uint8_t summaryBuf_[kSweepSummaryHeader + PowerSweep::kMaxSteps + kFrameBytes] = {0};
  size_t summaryLen_ = 0;

  // Escursione sulla rete di testo in corso
  bool guest_ = false;
  uint32_t guestUntilMs_ = 0;
  bool endRequested_ = false;  // fine anticipata chiesta durante un invio
  static constexpr size_t kMcGroupSlots = 5;
  char mcGroups_[kMcGroupSlots][8] = {{0}};
  size_t mcGroupN_ = 0;
  // Frame ricevuti nell'escursione in corso: tutti, di testo, di altro tipo, con FCS sbagliata, non validi (riepilogo in meshLog)
  uint32_t mcRxAll_ = 0, mcRxText_ = 0, mcRxOther_ = 0, mcRxBadFcs_ = 0, mcRxInvalid_ = 0;

  // Casella dei messaggi
  Inbox inbox_;
  bool storing_ = false;     // frame di un messaggio vocale in corso di salvataggio
  bool playing_ = false;     // riproduzione del messaggio vocale aperto
  bool textOpen_ = false;    // testo aperto e in mostra
  size_t playIdx_ = 0;
  bool busyActive_ = false;  // annuncio di ripetitore occupato in corso (solo informativo)
  uint32_t busyUntilMs_ = 0;
  bool lastBusyValid_ = false;
  uint16_t lastBusySession_ = 0;
  char lastBusyCall_[kMaxCallsign + 1] = {0};
  bool relayedNotified_ = false;  // rxViaRepeater gia' notificato per la ricezione in corso
  bool lastBeaconValid_ = false;
  uint16_t lastBeaconSession_ = 0;
  char lastBeaconCall_[kMaxCallsign + 1] = {0};
  bool lastTextValid_ = false;  // ultimo testo ricevuto (per scartare la seconda copia)
  uint16_t lastTextSession_ = 0;
  char lastTextCall_[kMaxCallsign + 1] = {0};
  bool textTx_ = false;      // trasmissione di un testo in corso
  uint8_t textWire_[kMaxRxBytes];
  size_t textWireLen_ = 0;
  uint8_t textCopies_ = 0;
  // Escursione MeshCom: ID visti (con l'ultima conferma mandata) e conferma di ricezione da trasmettere
  struct MeshSeen {
    uint32_t id = 0;
    uint32_t ackedAtMs = 0;
    bool used = false;
    bool acked = false;
  };
  static constexpr size_t kMeshSeenSlots = 70;
  MeshSeen meshSeen_[kMeshSeenSlots];
  size_t meshNext_ = 0;
  uint32_t meshIdCounter_ = 0x1000;
  bool ackPending_ = false;
  uint32_t ackAtMs_ = 0;
  uint8_t ackTries_ = 0;
  char ackDest_[16] = {0};
  char ackNum_[8] = {0};
  bool playbackNotified_ = false;  // rxPlaybackStarted() gia' segnalato in questa ricezione
};

}  // namespace lcr
