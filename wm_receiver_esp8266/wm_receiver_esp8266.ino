/*
 * The MAIN controller for Water Meter project. Based on esp8266 chip (nodeMCU or single esp8266 module)
 * with at least 1 MB of flash memory.
 * 
 * si4432     ESP8266
 * VCC        VCC
 * SDO        GPIO12  (MISO)
 * SDI        GPIO13  (MOSI)
 * SCLK       GPIO14
 * nSEL       GPIO15
 * nIrq       GPIO5
 * snd        GND
 * GND        GND
 */

#include <SPI.h>
#include <RH_RF22.h>
#define FS_NO_GLOBALS
#include <FS.h>
#include <BlynkSimpleEsp8266.h>
#include <Time.h>
#include <TimeLib.h>
#include <Wire.h>
#include "ntp.h"
#include "config.h"
#include "wm.h"
#include "web.h"
#include "log.h"
#include "mail.h"
#include "wm_data.h"

const byte ss_pin  = 15;                        // select pin number
const byte irq_pin = 5;                         // irq    pin number
const byte hb_pin  = 4;                         // heart beat LED pin

const uint16_t  web_port          =        80;
const byte      radio_wait        =       300;
const char*     clr_RED           = "#FF0000";
const char*     clr_YELLOW        = "#00FFFF";
const char*     clr_GREEN         = "#00FF00";

RH_RF22           rf22(ss_pin, irq_pin);        // Singleton instance of the radio driver
WMconfig          cfg;                          // Global variable, used in web.cpp and mail.cpp
WMpool            pool;                         // Global variable, used in web.cpp and mail.cpp
ntpClock          ntp;                          // Global variable, used in web.cpp
web               server(web_port);             // Global variable, used in web.cpp
wmlog             data_log;
notifier          e_notify;                     // The scheduled e-mail notifier
bool              log_data_loaded = false;      // This flag indicates that log data have been loaded
byte              blynk_wm_index = 0;
String b_auth;                                  // Blynk authentication key value

// Forward function declaration
void blynkInfoRefresh(void);
void loadLogData(void);

//------------------------------------------ Network status class for different modes --------------------------
class netMode {
  public:
    netMode()                                   { }
    virtual   netMode*  run(void);
    virtual   void      setupNextMode(byte ID, netMode* Mode) = 0;
    virtual   void      init(void)              { }
};

//------------------------------------------ Network modes: Not conected to Wi-Fi -------------------------------
class netDisconnected : public netMode {
  public:
    netDisconnected()                           { }
    virtual   netMode*  run(void);
    virtual   void      setupNextMode(byte ID, netMode* Mode);
  private:
    netMode*  modes[2];
    const     byte mode_net_init  = 0;
    const     byte mode_net_ap    = 1;
};

netMode* netDisconnected::run(void) {
  if (cfg.init()) {                             // the configuration has been succesfully loaded
    String nodename = cfg.nodeName();
    String ssid     = cfg.ssid();
    String passwd   = cfg.passwd();
    if (nodename.length() > 0 && ssid.length() > 0) {
      WiFi.hostname(nodename);
      WiFi.begin(ssid.c_str(), passwd.c_str());
    } else {
      if (server.setupAP()) {
        return modes[mode_net_ap];
      }
    }
    return modes[mode_net_init];
  }  else { 
    if (server.setupAP()) {
      return modes[mode_net_ap];
    }
  }
  return this;
}

void netDisconnected::setupNextMode(byte ID, netMode* Mode) {
  if (ID <= 1)
    modes[ID] = Mode;
}

//------------------------------------------ Network modes: NO config, run Access Point -------------------------
class netAP : public netMode {
  public:
    netAP()                                     { }
    virtual   netMode*  run(void);
    virtual   void      setupNextMode(byte ID, netMode* Mode) { }
};

netMode* netAP::run(void) {
  server.handleClient();
  return this;
}

//------------------------------------------ Network modes: Trying to Connect to Wi-Fi --------------------------
class netTrying : public netMode {
  public:
    netTrying()                                 { }
    virtual   netMode*  run(void);
    virtual   void      setupNextMode(byte ID, netMode* Mode);
    virtual   void      init(void);
  private:
    netMode*  modes[2];
    time_t    Wifi_try_timeout;
    const     byte mode_net_connected = 0;
    const     byte mode_net_ap        = 1;
    const     time_t wifi_to = 300;             // The connection to WiFi access point timeout
};

