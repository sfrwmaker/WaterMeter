#define FS_NO_GLOBALS
#include <FS.h>
#include "web.h"
#include "ntp.h"
#include "config.h"

extern WMconfig          cfg;                   // Global variable, declared in wm_receiver_esp8266.ino
extern WMpool            pool;                  // Global variable, declared in wm_receiver_esp8266.ino
extern ntpClock          ntp;                   // Global variable, declared in wm_receiver_esp8266.ino
extern web               server;                // Global variable, declared in wm_receiver_esp8266.ino
extern notifier          e_notify;              // Global variable, declared in wm_receiver_esp8266.ino

// WEB handlers
void handleRoot(void);
void handleSetup(void);
void initialSetup(void);
void handleWMinfo(void);
void handleWMsetup(void);
void handleWMremove(void);
void handleMailsetup(void);
void handleWMlog(void);
void handleNotFound(void);

// These functions are defined in the main file
void blynkMenuRefresh(void);
void blynkInfoRefresh(void);


// web page styles
const char style[] PROGMEM = R"=====(
<style>
body  {background-color:powderblue;}
input {background-color:powderblue;}
select {background-color:powderblue;}
t1    {color: black; align-text: center; font-size: 200%;}
h1    {color: blue;  padding: 5px 0; align-text: center; font-size: 150%; font-weight: bold;}
h2    {color: black; padding: 0 0; align-text: center; font-size: 120%; font-weight: bold;}
th    {vertical-align: middle; text-align: center; font-weight: normal;}
td    {vertical-align: middle; text-align: center;}
ul    {list-style-type: none; margin: 0; padding: 0; overflow: hidden; background-color: #333;}
li    {float: left; border-right:1px solid #bbb;}
li:last-child {border-right: none;}
li a  {display: block; color: white; text-align: center; padding: 14px 16px; text-decoration: none;}
li a:hover:not(.active) {background-color: #111;}
.active {background-color: #6aa7e3;}
form {padding: 20px;}
form .field {padding: 4px; margin: 1px;}
form .field label {display: inline-block; width:240px; margin-left:5px; text-align: left;}
form .field input {display: inline-block; size=20;}
form .field select {display: inline-block; size=20;}
.myframe {width:500px; -moz-border-radius:5px; border-radius:5px; -webkit-border-radius:5px;}
.myheader {margin-left:10px; margin-top:10px;}
</style>
</head>
)=====";

//------------------------------------------ WEB server --------------------------------------------------------
bool web::setupAP(void) {
  const char *ssid = "esp8266-wm";
  bool stat = WiFi.softAP(ssid);                // use default IP address, 192.168.4.1
  if (stat) {
    ESP8266WebServer::on("/", initialSetup);
    ESP8266WebServer::on("/wifi_setup", initialSetup);
    ESP8266WebServer::onNotFound(handleNotFound);
    ESP8266WebServer::begin();
  }
  return stat;
}

void web::setupWEBserver(void) {
  ESP8266WebServer::on("/", handleRoot);
  ESP8266WebServer::on("/wifi_setup",  handleSetup);
  ESP8266WebServer::on("/wm_info",     handleWMinfo);
  ESP8266WebServer::on("/wm_setup",    handleWMsetup);
  ESP8266WebServer::on("/wm_remove",   handleWMremove);
  ESP8266WebServer::on("/mail_setup",  handleMailsetup);
  ESP8266WebServer::on("/log",         handleWMlog);
  ESP8266WebServer::onNotFound(handleNotFound);
  ESP8266WebServer::begin();
}
//==============================================================================================================

String main_menu[3][2] = {
  {"Main", "/"},
  {"Setup", "/wifi_setup"},
  {"Notifications", "/mail_setup"},
};

void header(const String title, bool refresh = false) {
  String hdr = "<!DOCTYPE HTML>\n<html><head>\n";
  if (refresh) hdr += "<meta http-equiv='refresh' content='40'/>";
  hdr += "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n<title>";
  hdr += title;
  hdr += "</title>\n";
  server.sendContent(hdr);
  server.sendContent_P(style);
}

String mainMenu(byte active, byte active_WM) {
  if ((active_WM == 0) && (active >= 3))        // If selected menu item, not water meter
    active = 0;
  byte wm_ID[MAX_WM];
  int wm_num = pool.idList(wm_ID);
  String menu = "<ul>";
  for (byte i = 0; i < 3; ++i) {                // Main menu
      menu += "<li><a ";
    if ((active_WM == 0) && (i == active))      // This is active menu item
      menu += "class='active' ";
      menu += "href='" + main_menu[i][1] + "'>" + main_menu[i][0] + "</a></li>\n";
  }
  for (byte i = 0; i < wm_num; ++i) {           // Water meter
    menu += "<li><a ";
    if (active_WM == wm_ID[i])
      menu += "class='active' ";
    menu += "href='/wm_info?id="; 
    menu += String(wm_ID[i]);
    menu += "'>";
    menu += cfg.location(wm_ID[i]);
    menu += "</a></li>\n";
  }
  menu += "<li style='float:right'><a href='/wifi_setup'>";
  menu += ntp.ntpTimeS();
  menu += "</a></li>\n</ul>\n";
  return menu;
}

void setupPage(bool menu = true) {
  if (server.args() > 0) {                      // Setup new NTP and WiFi values
    String sn = server.arg("server_name");
    ntp.srvSet(sn);
    String p = server.arg("tz_minute");
    int tz = p.toInt();
    ntp.tzSet(tz);
    cfg.setNTP(sn, tz);
    String s = server.arg("ssid");
    p = server.arg("passwd");    
    cfg.setWifi(s, p);
    p = server.arg("auth");
    cfg.setAuth(p);
    cfg.save();
  }
  String body = "<body>";
  if (menu) {
    body += mainMenu(1, 0);
  }
  body += "<div align='center'><t1>Network Setup";
  header("setup");
  body += "</t1></div><br>\n<form action='/wifi_setup'>\n";
  body += "<div align='center'><fieldset class='myframe'>";
  body += "<div class='field'><label for='ssid'>Wifi SSID:</label>";
  body += "<input type='text' name='ssid' value='";
  body += cfg.ssid();
  body += "'></div>\n<div class='field'><label for='passwd'>Wifi Password:</label>";
  body += "<input type='text' name='passwd' value='";
  body += cfg.passwd();
  body += "'></div>\n<div class='field'><label for='auth'>Blynk Auth Key:</label>";
  body += "<input type='text' name='auth' value='";
  body += cfg.auth();
  body += "'></div>\n<div class='field'><label for='server_name'>NTP Server Name:</label>";
  body += "<input type='text' name='server_name' value='";
  body += cfg.ntp();
  body += "'></div>\n<div class='field'><label for='tz_minute'>Greenwich Difference in Minutes:</label>";
  body += "<input type='text' name='tz_minute' value='";
  body += cfg.tz();
  body += "'></div></fieldset>\n</div><br><div align=\"center\"><input type=\"submit\" value=\"Apply\"></div>\n";
  body += "</form>\n</body>\n</html>";
  server.sendContent(body);
}

String dateStr(time_t ts) {
  if (ts == 0) ts = ntp.ntpTime();
  char buff[15];
  sprintf(buff, "%02d-%02d-%4d", day(ts), month(ts), year(ts));
  return String(buff);
}

time_t strDate(String tm) {
  tmElements_t tms;
  uint16_t ye;
  byte stat = 0;
  char c;
  for (byte i = 0; i < tm.length(); ++i) {
    while (i < tm.length()) {                   // Skip preceded invalid symbols
      c = tm.charAt(i);
      if ((c >= '0') || (c <= '9')) break;
      ++i;
    }
    byte j = 0;
    while (i+j < tm.length()) {
      c = tm.charAt(i+j);
      if ((c < '0') || (c > '9')) break;
      ++j;
    }
    switch (stat) {
      case 0:                                   // Day
        if (c == '-') {
          tms.Day = tm.substring(i, i+j).toInt();
          if (tms.Day > 31) tms.Day = 15;
          ++stat;
        }
        break;

      case 1:                                   // Month
        if (c == '-') {
          tms.Month = tm.substring(i, i+j).toInt();
          if (tms.Month > 12) tms.Month = 12;
          ++stat;
        }
        break;

      case 2:                                   // Year
        ye = tm.substring(i, i+j).toInt();
        if (ye > 1970) ye -= 1970;
        if (ye < 30) ye = 30;
        tms.Year = ye;
        ++stat;
        break;

      default:                                  // Invalid status
        break;
    }
    i += j;
  }
  tms.Second = 0;
  tms.Minute = 0;
  tms.Hour   = 12;
  time_t t = makeTime(tms);
  return t;
}

//------------------------------------------ URL handlers ------------------------------------------------------
void handleRoot(void) {
  time_t n = now();
  byte wm_ID[MAX_WM];
  byte wm_num = pool.idList(wm_ID);
  header("Water Meters", true);
  String body = "<body>";
  body += mainMenu(0, 0);
  body += "<div align=\"center\"><t1>Water Meter Data</t1></div>\n";
  if (wm_num == 0) {
    body += "<h1><div align=\"center\">No controller available</div></h1></body>\n</html>\n"; 
  } else {
    body += "<table cellspacing='1' cellpadding='10' border='1' align='center'>\n<tbody>\n<tr>\n";
    body += "<th><h1>Location</h1></th>\n";
    body += "<th><h1>Water</h1></th><th><h1>Serial number</h1></th>\n<th><h1>Data (m<sup>3</sup>)</h1></th>\n";
    body += "<th><h1>Updated</h1></th></tr>";
    for (byte i = 0; i < wm_num; ++i) {
      byte ID = wm_ID[i];
      body += "<tr>\n<td rowspan='2' colspan='1'><a href=\"/wm_info?id=";
      body += String(ID);
      body += "\">";
      body += cfg.location(ID);
      body += "</a></td>\n<td>cold water</td><td>";
      body += cfg.serial(ID, false);
      body += "</td>\n<td align='right'>";
      time_t changed = pool.tsDataChanged(ID, true);
      if ((n - changed) < 900) {                // Was changed last 15 minutes
        body += "<font color='red'>";
        body += pool.valueS(ID, false);
        body += "</font>";
      } else {
        body += pool.valueS(ID, false);
      }
      body += "</td>\n";
      body += "<td rowspan='2' colspan='1'>";
      if (pool.battery(ID) > 0) {
        time_t ts = pool.ts(ID);
        body += ntp.ntpTimeS(ts);
      } else {
        body += String("---");
      }
      body += "</td></tr>\n<tr>\n<td>hot water</td><td>";
      body += cfg.serial(ID, true);
      body += "</td>\n<td align=\"right\">";
      changed = pool.tsDataChanged(ID, true);
      if ((n - changed) < 900) {                // Was changed last 15 minutes
        body += "<font color='red'>";
        body += pool.valueS(ID, true);
        body += "</font>";
      } else {
        body += pool.valueS(ID, true);
      }      body += "</td>\n</tr>\n";
    }
    body += "</tbody>\n</table><br>\n</body></html>";
  }
  server.sendContent(body);
}

void handleWMinfo(void) {
  header("WM setup");
  String body = "<body>\n";
  if (server.hasArg("id")) {
    String id = server.arg("id");
    byte ID = id.toInt();
    byte wm_num = pool.numWM();
    body += mainMenu(0, ID);
    body += "<div align='center'><t1>Controller setup";
    String n = "Water Meter ";
    n += id;
    body += "</t1></div>\n<form action='/wm_setup'>\n<input type='hidden' name='ctrl_id' value='";
    body += id;
    body += "'>\n<div align='center'>\n<fieldset class='myframe'><legend>Controller (ID = ";
    body += String(ID);
    body += ")</legend>\n";
    body += "<div class='field'><label for='location'>Location:</label>\n";
    body += "<input type='text' name='location' value='";
    body += cfg.location(ID);
    body += "'></div>\n<div class='field'><label for='voltage'>Battery Voltage:</label>\n";
    body += "<input type='text' name='voltage' value='";
    body += pool.batteryS(ID);
    body += "' readonly></div>\n<div class='field'><label for='frac_size'>Fraction Size:</label>\n";
    body += "<input type='number' min='1' max='4' step='1' name='frac_size' value='";
    body += String(cfg.frac());
    body += "'></div></fieldset>\n<fieldset class='myframe'><legend>Cold Water</legend>\n";
    body += "<div class='field'><label for='sn_cold'>Serial:</label><input type='text' name='sn_cold' value='";
    body += cfg.serial(ID, false);
    body += "'></div>\n<div class='field'><label for='maint_cold'>Next Inspection:</label>\n";
    body += "<input type='text' name='maint_cold' value='";
    time_t nxt = cfg.nextMaintenance(ID, false);
    body += dateStr(nxt);
    body += "'></div>\n<div class='field'><label for='value_cold'>Data:</label>\n";
    body += "<input type='number' step=any name='value_cold' value='";
    body += pool.valueS(ID, false);
    body += "'></div>\n</fieldset><fieldset class='myframe'><legend>Hot Water</legend>\n";
    body += "<div class='field'><label for='sn_hot'>Serial:</label><input type='text' name='sn_hot' value='";
    body += cfg.serial(ID, true);
    body += "'></div>\n<div class='field'><label for='maint_hot'>Next Inspection:</label>\n";
    body += "<input type='text' name='maint_hot' value='";
    nxt = cfg.nextMaintenance(ID, true);
    body += dateStr(nxt);
    body += "'></div>\n<div class='field'><label for='value_hot'>Data:</label><input type='number' step=any name='value_hot' value='";
    body += pool.valueS(ID, true);
    body += "'></div></fieldset>\n</div><br>";
    body += "<div align='center'><input type='submit' formaction='/wm_remove' style='margin-right:50px' value='Remove'></td>";
    body += "<input type='submit' value='Save'></div>\n";
    body += "</form>\n</body>\n</html>";
  } else {
    body += "<div align='center'><t1>Controller setup";
    body += "<h1><div align='center'>No controller defined</div></h1></body>\n</html>\n"; 
  }
  server.sendContent(body);
}

void handleWMsetup(void) {
  if (server.hasArg("ctrl_id")) {
    byte ID = server.arg("ctrl_id").toInt();
    String p  = server.arg("location");
    if (p.length() > 0)
      cfg.setLocation(ID, p);
    p = server.arg("frac_size");
      cfg.setFrac(p.toInt());
    p = server.arg("value_cold");
    if (p.length() > 0) {
      pool.setAbsValueS(ID, false, p);
    }
    p = server.arg("value_hot");
    if (p.length() > 0) {
      pool.setAbsValueS(ID, true, p);
    }
    long cold_shift = pool.shift(ID, false);
    long hot_shift  = pool.shift(ID, true);
    cfg.updateWM(ID, cold_shift, hot_shift);

    String sn = server.arg("sn_cold");
    p = server.arg("maint_cold");
    time_t nxt = strDate(p);
    cfg.updateWMserial(ID, false, sn, nxt);
    sn = server.arg("sn_hot");
    p  = server.arg("maint_hot");
    nxt = strDate(p);
    cfg.updateWMserial(ID, true, sn, nxt);
  }
  cfg.save();
  e_notify.init();
  blynkInfoRefresh();                           // Defined in the main file
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleWMremove(void) {
  if (server.hasArg("ctrl_id")) {
    byte ID = server.arg("ctrl_id").toInt();
    cfg.removeWM(ID);
    e_notify.init();
    cfg.save();
  }
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleMailsetup(void) {
  if (server.args() > 0) {                      // submit button pressed, setup new values
    mailValidate *mv = new mailValidate;
    String sn = server.arg("host");
    sn = mv->validateFQDN(sn);
    String value = server.arg("port");
    uint16_t p = value.toInt();
    value = server.arg("ssl");
    bool ssl = (value.compareTo("on") == 0);
    cfg.setSmtpRelayHost(sn, p, ssl);
    String user = server.arg("user");
    String pass = server.arg("password");
    cfg.setSmtpAuthUser(user, pass);
    value = server.arg("sender");
    value = mv->validateEmail(value);
    cfg.setSmtpRelayFrom(value);

    value = server.arg("address");
    value = mv->validateEmail(value);
    cfg.setSmtpEmailTo(value);
    value = server.arg("tr_urgent");
    time_t urgent = value.toInt();
    value = server.arg("tr_warn");
    time_t warn   = value.toInt();
    cfg.setMaintenanceDays(urgent, warn);
    String period = server.arg("period");
    value = server.arg("period_value");
    cfg.setDataSendPeriod(period, value);
    delete mv;
    cfg.save();
    e_notify.init();
    e_notify.testLetter();                      // Send test letter in one minute!
  }
  header("Mail setup");
  String body = "<body>\n";
  body += mainMenu(2, 0);
  body += "<div align='center'>\n<form method='post' action='/mail_setup'>";
  body += "<fieldset class='myframe'><legend>Mail Relay Server</legend>";
  body += "<div class='field'><label for='host'>Server Name:</label>";
  body += "<input type='text' name='host' value='";
  body += cfg.smtpRelayHost();
  body +="'></div>\n<div class='field'><label for='port'>Port:</label>";
  body += "<input type='number' step='1' min='25' max='65536' name='port' value='";
  body += cfg.smtpRelayPort();
  body += "'></div>\n<div class='field'><label for='user'>User Name:</label>";
  body += "<input type='text' name='user' value='";
  body += cfg.smtpAuthUser();
  body += "'></div>\n<div class='field'><label for='password'>Password:</label>";
  body += "<input type='password' name='password' size='20' value='";
  body += cfg.smtpAuthPass();
  body += "'></div>\n<div class='field'><label for='sender'>Sender Address:</label>";
  body += "<input type='email' name='sender' value='";
  body += cfg.smtpRelayFrom();
  body += "'></div>\n<div align='right'><input type='hidden' name='ssl'";
  if (cfg.smtpRelaySSL()) {
    body += " checked";
  }
  body += "></div></fieldset>\n";
  body += "<fieldset class='myframe'><legend>Notifications</legend>\n";
  body += "<div class='field'><label for='address'>Send To:</label>";
  body += "<input type='email' name='address' value='";
  body += cfg.smtpEmailTo();
  body += "'></div>\n<div align='left' class='myheader'>Notification Types:</div>\n";
  body += "<div class='field'><label for='tr_urgent'>Urgent Inspection (days):</label>";
  body += "<input type='number' min='0' max='30' step='1' name='tr_urgent' value='";
  body += cfg.urgentMaintDays();
  body += "'></div>\n<div class='field'><label for='tr_warn'>Warning About Inspection (days):</label>";
  body += "<input type='number' min='0' max='100' step='1' name='tr_warn' value='";
  body += cfg.warnMaintDays();
  body += "'></div>\n<div class='field'><label for='period'>Water Meter Data Send Period:</label>\n";

  // Create the drop-down menu, (type='select') based on possible values on config class, see config.h
  body += "<select name='period'>\n";
  byte period = cfg.dataSendPeriod();
  for (byte i = 0; i < cfg.numDataSendPeriods(); ++i) {
    body += "<option value='" + cfg.dataSendPeriodName(i) + "'";
    if (period == i) body += " selected";
    body += ">" + cfg.dataSendPeriodLabel(i) + "</option>\n";
  }
  body += "</select></div>\n";
  
  body += "<div class='field'><label for='period_value'>Send At:</label><input type='text' name='period_value' value='";
  body += cfg.dataSendAt();
  body += "'></div>\n</fieldset>\n<div style='margin-top:30px'>"; 
  body += "<input type='submit' value='Apply'></div>\n";
  body += "</form></div></body>\n</html>";
  server.sendContent(body);
}

void handleSetup(void) {
  setupPage(true);
}

void initialSetup(void) {
  setupPage(false);
}

void handleWMlog(void) {
  if (server.args() > 0) {                      // File has been selected
    if (server.hasArg("fn")) {
      String fn = server.arg("fn");
      if (server.hasArg("remove")) {
        SPIFFS.remove(fn);
      } else {
        fs::File f = SPIFFS.open(fn, "r");
        server.streamFile(f, "text/html");
        f.close();
        return;
      }
    }
  }
  
  header("WM Log");
  String body = "<body>\n";
  body += "<div align='center'><h1>Water Meter logs</h1></div>";
  body += "<table cellspacing='1' cellpadding='10' border='0' align='center'>\n<tbody>\n<tr>\n";
  fs::Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fn = dir.fileName();
    if (fn.indexOf(".log") == -1)
      continue;
    body += "<td align='left'><a href='/log?fn=";
    body += fn;
    body += "'>";
    body += fn;
    body += "</a></td><td align='right'>";
    uint32_t s = dir.fileSize();
    char m = ' ';
    if (s >= 1024) {
      s >>= 10;
      m = 'K';
      if (s >= 1024) {
        s >>= 10;
        m = 'M';
      }
    }
    body += String(s) + m;
    body += "</td><td><a href='/log?remove=1&fn=";
    body += fn;
    body += "'>remove</a></td></tr>\n";
  }
  body += "</tbody></table></body>\n</html>";
  server.sendContent(body);
}

void handleNotFound(void) {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (byte i = 0; i < server.args(); ++i) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

