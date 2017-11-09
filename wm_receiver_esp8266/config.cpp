#define FS_NO_GLOBALS
#include <FS.h>
#include <Time.h>
#include <TimeLib.h>
#include "config.h"

#define BUFF_SIZE 128

void WMconfig::resetWM(byte i) {
  wm_data[i].ID                 = 0;
  wm_data[i].wm_location        = "";
  wm_data[i].wm_serial[0]       = "x";
  wm_data[i].wm_serial[1]       = "x";
  wm_data[i].wm_maintenance[0]  = 0;
  wm_data[i].wm_maintenance[1]  = 0;
  wm_data[i].wm_shift[0]        = 0;
  wm_data[i].wm_shift[1]        = 0;
  wm_set[i]                     = false;
}

bool WMconfig::init(void) {
  for (byte i = 0; i < MAX_WM; ++i) {
    resetWM(i);
  }
  frac_size = 2;
  smtp_relay_host = smtp_relay_from = smtp_relay_user = smtp_relay_pass = smtp_email_to = "";
  smtp_relay_ssl  = false;
  smtp_relay_port = 25;
  maintenance_warning   = 7776000;              // 90 days
  maintenance_urgent    =  604800;              // one week
  smtp_period           = 0;                    // do not send e-mail
  smtp_send_at          = 0;
  
  for (byte i = 0; i < WM_max_layers; ++i)
    p_key[i] = "";
  currentParent = currentKey = "";
  index    = 0;
  curr_ID  = 0;
  w_nodename = String("esp8266-wm");
  
  JsonStreamingParser parser;
  parser.setListener(this);

  byte buff[BUFF_SIZE];
  fs::File cf = SPIFFS.open(cf_name, "r");
  if (!cf) return false;

  int last = cf.size();
  bool isBody = false;

  while(last > 0) {
    int rb = last;
    if (rb > BUFF_SIZE) rb = BUFF_SIZE;
    rb = cf.read(buff, rb);
    if (!rb) {
      cf.close();
      return false;
    }
    last -= rb;
    for (byte i = 0; i < rb; ++i) {
      char c = buff[i];
      if (c == '{' || c == '[') {
        isBody = true;
      }
      if (isBody) {
        parser.parse(c);
      }
    }
  }
  cf.close();

  for (byte i = 0; i < MAX_WM; ++i) {
    wm_set[i] = (wm_data[i].ID && wm_data[i].wm_shift[WM_COLD] && wm_data[i].wm_shift[WM_HOT]);
    if (!wm_set[i]) resetWM(i);
  }
  return true;
}

void WMconfig::updateWM(byte ID, long cold_shift, long hot_shift) {
  byte indx = wm_index(ID);
  if (indx >= MAX_WM) return;                   // No valid water meter found in the config. Do not save!
  wm_data[indx].wm_shift[0] = cold_shift;
  wm_data[indx].wm_shift[1] = hot_shift;
  wm_set[indx] = true;
}

void WMconfig::removeWM(byte ID) {
  byte indx = wm_index(ID);
  if (indx >= MAX_WM) return;                   // No valid water meter found in the config.
  resetWM(indx);
}

void WMconfig::updateWMserial(byte ID, bool hot, String sn, time_t nxt) {
  byte indx = wm_index(ID);
  if (indx >= MAX_WM) return;                   // No valid water meter found in the config. Do not save!
  wm_data[indx].wm_serial[byte(hot)] = sn;
  wm_data[indx].wm_maintenance[byte(hot)] = nxt;
  wm_set[indx] = true;
}

byte WMconfig::wm_index(byte ID) {
  for (byte i = 0; i < MAX_WM; ++i) {
    if (wm_data[i].ID == ID) return i;
  }
  for (byte i = 0; i < MAX_WM; ++i) {
    if (wm_data[i].ID == 0) {
      wm_data[i].ID = ID;
      return i;
    }
  }
  return MAX_WM;                                // No valid slot found
}

void WMconfig::setLocation(byte ID, String location) {
  byte indx = wm_index(ID);
  if (location.length() > 23)
    wm_data[indx].wm_location = location.substring(0, 23);
  else
    wm_data[indx].wm_location = location;
}

byte WMconfig::wmCount() {
  byte cnt = 0;
  for (byte i = 0; i < MAX_WM; ++i) {
    if (wm_set[i]) ++cnt;
  }
  return cnt;
}

String WMconfig::location(byte ID) {
  byte indx = wm_index(ID);
  return wm_data[indx].wm_location;
}

String WMconfig::serial(byte ID, bool hot) {
  byte indx = wm_index(ID);
  return wm_data[indx].wm_serial[byte(hot)];
}

time_t WMconfig::nextMaintenance(byte ID, bool hot) {
  byte indx = wm_index(ID);
  return wm_data[indx].wm_maintenance[byte(hot)];
}

bool WMconfig::warningMaintenance(byte ID, bool hot) {
  if (maintenance_warning) {
    return (now() + maintenance_warning > nextMaintenance(ID, hot));
  }
  return false;
}

