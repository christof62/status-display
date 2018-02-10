/**
 * @file btcom.h
 * @author Christof Menzenbach
 * @date 9 Feb 2018
 * @brief BLE communication with home environment service.
 *
 * - Time syncronization 
 * - Livingroom temperature and humidity
 * - Outdoor temperature and humidity
 * - Window opening status
 * - Next garbage collection
 * - Bus time schedule
 */
 
#ifndef _BTCOM_H_
#define _BTCOM_H_

#include <BLEDevice.h>

enum GarbageType {ORGANIC, RESIDUAL, PAPER, PLASTIC, UNDEFINED};
struct Garbage {
  enum GarbageType type;
  uint8_t days;
};


struct Garbage getNextGarbageCollection();
uint16_t* getBusTimeTable();
float getTemperature();
uint8_t getHumidity();
float getOutdoorTemperature();
uint8_t getOutdoorHumidity();
uint8_t* getWindows();
void writePartyMode(uint8_t hour, uint8_t minute);
bool partyModeWritten();
void writeHomeMode(boolean home);
bool homeModeWritten();
void writeAudioMode(boolean home);
bool audioModeWritten();
void BLEscan(void);
void connect();

#endif
