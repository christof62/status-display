#ifndef _BUTTON_H_
#define _BUTTON_H_

#include "Arduino.h"

class CTouch
{
public:
  CTouch (uint8_t pin, uint8_t threshold, void (*stateChangeCB)(uint8_t, bool));
  void debounce();  
  void setStateChangeCB (void (*function)(uint8_t pin, bool state));
  void inject();
  ~CTouch();

protected:

private:
  void setState(boolean state);
  uint8_t _pin;
  uint8_t _threshold;
  uint32_t _lastTick;
  bool _state;
  void (*_stateChangeCB)(uint8_t, bool) = NULL;
};

#endif
