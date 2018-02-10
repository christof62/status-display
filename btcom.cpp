/**
 * @file btcom.cpp
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
 
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <TimeLib.h>
#include <sys/time.h>
#include <Fsm.h>
#include <FreeRTOS.h>
#include "btcom.h"
#include "hmi.h"

#define BASE_UUID "-0000-1000-8000-00805f9b34fb"

// Remote service
static BLEUUID homeEnvServiceUUID("00000a00" BASE_UUID);
// Characteristics of the remote service
static BLEUUID curTimeUUID("00002a2b" BASE_UUID);       // Provides current time according to GATT
static BLEUUID temperatureUUID("00002a1f" BASE_UUID);   // Temperature
static BLEUUID humidityUUID("00002a6f" BASE_UUID);      // Humidity
static BLEUUID outdoorTemperatureUUID("00003a1f" BASE_UUID);   // Outdoor Temperature
static BLEUUID outdoorHumidityUUID("00003a6f" BASE_UUID);      // Humidity
static BLEUUID partyModeUUID("0000d379" BASE_UUID);     // Party mode prolongs heating
static BLEUUID presenceUUID("0000d380" BASE_UUID);      // Home presence
static BLEUUID windowUUID("0000d390" BASE_UUID);        // Window state
static BLEUUID audioUUID("0000d3A0" BASE_UUID);         // Audio on//off
static BLEUUID busUUID("0000d3B0" BASE_UUID);           // Bus timetable
static BLEUUID garbageUUID("0000d392" BASE_UUID);       // Next garbage collection

static BLEAddress *pServerAddress;
static BLERemoteService* pRemoteService;
static BLERemoteCharacteristic* pDateTimeCharacteristic;
static BLERemoteCharacteristic* pPartyModeCharacteristic;
static BLERemoteCharacteristic* pPresenceCharacteristic;
static BLERemoteCharacteristic* pTemperatureCharacteristic;
static BLERemoteCharacteristic* pHumidityCharacteristic;
static BLERemoteCharacteristic* pOutdoorTemperatureCharacteristic;
static BLERemoteCharacteristic* pOutdoorHumidityCharacteristic;
static BLERemoteCharacteristic* pWindowCharacteristic;
static BLERemoteCharacteristic* pAudioCharacteristic;
static BLERemoteCharacteristic* pBusCharacteristic;
static BLERemoteCharacteristic* pGarbageCharacteristic;
static BLEAdvertisedDevice bleDevice;
static uint16_t scanStartTime;

// Party mode characteristic
#define PM_INVALID 99
RTC_DATA_ATTR uint8_t pmHour = PM_INVALID;
RTC_DATA_ATTR uint8_t pmMinute = 0;
RTC_DATA_ATTR float temperature;
RTC_DATA_ATTR uint8_t humidity;
RTC_DATA_ATTR float outdoorTemperature;
RTC_DATA_ATTR uint8_t outdoorHumidity;
RTC_DATA_ATTR uint8_t windows[10];
RTC_DATA_ATTR uint16_t busTimeTable[9];
RTC_DATA_ATTR struct Garbage nextGarbageCollection = {GarbageType::UNDEFINED, 255};

/**
 * @brief get charcteristic with given UUID
 * 
 * @param uuid 
 * @return BLERemoteCharacteristic* 
 */
BLERemoteCharacteristic*  getCharacteristic(BLEUUID uuid){
  BLERemoteCharacteristic* pCharacteristic = pRemoteService->getCharacteristic(uuid);
  if (pCharacteristic == nullptr) {
    Serial.print("Failed to find characteristic UUID: ");
    Serial.println(uuid.toString().c_str());
  }
  return pCharacteristic;
}

// Presence characteristic
boolean homeModeSent = true;
boolean atHome = false;

// Audio characteristic
boolean audioModeSent = true;
boolean audioOn = false;

/**
 * @brief Get next garbage collection
 * 
 * @return struct Garbage 
 */
struct Garbage getNextGarbageCollection(){
  return nextGarbageCollection;
}

/**
 * @brief Get temperature
 * 
 * @return float 
 */
float getTemperature(){
  return temperature;
}

/**
 * @brief Get humidity
 * 
 * @return uint8_t 
 */
uint8_t getHumidity(){
  return humidity;
}

/**
 * @brief Get outdor temperature
 * 
 * @return float 
 */
float getOutdoorTemperature(){
  return outdoorTemperature;
}

/**
 * @brief Get outdoor humidity
 * 
 * @return uint8_t 
 */
