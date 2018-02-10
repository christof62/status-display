/**
 * @file hmi.cpp
 * @author Christof Menzenbach
 * @date 9 Feb 2018
 * @brief 4.2" Softkey based HMI of status display.
 *
 * A central ScreenManager manages different screens with event handlers.
 */

#include <TimeLib.h>
#include <epd4in2.h>
#include <Adafruit_GFX.h>
#include <FreeRTOS.h>
#include <freertos/timers.h>

#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "hmi.h"
#include "btcom.h"
#include "configuration.h"
#include "icons.h"
#include "main.h"

#define EPD_WHITE 0
#define EPD_BLACK 1
#define DISPLAY_WIDTH 400
#define DISPLAY_HEIGHT 300

Epd epd;
Epd_GFX gfx (DISPLAY_WIDTH, DISPLAY_HEIGHT);
boolean firstBoot;

/**
 * @brief Timeout function for user interaction. Will be called if no user interaction during specified time.
 * 
 * @param xTimer Not used.
 */
void userTimeout(TimerHandle_t xTimer){
  screenManager.triggerEvent(Event::USER_TIMEOUT);
}
TimerHandle_t inUseTimer = xTimerCreate("in use", 5000, pdFALSE,( void * ) 0, userTimeout);

/**
 * @brief Timeout to enter deep sleep.
 * 
 * @param xTimer Not used.
 */
void offTimeout(TimerHandle_t xTimer){
  sleep();
}
TimerHandle_t offTimer = xTimerCreate("switch to sleep", 2000, pdFALSE,( void * ) 0, offTimeout);


/**
 * @brief Timeout to start redraw of current screen.
 * 
 * @param xTimer Not used.
 */
void redrawTimeout(TimerHandle_t xTimer){
  screenManager.triggerEvent(Event::REDRAW);
}
TimerHandle_t redrawTimer = xTimerCreate("redraw", 500, pdFALSE,( void * ) 0, redrawTimeout);

/**
 * @brief Class representing behavior of entry screen.
 * 
 */
class EntryScreen: public Screen {
  public:
    EntryScreen():Screen(){
      addSoftkey (0, Event::SCREEN_HEATING, heatingOn32);
      addSoftkey (1, Event::SCREEN_LIGHT, bulbOn32);
      addSoftkey (2, Event::SCREEN_AUDIO, audio32);
      addSoftkey (3, Event::SCREEN_ABSENT, absentHome32);
    }

    /**
     * @brief Get name of screen.
     * 
     * @return char* Name of screen.
     */
    char* getName(){
      return "Entry";
    }
    
    /**
     * @brief Activation of entry screen. Method activate from super class is not called to avoid drawing of this invisible screen.
     * 
     */
    void activate(){
      
    }
    
    /**
     * @brief Eventhandler of entry screen. This screen does not draw the display content and handles events after wakeup from deep sleep.
     * 
     * @param event 
     */
    void triggerEvent(Event event){
      Screen::triggerEvent(event);
      switch (event) {
        case Event::KEY_0: 
          screenManager.triggerEvent(Event::SCREEN_HEATING);
        break;
        case Event::KEY_1: 
          screenManager.triggerEvent(Event::SCREEN_LIGHT);
        break;
        case Event::KEY_2: 
          screenManager.triggerEvent(Event::SCREEN_AUDIO);
        break;
        case Event::KEY_3: 
          screenManager.triggerEvent(Event::SCREEN_ABSENT);
        break;          
        case Event::CONNECTION_FINISHED:
          this->draw();
          xTimerStart (offTimer, 10);
        break;
        case Event::CONNECTION_FAILED:
          xTimerStart (offTimer, 10);
        break;      
      }
    }
};

EntryScreen entryScreen;


/**
 * @brief Main screen, which shows the status information and time.
 * 
 */
class MainScreen: public Screen {
  public:
    MainScreen():Screen(){
      addSoftkey (0, Event::SCREEN_HEATING, heatingOn32);
      addSoftkey (1, Event::SCREEN_LIGHT, bulbOn32);
      addSoftkey (2, Event::SCREEN_AUDIO, audio32);
      addSoftkey (3, Event::SCREEN_ABSENT, absentHome32);
    }