bool WMconfig::urgentMaintenance(byte ID, bool hot) {
  if (maintenance_urgent) {
    return (now() + maintenance_urgent > nextMaintenance(ID, hot));
  }
  return false;
}

String WMconfig::dataSendAt(void) {
  String ret = "";
  if (smtp_period == 1) {                       // Monthly
    ret = String(smtp_send_at);
    ret += " (day of month)";
  } else
  if (smtp_period == 2) {                       // Weekly
    ret = String(smtp_send_at);
    ret += " (" + week_days[smtp_send_at] + ")";
  } else
  if (smtp_period == 3) {                       // Daily
    ret = String(smtp_send_at);
    ret += " (hour)";
  }
  return ret;
}

time_t WMconfig::nextTimeDataSend(time_t at) {
  if (at == 0) at = now();
  tmElements_t tm;
  breakTime(at, tm);
  
  switch (smtp_period) {
    case 1:                                     // monthly
      if (smtp_send_at < tm.Day) {              // Next month
        if (++tm.Month > 12) {
          tm.Month = 1;
          ++tm.Year;
        }
      }
      tm.Day = smtp_send_at;
      tm.Hour = 12;
      break;
    case 2:                                     // weekly
      if (smtp_send_at > tm.Wday) {             // This week
        time_t nxt = at + (smtp_send_at - tm.Wday) * 86400;
        breakTime(nxt, tm);
      } else {                                  // Next week
        time_t nxt = at + (7 + smtp_send_at - tm.Wday) * 86400;
        breakTime(nxt, tm);
      }
      tm.Hour = 12;
      break;
    case 3:                                     // daily
      if (smtp_send_at < tm.Hour) {             // Tomorrow
        time_t nxt = at + 86400;
        breakTime(nxt, tm);
      }
      tm.Hour = smtp_send_at;
      break;
    default:                                    // do not send
      return 0;
  }
  tm.Minute = 0;
  tm.Second = 0;
  time_t nxt = makeTime(tm);
  return nxt;
}

String WMconfig::warnMaintDays(void) {
  time_t d = maintenance_warning / 86400;
  return String(d);
}

String WMconfig::urgentMaintDays(void) {
  time_t d = maintenance_urgent / 86400;
  return String(d);
}

void WMconfig::setSmtpRelayHost(String& host, uint16_t port, bool ssl) {
  smtp_relay_host = host;
  smtp_relay_port = port;
  smtp_relay_ssl  = ssl;
}

void WMconfig::setSmtpAuthUser(String& user, String& pass) {
  smtp_relay_user = user;
  smtp_relay_pass = pass;
}

void WMconfig::setMaintenanceDays(time_t urgent, time_t warn) {
  maintenance_urgent  = urgent * 86400;
  maintenance_warning = warn   * 86400;
}

void WMconfig::setDataSendPeriod(String& period, String& at) {
  for (byte i = 0; i < sizeof(valid_period) / 2; ++i) {
    if (period.compareTo(valid_period[i][0]) == 0) {
      smtp_period = i;
      break;
    }
  }

  if (smtp_period == 0) {                       // Never
    smtp_send_at = 0;
  } else {
    smtp_send_at = at.toInt();                  // Expected day of month, week day number of hour
  }

  if (smtp_period == 2) {                       // Weekly
    smtp_send_at %= 7;
  }
}

bool WMconfig::save(void) {
  fs::File cf = SPIFFS.open(cf_name, "w");
  if (!cf) return false;

  cf.println("{\n \"wifi\": {");
  cf.println("  \"nodename\": \"" + w_nodename + "\",");
  cf.println("  \"ssid\": \"" + w_ssid + "\",");
  cf.println("  \"password\": \"" + w_password + "\"\n },");
  cf.println(" \"ntp\": {");
  cf.println("  \"server\": \"" + t_server + "\",");
  cf.print("  \"time_shift\": \""); cf.print(t_shift); cf.println("\"\n },");
  cf.println(" \"blink_auth\": \"" + b_auth + "\",");
  byte wm_count = wmCount();
  cf.print(" \"wm_count\": \"");  cf.print(wm_count, DEC); cf.println("\",");
  cf.print(" \"fraction_digits\": \"");  cf.print(frac_size, DEC);
  if (wm_count > 0) {
    cf.println("\","); cf.println(" \"wm_list\": [");
    for (byte i = 0; i < wm_count; ++i) {
      if (!wm_set[i]) continue;
      cf.print("   { \"ID\": \""); cf.print(wm_data[i].ID, DEC);
      cf.print("\", \"location\": \"" + wm_data[i].wm_location + "\",");
      cf.print(" \"MAINT_COLD\": \""); cf.print(wm_data[i].wm_maintenance[WM_COLD], DEC);
      cf.print("\", \"SN_COLD\": \"" + wm_data[i].wm_serial[WM_COLD] + "\",");
      cf.print(" \"cold_shift\": \""); cf.print(wm_data[i].wm_shift[WM_COLD], DEC); cf.print("\", ");
      cf.print(" \"MAINT_HOT\": \"");  cf.print(wm_data[i].wm_maintenance[WM_HOT], DEC);
      cf.print("\", \"SN_HOT\":  \"" + wm_data[i].wm_serial[WM_HOT] + "\",");
      cf.print(" \"hot_shift\": \""); cf.print(wm_data[i].wm_shift[WM_HOT], DEC);
      if (i < wm_count -1) {
        cf.println("\"},");
      } else {
        cf.println("\"}");
      }
    }
    cf.print(" ]");
  } else {
    cf.print("\"");
  }
  if (smtp_relay_host.length() && smtp_email_to.length() && smtp_relay_from.length()) {
     cf.println(",\n \"smtp\": {");
     cf.print("  \"relay\": \"" + smtp_relay_host + "\", \"port\": \"");
     cf.print(smtp_relay_port, DEC); cf.print("\", \"ssl\": \""); cf.print(smtp_relay_ssl?'1':'0');
     cf.println("\",\n  \"from\": \"" + smtp_relay_from + "\", \"to\": \"" + smtp_email_to + "\",");
     cf.print("  \"user\": \"" + smtp_relay_user + "\", \"password\": \"" + smtp_relay_pass + "\"\n }");
  }
  cf.print(",\n \"notify\": {\n");
  cf.print("   \"warning\": \""); cf.print(maintenance_warning, DEC); cf.println("\",");
  cf.print("   \"urgent\": \"");     cf.print(maintenance_urgent, DEC);
  cf.print("\",\n    \"send_period\": \""); cf.print(smtp_period, DEC);
  cf.print("\",\n    \"send_at\": \""); cf.print(smtp_send_at, DEC);  
  cf.println("\"\n  }\n}");
  cf.close();
  return true;
}

