#ifndef _ESP_WM_H
#define _ESP_WM_H

#include <Time.h>
#include <TimeLib.h>
#include "config.h"

//------------------------------------------ water meter data --------------------------------------------------
class WM {
  public:
    WM()                                        { }
    void      init(void);
    void      WMinit(byte wm_ID, long cold_shift, long hot_shift);
    byte      id(void)                          { return ID; }
    long      data(bool hot);
    long      shift(bool hot)                   { return wm_shift[byte(hot)]; }
    uint16_t  battery(void)                     { return batt_mv; }
    time_t    ts(void)                          { return updated; }
    time_t    tsDataChanged(bool hot)           { return ts_data_changed[byte(hot)]; }
    void      setID(byte id)                    { ID = id; }
    void      setValue(bool hot, long d, time_t ts = 0);
    void      setAbsValue(bool hot, long d);
    void      setBattery(uint16_t mv)           { batt_mv = mv; updated = now(); }
  private:
    uint16_t  batt_mv;                          // The battery moltage, mV
    byte      ID;                               // WM controller ID, must be > 0
    long      wm_data[2];                       // Cold and Hot water'ticks' from the last reset
    long      wm_shift[2];                      // Cold and Hot water shift to ubtain the absolute value
    time_t    ts_data_changed[2];               // Time the data for cold and hot water was changed
    time_t    updated;                          // Time the last data received from the WM controller
};

//------------------------------------------ water meter pool --------------------------------------------------
class WMpool {
  public:
    WMpool()                                      { }   
    void     init(void)                           { for (byte i = 0; i < MAX_WM; ++i) wm[i].init(); frac_size = 2; }
    void     update(struct data &wmd, time_t ts = 0);
    void     WMinit(byte ID, long cold_shift, long hot_shift);
    byte     numWM(void);
    byte     idList(byte list[MAX_WM]);           // Number of water meters registered in the pool. Modifies lsit with its IDs
    byte     id(byte index)                       { if (index < MAX_WM) return wm[index].id(); return 0; }
    long     value(byte ID, bool hot);
    String   valueS(byte ID, bool hot);
    long     shift(byte ID, bool hot);
    time_t   ts(byte ID);
    time_t   tsDataChanged(byte ID, bool hot);
    uint16_t battery(byte ID);
    String   batteryS(byte ID);
    void     setAbsValue(byte ID, bool hot, long value);
    void     setAbsValueS(byte ID, bool hot, String value);
    void     setFractionDigits(byte f)            { frac_size = f; }
  private:
    byte     index(byte ID);
    WM       wm[MAX_WM];
    byte     frac_size;                           // The float fraction size (decimal digits)
};

#endif
