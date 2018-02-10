/**
 * @file BLE_Display4in2.ino
 * @author Christof Menzenbach
 * @date 9 Feb 2018
 * @brief 4.2" e-ink display with softkey HMI
 *
 * - Initialization
 * - Handling of deep sleep and wakeup every minute for time update
 */

#include <Arduino.h>
#include <TimeLib.h>
#include <time.h>
#include <sys/time.h>
#include <soc/rtc.h>
#include <esp_deep_sleep.h>
#include <Fsm.h>
#include "btcom.h"
#include "configuration.h"
#include "hmi.h"
#include "touch.h"
#include "main.h"

#define uS_TO_S_FACTOR 1000000    
RTC_DATA_ATTR int bootCount = 0;

uint32_t startTime;
uint32_t lastInteractionTime;
esp_sleep_wakeup_cause_t wakeupReason;
uint16_t sleepTimeout = 13000;

CTouch touchL(T6, BUTTON_L_TH, &handleTouch);
CTouch touchLM(T7, BUTTON_LM_TH, &handleTouch);
CTouch touchRM(T9, BUTTON_RM_TH, &handleTouch);
CTouch touchR(T8, BUTTON_R_TH, &handleTouch);


void setup(){
  startTime = millis();
  lastInteractionTime = startTime;
  Serial.begin(115200);

  struct timeval tv;
  gettimeofday(&tv, NULL);
  setTime(tv.tv_sec);

  // Prepare touch inputs for wakeup
  touch_pad_t touchPin = esp_sleep_get_touchpad_wakeup_status();
  touchAttachInterrupt(T6, nullptr, BUTTON_L_TH);
  touchAttachInterrupt(T7, nullptr, BUTTON_LM_TH);
  touchAttachInterrupt(T9, nullptr, BUTTON_RM_TH);
  touchAttachInterrupt(T8, nullptr, BUTTON_R_TH);
    
  displayInit (bootCount == 0);
  wakeupReason = esp_sleep_get_wakeup_cause();
  ++bootCount;
  screenManager.triggerEvent(Event::SCREEN_ENTRY); // Event handler after wakeup, this screen is invisble
  
  if (((bootCount % commIntervalls[hour()]) == 0) || (year() < 2016) || wakeupReason == ESP_DEEP_SLEEP_WAKEUP_TOUCHPAD){ 
    if(wakeupReason == ESP_DEEP_SLEEP_WAKEUP_TOUCHPAD){
      sleepTimeout = 2*60*1000;
      // get GPOI which caused wakeup and stimulate event loop
      switch(touchPin)
      {
        case 6: touchL.inject(); break;
        case 7: touchLM.inject(); break;
        case 8: touchR.inject(); break;
        case 9: touchRM.inject(); break;
      }
    } else{
      screenManager.triggerEvent(Event::SCREEN_MAIN);
    }
    BLEscan();
  } else {
    screenManager.triggerEvent(Event::SCREEN_MAIN);  
    sleep();
  }   
}

void loop() {
  vTaskDelay(50);
  if ((millis()-lastInteractionTime) > sleepTimeout){
    sleep(); // just to make shure to sleep if something wents wrong
  }
  touchL.debounce();
  touchLM.debounce();
  touchRM.debounce();
  touchR.debounce();    
}

void handleTouch (uint8_t touch, boolean state) {
  Serial.print("Touch: ");
  Serial.println (touch);
  if (state == true){
    Event event;
    switch (touch){
      case BUTTON_L: event = Event::KEY_0; break;
      case BUTTON_LM: event = Event::KEY_1; break;
      case BUTTON_RM: event = Event::KEY_2; break;
      case BUTTON_R: event = Event::KEY_3; break;                        
    }
    screenManager.triggerEvent(event);
  }
}

/*! \fn void sleep()
 *  \brief enter deep sleep until new minute starts or button pressed
 */
void sleep(){
    Serial.print("Going to sleep after ");
    Serial.println((uint32_t)(millis()-startTime));
    displayOff();
    esp_deep_sleep_enable_touchpad_wakeup();
    uint8_t sleepTime = 60-second();
    esp_sleep_enable_timer_wakeup(sleepTime * uS_TO_S_FACTOR);
    esp_deep_sleep_start();  
}





