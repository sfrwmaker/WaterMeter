#define FS_NO_GLOBALS
#include <FS.h>
#include "mail.h"
#include "web.h"

extern WMconfig          cfg;                   // Global variable, declared in wm_receiver_esp8266.ino
extern WMpool            pool;                  // Global variable, declared in wm_receiver_esp8266.ino

/*
 * base64 encoder and decoder, http://www.cplusplus.com/forum/beginner/51572/
 */

bool base64::isBase64(byte c) {
  return (((c >= 'A') && (c <= 'Z')) ||
          ((c >= 'a') && (c <= 'z')) ||
          ((c >= '0') && (c <= '9')) ||
          (c == '+') || (c == '/'));
}

String base64::encode(String source) {
  String ret;
  int len = source.length();
  int index = 0;
  int i = 0;
  byte char_array_3[3], char_array_4[4];

  while (len--) {
    char_array_3[i++] = source.charAt(index++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; i < 4; ++i)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for(int j = i; j < 3; ++j)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (int j = 0; j < i + 1; ++j)
      ret += base64_chars[char_array_4[j]];

    while(i++ < 3)
      ret += '=';
  }

  return ret;
}

String base64::decode(String source) {
  int in_len = source.length();
  int i = 0;
  int in_ = 0;
  byte char_array_4[4], char_array_3[3];
  String ret;

  while (in_len-- && (source.charAt(in_) != '=') && isBase64(source.charAt(in_))) {
    char_array_4[i++] = source.charAt(in_++);
    if (i == 4) {
      for (i = 0; i < 4; ++i)
        char_array_4[i] = static_cast<byte>(base64Pos(char_array_4[i]));

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; i < 3; ++i)
        ret += static_cast<char>(char_array_3[i]);
      i = 0;
    }
  }

  if (i) {
    for (int j = i; j < 4; ++j)
      char_array_4[j] = 0;

    for (int j = 0; j < 4; ++j)
      char_array_4[j] = static_cast<byte>(base64Pos(char_array_4[j]));

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (int j = 0; j < i - 1; ++j)
      ret += static_cast<char>(char_array_3[j]);
  }

  return ret;
}

int base64::base64Pos(byte sym) {
  for (byte i = 0; i < sizeof(base64_chars); ++i) {
    if (base64_chars[i] == sym)
      return i;
  }
  return -1;
}

String mailValidate::validateFQDN(String& fqdn) {
  bool answer = true;
  for (byte i = 0; i < fqdn.length(); ++i) {
    char c = fqdn.charAt(i);
    if ((c >= 'A') && (c <= 'Z')) {
      continue;
    } else
    if ((c >= 'a') && (c <= 'z')) {
      continue;
    } else
    if ((c >= '0') && (c <= '9')) {
      continue;
    }
    bool special = false;
    for (byte i = 0; i < sizeof(valid_fqdn); ++i) {
      if (c == valid_fqdn[i]) {
        special = true;
        break;
      }
    }
    if (special) continue;
    answer = false;
    break;
  }
  if (answer)
    return fqdn;
  else
    return "";
}

    String    validateFQDN(String& fqdn);

String mailValidate::validateEmail(String& address) {
  bool answer = true;
  for (byte i = 0; i < address.length(); ++i) {
    char c = address.charAt(i);
    if ((c >= 'A') && (c <= 'Z')) {
      continue;
    } else
    if ((c >= 'a') && (c <= 'z')) {
      continue;
    } else
    if ((c >= '0') && (c <= '9')) {
      continue;
    }
    if (c == '@') {
      if (address.length() > i) {
        String fqdn = address.substring(i+1);
        fqdn = validateFQDN(fqdn);
        if (fqdn.length() > 0)
          return address;
      }
    }
    bool special = false;
    for (byte i = 0; i < sizeof(valid_email_user); ++i) {
      if (c == valid_fqdn[i]) {
        special = true;
        break;
      }
    }
    if (special) continue;
    answer = false;
    break;
  }
  if (answer)
    return address;
  else
    return "";
}

void mail::server(const String& srv, uint16_t port, bool ssl) {
  smtp_server = srv; smtp_port = port;
  if (secure == ssl) return;
  if (client) delete client;
  secure = ssl;
}