netMode* netTrying::run(void) {
  if (WiFi.status() == WL_CONNECTED) {
    String node = cfg.nodeName();
    MDNS.begin(node.c_str());
    ntp.init(cfg.ntp(), cfg.tz());
    ntp.syncTime();
    server.setupWEBserver();
    return modes[mode_net_connected];
  } else {
    if (now() > Wifi_try_timeout) {
      if (server.setupAP()) {
         return modes[mode_net_ap];
      }
    }
  }
  return this;
}

void netTrying::setupNextMode(byte ID, netMode* Mode) {
  if (ID <= 1)
    modes[ID] = Mode;
}

void netTrying::init(void) {
  Wifi_try_timeout = now() + wifi_to;
}

//------------------------------------------ Network modes: Trying to Connect to Blynk --------------------------
class blynkTrying : public netMode {
   public:
    blynkTrying()                               { }
    virtual   netMode*  run(void);
    virtual   void      setupNextMode(byte ID, netMode* Mode);
  private:
    netMode*  modes[2];
    const     byte mode_blynk_connected = 0;
    const     byte mode_net_noWiFi      = 1;
};

netMode* blynkTrying::run(void) {
  if (b_auth.length() > 10) {
    Blynk.config(b_auth.c_str(), BLYNK_DEFAULT_DOMAIN, BLYNK_DEFAULT_PORT);
    if (Blynk.connect()) {
      return modes[mode_blynk_connected];
    }
  }
  if (WiFi.status() != WL_CONNECTED)
    return modes[mode_net_noWiFi];

  server.handleClient();

  ntp.syncTime();                               // Sync local clock with ntp (not every call)
  if (!log_data_loaded) {                       // Trying to load data from the current log
    loadLogData();
    log_data_loaded = true;
  }
  return this;
}

void blynkTrying::setupNextMode(byte ID, netMode* Mode) {
  if (ID <= 1)
    modes[ID] = Mode;
}

//------------------------------------------ Network modes: Wifi and Blynk are connected ------------------------
class blynkOK : public netMode {
   public:
    blynkOK()                                   { update_cloud = true; }
    virtual   netMode*  run(void);
    virtual   void      setupNextMode(byte ID, netMode* Mode);
    virtual   void      init(void);
  private:
    bool      update_cloud;
    time_t    when_update;                      // The time when to update the blynk cloud data
    netMode*  noBlynk;
    const     time_t    update_delay = 10;
};

netMode* blynkOK::run(void) {
  if (!Blynk.connected()) {
    return noBlynk;
  }
  Blynk.run();
  server.handleClient();
  ntp.syncTime();                               // Sync local clock with ntp (not every call)
  if (!log_data_loaded) {                       // Trying to load data from the current log
    loadLogData();
    log_data_loaded = true;
  }
  if (when_update && now() >= when_update) {
    blynkInfoRefresh();
    when_update = 0;
    update_cloud = false;                       // update the cloud information only once
  }
  return this;
}

void blynkOK::setupNextMode(byte ID, netMode* Mode) {
  if (ID <= 0)
    noBlynk = Mode;
}