uint8_t getOutdoorHumidity(){
  return outdoorHumidity;
}

/**
 * @brief Get array with window status
 * 
 * @return uint8_t* 
 */
uint8_t* getWindows(){
  return windows;
}

/**
 * @brief Get array with timetable of next busses
 * 
 * @return uint16_t* 
 */
uint16_t* getBusTimeTable(){
  return busTimeTable;
}


/**
 * @brief Write the new time to end the party mode as BLE value
 * 
 * @param hour Hour when party mode should be ended
 * @param minute Minute when party mode should be ended
 */
void writePartyMode(uint8_t hour, uint8_t minute){
  if (pRemoteService != nullptr){
    pPartyModeCharacteristic = getCharacteristic(partyModeUUID);
    if (pPartyModeCharacteristic != nullptr){
      char buffer[6];
      snprintf(buffer, 6, "%02d:%02d", hour, minute);
      pPartyModeCharacteristic->writeValue(buffer, true);
      pmHour = PM_INVALID;
      Serial.println ("PM-Value written");
      screenManager.triggerEvent(Event::DATA_SENT);
    }
  } else {
    pmHour = hour;
    pmMinute = minute;
  }
}


/**
 * @brief Status of BLE communication regarding successfully written party mode end time
 * 
 * @return true Party mode end time successfully written
 * @return false Party mode end time not written
 */
bool partyModeWritten(){
  return pmHour == PM_INVALID;
}


/**
 * @brief Set home mode as home or absent as BLE value 
 * 
 * @param home True if at home
 */
void writeHomeMode(boolean home){
  if (pRemoteService != nullptr){
    pPresenceCharacteristic = getCharacteristic(presenceUUID);
    if (pPresenceCharacteristic != nullptr){
      pPresenceCharacteristic->writeValue(home?"home":"absent", true);
      Serial.println ("Presence-Value written");
      homeModeSent = true; 
      screenManager.triggerEvent(Event::DATA_SENT);
    }
  } else {
    atHome = home;
    homeModeSent = false; 
  }
}


/**
 * @brief Status of BLE communication regarding successfully written home mode
 * 
 * @return true Home mode value successfully written via BLE
 * @return false Home mode value not written via BLE
 */
bool homeModeWritten(){
  return homeModeSent;
}


/**
 * @brief Set amplifier on or off as BLE value 
 * 
 * @param on 
 */
void writeAudioMode(boolean on){
  if (pRemoteService != nullptr){
    pAudioCharacteristic = getCharacteristic(audioUUID);
    if (pAudioCharacteristic != nullptr){
      pAudioCharacteristic->writeValue(on?"on":"off", true);
      Serial.println ("Audio-Value written");
      audioModeSent = true; 
      screenManager.triggerEvent(Event::DATA_SENT);
    }
  } else {
    audioOn = on;
    audioModeSent = false; 
  }
}

/**
 * @brief Status of BLE communication regarding successfully written audio mode
 * 
 * @return true Audio mode value successfully written via BLE
 * @return false Audio mode value not written via BLE
 */
bool audioModeWritten(){
  return audioModeSent;
}

/**
 * @brief Build a connection to the BLE server
 * 
 * @param pAddress MAC address of server
 * @return true Connection successful
 * @return false Connection failed
 */