    /**
     * @brief Get name of main screen.
     * 
     * @return char* 
     */
    char* getName(){
      return "Main";
    }
    
    /**
     * @brief Deactivate main screen. Set timer to enter deep sleep after timeout.
     * 
     */
    void deactivate(){
      Screen::deactivate();
      xTimerStop (offTimer, 0);
    }
    
    /**
     * @brief Event handler of main screen.
     * 
     * @param event 
     */
    void triggerEvent(Event event){
      Screen::triggerEvent(event);
      switch (event) {
        case Event::KEY_0: 
          screenManager.triggerEvent(Event::SCREEN_HEATING);
        break;
        case Event::KEY_1: 
          screenManager.triggerEvent(Event::SCREEN_LIGHT);
        break;
        case Event::KEY_2: 
          screenManager.triggerEvent(Event::SCREEN_AUDIO);
        break;
        case Event::KEY_3: 
          screenManager.triggerEvent(Event::SCREEN_ABSENT);
        break;          
        case Event::TIME_UPDATE:
        break;
        case Event::CONNECTION_FINISHED:
          this->draw();
          xTimerStart (offTimer, 10);
        break;
        case Event::CONNECTION_FAILED:
          xTimerStart (offTimer, 10);
        break;      
      }
    }
  
    /**
     * @brief Draw headline of main screen.
     * 
     */
    void drawHeadline(){
      Screen::drawHeadline(); 
      char buffer[11];
      snprintf(buffer, 11, "%02d.%02d.%02d", day(), month(), year());
      gfx.setCursor(5, R1_Y-6);
      gfx.print(buffer);
      snprintf(buffer, 6, "%02d:%02d", hour(), minute());
      int16_t  x1, y1;
      uint16_t w, h;
      gfx.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
      gfx.setCursor(DISPLAY_WIDTH - 5 - w - x1, R1_Y-6);
      gfx.print(buffer);
    } 
  
