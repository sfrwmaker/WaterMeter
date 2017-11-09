#ifndef _ESP_WM_NTP
#define _ESP_WM_NTP

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Time.h>
#include <TimeLib.h>

//------------------------------------------ NTP clock ---------------------------------------------------------
const int NTP_PACKET_SIZE = 48;                 // NTP time stamp is in the first 48 bytes of the message
class ntpClock {
  public:
    ntpClock()                                  { }
    void    syncTime(bool force = false);       // Synchronize the local RTC with NTP source
    void    init(String sn, int tz_mins)        { udp.begin(ntp_port); tz = tz_mins; ntp_server_name = sn; nxt_sync = 0; }
    time_t  ntpTime(void);
    String  ntpTimeS(time_t ts = 0);
    int     tzMinutes(void)                     { return tz; }
    String  ntpServerName(void)                 { return ntp_server_name; }
    void    srvSet(String ntp_server);
    void    tzSet(int TZ)                       { tz = TZ; }
  private:
    void    sendNTPpacket(IPAddress& address);
    uint8_t packetBuffer[NTP_PACKET_SIZE];
    String  ntp_server_name;
    int     tz;                                 // Time zone difference in minutes
    WiFiUDP udp;                                // A UDP instance to let us send and receive packets over UDP
    time_t  nxt_sync;
    const time_t   sync_period = 1800;          // Synchronization local clock period, seconds
    const uint16_t ntp_port = 2390;             // local port to listen for UDP packets
};

#endif
