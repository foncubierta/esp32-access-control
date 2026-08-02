#include "SimpleHttp.h"

namespace {
constexpr uint32_t CONNECT_TIMEOUT_MS = 8000;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 12000;
}  // namespace

HttpResponse SimpleHttp::request(Client &client, const String &host, uint16_t port, const String &path,
                                  const String &method, const String &apiKey, const String &body) {
  HttpResponse resp;

  client.setTimeout(CONNECT_TIMEOUT_MS);
  if (!client.connect(host.c_str(), port)) {
    return resp;  // statusCode stays -1
  }

  client.print(method);
  client.print(' ');
  client.print(path);
  client.print(" HTTP/1.1\r\n");
  client.print("Host: ");
  client.print(host);
  client.print("\r\n");
  client.print("X-Api-Key: ");
  client.print(apiKey);
  client.print("\r\n");
  client.print("Connection: close\r\n");
  if (body.length() > 0) {
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: ");
    client.print(body.length());
    client.print("\r\n\r\n");
    client.print(body);
  } else {
    client.print("\r\n");
  }

  uint32_t start = millis();
  while (client.connected() && !client.available() && millis() - start < RESPONSE_TIMEOUT_MS) {
    delay(10);
  }
  if (!client.available()) {
    client.stop();
    return resp;
  }

  // Status line, e.g. "HTTP/1.1 200 OK"
  String statusLine = client.readStringUntil('\n');
  int firstSpace = statusLine.indexOf(' ');
  int secondSpace = (firstSpace >= 0) ? statusLine.indexOf(' ', firstSpace + 1) : -1;
  if (firstSpace > 0 && secondSpace > firstSpace) {
    resp.statusCode = statusLine.substring(firstSpace + 1, secondSpace).toInt();
  }

  // Headers — skipped except for Content-Length, which tells us exactly
  // how many body bytes to read even if the server keeps the socket open.
  long contentLength = -1;
  while (client.connected() || client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;  // blank line = end of headers
    String lower = line;
    lower.toLowerCase();
    if (lower.startsWith("content-length:")) {
      contentLength = line.substring(line.indexOf(':') + 1).toInt();
    }
    if (millis() - start > RESPONSE_TIMEOUT_MS) break;
  }

  if (contentLength >= 0) {
    resp.body.reserve(contentLength);
    while ((long)resp.body.length() < contentLength && millis() - start < RESPONSE_TIMEOUT_MS) {
      while (client.available() && (long)resp.body.length() < contentLength) {
        resp.body += (char)client.read();
      }
      if (!client.connected() && !client.available()) break;
    }
  } else {
    while ((client.connected() || client.available()) && millis() - start < RESPONSE_TIMEOUT_MS) {
      while (client.available()) resp.body += (char)client.read();
      if (!client.connected() && !client.available()) break;
    }
  }

  client.stop();
  return resp;
}