    /**
     * @brief Draw content area of main screen.
     * 
     */
    void drawMain(){
      Screen::drawMain();
      char buffer[21];
      // temperature and humidity
      gfx.setFont(&FreeSans18pt7b);
      gfx.drawBitmap (5, R2_Y-40, tempIn32, 32, 32, EPD_BLACK);  
      gfx.setCursor(40, R2_Y-12);
      snprintf(buffer, 8, "%2.1f", getTemperature());
      gfx.print(buffer);
      int16_t  x1, y1;
      uint16_t w, h;
      gfx.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h); 
      gfx.drawBitmap (x1 + w + 42, R2_Y-37, degree13, 18, 18, EPD_BLACK);  
      snprintf(buffer, 8, "%2d%%", getHumidity());
      gfx.setCursor(58 + w + x1, R2_Y-12);  
      gfx.print(buffer);
      gfx.drawBitmap (200, R2_Y-40, tempOut32, 32, 32, EPD_BLACK);
      gfx.setCursor(235, R2_Y-12);
      snprintf(buffer, 8, "%2.1f", getOutdoorTemperature());
      gfx.print(buffer);
      gfx.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h); 
      gfx.drawBitmap (x1 + w + 237, R2_Y-38, degree13, 18, 18, EPD_BLACK);       
      snprintf(buffer, 8, "%2d%%", getOutdoorHumidity());
      gfx.setCursor(263 + w + x1, R2_Y-12);  
      gfx.print(buffer);
      
      // Bus timetable
      gfx.drawBitmap (5, R1_Y+15, bus64, 64, 64, EPD_BLACK);
      uint16_t* busTimeTable = getBusTimeTable();
      gfx.setFont(&FreeSans18pt7b);
      gfx.setCursor(75, R1_Y+42);
      snprintf(buffer, 20, "%02d:%02d - %02d:%02d  %d", busTimeTable[0]/60, busTimeTable[0]%60, busTimeTable[1]/60, busTimeTable[1]%60, busTimeTable[2]);
      gfx.print(buffer);      
      gfx.setCursor(75, R1_Y+75);
      snprintf(buffer, 20, "%02d:%02d - %02d:%02d  %d", busTimeTable[3]/60, busTimeTable[3]%60, busTimeTable[4]/60, busTimeTable[4]%60, busTimeTable[5]);
      gfx.print(buffer);        
           
      // window state
      gfx.setFont(&FreeSans12pt7b);
      boolean open = false;
      int i = 0;
      uint8_t* windows = getWindows();
      gfx.setFont(&FreeSans12pt7b);
      for (int room=(int)Room::LIVINGROOM; room<(int)Room::LAST; room++){
        uint8_t window = windows[(int)room]&0x03;
        if (window == 1 || window == 2){
          open = true;
          char* openRoom;
          switch (room){
            case (int)Room::DININGROOM: openRoom = "Esszimmer"; break;
            case (int)Room::BEDROOM: openRoom = "Schlafz."; break;
            case (int)Room::KITCHEN: openRoom = "Kueche"; break;
            case (int)Room::BATHROOM_GF: openRoom = "Bad"; break;                                    
          }
          if (i<3){
            gfx.setCursor(270, R1_Y+122+20*i);  
            gfx.print(openRoom);
            i++;
          }
        }
      }
      gfx.drawBitmap (200, 140, open?windowOpen64:windowClosed64, 64, 64, EPD_BLACK);
      gfx.setFont(&FreeSans18pt7b);
      if (!open){
            gfx.setCursor(275, R1_Y+147);  
            gfx.print("OK");
      } 

      // Garbage
      gfx.setFont(&FreeSans18pt7b);
      gfx.drawBitmap(10, 140, trash64, 46, 64, EPD_BLACK);
      struct Garbage nextCollection = getNextGarbageCollection();
      if (nextCollection.days != 255){
        gfx.setCursor(70, 165);
        char* typeNames[] = {"Braun", "Grau", "Blau", "Gelb", "---"};
        gfx.print(typeNames[nextCollection.type]);
        gfx.setCursor(70, 202);
        uint8_t days = nextCollection.days;
        snprintf(buffer, 15, "%d ", days);
        gfx.print(buffer);
        gfx.print(days == 1? "Tag":"Tage");
      }
    }
};

MainScreen mainScreen;

/**
 * @brief  Audio screen, which switches amplifier on or off.
 * 
 */
class AudioScreen: public Screen {
  public:
    AudioScreen():Screen(){
      addSoftkey (0, Event::OFF, off32);
      addSoftkey (1, Event::ON, on32);
      addSoftkey (3, Event::BACK, back32);
    }

    /**
     * @brief Get name of audio screen.
     * 
     * @return char* 
     */
    char* getName(){
      return "Audio";
    }
  
    /**
     * @brief Draw headline of audio screen.
     * 
     */
    void drawHeadline(){
      Screen::drawHeadline();
      gfx.print("Audio");
    }

    /**
     * @brief Draw status of BLE communication to switch amplifier on or off.
     * 
     */
    void drawMain(){
      Screen::drawMain();
      gfx.setFont(&FreeSansBold24pt7b);
      gfx.setCursor(17, R1_Y+85);    
      if (_communicating){
        gfx.print(audioModeWritten()?"OK":"Sende Daten");
      }
    }   
      
    /**
     * @brief Event handler of audio screen.
     * 
     * @param event 
     */
    void triggerEvent(Event event){
      Screen::triggerEvent(event);
      switch (event) {
        case Event::OFF: 
          writeAudioMode(false);
          _communicating = true;
          draw();
        break;
        case Event::ON:
          writeAudioMode(true);
          _communicating = true;
          draw();
        break;
        case Event::DATA_SENT:
          draw();          
          _communicating = false;       
        break;
      }
    }
  private:
    boolean _communicating = false;
    boolean _dataSent = false;    
};

AudioScreen audioScreen;

/**
 * @brief Heating screen, which is used to enter the end of the party mode.
 * 
 */
