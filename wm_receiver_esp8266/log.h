#ifndef WM_log_h
#define WM_log_h

/*
 * Logging the Water Meter counters data using json syntax in the following form:
 * { "ID": "<Controller ID>", "ts": "<unixtime>", "cold": "<cold counter data>", "hot": "<hot counter data>" }
 *
 */
 
#include <TimeLib.h>
#include <Wire.h>
#include <JsonListener.h>
#include <JsonStreamingParser.h>
#include "config.h"

struct wm_log {
  time_t    ts;
  uint32_t  cold;
  uint32_t  hot;
  byte      ID;
};

//------------------------------------------ water meter controller log data -----------------------------------
class wmlog : public JsonListener {
  public:
    wmlog()                                     { }
    void      loadLog(byte *wm_list, byte num);
    bool      data(byte ID, uint32_t& cold, uint32_t& hot, time_t& ts);
    void      log(byte ID, uint32_t cold, uint32_t hot);
    void      removeOldLog(time_t ts);
    virtual   void key(String key)              { currentKey = String(key); }
    virtual   void endObject()                  { }
    virtual   void startObject()                { }
    virtual   void whitespace(char c)           { }
    virtual   void startDocument()              { }
    virtual   void endArray()                   { }
    virtual   void endDocument()                { end_record = true; }
    virtual   void startArray()                 { }
    virtual   void value(String value);    

  private: 
    time_t  nextLogTime(time_t ts)              { return ts - (ts % period) + period; }
    String  logName(bool previous = false);
    void    resetData(void);
    byte    index(byte ID);
    byte    num_wm;
    struct  wm_log    wm_data[MAX_WM];
    time_t  next[MAX_WM];
    String  currentKey;
    byte    currentID;
    bool    end_record;
    const   int tail_bytes = 600;               // Maximum number of bytes to be loaded from the tail of the log
    const   time_t period = 86400;              // Period data log, seconds
    const   uint16_t matters = 10;              // Minimal data change for logging
};

#endif