mail::ANSWER mail::send(const String& to, const String& message, const String& subject, const String& from) {  
  if (((from.length() == 0) && (smtp_from.length() == 0)) || smtp_server.length() == 0)
    return MAIL_SERVER;

  if (!client) {
    if (secure) {
      client = new WiFiClientSecure;
    } else {
      client = new WiFiClient;
    }
  }

  if (!client->connect(smtp_server.c_str(), smtp_port)) {
    return MAIL_CONNECT;
  }
  if (!expect("220")) {
    return MAIL_NO_ANSWER;
  }
  client->println("HELO there");
  if (!expect("250")){
    return MAIL_IDENT;
  }
  if (auth_user.length() > 0 && auth_pass.length() > 0) {
    client->println("AUTH LOGIN");
    expect();
    client->println(base64::encode(auth_user));
    expect();
    client->println(base64::encode(auth_pass));
    if (!expect("235")) {
      return MAIL_AUTH;
    }
  }
  
  String mailFrom = "MAIL FROM: <";
  if (from.length() > 0) {
    mailFrom += from + '>';
  } else {
    mailFrom += smtp_from + '>';
  }
  client->println(mailFrom);
  expect();

  String rcpt = "RCPT TO: <" + to + '>';
  client->println(rcpt);
  expect();

  client->println("DATA");
  if (!expect("354")) {
    return MAIL_DATA;
  }
  
  client->print("From: <");
  if (from.length() > 0) {
    client->println(from + '>');
  } else {
    client->println(smtp_from + '>');
  }
  client->println("To: <" + to + '>');
  if (subject.length() >0) {
    client->print("Subject: ");
    client->println(subject);
  }
  
  client->println("Mime-Version: 1.0");
  client->println("Content-Type: text/html; charset=\"UTF-8\"");
  client->println("Content-Transfer-Encoding: 8bit");
  client->println();
  String body = "<!DOCTYPE html><html lang=\"ru\">" + message + "</html>";
  client->println(message);
  client->println(".");
  if (!expect("250")) {
    return MAIL_SEND_ERROR;
  }
  client->println("QUIT");
  if (!expect("221")) {
    return MAIL_DISCONNECT;
  }
  return MAIL_OK;
}

bool mail::expect(const String responce, uint16_t timeout) {
  uint32_t ts = millis();
  while (!client->available()) {
    if (millis() > (ts + timeout)) {
      return false;
    }
  }
  String resp = client->readStringUntil('\n');
  if (responce && resp.indexOf(responce) == -1)
    return false;
  return true;
}

#define BUFF_SIZE 128

bool notifier::init(void) {
  warn_notify_sent = urgent_notify_sent = data_sent = 0;

  JsonStreamingParser parser;
  parser.setListener(this);
  
  byte buff[BUFF_SIZE];
  fs::File cf = SPIFFS.open(cf_name, "r");
  if (!cf) {
    calculateNextEvents();
    return false;
  }

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

  calculateNextEvents();
  return true;
}

bool notifier::save(void) {
  fs::File cf = SPIFFS.open(cf_name, "w");
  if (!cf) return false;

  cf.print("{\n  \"warn\": \"");
  cf.print(warn_notify_sent, DEC);
  cf.println("\",");
  cf.print("  \"urgent\": \"");
  cf.print(urgent_notify_sent, DEC);
  cf.println("\",");
  cf.print("  \"data\": \"");
  cf.print(data_sent, DEC);
  cf.println("\"\n}");
  cf.close();
  return true;
}

