#ifndef _ESP_WM_WEB
#define _ESP_WM_EWB

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "config.h"
#include "ntp.h"
#include "wm.h"
#include "mail.h"

String dateStr(time_t ts);

//------------------------------------------ WEB server --------------------------------------------------------
class web : public ESP8266WebServer {
  public:
    web(uint16_t port) : ESP8266WebServer(port) { }
    bool setupAP(void);
    void setupWEBserver(void);
};

#endif
