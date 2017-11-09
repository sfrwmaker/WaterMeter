#ifndef WM_config_h
#define WM_config_h

#include <JsonListener.h>
#include <JsonStreamingParser.h>
#include "wm_data.h"

#define WM_max_layers 4
#define MAX_WM  4                               // The maximum number of the water meter controllers
#define WM_COLD 0
#define WM_HOT  1

//------------------------------------------ water meter configutation data ------------------------------------
class WMuData {
  public:
    WMuData()                                   { }
    byte     ID;
    String   wm_location;
    String   wm_serial[2];
    time_t   wm_maintenance[2];
    long     wm_shift[2];
};

//------------------------------------------ water meter configutation parameters ------------------------------
class WMconfig : public JsonListener {
  public:
    WMconfig(const char *cf = "/config.json") : JsonListener() { cf_name = cf; }
    bool init(void);
    bool save(void);
    virtual   void key(String key)              { currentKey = String(key); }
    virtual   void endObject();
    virtual   void startObject();
    virtual   void whitespace(char c)           { }
    virtual   void startDocument()              { }
    virtual   void endArray();
    virtual   void endDocument()                { }
    virtual   void startArray();
    virtual   void value(String value);
    String    ssid(void)                        { return w_ssid; }
    String    passwd(void)                      { return w_password; }
    String    ntp(void)                         { return t_server; }
    int       tz(void)                          { return t_shift; }
    String    nodeName(void)                    { return w_nodename; }
    String    auth(void)                        { return b_auth; }
    void      setNodeName(String n)             { w_nodename = n; }
    void      setWifi(String s, String p)       { w_ssid = s; w_password = p; }
    void      setNTP(String sn, int tz)         { t_server = sn; t_shift = tz; }
    void      setAuth(String auth)              { b_auth = auth; }
    byte      frac(void)                        { return frac_size; }
    void      setFrac(byte f)                   { frac_size = f; }
    byte      wmCount(void);
    WMuData*  getWMuData(void)                  { return wm_data; }
    void      updateWM(byte ID, long cold_shift, long hot_shift);
    void      removeWM(byte ID);                // Remove water meter controller data from the config
    void      setLocation(byte ID, String location);
    void      updateWMserial(byte ID, bool hot, String sn, time_t nxt = 0);
    String    location(byte ID);                // The water meter location by its ID
    String    serial(byte ID, bool hot);        // The serial number of WM
    String    smtpRelayHost(void)               { return smtp_relay_host; }
    uint16_t  smtpRelayPort(void)               { return smtp_relay_port; }
    bool      smtpRelaySSL(void)                { return smtp_relay_ssl; }
    String    smtpRelayFrom(void)               { return smtp_relay_from; }
    String    smtpAuthUser(void)                { return smtp_relay_user; }
    String    smtpAuthPass(void)                { return smtp_relay_pass; }
    String    smtpEmailTo(void)                 { return smtp_email_to; }
    time_t    nextMaintenance(byte ID, bool hot);
    bool      warningMaintenance(byte ID, bool hot);
    bool      urgentMaintenance(byte ID, bool hot);
    time_t    warnMaintPeriod(void)             { return maintenance_warning; }
    time_t    urgentMaintPeriod(void)           { return maintenance_urgent; }
    String    warnMaintDays(void);
    String    urgentMaintDays(void);
    byte      dataSendPeriod(void)              { return smtp_period; }
    byte      numDataSendPeriods(void)          { return 4; }
    String    dataSendPeriodName(byte i)        { if (i < 4) return valid_period[i][0]; return "-"; }
    String    dataSendPeriodLabel(byte i)       { if (i < 4) return valid_period[i][1]; return "-"; }
    String    dataSendAt(void);
    time_t    nextTimeDataSend(time_t at = 0);  // Next time to send status of the water meters
    void      setSmtpRelayHost(String& host, uint16_t port, bool ssl = false);
    void      setSmtpAuthUser(String& user, String& pass);
    void      setSmtpRelayFrom(String& from)    { smtp_relay_from = from; }
    void      setSmtpEmailTo(String& to)        { smtp_email_to = to; }
    void      setMaintenanceDays(time_t urgent, time_t warn);
    void      setDataSendPeriod(String& period, String& at);
  private:
    byte      wm_index(byte ID);
    void      resetWM(byte index);
    String    cf_name;                          // config file name
    byte      index;                            // Index of the current water meter controller read from the config
    byte      curr_ID;                          // ID of the current WM controller
    byte      frac_size;                        // Decimal fraction size (number of digits after cubic meters)
    String    currentKey;                       // Internal variables for json parser
    String    currentParent;
    String    currentArray;
    String    p_key[WM_max_layers];
    String    w_nodename;                       // WiFi nodename to connect to
    String    w_ssid;                           // WiFi ssid
    String    w_password;                       // WiFi password
    String    t_server;                         // NTP server for synchronization
    int       t_shift;                          // Time zone shift in minutes
    String    b_auth;                           // Blynk authentication key
    String    smtp_relay_host;                  // Parameters for sending e-mail notofications
    uint16_t  smtp_relay_port;
    bool      smtp_relay_ssl;
    String    smtp_relay_from;
    String    smtp_relay_user;
    String    smtp_relay_pass;
    String    smtp_email_to;                    // Destination e-mail address
    time_t    maintenance_warning;              // Time threshold for warning message about the water counter maintenance
    time_t    maintenance_urgent;               // Time threshold for urgent message about the water meter maintenance
    byte      smtp_period;                      // Period to send the counter data: 0 not send, 1 - monthly, 2 - weekly, 3 - daily
    byte      smtp_send_at;
    WMuData   wm_data[MAX_WM];
    bool      wm_set[MAX_WM];
    const     String valid_period[4][2] = {
                {"never",   "never"},
                {"monthly", "monthly"},
                {"weekly",  "weekly"},
                {"daily",   "daily"}
    };
    const     String week_days[7] = {"mo", "tu", "we", "th", "fr", "sa", "su"};
};

#endif