class HeatingScreen: public Screen {
  public:
    HeatingScreen():Screen(){
      addSoftkey (0, Event::MINUS, minus32);
      addSoftkey (1, Event::PLUS, plus32);
      addSoftkey (2, Event::CONFIRM, tick32);
      addSoftkey (3, Event::BACK, back32);
      if (hour() < 6){
        _partyUntil = (hour()+1)*10; // set to current hour + 1 if after midnight
      }
    }

    /**
     * @brief Get name of heating screen.
     * 
     * @return char* 
     */
    char* getName(){
      return "Heating";
    }    
  
    /**
     * @brief Draw headline of heating screen.
     * 
     */
    void drawHeadline(){
      Screen::drawHeadline(); 
      gfx.print("Heizung");
    } 
  
    /**
     * @brief Draw party mode end time end status of BLE communication after confirmation.
     * 
     */
    void drawMain(){
      Screen::drawMain();
      gfx.setFont(&FreeSansBold24pt7b);
      gfx.setCursor(17, R1_Y+85);    
      if (!_communicating){
        gfx.print("Heizen bis");
        gfx.setCursor(17, R1_Y+130);
        char buffer [10];
        sprintf(buffer, "%02d:%02d Uhr",_partyUntil/10, (_partyUntil%10)*6);
        gfx.print(buffer);
      } else {
        gfx.print(partyModeWritten()?"OK":"Sende Daten");
      }
    } 
  
    /**
     * @brief Event handler of heating screen.
     * 
     * @param event 
     */
    void triggerEvent(Event event){
      Screen::triggerEvent(event);
      switch (event) {
        case Event::MINUS: 
          if (_partyUntil > 0){
            _partyUntil -= 5;
            draw();
          }
        break;
        case Event::PLUS:
          if (_partyUntil < 60){
            _partyUntil += 5;
            draw();
          }
        break;
        case Event::CONFIRM:
          writePartyMode(_partyUntil/10, (_partyUntil%10)*6);
          _communicating = true;
          draw(2);
        break;
        case Event::DATA_SENT:
          draw();
        break;
      }
    }  
  private:
    uint8_t _partyUntil = 5;
    boolean _communicating = false;
};

HeatingScreen heatingScreen;

/**
 * @brief Heating screen, which is used to change between absent and home mode.
 * 
 */
class AbsentScreen: public Screen {
  public:
    AbsentScreen():Screen(){
      addSoftkey (0, Event::ABSENT, absent32);
      addSoftkey (1, Event::HOME, absentHome32);    
      addSoftkey (3, Event::BACK, back32);
    }

    /**
     * @brief Get name of absent screen.
     * 
     * @return char* 
     */
    char* getName(){
      return "Absent";
    }
  
    /**
     * @brief Draw headline of absent screen.
     * 
     */
    void drawHeadline(){
      Screen::drawHeadline();
      gfx.print("Zuhause");     
    } 

    /**
     * @brief Draw status of BLE communication to switch between home and absent mode.
     * 
     */
    void drawMain(){
      Screen::drawMain();
      gfx.setFont(&FreeSansBold24pt7b);
      gfx.setCursor(17, R1_Y+85);    
      if (_communicating){
        gfx.print(homeModeWritten()?"OK":"Sende Daten");
      }
    }   
    
    /**
     * @brief Eventhandler of absent screen.
     * 
     * @param event 
     */
    void triggerEvent(Event event){
      Screen::triggerEvent(event);
      switch (event) {
        case Event::ABSENT: 
          writeHomeMode(false);
          _communicating = true;
          draw();
        break;
        case Event::HOME:
          writeHomeMode(true);
          _communicating = true;
          draw();
        break;
        case Event::DATA_SENT:
          draw();          
          _communicating = false;
          
        break;
      }
    }
  private:
    boolean _communicating = false;
    boolean _dataSent = false;
};

AbsentScreen absentScreen;


/**
 * @brief Screen class implements common behavior regarding activation, event handling, headline, and softkey handling. 
 * 
 */
Screen::Screen(){
}

/**
 * @brief Add a softkey to a screen object.
 * 
 * @param index Position of softkey starting with 0 for left key.
 * @param event Event which is dispatched after softkey is pressed.
 * @param icon Icon to be shown as softkey.
 */
