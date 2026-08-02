#pragma once
#include <Arduino.h>
#include <Client.h>

struct HttpResponse {
  int statusCode = -1;  // stays -1 if the connection itself failed
  String body;
};

// The ESP32 Arduino core's built-in HTTPClient only accepts a WiFiClient,
// which rules out driving it over an EthernetClient (W5500) — both merely
// implement the generic Client interface, EthernetClient isn't a
// WiFiClient. This is a small HTTP/1.1 client good enough for short JSON
// GET/POST requests against our own backend on the LAN (no chunked
// transfer-encoding, no redirects, no TLS) that works over *any* Client.
namespace SimpleHttp {
HttpResponse request(Client &client, const String &host, uint16_t port, const String &path,
                      const String &method, const String &apiKey, const String &body = "");
}
