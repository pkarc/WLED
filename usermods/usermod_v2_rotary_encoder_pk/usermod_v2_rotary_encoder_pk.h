#pragma once

#include "wled.h"

//
// Inspired by the original v2 usermods
// * usermod_v2_rotary_encoder_ui
//
// v2 usermod that provides a rotary encoder-based UI.
//
// This usermod allows you to control:
// 
// * Brightness
// * main color
// * saturation of main color
//
// Change between modes by pressing a button.
//

#ifdef USERMOD_MODE_SORT
  #error "Usermod Mode Sort is no longer required. Remove -D USERMOD_MODE_SORT from platformio.ini"
#endif

#ifndef ENCODER_DT_PIN
#define ENCODER_DT_PIN 5
#endif

#ifndef ENCODER_CLK_PIN
#define ENCODER_CLK_PIN 9
#endif

#ifndef ENCODER_SW_PIN
#define ENCODER_SW_PIN 7
#endif

#ifndef ENCODER_MAX_DELAY_MS    // max delay between polling encoder pins
#define ENCODER_MAX_DELAY_MS 8  // 8 milliseconds => max 120 change impulses in 1 second, for full turn of a 30/30 encoder (4 changes per segment, 30 segments for one turn)
#endif

#define LAST_UI_STATE 2

class RotaryEncoderPkUsermod : public Usermod {

  private:

    const int8_t fadeAmount;    // Amount to change every step (brightness)
    unsigned long loopTime;

    unsigned long buttonPressedTime;
    unsigned long buttonWaitTime;
    bool buttonPressedBefore;
    bool buttonLongPressed;

    int8_t pinA;                // DT from encoder
    int8_t pinB;                // CLK from encoder
    int8_t pinC;                // SW from encoder

    unsigned char select_state; // 0: brightness, 1: effect, 2: effect speed, ...

    uint16_t currentHue1;       // default boot color
    byte currentSat1;
    uint8_t currentCCT;

    struct { // reduce memory footprint
      bool Enc_A      : 1;
      bool Enc_B      : 1;
      bool Enc_A_prev : 1;
    };

    bool applyToAll;

    bool initDone;
    bool enabled;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _DT_pin[];
    static const char _CLK_pin[];
    static const char _SW_pin[];
    static const char _applyToAll[];

    /**
     * readPin() - read rotary encoder pin value
     */
    byte readPin(uint8_t pin);

  public:

    RotaryEncoderPkUsermod()
      : fadeAmount(5)
      , buttonPressedTime(0)
      , buttonWaitTime(0)
      , buttonPressedBefore(false)
      , buttonLongPressed(false)
      , pinA(ENCODER_DT_PIN)
      , pinB(ENCODER_CLK_PIN)
      , pinC(ENCODER_SW_PIN)
      , select_state(0)
      , currentHue1(16)
      , currentSat1(255)
      , applyToAll(true)
      , initDone(false)
      , enabled(true)
    {}

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId() { return USERMOD_ID_ROTARY_ENC_UI; }
    /**
     * Enable/Disable the usermod
     */
    inline void enable(bool enable) { if (!(pinA<0 || pinB<0 || pinC<0)) enabled = enable; }

    /**
     * Get usermod enabled/disabled state
     */
    inline bool isEnabled() { return enabled; }

    /**
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup();

    /**
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    //void connected();

    /**
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     */
    void loop();

#ifndef WLED_DISABLE_MQTT
    //bool onMqttMessage(char* topic, char* payload);
    //void onMqttConnect(bool sessionPresent);
#endif

    /**
     * handleButton() can be used to override default button behaviour. Returning true
     * will prevent button working in a default way.
     * Replicating button.cpp
     */
    //bool handleButton(uint8_t b);

    /**
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     */
    //void addToJsonInfo(JsonObject &root);

    /**
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void addToJsonState(JsonObject &root);

    /**
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void readFromJsonState(JsonObject &root);

    /**
     * provide the changeable values
     */
    void addToConfig(JsonObject &root);

