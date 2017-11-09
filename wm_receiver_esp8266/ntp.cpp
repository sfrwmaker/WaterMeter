#include <Time.h>
#include <TimeLib.h>
#include "ntp.h"

//------------------------------------------ NTP clock ---------------------------------------------------------
time_t ntpClock::ntpTime(void) {
  IPAddress timeServerIP;
  const char *c = ntp_server_name.c_str();
  WiFi.hostByName(c, timeServerIP); 
  sendNTPpacket(timeServerIP);
  delay(1000);
  int cb = udp.parsePacket();
  if (cb) {
    udp.read(packetBuffer, NTP_PACKET_SIZE);    // read the packet into the buffer
    // the timestamp starts at byte 40 of the received packet and is four bytes, or two words, long. First, esxtract the two words:
    uint32_t highWord = word(packetBuffer[40], packetBuffer[41]);
    uint32_t lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer this is NTP time (seconds since Jan 1 1900):
    uint32_t secsSince1900 = highWord << 16 | lowWord;
    // now convert NTP time into everyday time: Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    time_t epoch = secsSince1900 - 2208988800UL;
    epoch += long(tz) * 60;
    setTime(epoch);
    return epoch; 
  }
  return 0;
}

String ntpClock::ntpTimeS(time_t ts) {
  if (ts == 0) ts = ntpTime();
  char buff[30];
  sprintf(buff, "%2d:%02d %02d-%02d-%4d", hour(ts), minute(ts), day(ts), month(ts), year(ts));
  return String(buff);
}

void ntpClock::sendNTPpacket(IPAddress& address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;                 // LI, Version, Mode
  packetBuffer[1] = 0;                          // Stratum, or type of clock
  packetBuffer[2] = 6;                          // Polling Interval
  packetBuffer[3] = 0xEC;                       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123);                // NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void ntpClock::srvSet(String ntp_server) {
  if (ntp_server.length() > 32) {
    ntp_server_name = ntp_server.substring(0, 32);
  } else {
    ntp_server_name = ntp_server;
  }
}

void ntpClock::syncTime(bool force) {
  if (force || now() > nxt_sync) {
    time_t t = ntpTime();
    setTime(t);
    nxt_sync = t + sync_period;
  }
}