void blynkOK::init(void) {
  if (update_cloud)
    when_update = now() + update_delay;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Network modes: Last class ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

netDisconnected   nNOwifi;
netAP             nAP;
netTrying         nTry;
blynkTrying       nBlynkTry;
blynkOK           nOK;
netMode*          currentMode = &nNOwifi;

void setup(void) {
//  Serial.begin(115200); Serial.println("\n\n\n\n.");
  pinMode(hb_pin, OUTPUT);
  if (rf22.init()) {                            // Defaults after init are 434.0MHz, 0.05MHz AFC pull-in, modulation FSK_Rb2_4Fd36
    rf22.setPromiscuous(true);                  // wish to receive messages with any TO address
    rf22.setFrequency(433.0, 0.5);
    rf22.setModemConfig(RH_RF22::GFSK_Rb2Fd5);  ///< GFSK, No Manchester, Rb = 2kbs,    Fd = 5kHz
    rf22.setTxPower(RH_RF22_TXPOW_5DBM);
  } else {
    while(1) yield();                             
  }

  if (!SPIFFS.begin()) {
    SPIFFS.format();                            // Format the SPIFFS for the first time
  }
  if (!SPIFFS.begin()) {
    while(1) yield();                           // Stay here twiddling thumbs waiting
  }

  pool.init();                                  // Initialize the water meters pool
  if (cfg.init()) {                             // the configuration has been succesfully loaded
    byte wm_num = cfg.wmCount();
    if (wm_num > 0) {
      WMuData *wud = cfg.getWMuData();
      for (byte i = 0; i < wm_num; ++i) {
        pool.WMinit(wud[i].ID, wud[i].wm_shift[WM_COLD], wud[i].wm_shift[WM_HOT]);
      }
      byte frac_size = cfg.frac();
      pool.setFractionDigits(frac_size);
      b_auth = cfg.auth();                      // b_auth is the global variable
      e_notify.init();                          // Setup the e-mail notoficator
    }
  }     

  // Setup network modes structures
  nNOwifi.setupNextMode(0, &nTry);              // Trying to connect to WIFI
  nNOwifi.setupNextMode(1, &nAP);               // Turn Access Point mode
  nTry.setupNextMode(0, &nBlynkTry);            // Trying to connect to blynk server
  nTry.setupNextMode(1, &nAP);                  // Timed out to connect to Wifi, turn Access Point mode
  nBlynkTry.setupNextMode(0, &nOK);             // Trying to connect to blynk server
  nBlynkTry.setupNextMode(1, &nTry);            // Reconnect to the WiFi network
  nOK.setupNextMode(0, &nBlynkTry);             // Reconnect to connect to blynk server
  currentMode->init();
}

void loadLogData(void) {
  byte wm_id[MAX_WM];
  byte wm_num = pool.idList(wm_id);
  data_log.loadLog(wm_id, wm_num);
  for (byte i = 0; i < wm_num; ++i) {
    uint32_t cold, hot;
    time_t   ts;
    byte ID = wm_id[i];
    if (data_log.data(ID, cold, hot, ts) ) {
      struct data wmd;
      long sc = pool.shift(ID, false);
      long sh = pool.shift(ID, true);
      wmd.wm_data[WM_COLD] = cold - sc;
      wmd.wm_data[WM_HOT]  = hot  - sh;
      wmd.batt_mv = 3000;
      wmd.ID = ID;
      pool.update(wmd, ts);
    }
  }
}

const char* colorMaintenance(byte ID, bool hot) {
  if (cfg.urgentMaintenance(ID, hot)) {
    return clr_RED;
  } else
  if (cfg.warningMaintenance(ID, hot)) {
    return clr_YELLOW;
  }
  return clr_GREEN;
}

void blynkInfoRefresh(void) {
  if (Blynk.connected()) {                     
    byte num_wm = pool.numWM();
    BlynkParamAllocated items(32*num_wm);        // list length, in bytes
    for (byte i = 0; i < num_wm; ++i) {
      byte id = pool.id(i);
      String loc = cfg.location(id);
      items.add(loc);
    }
    Blynk.setProperty(V0, "labels", items);
    Blynk.virtualWrite(0, blynk_wm_index+1);
    byte ID = pool.id(blynk_wm_index);
    Blynk.virtualWrite(1, cfg.serial(ID, true));
    Blynk.virtualWrite(3, cfg.serial(ID, false));
    Blynk.virtualWrite(2, pool.valueS(ID, true));
    Blynk.virtualWrite(4, pool.valueS(ID, false));
    time_t nxt = cfg.nextMaintenance(ID, true);
    Blynk.virtualWrite(5, dateStr(nxt));
    Blynk.setProperty(V5, "color", colorMaintenance(ID, true));
    nxt = cfg.nextMaintenance(ID, true);
    Blynk.virtualWrite(6, dateStr(nxt));
    Blynk.setProperty(V6, "color", colorMaintenance(ID, false));
    Blynk.virtualWrite(7, pool.batteryS(ID));
  }
}

void heartBeatBlink(netMode* Mode) {
  static uint32_t nxt_change  = 0;
  static byte     led_counter = 0;
  static byte     led_mode    = 0;
  static uint16_t m_period[5][4] = {
    {   200, 100,   200, 100},                  // NO WIFI mode
    {  1000, 200,   200, 200},                  // Access Poing mode
    { 10000, 200, 10000, 300},                  // Trying to connect to WiFi
    { 10000, 500,  2000, 200},                  // Trying to authenticate to the Blynk
    {  5000, 200,  5000, 200}                   // Main mode, Everything is connected
  };

  if (millis() >= nxt_change) {
    if (Mode == &nNOwifi) {                     // No connection
      led_mode = 0;
    } else
    if (Mode == &nAP) {                         // Access Point Mode
      led_mode = 1;
    } else
    if (Mode == &nTry) {                        // Trying to connect to WiFi
      led_mode = 2;
    } else
    if (Mode == &nBlynkTry) {                   // Trying to conect to Blynk
      led_mode = 3;
    } else {                                    // The main mode
      led_mode = 4;
    }
    if (++led_counter > 3) led_counter = 0;;
    nxt_change = millis() + m_period[led_mode][led_counter];
    digitalWrite(hb_pin, led_counter & 1);
  }
}

//================================ Blynk interface callbacks ===================================================
BLYNK_WRITE(V0) {
  byte menu_item = param.asInt();
  if (menu_item >0 && menu_item <= pool.numWM()) {
    blynk_wm_index = menu_item -1;
    blynkInfoRefresh();
  } 
}

BLYNK_READ(V1) {                                // hot meter serial number
  byte ID = pool.id(blynk_wm_index);
  Blynk.virtualWrite(1, cfg.serial(ID, true));
}

BLYNK_READ(V2) {                                // hot water readings
  byte ID = pool.id(blynk_wm_index);
  Blynk.virtualWrite(2, pool.valueS(ID, true));
}

BLYNK_READ(V3) {                                // cold water serial number
  byte ID = pool.id(blynk_wm_index);
  Blynk.virtualWrite(3, cfg.serial(ID, false));
}

BLYNK_READ(V4) {                                // cold water readings
  byte ID = pool.id(blynk_wm_index);
  Blynk.virtualWrite(4, pool.valueS(ID, false));
}

BLYNK_READ(V5) {                                // hot water meter maintenance time
  byte ID = pool.id(blynk_wm_index);
  time_t nxt = cfg.nextMaintenance(ID, true);
  Blynk.virtualWrite(5, dateStr(nxt));          // dateStr() is defined in web.cpp
  Blynk.setProperty(V5, "color", colorMaintenance(ID, true));
}

BLYNK_READ(V6) {                                // cold water meter maintenance time
  byte ID = pool.id(blynk_wm_index);
  time_t nxt = cfg.nextMaintenance(ID, false);
  Blynk.virtualWrite(6, dateStr(nxt));          // dateStr() is defined in web.cpp
  Blynk.setProperty(V6, "color", colorMaintenance(ID, false));
}

BLYNK_READ(V7) {                                // Battery voltage
  byte ID = pool.id(blynk_wm_index);
  Blynk.virtualWrite(7, pool.batteryS(ID));
}

//==============================================================================================================
void loop() {
  static time_t log_remove = 0;
  
  if (rf22.waitAvailableTimeout(radio_wait)) {
    struct data wm;                             // Defined in wm_data.h file
    byte len = sizeof(struct data);
    if (rf22.recv((byte*)&wm, &len)) {
      pool.update(wm);
      String loc = cfg.location(wm.ID);
      if (loc.length() == 0) {
        cfg.setLocation(wm.ID, String(wm.ID));
      }
      long cold = pool.shift(wm.ID, false) + wm.wm_data[WM_COLD];
      long hot  = pool.shift(wm.ID, true)  + wm.wm_data[WM_HOT];
      data_log.log(wm.ID, cold, hot);
    }
  }
  yield();

  netMode* nxtMode = currentMode->run();
  if (nxtMode != currentMode) {
    currentMode = nxtMode;
    currentMode->init();
    if (currentMode == &nBlynkTry) {            // Successfully connected to WIFI
      nTry.setupNextMode(1, &nTry);             // If once connected to wifi, do not switch to the AP mode ever
    }
  }
  heartBeatBlink(currentMode);
  yield();

  if (currentMode == &nOK) {
    e_notify.send();                            // Send e-mail notofications
    time_t ts = now();
    if (ts >= log_remove) {
      log_remove = ts + 60 * 60 * 24;           // Run next log remove procedure in 24 hours
      ts -= 60 * 60 * 24 * 90;                  // remove log files created 90 days ago
      data_log.removeOldLog(ts);
    }
  }
}

