// Un HTTP minimale per la pagina di amministrazione del reflector (tools/vlreflector.cpp): leggere una richiesta, comporre una risposta, e qualche
// funzione per i parametri e per il JSON. Senza socket (tests/test_http_mini.cpp). Non e' un server generico: richieste piccole, una per
// connessione ("Connection: close").
#pragma once

#include <cstddef>
#include <map>
#include <string>

namespace lcr {

struct HttpRequest {
  std::string method;                          // "GET", "POST"
  std::string path;                            // senza i parametri
  std::string query;                           // dopo il '?'
  std::map<std::string, std::string> headers;  // nomi in minuscolo
  std::string body;
};

enum class HttpParse { Incomplete, Ok, Bad, TooBig };

constexpr size_t kHttpMaxHeaders = 8192;
constexpr size_t kHttpMaxBody = 4096;

// Prova a leggere una richiesta da quello che e' arrivato finora. Incomplete: ne serve ancora; TooBig: intestazioni o corpo oltre i limiti.
HttpParse parseHttpRequest(const std::string& data, HttpRequest& req);

// Una risposta completa (stato, tipo, corpo; sempre "Cache-Control: no-store" e "Connection: close"). extraHeaders: righe gia' composte con "\r\n".
std::string httpResponse(int status, const std::string& contentType, const std::string& body, const std::string& extraHeaders = "");

std::string urlDecode(const std::string& s);
// Il valore di un parametro "a=1&b=2" (anche di un corpo form), decodificato; "" se manca.
std::string formParam(const std::string& form, const std::string& key);
// Per mettere un testo fra virgolette in un JSON.
std::string jsonEscape(const std::string& s);
// Confronto a tempo costante (per il token).
bool constantTimeEquals(const std::string& a, const std::string& b);

}  // namespace lcr