void WMconfig::startObject() {
  for (byte i = WM_max_layers-1; i > 0; --i) {
    p_key[i] = p_key[i-1];
  }
  p_key[0] = currentParent;
  currentParent = currentKey;
}

void WMconfig::endObject() {
  currentParent = p_key[0];
  for (byte i = 0; i < WM_max_layers-1; ++i) {
    p_key[i] = p_key[i+1];
  }
  currentKey = currentArray;
}

void WMconfig::startArray() {
  currentArray = currentKey;
}

void WMconfig::endArray() {
  currentArray = "";
}

void WMconfig::value(String value) {
  if (currentParent == "wifi" && currentKey == "nodename") {
    w_nodename = value;
  } else
  if (currentParent == "wifi" && currentKey == "ssid") {
    w_ssid = value;
  } else
  if (currentParent == "wifi" && currentKey == "password") {
    w_password = value;
  } else
  if (currentParent == "ntp" && currentKey == "server") {
    t_server = value;
  }  else
  if (currentParent == "ntp" && currentKey == "time_shift") {
    t_shift = value.toInt();
  } else
  if (currentKey == "blink_auth") {
    b_auth  = value;
  } else
  if (currentKey == "fraction_digits") {
    frac_size  = value.toInt();
  } else {
    if (currentParent == "wm_list") {
      if (currentKey == "ID") {
        if (curr_ID == 0) {
          index  = 0;
          curr_ID = value.toInt(); 
          wm_data[index].ID = curr_ID;
        } else {
          if (index < MAX_WM - 1) {
            ++index;
            curr_ID = value.toInt();
            wm_data[index].ID = curr_ID;
          }
        }
      } else
      if (currentKey == "location") {
        wm_data[index].wm_location = value;
      } else
      if (currentKey == "MAINT_COLD") {
        wm_data[index].wm_maintenance[WM_COLD] = value.toInt();
      } else
      if (currentKey == "MAINT_HOT") {
        wm_data[index].wm_maintenance[WM_HOT] = value.toInt();
      } else
      if (currentKey == "SN_COLD") {
        wm_data[index].wm_serial[WM_COLD] = value;
      } else
      if (currentKey == "SN_HOT") {
        wm_data[index].wm_serial[WM_HOT]  = value;
      } else
      if (currentKey == "cold_shift") {
        wm_data[index].wm_shift[WM_COLD] = value.toInt();
      } else
      if (currentKey == "hot_shift") {
        wm_data[index].wm_shift[WM_HOT]  = value.toInt();
      }
    } else
    if (currentParent == "smtp") {
      if (currentKey == "relay") {
        smtp_relay_host = value;
      } else
      if (currentKey == "port") {
        smtp_relay_port = value.toInt();
      } else
      if (currentKey == "ssl") {
        smtp_relay_ssl = (value.toInt() == 1);
      } else
      if (currentKey == "from") {
        smtp_relay_from = value;
      } else
      if (currentKey == "to") {
        smtp_email_to = value;
      } else
      if (currentKey == "user") {
        smtp_relay_user = value;
      } else
      if (currentKey == "password") {
        smtp_relay_pass = value;
      }
    } else
    if (currentParent == "notify") {
      if (currentKey == "warning") {
        maintenance_warning = value.toInt();
      } else
      if (currentKey == "urgent") {
        maintenance_urgent = value.toInt();
      }  else
      if (currentKey == "send_period") {
        smtp_period = value.toInt();
      } else
      if (currentKey == "send_at") {
        smtp_send_at = value.toInt();
      }
    }
  }
}

