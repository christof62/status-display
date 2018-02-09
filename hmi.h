#include <FreeRTOS.h>
#include <Adafruit_GFX.h>

#define R1_Y 38
#define R2_Y 260
#define R3_Y 299

enum class Event {KEY_0, KEY_1, KEY_2, KEY_3, REDRAW, CONNECTION_FINISHED, CONNECTION_FAILED, DATA_SENT, USER_TIMEOUT, TIME_UPDATE, TEMPERATURE, HUMIDITY, WINDOW, OFF, ON, PLUS, MINUS, CONFIRM, ABSENT, HOME, BACK, SCREEN_ENTRY, SCREEN_MAIN, SCREEN_LIGHT, SCREEN_AUDIO, SCREEN_HEATING, SCREEN_ABSENT};
enum class Room  {LIVINGROOM, DININGROOM, KITCHEN, BEDROOM, BATHROOM_GF, CORRIDOR_GF, BATHROOM_UF, CORRIDOR_UF, SVENJA, ROBIN, LAST};

class Epd_GFX:public Adafruit_GFX {
  public:  
  Epd_GFX (int16_t w, int16_t h);
  void drawPixel(int16_t x, int16_t y, uint16_t color);
  uint8_t * getImage();
  void clear(uint16_t y1, uint16_t y2, uint16_t color);

  private:
  uint8_t *_framebuffer;
};

struct Softkey {
  enum {SCREEN, ACTION, UNDEFINED} tag = UNDEFINED;
  const unsigned char* icon;
  Event event;
};

void displayInit (bool first);
void displayOff(void);


class Screen {    
  public:
    Screen();
    void enter();    
    void draw();
    void draw(uint8_t number);
    void addSoftkey (uint8_t index, Event event, const unsigned char* icon);
    virtual void triggerEvent(Event event);
    virtual void activate();
    virtual void deactivate();
    virtual char* getName();
  protected:
    virtual void drawHeadline();
    virtual void drawMain();
    virtual void drawSoftkeys();
  private:
    void screenToDisplay();
    void drawSoftkey(uint8_t index, const unsigned char* bmp);
    struct Softkey _softkeys[4];
    uint8_t _drawCounter = 0;
};

class ScreenManager {
  public:
    ScreenManager();
    void triggerEvent(Event event);
    void requestScreen (Screen *screen);
    void execute();
  private:
    Screen *_activeScreen = nullptr;
    QueueHandle_t _eventQueue; 
};

extern ScreenManager screenManager;

