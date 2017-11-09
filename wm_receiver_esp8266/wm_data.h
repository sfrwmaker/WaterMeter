#ifndef _ESP_WM_DATA_H
#define _ESP_WM_DATA_H

/* 
 *  The remote sensor uses this packet structure to send the water meter information to the central controller
 */
struct data {                                   // The data to be transmitted to the WaterMeter Node
  uint32_t wm_data[2];
  uint16_t batt_mv;
  byte     ID;
};
const byte pl_size    = sizeof(struct data);    // The packet size

#endif
