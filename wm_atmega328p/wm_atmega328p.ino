/*
 * Water Meter local controller for two water meters: for cold and hot water.
 * Build on altmega328p-pu microcontroller running on the interhal 8MHz oscillator,
 * powered by two AAA batteries.
 * 
 * si4432     atmega328p-pu
 * VCC        VCC
 * SDO        D12     (MISO)
 * SDI        D11     (MOSI)
 * SCLK       D13     (SCK)
 * nSEL       D10     (SS)
 * nIrq       D2      (irq0)
 * snd        GND
 * GND        GND
 */
 
#include <avr/sleep.h>
#include <avr/power.h>
#include <SPI.h>
#include <RH_RF22.h>
#include <EEPROM.h>

const byte     wm_pin[2] = {3, 4};                    // Water meter PIN number for cold and hot water meters
const byte     led_pin   = 6;                         // The check led lits with the transmition
const byte     reset_pin = A1;                        // The reset EEPROM pin
const int      transmit_period = 40;                  // The period in 1.0-sec. interval to transmit the data
const byte     my_ID = 3;                             // ID of the WaterMeter checker

//------------------------------------------ water meter data in EEPROM ----------------------------------------
/* Config record in the EEPROM has the following format:
  uint32_t ID                           each time increment by 1
  struct cfg                            config data
  byte CRC                              the checksum
*/
struct cfg {
  uint32_t wm_data[2];                                // Water meter data count for cold(0) and hot(1) water
};

class CONFIG {
  public:
    CONFIG() {
      can_write     = false;
      buffRecords   = 0;
      rAddr = wAddr = 0;
      eLength       = 0;
      nextRecID     = 0;
      byte rs = sizeof(struct cfg) + 5;               // The total config record size
      // Select appropriate record size; The record size should be power of 2, i.e. 8, 16, 32, 64, ... bytes
      for (record_size = 8; record_size < rs; record_size <<= 1);
    }
    void init();
    bool load(void);
    bool save(void);                                  // Save current config copy to the EEPROM
    void erase();

  protected:
    struct   cfg Config;

  private:
    bool     readRecord(uint16_t addr, uint32_t &recID);
    bool     can_write;                               // The flag indicates that data can be saved
    byte     buffRecords;                             // Number of the records in the outpt buffer
    uint16_t rAddr;                                   // Address of thecorrect record in EEPROM to be read
    uint16_t wAddr;                                   // Address in the EEPROM to start write new record
    uint16_t eLength;                                 // Length of the EEPROM, depends on arduino model
    uint32_t nextRecID;                               // next record ID
    byte     record_size;                             // The size of one record in bytes
};

 // Read the records until the last one, point wAddr (write address) after the last record
void CONFIG::init(void) {
  eLength = EEPROM.length();
  uint32_t recID;
  uint32_t minRecID = 0xffffffff;
  uint16_t minRecAddr = 0;
  uint32_t maxRecID = 0;
  uint16_t maxRecAddr = 0;
  byte records = 0;

  nextRecID = 0;

  // read all the records in the EEPROM find min and max record ID
  for (uint16_t addr = 0; addr < eLength; addr += record_size) {
    if (readRecord(addr, recID)) {
      ++records;
      if (minRecID > recID) {
        minRecID = recID;
        minRecAddr = addr;
      }
      if (maxRecID < recID) {
        maxRecID = recID;
        maxRecAddr = addr;
      }
    } else {
      break;
    }
  }

  if (records == 0) {
    wAddr = rAddr = 0;
    can_write = true;
    return;
  }

  rAddr = maxRecAddr;
  if (records < (eLength / record_size)) {            // The EEPROM is not full
    wAddr = rAddr + record_size;
    if (wAddr > eLength) wAddr = 0;
  } else {
    wAddr = minRecAddr;
  }
  can_write = true;
}

