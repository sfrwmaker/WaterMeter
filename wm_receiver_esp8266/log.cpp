#define FS_NO_GLOBALS
#include <FS.h>
#include "log.h"

#define BUFF_SIZE 128

void wmlog::loadLog(byte *wm_list, byte num) {
  resetData();
  num_wm = num;
  for (byte i = 0; i < num; ++i) {
    wm_data[i]. ID = wm_list[i];
  }

  currentKey = ""; currentID  = 0;
  end_record = false;
  JsonStreamingParser parser;
  parser.setListener(this);
  
  byte buff[BUFF_SIZE];

  String log_file = logName();
  fs::File wml = SPIFFS.open(log_file, "r");
  if (!wml) {
    log_file = logName(true);                   // Try to load log file for the last month
    wml = SPIFFS.open(log_file, "r");
    if (!wml)
      return;
  }
  long last = wml.size();
  if (last > tail_bytes) {                      // Read tail of the long log
    wml.seek(last-tail_bytes, fs::SeekSet);
    last = tail_bytes;
  }
  
  bool isBody = false;                          // Looking for the first '{' to start parsing
  while(last > 0) {
    int rb = last;
    if (rb > BUFF_SIZE) rb = BUFF_SIZE;
    rb = wml.read(buff, rb);
    if (!rb) {
      wml.close();
      return;
    }
    last -= rb;
    for (byte i = 0; i < rb; ++i) {
      char c = buff[i];
      if (!isBody && (c == '{')) {
        isBody = true;
      }
      if (isBody) {
        if (end_record) {                       // This variable is set in endDocument() callback
          parser.reset();                       // Start reading new record
          end_record = false;
        }
        parser.parse(c);
      }
    }
  }
  wml.close();

  for (byte i = 0; i < num_wm; ++i) {           // Calculate next log time
    next[i] = nextLogTime(wm_data[i].ts);
  }
}

bool wmlog::data(byte ID, uint32_t& cold, uint32_t& hot, time_t& ts) {
  byte i = index(ID);
  if ((i < MAX_WM) && (wm_data[i].ts)) {
    cold  = wm_data[i].cold;
    hot   = wm_data[i].hot;
    ts    = wm_data[i].ts;
    return true;
  }
  return false;
}

void wmlog::log(byte ID, uint32_t cold, uint32_t hot) {
  byte indx = index(ID);
  if (indx >= MAX_WM) return;
  time_t n = now();
  bool do_write = false;                        // Write new log entry only if some counter has been changed
  if (n >= next[indx]) {                        // It is time to write the log
    do_write = true;
  }
  if (abs(cold - wm_data[indx].cold) >= matters) {
    wm_data[indx].cold = cold;
    do_write = true;
  }
  if (abs(hot - wm_data[indx].hot) >= matters)  {
    wm_data[indx].hot  = hot;
    do_write = true;
  }
  if (do_write) {
    wm_data[indx].ts   = n;
    next[indx]         = nextLogTime(n);
    String log_file = logName(); 
    fs::File wml = SPIFFS.open(log_file, "a");
    if (!wml) return;

    wml.print("{ \"ID\": \""); wml.print(ID, DEC); wml.print("\", \"ts\": \"");
    wml.print(n, DEC); wml.print("\", \"cold\": \""); wml.print(cold, DEC);
    wml.print("\", \"hot\": \""); wml.print(hot, DEC);
    wml.print("\" }\n");
    wml.close();
  }
}

void wmlog::removeOldLog(time_t ts) {
  fs::Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fn = dir.fileName();
    if (fn.indexOf("/wmlog") == 0) {          // Found the log file
      tmElements_t tm;
      String y = fn.substring(7, 11);
      String m = fn.substring(12, 14);
      uint16_t iy = y.toInt();
      byte     im = m.toInt();
      if (iy > 1970 && im > 0) {
        tm.Year   = iy - 1970;
        tm.Month  = im;
        tm.Day    = 15;
        tm.Hour   = 12;
        tm.Minute = 0;
        tm.Second = 0;
        time_t file_ts = makeTime(tm);
        if (file_ts < ts) {                   // This file should be deleted
          SPIFFS.remove(fn);
        }
      }
    }
  }
}

void wmlog::resetData(void) {
  for (byte i = 0; i < MAX_WM; ++i) {
    wm_data[i].ts     = 0;
    wm_data[i].cold   = 0;
    wm_data[i].hot    = 0;
    wm_data[i].ID     = 0;
    next[i]           = 0;
  }
}

byte wmlog::index(byte ID) {
  for (byte i = 0; i < MAX_WM; ++i) {
    if (wm_data[i].ID == ID) return i;
  }
  return MAX_WM;
}

String wmlog::logName(bool previous) {
  time_t n = now();
  if (previous)
    n -= 60 * 60 * 3600 * 31;                   // 31 days ago, last month
  String y = String(year(n));
  String m = String(month(n));
  return "/wmlog_" + y + "-" + m + ".log";
}

void wmlog::value(String value) {
  if (currentKey == "ID") {
    currentID = value.toInt();
  } else
  if (currentKey == "ts") {
    byte indx = index(currentID);
    if (indx < MAX_WM) {
      wm_data[indx].ts = value.toInt();
      time_t ts = wm_data[indx].ts;
    } 
  } else
  if (currentKey == "cold") {
    byte indx = index(currentID);
    if (indx < MAX_WM) {
      wm_data[indx].cold = value.toInt();
    }
  } else
  if (currentKey == "hot") {
    byte indx = index(currentID);
    if (indx < MAX_WM) {
      wm_data[indx].hot = value.toInt();
    }
  }
}