void Screen::addSoftkey (uint8_t index, Event event, const unsigned char* icon){
  _softkeys[index].tag = Softkey::SCREEN;
  _softkeys[index].icon = icon;
  _softkeys[index].event = event;
}

/**
 * @brief Get name of screen.
 * 
 * @return char* 
 */
char* Screen::getName(){
  return "undefined";
}

/**
 * @brief Default event handler. Translates generic key event in dedicated softkey event and handles screen redraw.
 * 
 * @param event 
 */
void Screen::triggerEvent(Event event){
  Serial.print("Trigger event: ");
  Serial.println((int)event);  
  // generate event according softkey
  if (event >= Event::KEY_0 && event <= Event::KEY_3){
    Event newEvent = _softkeys[(int)event].event;
    screenManager.triggerEvent(newEvent);
  }else if (event == Event::REDRAW){
    screenToDisplay();
  }
}

/**
 * @brief Activate screen. Draw screen 2 times due to behavior of e-inc display.
 * 
 */
void Screen::activate(){
  draw(2);
}

/**
 * @brief Deactivate screen.
 * 
 */
void Screen::deactivate(){
}

/**
 * @brief Draw screen.
 * 
 */
void Screen::draw(){
  Serial.println("Draw Screen");
  this->drawHeadline();
  drawMain();
  drawSoftkeys();
  screenToDisplay();
}

/**
 * @brief Draw screen multiple times to improve quality on e-inc display.
 * 
 * @param number
 */
void Screen::draw(uint8_t number){
  _drawCounter = number;
  draw();
}

/**
 * @brief Draw screen headline.
 * 
 */
void Screen::drawHeadline(){
  gfx.clear (0, R1_Y, EPD_BLACK);
  gfx.setTextColor(EPD_WHITE);
  gfx.setTextSize(1);
  gfx.setFont(&FreeSansBold18pt7b);
  gfx.setCursor(0, R1_Y-9);
}

/**
 * @brief Draw Main area.
 * 
 */
void Screen::drawMain(){
  gfx.clear (R1_Y+1, R2_Y, EPD_WHITE);
  gfx.setTextColor(EPD_BLACK);
  gfx.setTextSize(1);
  gfx.setFont(&FreeSans24pt7b);
}

/**
 * @brief Draw Softkeys.
 * 
 */
void Screen::drawSoftkeys(){
    gfx.clear (R2_Y+1, R3_Y, EPD_BLACK);
    for (int i=0; i<4; i++){
      if(_softkeys[i].icon != nullptr){
        drawSoftkey(i, _softkeys[i].icon);
      }
    }
}

/**
 * @brief Draw a single softkey.
 * 
 * @param pos Index of softkey starting with 0 for left softkey.
 * @param bmp Softkey icon.
 */
void Screen::drawSoftkey(uint8_t pos, const unsigned char* bmp){
  gfx.drawBitmap (30+103*pos, R2_Y+(R3_Y-R2_Y)/2-16, bmp, 32, 32, EPD_WHITE);
}

/**
 * @brief Send framebuffer to display device. Handle redraw if required.
 * 
 */
void Screen::screenToDisplay(){
  Serial.println("Screen to display");
  epd.WaitUntilIdle();
  if (firstBoot){
    epd.SetPartialWindow(gfx.getImage(), 0, 0, gfx.width(), R3_Y, 1);
    firstBoot = false;
  }
  epd.SetPartialWindow(gfx.getImage(), 0, 0, gfx.width(), R3_Y, 2);
  epd.DisplayFrameQuick();
  if (_drawCounter > 0){
    _drawCounter--;
    uint16_t delay = _drawCounter?800:200;
    xTimerStart (redrawTimer, delay);
  }  
}


/**
 * @brief Start execution of screen manager.
 * 
 * @param param Not used.
 */
void startExecute(void* param){
  screenManager.execute();
}

ScreenManager::ScreenManager(){
  _eventQueue = xQueueCreate(4, sizeof(Event));
  xTaskCreate(&startExecute, "eventloop", 2048, NULL, 2, NULL);
}

