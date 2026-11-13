#include "http_mini.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace lcr {

static std::string lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return s;
}

static std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
  return s.substr(a, b - a);
}

HttpParse parseHttpRequest(const std::string& data, HttpRequest& req) {
  const size_t end = data.find("\r\n\r\n");
  if (end == std::string::npos) return data.size() > kHttpMaxHeaders ? HttpParse::TooBig : HttpParse::Incomplete;
  if (end > kHttpMaxHeaders) return HttpParse::TooBig;
  req = HttpRequest();
  const std::string head = data.substr(0, end);
  const size_t le = head.find("\r\n");
  const std::string first = head.substr(0, le);
  const size_t s1 = first.find(' ');
  const size_t s2 = s1 == std::string::npos ? std::string::npos : first.find(' ', s1 + 1);
  if (s1 == std::string::npos || s2 == std::string::npos) return HttpParse::Bad;
  req.method = first.substr(0, s1);
  std::string target = first.substr(s1 + 1, s2 - s1 - 1);
  const std::string version = first.substr(s2 + 1);
  if (version.compare(0, 5, "HTTP/") != 0 || target.empty() || target[0] != '/') return HttpParse::Bad;
  for (char c : req.method) {
    if (!isupper(static_cast<unsigned char>(c))) return HttpParse::Bad;
  }
  const size_t q = target.find('?');
  req.path = target.substr(0, q);
  if (q != std::string::npos) req.query = target.substr(q + 1);
  size_t pos = le == std::string::npos ? head.size() : le + 2;
  while (pos < head.size()) {
    size_t e = head.find("\r\n", pos);
    if (e == std::string::npos) e = head.size();
    const std::string line = head.substr(pos, e - pos);
    pos = e + 2;
    const size_t c = line.find(':');
    if (c == std::string::npos || c == 0) return HttpParse::Bad;
    req.headers[lower(line.substr(0, c))] = trim(line.substr(c + 1));
  }
  size_t len = 0;
  auto it = req.headers.find("content-length");
  if (it != req.headers.end()) {
    char* e2 = nullptr;
    const unsigned long v = strtoul(it->second.c_str(), &e2, 10);
    if (it->second.empty() || *e2 != 0) return HttpParse::Bad;
    if (v > kHttpMaxBody) return HttpParse::TooBig;
    len = static_cast<size_t>(v);
  }
  const size_t bodyStart = end + 4;
  if (data.size() < bodyStart + len) return HttpParse::Incomplete;
  req.body = data.substr(bodyStart, len);
  return HttpParse::Ok;
}

std::string httpResponse(int status, const std::string& contentType, const std::string& body, const std::string& extraHeaders) {
  const char* text = status == 200 ? "OK" : status == 400 ? "Bad Request" : status == 401 ? "Unauthorized" : status == 404 ? "Not Found"
                     : status == 405 ? "Method Not Allowed" : status == 413 ? "Payload Too Large" : status == 500 ? "Internal Server Error" : "Error";
  char b[160];
  snprintf(b, sizeof(b), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n", status, text, contentType.c_str(), body.size());
  std::string r = b;
  r += "Cache-Control: no-store\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\n";
  r += extraHeaders;
  r += "\r\n";
  r += body;
  return r;
}

std::string urlDecode(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') {
      o += ' ';
    } else if (s[i] == '%' && i + 2 < s.size() + 0 && isxdigit(static_cast<unsigned char>(s[i + 1])) && isxdigit(static_cast<unsigned char>(s[i + 2]))) {
      o += static_cast<char>(strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
      i += 2;
    } else {
      o += s[i];
    }
  }
  return o;
}

std::string formParam(const std::string& form, const std::string& key) {
  size_t pos = 0;
  while (pos <= form.size()) {
    size_t amp = form.find('&', pos);
    if (amp == std::string::npos) amp = form.size();
    const std::string kv = form.substr(pos, amp - pos);
    const size_t eq = kv.find('=');
    if (eq != std::string::npos && urlDecode(kv.substr(0, eq)) == key) return urlDecode(kv.substr(eq + 1));
    pos = amp + 1;
  }
  return "";
}

std::string jsonEscape(const std::string& s) {
  std::string o;
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if (u < 0x20 || u == 0x7F) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", u);
      o += b;
    } else {
      o += c;
    }
  }
  return o;
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
  unsigned char d = static_cast<unsigned char>(a.size() ^ b.size());
  const size_t n = a.size() < b.size() ? a.size() : b.size();
  for (size_t i = 0; i < n; i++) d |= static_cast<unsigned char>(a[i] ^ b[i]);
  return d == 0;
}

}  // namespace lcr