void notifier::calculateNextEvents() {
  time_t wp = cfg.warnMaintPeriod();
  time_t up = cfg.urgentMaintPeriod();
  time_t n  = now();
  next_warn_notify = next_urgent_notify = 0;
  next_data_send = cfg.nextTimeDataSend(data_sent);
  if (next_data_send <= n + 30)
    next_data_send = n + 300;                  // Send the data in five minutes, make sure to get ready after power on

  byte wm_num = cfg.wmCount();
  if (wm_num > 0) {
    WMuData *wud = cfg.getWMuData();
    for (byte i = 0; i < wm_num; ++i) {
      for (byte j = 0; j < 2; ++j) {
        time_t maint = wud[i].wm_maintenance[j];
        time_t warn = 0;
        if (maint > wp)
          warn = maint - wp;
        time_t urgent = 0;
        if (maint > up)
          urgent = maint - up;

        if (warn > warn_notify_sent) {          // The warning notofocation for this WM was not sent yet!
          if (next_warn_notify == 0) {          // The time was not specified
            next_warn_notify = warn;
          } else {
            if (warn < next_warn_notify)
              next_warn_notify = warn;
          }
        }

        if (urgent > urgent_notify_sent) {
          if (next_urgent_notify == 0) {        // The time was not specified
            next_urgent_notify = urgent;
          } else {
            if (urgent < next_urgent_notify)
              next_urgent_notify = urgent;
          }
        }

      }                                         // j loop
    }                                           // i loop
  }
}

void notifier::send(void) {
  if (cfg.wmCount() == 0) return;               // Do not notify because there is not WM in the config
  time_t n = now();
  String message = "";
  String subject = "";
  time_t *ts = 0;
  byte id[MAX_WM];
  byte wm_num = pool.idList(id);
  if (wm_num == 0) return;                      // No info received from the water controller
  bool msg_ready = false;

  if (n >= next_data_send) {
    subject = "WM status";
    message = "Current data of Water Meters:\n";
    for (byte i = 0; i < wm_num; ++i) {
      if (pool.battery(id[i]) > 0) msg_ready = true;
      message += cfg.location(id[i]);
      message += ". hot water: ";
      message += pool.valueS(id[i], true);
      message += ", cold water: ";
      message += pool.valueS(id[i], false);
      message += ", battery voltage: ";
      message += pool.batteryS(id[i]);
      message += ".\n";
    }
    next_data_send = n + resend_period;
    ts = &data_sent;
  } else
  if (n >= next_urgent_notify) {
    subject = "WM urgent!";
    message = "You must inspect the following water meters:\n";
    for (byte i = 0; i < wm_num; ++i) {
      if (cfg.urgentMaintenance(id[i], true)) {
        message += cfg.location(id[i]);
        message += ", hot water.\n";
        msg_ready = true;
      }
      if (cfg.urgentMaintenance(id[i], false)) {
        message += cfg.location(id[i]);
        message += ", cold water.\n";
        msg_ready = true;
      }
    }
    next_urgent_notify = n + resend_period;
    ts = &urgent_notify_sent;
    msg_ready = true;
  } else
  if (n >= next_warn_notify) {
    subject = "WM warning!";
    message = "Inspection time is near:\n";
    for (byte i = 0; i < wm_num; ++i) {
      if (cfg.warningMaintenance(id[i], true)) {
        message += cfg.location(id[i]);
        message += ", hot water.\n";
        msg_ready = true;
      }
      if (cfg.warningMaintenance(id[i], false)) {
        message += cfg.location(id[i]);
        message += ", cold water.\n";
        msg_ready = true;
      }
    }
    ts = &warn_notify_sent;
    next_warn_notify = n + resend_period;
  }

  if (msg_ready && message.length() > 0) {
    String    sn    = cfg.smtpRelayHost();
    uint16_t  p     = cfg.smtpRelayPort();
    bool      ssl   = cfg.smtpRelaySSL();
    String    from  = cfg.smtpRelayFrom();
    String    user  = cfg.smtpAuthUser();
    String    pass  = cfg.smtpAuthPass();
    String    to    = cfg.smtpEmailTo();
    mail *e_mail = new mail;
    e_mail->server(sn, p, ssl);
    e_mail->auth(user, pass);
    mail::ANSWER ans = e_mail->send(to, message, subject, from);
    if (ans == mail::MAIL_OK) {
      *ts = n;                                  // Set timestamp of the sent message
      save();
      calculateNextEvents();
    }
    delete e_mail;
  }
}

void notifier::value(String value) {
  if (currentKey == "warn") {
    warn_notify_sent = value.toInt();
  } else
  if (currentKey == "urgent") {
    urgent_notify_sent = value.toInt();
  } else
  if (currentKey == "data") {
    data_sent = value.toInt();
  }
}
