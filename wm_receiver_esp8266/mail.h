#ifndef WM_mail_h
#define WM_mail_h

#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include "config.h"
#include "wm.h"

//------------------------------------------ base64 decoder & encoder ------------------------------------------
class base64 {
  public:
    String  encode(String source);
    String  decode(String source);
  private:
    int     base64Pos(byte sym);
    bool    isBase64(byte c);
    const   char   base64_chars[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
};

class mailValidate {
  public:
    mailValidate()                              { }
    String    validateFQDN(String& fqdn);
    String    validateEmail(String& address);
  private:
    const     char valid_fqdn[2]       = {'.', '-'};
    const     char valid_email_user[3] = {'!', '_', '.'};
};

//------------------------------------------ water meter e-mail client -----------------------------------------
class mail : public base64 {
  public:
    typedef   enum {
      MAIL_OK = 0, MAIL_SERVER, MAIL_CONNECT, MAIL_NO_ANSWER, MAIL_IDENT,
      MAIL_AUTH, MAIL_DATA, MAIL_SEND_ERROR, MAIL_DISCONNECT
    } ANSWER;

    mail() : base64()                           { smtp_port = 25; smtp_server = auth_user = auth_pass = smtp_from = ""; client = 0; secure = false; }
    ~mail()                                     { if (client) delete client; }
    void      server(const String& srv, uint16_t port = 25, bool ssl = false);
    void      from(const String& f)             { smtp_from = f; }
    void      auth(const String& login, const String& password)
                                                { auth_user = login; auth_pass = password; }
    ANSWER    send(const String& to, const String& message, const String& subject = "", const String& from = "");
  private:
    bool      expect(const String responce = "", uint16_t timeout = 10000);
    bool      secure;
    String    smtp_server;
    uint16_t  smtp_port;
    String    auth_user;
    String    auth_pass;
    String    smtp_from;
    WiFiClient  *client;
};

class notifier : public JsonListener {
  public:
    notifier(const char *cf = "/notify.json") : JsonListener() { cf_name = cf; }
    bool      init(void);
    void      send(void);
    void      testLetter(void)                  { next_data_send = now() + 60; }
    virtual   void key(String key)              { currentKey = String(key); }
    virtual   void endObject()                  { }
    virtual   void startObject()                { }
    virtual   void whitespace(char c)           { }
    virtual   void startDocument()              { }
    virtual   void endArray()                   { }
    virtual   void endDocument()                { }
    virtual   void startArray()                 { }
    virtual   void value(String value);
  private:
    void      calculateNextEvents(void);        // Calculate the timestapps of the next events
    bool      save(void);                       // Update the configuration file
    String    cf_name;                          // The configuration file name
    String    currentKey;                       // Internal variables for json parser
    time_t    warn_notify_sent;                 // The time when the warning about water counter maintenance was sent
    time_t    urgent_notify_sent;               // The time when the alert   about water counter maintenance was sent
    time_t    data_sent;                        // The time when the current status of the mater meters was sent
    time_t    next_data_send;                   // The time whan to send water mater data
    time_t    next_warn_notify;                 // When to send next warning
    time_t    next_urgent_notify;               // When to send next alert
    WMconfig  *pCfg;
    const     time_t resend_period = 600;       // The period to resend the letter in case of failure
};

#endif