    void appendConfigData();

    /**
     * restore the changeable values
     * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
     * 
     * The function should return true if configuration was successfully loaded or false if there was no configuration.
     */
    bool readFromConfig(JsonObject &root);

    // custom methods
    void lampUdated();
    void changeBrightness(bool increase);
    void changeHue(bool increase);
    void changeSat(bool increase);
};


/**
 * readPin() - read rotary encoder pin value
 */
byte RotaryEncoderPkUsermod::readPin(uint8_t pin) {
  return digitalRead(pin);
}

// public methods


/*
  * setup() is called once at boot. WiFi is not yet connected at this point.
  * You can use it to initialize variables, sensors or similar.
  */
void RotaryEncoderPkUsermod::setup()
{
  DEBUG_PRINTLN(F("Usermod Rotary Encoder init."));

  PinManagerPinType pins[3] = { { pinA, false }, { pinB, false }, { pinC, false } };
  if (pinA<0 || pinB<0 || !pinManager.allocateMultiplePins(pins, 3, PinOwner::UM_RotaryEncoderUI)) {
    pinA = pinB = pinC = -1;
    enabled = false;
    return;
  }

  #ifndef USERMOD_ROTARY_ENCODER_GPIO
    #define USERMOD_ROTARY_ENCODER_GPIO INPUT_PULLUP
  #endif
  pinMode(pinA, USERMOD_ROTARY_ENCODER_GPIO);
  pinMode(pinB, USERMOD_ROTARY_ENCODER_GPIO);
  if (pinC>=0) pinMode(pinC, USERMOD_ROTARY_ENCODER_GPIO);

  loopTime = millis();

  currentCCT = (approximateKelvinFromRGB(RGBW32(col[0], col[1], col[2], col[3])) - 1900) >> 5;

  initDone = true;
  Enc_A = readPin(pinA); // Read encoder pins
  Enc_B = readPin(pinB);
  Enc_A_prev = Enc_A;
}

/*
  * loop() is called continuously. Here you can check for events, read sensors, etc.
  * 
  * Tips:
  * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
  *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
  * 
  * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
  *    Instead, use a timer check as shown here.
  */
void RotaryEncoderPkUsermod::loop()
{
  if (!enabled) return;
  unsigned long currentTime = millis(); // get the current elapsed time
  if (strip.isUpdating() && ((currentTime - loopTime) < ENCODER_MAX_DELAY_MS)) return;  // be nice, but not too nice

  if (currentTime >= (loopTime + 2)) // 2ms since last check of encoder = 500Hz
  {
    bool buttonPressed = !readPin(pinC); //0=pressed, 1=released
    if (buttonPressed) {
      if (!buttonPressedBefore) buttonPressedTime = currentTime;
      buttonPressedBefore = true;
      if (currentTime-buttonPressedTime > 3000) {
        //if (!buttonLongPressed) //long press for network info
        buttonLongPressed = true;
      }
    } else if (!buttonPressed && buttonPressedBefore) {
      bool doublePress = buttonWaitTime;
      buttonWaitTime = 0;
      if (!buttonLongPressed) {
        if (doublePress) {
          toggleOnOff();
          lampUdated();
        } else {
          buttonWaitTime = currentTime;
        }
      }
      buttonLongPressed = false;
      buttonPressedBefore = false;
    }
    if (buttonWaitTime && currentTime-buttonWaitTime>350 && !buttonPressedBefore) { //same speed as in button.cpp
      buttonWaitTime = 0;
      char newState = select_state + 1;
      bool changedState = false;
      char lineBuffer[64];
      do {
        // find new state
        switch (newState) {
          case  0: strcpy_P(lineBuffer, PSTR("Brightness")); changedState = true; break;
          case  1: strcpy_P(lineBuffer, PSTR("Main Color")); changedState = true; break;
          case  2: strcpy_P(lineBuffer, PSTR("Saturation")); changedState = true; break;
        }
        if (newState > LAST_UI_STATE) newState = 0;
      } while (!changedState);
      if (changedState) select_state = newState;
    }

    Enc_A = readPin(pinA); // Read encoder pins
    Enc_B = readPin(pinB);
    if ((Enc_A) && (!Enc_A_prev))
    { // A has gone from high to low
      if (Enc_B == LOW)    //changes to LOW so that then encoder registers a change at the very end of a pulse
      { // B is high so clockwise
        switch(select_state) {
          case  0: changeBrightness(true);      break;
          case  1: changeHue(true);             break;
          case  2: changeSat(true);             break;
        }
      }
      else if (Enc_B == HIGH)
      { // B is low so counter-clockwise
        switch(select_state) {
          case  0: changeBrightness(false);      break;
          case  1: changeHue(false);             break;
          case  2: changeSat(false);             break;
        }
      }
    }
    Enc_A_prev = Enc_A;     // Store value of A for next time
    loopTime = currentTime; // Updates loopTime
  }
}