bool CONFIG::save(void) {
  if (!can_write) return can_write;
  if (nextRecID == 0) nextRecID = 1;

  uint16_t startWrite = wAddr;
  uint32_t nxt = nextRecID;
  byte summ = 0;
  for (byte i = 0; i < 4; ++i) {
    EEPROM.write(startWrite++, nxt & 0xff);
    summ <<=2; summ += nxt;
    nxt >>= 8;
  }
  byte* p = (byte *)&Config;
  for (byte i = 0; i < sizeof(struct cfg); ++i) {
    summ <<= 2; summ += p[i];
    EEPROM.write(startWrite++, p[i]);
  }
  summ ++;                                            // To avoid empty records
  EEPROM.write(wAddr+record_size-1, summ);

  rAddr = wAddr;
  wAddr += record_size;
  if (wAddr > EEPROM.length()) wAddr = 0;
  nextRecID ++;                                       // Get ready to write next record
  return true;
}

bool CONFIG::load(void) {
  bool is_valid = readRecord(rAddr, nextRecID);
  nextRecID ++;
  return is_valid;
}

bool CONFIG::readRecord(uint16_t addr, uint32_t &recID) {
  byte Buff[record_size];

  for (byte i = 0; i < record_size; ++i) 
    Buff[i] = EEPROM.read(addr+i);
  
  byte summ = 0;
  for (byte i = 0; i < sizeof(struct cfg) + 4; ++i) {

    summ <<= 2; summ += Buff[i];
  }
  summ ++;                                            // To avoid empty fields
  if (summ == Buff[record_size-1]) {                  // Checksumm is correct
    uint32_t ts = 0;
    for (char i = 3; i >= 0; --i) {
      ts <<= 8;
      ts |= Buff[byte(i)];
    }
    recID = ts;
    memcpy(&Config, &Buff[4], sizeof(struct cfg));
    return true;
  }
  return false;
}

void CONFIG::erase(void) {
  for (int i = 0; i < eLength; ++i)
    EEPROM.write(i, 0);
}

//------------------------------------------ class water meter data --------------------------------------------
class WM_DATA : public CONFIG {
  public:
    WM_DATA()                                   { }
    void     init(void);
    uint32_t data(byte index);                  // Water meter data for cold (0) or hot (1) water
    void     increment(byte index);             // Increment water meter data for cold (0) or hot (1) water by 1
    void     reset(void);
  private:
    void     setDefaults(void);                 // Set default parameter values if failed to load data from EEPROM
};

void WM_DATA::init(void) {
  CONFIG::init();
  if (!CONFIG::load()) setDefaults();           // If failed to load the data from EEPROM, initialize the config data with default values
}

uint32_t WM_DATA::data(byte index) {
  if (index <= 1) return Config.wm_data[index];
  return 0;
}

void WM_DATA::increment(byte index) {
  if (index <= 1) {
    ++Config.wm_data[index];
  }
}

void WM_DATA::setDefaults(void) {
  Config.wm_data[0] = 0;
  Config.wm_data[1] = 0;
}

void WM_DATA::reset(void) {
  CONFIG::erase();
  init();
}
//==============================================================================================================

RH_RF22       radio;
WM_DATA       wm_data;                          // The water meter counters are saved in the EEPROM
volatile int  f_wdt = 1;                        // The flag of WDT interrupt
bool          wm_status[2];                     // The water meter status (LOW, HIGH)

struct data {                                   // The data to be transmitted to the WaterMeter Node
  uint32_t wm_data[2]; 
  uint16_t batt_mv;
  byte     ID;
};
const byte pl_size = sizeof(struct data);       // The packet size

// Read the Vcc in millivolts by using internal voltage regulator of 1.1 volts
uint32_t readVcc() {
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2);                             // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);                  // Start conversion
  while (bit_is_set(ADCSRA,ADSC));      // measuring
  uint8_t low  = ADCL;                  // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH;                  // unlocks both
  long result = (high<<8) | low;

  result = 1125300L / result;           // Calculate Vcc (in mV); 112530 = 1.1*1023*1000
  return result;
}