bool connectToServer(BLEAddress pAddress) {
  Serial.print("Connecting to ");
  Serial.println(pAddress.toString().c_str());

  BLEClient*  pClient  = BLEDevice::createClient();
  Serial.print(" - Client created ");
  Serial.println(millis() - scanStartTime);

  // Connect to the remove BLE Server.
  if (pClient->connect(pAddress)){
    Serial.print(" - Connected to server: ");
    Serial.println(millis() - scanStartTime);
  } else {
    return false;
  }

  // Obtain a reference to the service we are after in the remote BLE server.
  pRemoteService = pClient->getService(homeEnvServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.println(" - Failed to find service UUID: ");
    return false;
  }
  Serial.print(" - Found service ");
  Serial.println(millis() - scanStartTime);

  pDateTimeCharacteristic = getCharacteristic(curTimeUUID);
  if (pDateTimeCharacteristic != nullptr){
    std::string value = pDateTimeCharacteristic->readValue();
    uint16_t yr = (uint8_t)value[0] + 256*(uint8_t)value[1];
    uint8_t mth = (uint8_t)value[2];
    uint8_t d = (uint8_t)value[3];
    uint8_t h = (uint8_t)value[4];
    uint8_t m = (uint8_t)value[5];
    uint8_t s = (uint8_t)value[6];
    setTime(h, m, s, d, mth, yr);
    // use ESP 32 RTC which continues during deep sleep
    struct timeval tv; 
    tv.tv_sec = now();
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    screenManager.triggerEvent(Event::TIME_UPDATE);
  }

  if (pmHour <= 24){ // value was set
    writePartyMode(pmHour, pmMinute);
  }

  if(!homeModeSent){
    writeHomeMode(atHome);
  }

  if(!audioModeSent){
    writeAudioMode(audioOn);
  }
  
  pTemperatureCharacteristic = getCharacteristic(temperatureUUID);
  if (pTemperatureCharacteristic != nullptr){
    std::string value = pTemperatureCharacteristic->readValue();
    int16_t temp = value[0] | value[1]<<8;
    temperature = (float)temp/10;
    screenManager.triggerEvent(Event::TEMPERATURE);
  }
  
  pHumidityCharacteristic = getCharacteristic(humidityUUID);
  if (pHumidityCharacteristic != nullptr){
    std::string value = pHumidityCharacteristic->readValue();
    humidity = (value[0] + (value[1]<<8))/100;
    screenManager.triggerEvent(Event::HUMIDITY);
  }

  pOutdoorTemperatureCharacteristic = getCharacteristic(outdoorTemperatureUUID);
  if (pOutdoorTemperatureCharacteristic != nullptr){
    std::string value = pOutdoorTemperatureCharacteristic->readValue();
    int16_t temp = value[0] | value[1]<<8;
    outdoorTemperature = (float)temp/10;
  }

  pOutdoorHumidityCharacteristic = getCharacteristic(outdoorHumidityUUID);
  if (pOutdoorHumidityCharacteristic != nullptr){
    std::string value = pOutdoorHumidityCharacteristic->readValue();
    outdoorHumidity = (value[0] + (value[1]<<8))/100;
  }  
  
  pWindowCharacteristic = getCharacteristic(windowUUID);
  if (pWindowCharacteristic != nullptr){
    std::string value = pWindowCharacteristic->readValue();
    memcpy (windows, value.c_str(), 10);
    screenManager.triggerEvent(Event::WINDOW);
  }

  if (hour() == 0 || nextGarbageCollection.days == 255){        // sync @ midnight or if not synced before
    pGarbageCharacteristic = getCharacteristic(garbageUUID);
    if (pGarbageCharacteristic != nullptr){
      std::string value = pGarbageCharacteristic->readValue();
      uint8_t *buffer = (uint8_t*)value.c_str();
      nextGarbageCollection.type = (enum GarbageType)buffer[0];
      nextGarbageCollection.days = buffer[1];
    }
  }

  pBusCharacteristic = getCharacteristic(busUUID);
  if (pBusCharacteristic != nullptr){
    std::string value = pBusCharacteristic->readValue();
    memcpy(busTimeTable, value.c_str(), 12);
  }  

  Serial.print(" - Data received ");
  Serial.println(millis() - scanStartTime);  

  return true;
}

/**
 * @brief Task to perform connection
 * 
 * @param parameter Not used
 */
void connect(void * parameter){
  if (connectToServer(*pServerAddress)) {
    Serial.println("Connected to BLE Server.");
    screenManager.triggerEvent(Event::CONNECTION_FINISHED);
  } else {
    Serial.println("Failed to connect to the server.");
    screenManager.triggerEvent(Event::CONNECTION_FAILED);
  }
  vTaskDelete(nullptr);
}


/**
 * @brief Scan for BLE servers and find the first one that advertises the service we are looking for.
 * 
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
   /**
    * @brief Called for each advertising BLE server
    * 
    * @param advertisedDevice 
    */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(homeEnvServiceUUID)) {
      Serial.print("Found device ");
      Serial.println(millis() - scanStartTime);
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      xTaskCreate(connect, "connect", 4096, nullptr, 0, nullptr);
    } // Found server
  }   // onResult
};    // MyAdvertisedDeviceCallbacks

/**
 * @brief Task to perform BLE scan
 * 
 * @param parameter Not used
 */
void scan(void * parameter){
  BLEDevice::init(""); 
  Serial.println("Enter scan"); 
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false);
  scanStartTime = millis();
  pBLEScan->start(2);
  vTaskDelete(nullptr);
}


/**
 * @brief Start BLE scan
 * 
 */
void BLEscan () {
  xTaskCreate(scan, "scan", 2048, nullptr, 0, nullptr);
}