void RotaryEncoderPkUsermod::lampUdated() {
  //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
  // 6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa
  //setValuesFromFirstSelectedSeg(); //to make transition work on main segment (should no longer be required)
  stateUpdated(CALL_MODE_BUTTON);
  updateInterfaces(CALL_MODE_BUTTON);
}

void RotaryEncoderPkUsermod::changeBrightness(bool increase) {
  //bri = max(min((increase ? bri+fadeAmount : bri-fadeAmount), 255), 0);
  if (bri < 40) bri = max(min((increase ? bri+fadeAmount/2 : bri-fadeAmount/2), 255), 0); // slower steps when brightness < 16%
  else bri = max(min((increase ? bri+fadeAmount : bri-fadeAmount), 255), 0);
  lampUdated();
}

void RotaryEncoderPkUsermod::changeHue(bool increase){
  currentHue1 = max(min((increase ? currentHue1+fadeAmount : currentHue1-fadeAmount), 255), 0);
  colorHStoRGB(currentHue1*256, currentSat1, col);
  stateChanged = true; 
  if (applyToAll) {
    for (byte i=0; i<strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive()) continue;
      seg.colors[0] = RGBW32(col[0], col[1], col[2], col[3]);
    }
  } else {
    Segment& seg = strip.getSegment(strip.getMainSegmentId());
    seg.colors[0] = RGBW32(col[0], col[1], col[2], col[3]);
  }
  lampUdated();
}

void RotaryEncoderPkUsermod::changeSat(bool increase){
  currentSat1 = max(min((increase ? currentSat1+fadeAmount : currentSat1-fadeAmount), 255), 0);
  colorHStoRGB(currentHue1*256, currentSat1, col);
  if (applyToAll) {
    for (byte i=0; i<strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive()) continue;
      seg.colors[0] = RGBW32(col[0], col[1], col[2], col[3]);
    }
  } else {
    Segment& seg = strip.getSegment(strip.getMainSegmentId());
    seg.colors[0] = RGBW32(col[0], col[1], col[2], col[3]);
  }
  lampUdated();
}

/*
  * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
  * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
  * Below it is shown how this could be used for e.g. a light sensor
  */
/*
void RotaryEncoderPkUsermod::addToJsonInfo(JsonObject& root)
{
  int reading = 20;
  //this code adds "u":{"Light":[20," lux"]} to the info object
  JsonObject user = root["u"];
  if (user.isNull()) user = root.createNestedObject("u");
  JsonArray lightArr = user.createNestedArray("Light"); //name
  lightArr.add(reading); //value
  lightArr.add(" lux"); //unit
}
*/

/*
  * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
  * Values in the state object may be modified by connected clients
  */
/*
void RotaryEncoderPkUsermod::addToJsonState(JsonObject &root)
{
  //root["user0"] = userVar0;
}
*/

/*
  * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
  * Values in the state object may be modified by connected clients
  */