/**
 * @brief Enter event in event queue.
 * 
 * @param event 
 */
void ScreenManager::triggerEvent (Event event){
  xQueueSend(_eventQueue, &event, 200);
}
    
/**
 * @brief Event loop and event handler for screen request events.
 * 
 */
void ScreenManager::execute () {
  while (true){
    Event event; // out of Freertos queue
    if( xQueueReceive(_eventQueue, &event, (TickType_t) 100)){;
      switch (event) {
        case Event::SCREEN_ENTRY:
          requestScreen(&entryScreen);
        break;        
        case Event::SCREEN_MAIN:
          requestScreen(&mainScreen);
        break;
        case Event::SCREEN_AUDIO:
          requestScreen(&audioScreen);
        break;        
        case Event::SCREEN_HEATING:
          requestScreen(&heatingScreen);
        break;
        case Event::SCREEN_ABSENT:
          requestScreen(&absentScreen);
        break;
        case Event::BACK:
          requestScreen(&mainScreen);
          xTimerStart (offTimer, 2000);
        break;
        case Event::USER_TIMEOUT:
          requestScreen(&mainScreen);
          xTimerStart (offTimer, 2000);
        break;             
        default: 
          if (_activeScreen != nullptr){
            _activeScreen->triggerEvent(event);        
          }
      }
      // Handle user idle timeout on root level. Switch back to entry screen
      if (event >= Event::KEY_0 && event <= Event::KEY_3){
        xTimerStart(inUseTimer, 5000);
      }
    }  
  }
}

/**
 * @brief Request a screen to be activated.
 * 
 * @param screen 
 */
void ScreenManager::requestScreen (Screen *screen){
  Serial.print("Request screen ");
  Serial.println(screen->getName());
  if (_activeScreen != nullptr){
    _activeScreen->deactivate();
  }
  _activeScreen = screen;
  screen->activate();
}

ScreenManager screenManager;


/**
 * @brief Edp graphics based on Adafruit GFX.
 * 
 * @param w Display width.
 * @param h Display height.
 */
Epd_GFX::Epd_GFX (int16_t w, int16_t h) : Adafruit_GFX(w, h) {
  _framebuffer = (uint8_t *)malloc (w*h/8);
}

/**
 * @brief Draw pixel method as interface between Adafruit GFX and EDP display driver.
 * 
 * @param x 
 * @param y 
 * @param color 
 */
void Epd_GFX::drawPixel(int16_t x, int16_t y, uint16_t color){
  uint16_t byteIndex = (y*width() + x)/8;
  uint8_t bitOffset = (y*width() + x)%8;
  uint8_t pixels = _framebuffer[byteIndex];
  if (color == EPD_BLACK){
    pixels = pixels & ~(0x80>>bitOffset);
  } else{
    pixels = pixels | 0x80>>bitOffset;
  }
  _framebuffer[byteIndex] = pixels;
}  

/**
 * @brief Get display framebuffer.
 * 
 * @return uint8_t* 
 */
uint8_t * Epd_GFX::getImage(){
  return _framebuffer;
}

/**
 * @brief Clear display.
 * 
 * @param y1 Start row.
 * @param y2 End row.
 * @param color Color to be set.
 */
void Epd_GFX::clear(uint16_t y1, uint16_t y2, uint16_t color){
  for (int i=width()*y1/8; i<width()*(y2+1)/8; i++){
    _framebuffer[i] = color==EPD_WHITE?0xff:0x00;
  }
}

/**
 * @brief Init display. Clear display after first boot, not after wakeup from deep sleep. 
 * 
 * @param first True if first boot. False if wakeup from deep sleep.
 */
void displayInit (boolean first){
  Serial.println("e-Paper init");
  firstBoot = first;
  if (epd.Init() != 0) {
    Serial.println("e-Paper init failed");
    return;
  }
  if (first) {
    epd.ClearFrame();
    epd.DisplayFrame(); 
  } 
}

/**
 * @brief Switch display off to minimize current consumption.
 * 
 */
void displayOff (){
  epd.Sleep();
}