// Enter into the power sleep mode. Wakes UP by WDT
void enterSleep(void) { 
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);                // the lowest power consumption
  sleep_enable();
  sleep_cpu();                                       // Now enter sleep mode.
  
  // The program will continue from here after the key pressed or WDT interrupt
  sleep_disable();                                    // First thing to do is disable sleep.
  power_all_enable();                                 // Re-enable the peripherals.
}

void setup() {
  pinMode(reset_pin, INPUT_PULLUP);
  for (byte i = 0; i < 2; ++i) {
    pinMode(wm_pin[i], INPUT);                        // Use external resister to pull-up the pin 
    delay(100);
    wm_status[i] = (digitalRead(wm_pin[i]) == LOW);
  }
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);
  wm_data.init();                                     // Load the water meter data from the EEPROM
  if (radio.init()) {                                 // Defaults after init are 434.0MHz, 0.05MHz AFC pull-in, modulation FSK_Rb2_4Fd36
    radio.setPromiscuous(true);                       // wish to receive messages with any TO address
    radio.setFrequency(433.0, 0.5);
    radio.setModemConfig(RH_RF22::GFSK_Rb2Fd5);       ///< GFSK, No Manchester, Rb = 2kbs,    Fd = 5kHz
    radio.setTxPower(RH_RF22_TXPOW_5DBM);
  }

  for (byte i = 0; i < 10; ++i) {                     // % times blink
    digitalWrite(led_pin, i & 1);
    delay(1000);
  }
  // Setup the WDT
  MCUSR &= ~(1<<WDRF);                                // Clear the reset flag
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  WDTCSR  = (1<<WDP2) | (1<<WDP1);                    // Set new watchdog timeout prescaler value 1.0 seconds
  WDTCSR |= (1<<WDIE);                                // Enable the WD interrupt (note no reset).
}

void checkWaterMeters(void) {
  bool modified = false;
  for (byte i = 0; i < 2; ++i) {
    bool s = (digitalRead(wm_pin[i]) == LOW);         // Read status of the water meter
    if (s && (s != wm_status[i])) {                   // The water meter have been just activated (0 in the last digit)
      delay(50);                                      // The timeout to debounce
      s = (digitalRead(wm_pin[i]) == LOW);
      if (s) {
        wm_data.increment(i);                         // Increment the count data on the water meter
        modified = true;
      }
    }
    wm_status[i] = s;                                 // Update the sensor status
  }
  if (modified) wm_data.save();                       // Save the updated data to the EEPROM
}

void sendWaterMeterData(void) {                       // Check the battery till it is not depleted
  digitalWrite(led_pin, HIGH);
  delay(10);
  uint16_t v = readVcc();
  struct data send_data;
  for (byte i = 0; i < 2; ++i)
    send_data.wm_data[i] = wm_data.data(i);           // Read the current water meter counter from the EEPROM
  send_data.batt_mv = v;
  send_data.ID = my_ID;
  digitalWrite(led_pin, LOW);
  delay(10);
  radio.send((const byte*)&send_data, pl_size);
  radio.waitPacketSent();
  delay(10);
  radio.sleep();
}

void loop() {
  static int transmit_count = 0;
  if (f_wdt == 1) {
    f_wdt = 0;
    if (digitalRead(reset_pin) == LOW) {              // Reset button pressed, erase the EEPROM
      for (byte i = 0; i < 5; ++i) {
        digitalWrite(led_pin, HIGH);
        delay(100);
        digitalWrite(led_pin, LOW);
        delay(100);
      }
      wm_data.reset();
    }
    checkWaterMeters();
    if (--transmit_count <= 0) {
      transmit_count = transmit_period + my_ID;
      sendWaterMeterData();
    }
    enterSleep();
  }
}

ISR(WDT_vect) {
  if (f_wdt == 0) f_wdt = 1;
}