/*
void RotaryEncoderPkUsermod::readFromJsonState(JsonObject &root)
{
  //userVar0 = root["user0"] | userVar0; //if "user0" key exists in JSON, update, else keep old value
  //if (root["bri"] == 255) Serial.println(F("Don't burn down your garage!"));
}
*/

/**
 * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
 */
void RotaryEncoderPkUsermod::addToConfig(JsonObject &root) {
  // we add JSON object: {"Rotary-Encoder":{"DT-pin":12,"CLK-pin":14,"SW-pin":13}}
  JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
  top[FPSTR(_enabled)] = enabled;
  top[FPSTR(_DT_pin)]  = pinA;
  top[FPSTR(_CLK_pin)] = pinB;
  top[FPSTR(_SW_pin)]  = pinC;
  top[FPSTR(_applyToAll)] = applyToAll;
  DEBUG_PRINTLN(F("Rotary Encoder config saved."));
}

void RotaryEncoderPkUsermod::appendConfigData() {
  oappend(SET_F("d.extra.push({'Rotary-Encoder-Pk':{pin:[['P0',100],['P1',101],['P2',102],['P3',103],['P4',104],['P5',105],['P6',106],['P7',107]]}});"));
}

/**
 * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
 *
 * The function should return true if configuration was successfully loaded or false if there was no configuration.
 */
bool RotaryEncoderPkUsermod::readFromConfig(JsonObject &root) {
  // we look for JSON object: {"Rotary-Encoder":{"DT-pin":12,"CLK-pin":14,"SW-pin":13}}
  JsonObject top = root[FPSTR(_name)];
  if (top.isNull()) {
    DEBUG_PRINT(FPSTR(_name));
    DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
    return false;
  }
  int8_t newDTpin  = top[FPSTR(_DT_pin)]  | pinA;
  int8_t newCLKpin = top[FPSTR(_CLK_pin)] | pinB;
  int8_t newSWpin  = top[FPSTR(_SW_pin)]  | pinC;

  enabled    = top[FPSTR(_enabled)] | enabled;
  applyToAll = top[FPSTR(_applyToAll)] | applyToAll;

  DEBUG_PRINT(FPSTR(_name));
  if (!initDone) {
    // first run: reading from cfg.json
    pinA = newDTpin;
    pinB = newCLKpin;
    pinC = newSWpin;
    DEBUG_PRINTLN(F(" config loaded."));
  } else {
    DEBUG_PRINTLN(F(" config (re)loaded."));
    // changing parameters from settings page
    if (pinA!=newDTpin || pinB!=newCLKpin || pinC!=newSWpin) {
      pinManager.deallocatePin(pinA, PinOwner::UM_RotaryEncoderUI);
      pinManager.deallocatePin(pinB, PinOwner::UM_RotaryEncoderUI);
      pinManager.deallocatePin(pinC, PinOwner::UM_RotaryEncoderUI);
      DEBUG_PRINTLN(F("Deallocated old pins."));
      pinA = newDTpin;
      pinB = newCLKpin;
      pinC = newSWpin;
      if (pinA<0 || pinB<0 || pinC<0) {
        enabled = false;
        return true;
      }
      setup();
    }
  }
  // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
  return false;
}


// strings to reduce flash memory usage (used more than twice)
const char RotaryEncoderPkUsermod::_name[]       PROGMEM = "Rotary-Encoder-Pk";
const char RotaryEncoderPkUsermod::_enabled[]    PROGMEM = "enabled";
const char RotaryEncoderPkUsermod::_DT_pin[]     PROGMEM = "DT-pin";
const char RotaryEncoderPkUsermod::_CLK_pin[]    PROGMEM = "CLK-pin";
const char RotaryEncoderPkUsermod::_SW_pin[]     PROGMEM = "SW-pin";
const char RotaryEncoderPkUsermod::_applyToAll[] PROGMEM = "apply-2-all-seg";