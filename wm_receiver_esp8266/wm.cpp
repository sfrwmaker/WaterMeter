#include "wm.h"

//------------------------------------------ water meter data --------------------------------------------------
void WM::init(void) {
  ID = 0;
  updated = 0;
  for (byte i = 0; i < 2; ++i) {
    wm_data[i]          = 0;
    wm_shift[i]         = 0;
    ts_data_changed[i]  = 0;
  }
}

void WM::WMinit(byte wm_ID, long cold_shift, long hot_shift) {
  ID = wm_ID;
  wm_shift[WM_COLD] = cold_shift;
  wm_shift[WM_HOT]  = hot_shift;
  updated = 0;
}

long WM::data(bool hot) {
  long d = wm_data[byte(hot)] + wm_shift[byte(hot)];
  if (d < 0) return 0;
  return d;
}

void WM::setValue(bool hot, long d, time_t ts) {
  if (wm_data[byte(hot)] && (wm_data[byte(hot)] != d)) {
    ts_data_changed[byte(hot)] = now();
  }
  wm_data[byte(hot)] = d;
  if (ts) {
    updated = ts;
  } else {
    updated = now();
  }
}

void WM::setAbsValue(bool hot, long d) {
  wm_shift[byte(hot)] = d - wm_data[byte(hot)];
}

//------------------------------------------ water meter pool --------------------------------------------------
void WMpool::WMinit(byte ID, long cold_shift, long hot_shift) {
  byte indx = index(ID);
  wm[indx].WMinit(ID, cold_shift, hot_shift);
}

void WMpool::update(struct data &wmd, time_t ts) {
  byte indx = index(wmd.ID);
  if (indx >= MAX_WM) return;
  wm[indx].setBattery(wmd.batt_mv);
  for (byte i = 0; i < 2; ++i)
    wm[indx].setValue(i, wmd.wm_data[i], ts);
}

byte WMpool::numWM(void) {
  byte n = 0;
  for (byte i = 0; i < MAX_WM; ++i) {
    if (wm[i].id()) ++n;
  }
  return n;
}

byte WMpool::idList(byte list[MAX_WM]) {
  byte n = 0;
  for (byte i = 0; i < MAX_WM; ++i) {
    if (wm[i].id()) {
      list[n] = wm[i].id();
      ++n;
    }
  }
  return n;
}

byte WMpool::index(byte ID) {
  for (byte i = 0; i < MAX_WM; ++i) {
    if (wm[i].id() == ID) return i;
  }
  for (byte i = 0; i < MAX_WM; ++i) {
    if (wm[i].id() == 0) {
      wm[i].setID(ID);
      return i;
    }
  }
  return MAX_WM;
}

long WMpool::value(byte ID, bool hot) {
  byte indx = index(ID);
  if (indx < MAX_WM)
    return wm[indx].data(hot);
  return 0;
}

String WMpool::valueS(byte ID, bool hot) {
  if (battery(ID) > 0) {
    long v = value(ID, hot);
    char buff[10];
    byte i = 8;
    for ( ; i > 0; --i) {                       // Write the long value from right to left, last <frac_size> digits are decimal fraction
      if (i == 8 - frac_size) {                 // Where the decimal point should be placed
        buff[i] = '.';                          // Place the decimal point
        --i;
      }
      buff[i] = char(v % 10 + '0');
      v /= 10;
      if (v == 0 && (i < 8-frac_size)) break;   // Surely stop on integer part, not decimal fraction
    }
    buff[9] = '\0';
    return String(&buff[i]);
  } else {
    return String("???");
  }
}

long WMpool::shift(byte ID, bool hot) {
  byte indx = index(ID);
  if (indx < MAX_WM)
    return wm[indx].shift(hot);
  return 0;
}

time_t WMpool::ts(byte ID) {
  byte indx = index(ID);
  if (indx < MAX_WM)
    return wm[indx].ts();
  return 0;
}

time_t WMpool::tsDataChanged(byte ID, bool hot) {
  byte indx = index(ID);
  if (indx < MAX_WM)
    return wm[indx].tsDataChanged(hot);
  return 0;
}

uint16_t WMpool::battery(byte ID) {
  byte indx = index(ID);
  if (indx < MAX_WM)
    return wm[indx].battery();
  return 0;
}

String WMpool::batteryS(byte ID) {
  uint16_t mv = battery(ID);
  if (mv > 0) {
    char buff[10];
    sprintf(buff, "%1d.%03d В", mv / 1000, mv % 1000);
    return String(buff);
  }
  return String("???");
}

void WMpool::setAbsValue(byte ID, bool hot, long value) {
  byte indx = index(ID);
  if (indx < MAX_WM)
    wm[indx].setAbsValue(hot, value);
}

void WMpool::setAbsValueS(byte ID, bool hot, String value) {
  byte indx = index(ID);
  if (indx < MAX_WM) {
    long v = 0;
    bool fr = false;
    byte frac_read = 0;
    for (byte i = 0; i < value.length(); ++i) {
      if (value.charAt(i) == '.' || value.charAt(i) == ',') {
        fr = true;
        ++i;
      }
      if (frac_read < frac_size) {
        byte dgt = uint16_t(value.charAt(i)) - '0';
        v *= 10;
        v += dgt;
        if (fr) ++frac_read;
      }
    }
    for (byte i = frac_read; i < frac_size; ++i) {
      v *= 10; 
    }
    wm[indx].setAbsValue(hot, v);
  }
}
