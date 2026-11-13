#include "http_mini.h"
#include "testing.h"

using namespace lcr;

int main() {
  HttpRequest r;
  // una GET con parametri e intestazioni
  CHECK(parseHttpRequest("GET /api/lookup?call=IZ5IOF&x=1 HTTP/1.1\r\nHost: a\r\nX-Token:  segreto \r\n\r\n", r) == HttpParse::Ok);
  CHECK(r.method == "GET" && r.path == "/api/lookup" && r.query == "call=IZ5IOF&x=1" && r.headers["x-token"] == "segreto" && r.headers["host"] == "a" && r.body.empty());
  CHECK(formParam(r.query, "call") == "IZ5IOF" && formParam(r.query, "x") == "1" && formParam(r.query, "y") == "");
  // una POST con corpo, in due pezzi
  const std::string post = "POST /api/register HTTP/1.1\r\nContent-Length: 18\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\ncall=A%2FB&key=abc";
  CHECK(parseHttpRequest(post.substr(0, 30), r) == HttpParse::Incomplete);
  CHECK(parseHttpRequest(post.substr(0, post.size() - 3), r) == HttpParse::Incomplete);  // manca la fine del corpo
  CHECK(parseHttpRequest(post, r) == HttpParse::Ok && r.method == "POST" && r.body == "call=A%2FB&key=abc" + std::string() && formParam(r.body, "call") == "A/B");
  // errori
  CHECK(parseHttpRequest("GET\r\n\r\n", r) == HttpParse::Bad);
  CHECK(parseHttpRequest("get / HTTP/1.1\r\n\r\n", r) == HttpParse::Bad);
  CHECK(parseHttpRequest("GET x HTTP/1.1\r\n\r\n", r) == HttpParse::Bad);
  CHECK(parseHttpRequest("GET / FTP/1.1\r\n\r\n", r) == HttpParse::Bad);
  CHECK(parseHttpRequest("GET / HTTP/1.1\r\nsenza due punti\r\n\r\n", r) == HttpParse::Bad);
  CHECK(parseHttpRequest("POST / HTTP/1.1\r\nContent-Length: abc\r\n\r\n", r) == HttpParse::Bad);
  CHECK(parseHttpRequest("POST / HTTP/1.1\r\nContent-Length: 99999\r\n\r\n", r) == HttpParse::TooBig);
  CHECK(parseHttpRequest(std::string(9000, 'a'), r) == HttpParse::TooBig);
  CHECK(parseHttpRequest("GET / HTTP/1.1\r\nHost: a", r) == HttpParse::Incomplete);
  // risposta
  const std::string resp = httpResponse(200, "application/json", "{\"a\":1}");
  CHECK(resp.find("HTTP/1.1 200 OK\r\n") == 0 && resp.find("Content-Length: 7\r\n") != std::string::npos && resp.find("Cache-Control: no-store") != std::string::npos);
  CHECK(resp.size() > 7 && resp.substr(resp.size() - 7) == "{\"a\":1}" && resp.find("\r\n\r\n{") != std::string::npos);
  CHECK(httpResponse(401, "text/plain", "no").find("401 Unauthorized") != std::string::npos && httpResponse(404, "text/plain", "").find("404 Not Found") != std::string::npos);
  // url e json
  CHECK(urlDecode("a%20b+c%2Fd%zz%4") == "a b c/d%zz%4" && urlDecode("%41%42") == "AB");
  CHECK(jsonEscape("a\"b\\c\n\t\x01") == "a\\\"b\\\\c\\n\\t\\u0001");
  CHECK(constantTimeEquals("abc", "abc") && !constantTimeEquals("abc", "abd") && !constantTimeEquals("abc", "ab") && constantTimeEquals("", ""));
  TEST_MAIN_END();
}
