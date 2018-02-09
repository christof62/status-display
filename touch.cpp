#include "touch.h"

/**
 * @brief Constructor for touch input object.
 * 
 * @param pin PIN of touch input.
 * @param threshold Touchthreshold.
 * @param stateChangeCB Callback to notify about pressed or released touch button.
 */
CTouch::CTouch (uint8_t pin, uint8_t threshold, void (*stateChangeCB)(uint8_t, bool)){
  _state = false;
  _pin = pin;
  _threshold = threshold;
  _lastTick = millis();
  _stateChangeCB = stateChangeCB; 
}

CTouch::~CTouch(){

}

/**
 * @brief Set the state of the touch button and notify registered callback if state is changed.
 * 
 * @param state State of the touch input.
 */
void CTouch::setState(boolean state){
  if (_state != state){
    if (_stateChangeCB != NULL){
      _stateChangeCB(_pin, state);
    } 
    _state = state;
  }
  _lastTick = millis();  
}

/**
 * @brief Trigger touch pressed event.
 * 
 */
void CTouch::inject(){
  setState(true);
}

/**
 * @brief Debounce the touch input and generate a notification call if state has changed. Shall be called frequently to avoid delays.
 * 
 */
void CTouch::debounce (){
  bool currentState = touchRead(_pin) < _threshold;
  if (currentState == true){
    setState(true);
  } else {
    if ((millis()-_lastTick) > 30){
      setState (false);
    }
  }
}

/**
 * @brief Set the callback function to be called if state change of the touch input is recognized.
 * 
 * @param stateChangeCB Callback function for notification of state change.
 */
void CTouch::setStateChangeCB (void (*stateChangeCB)(uint8_t, bool)){
  _stateChangeCB = stateChangeCB;  
}
