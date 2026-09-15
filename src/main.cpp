#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <RotaryEncoder.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <EEPROM.h>

/* TODO: 
- MAIN MENU                               DONE
- PRESETS FOR COMMON FILAMENTS            DONE
- COLOR SELECTION MENU (?)                IN PROGRESS
- ACTUALLY CONNECT THE HEATER
- DESIGN 3D PRINTED CASE                  IN PROGRESS
- ADD SPOOL ROLLERS ROTATOR MOTOR (?)
- ADD POWER SWITCH (?)
- MAKE VENTS
- ADD TEMP. AND HUM. SENSOR               DONE
- MAKE 3D PRINTABLE PROTOTYPE PCB (?)     IN PROGRESS (almost)
- MAKE IT RUN OFF 1 PSU                   IN PROGRESS (almost)
- GITHUB README
*/

#define LED_PIN 16
#define LED_COUNT 1

#define TFT_CS 13
#define TFT_DC 14
#define TFT_RST 15
#define TFT_MOSI 11
#define TFT_SCK 10

#define THS_SDA 28
#define THS_SCL 29

#define RE_SW 9 
#define RE_DT 8 //PLUG 8 AND 7 INTO OPPOSITE PINS OF THE ONES HERE
#define RE_CLK 7

#define MOSFET_PIN 2



uint32_t timeSinceTHData = 0;
bool pressed = false;
volatile int globalEncoderPosition = 0;
volatile bool positionChanged = false;
float humidity;
float temperature;
uint8_t LED_R = 0, LED_G = 0, LED_B = 0;
uint8_t currentMenu;
uint8_t padding = 30;
int8_t selectedPos = 0;
bool drewFSMenu = false;
uint32_t hour = 3600000;
uint8_t selectedFilament = 1;
bool selectingTemp = false;
bool selectingTime = false;
uint32_t startDryingTime;
uint32_t timeSinceLastTimeUpdate = 0;
uint32_t timeLeft;
uint32_t pauseTime = 0;
bool tempSensorWorking = false;



const uint8_t filamentQty = 16;
struct {
  String name;
  uint8_t dryingTemperature;
  uint32_t dryingTime;
} filament[filamentQty];

struct {
  uint8_t selectedcolorTheme;
} settings, currentStored;

const uint8_t colorThemesQty = 13;
struct {
  String name;
  uint16_t backgroundColor;
  uint16_t buttonUnselectedColor;
  uint16_t buttonSelectedColor;
  uint16_t accentColor;
  uint16_t textColor;
  bool flipTextColorOnAccentBg = false;
} colorTheme[colorThemesQty];

Adafruit_NeoPixel LED_RGB(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI1, TFT_CS, TFT_DC, TFT_RST);
GFXcanvas1 canvas(160, 32);
GFXcanvas1 canvas1(160, 16);
GFXcanvas16 canvasMMButton(320, 32);
GFXcanvas16 canvasPowerBar(320, 5);
GFXcanvas16 canvasBackButton(62, 23);
GFXcanvas16 canvasStartButton(84, 32);
GFXcanvas16 canvasTHBox(270, 130);

RotaryEncoder encoder(RE_CLK, RE_DT, RotaryEncoder::LatchMode::TWO03);

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
sensors_event_t hum, temp;

bool debounceButton(int pin, bool pressed);
void drawMainMenu();
void drawFilamentSelectionMenu();
void writeFilamentProperties();
void drawFilament(uint8_t type, uint8_t highlight);
void drawFilamentProperties(uint8_t type);
void drawDryingMenu();
void updateTimeLeft();
uint16_t getCenteredTextX(uint16_t xStart, uint16_t width, uint8_t textLenght);
uint8_t getCenteredTextY(uint8_t yStart, uint8_t height, uint8_t lines); 
void updateTemperature();
void updateHumidity();
void loadSettings();
void saveSettings();
void writecolorTheme();
void drawAnnealingMenu();
void drawSettingsMenu();
void drawAppearanceMenu();
void drawColorThBox(int16_t xStart, int8_t yStart, uint8_t color, bool selected);
void NEWdrawColorThBox(uint8_t color);

void limitEncoderPos(int16_t &pos, int16_t minPos, int16_t maxPos, bool wraping);

void setup() {
  Serial.begin(115200);
  Wire.setSDA(THS_SDA);
  Wire.setSCL(THS_SCL);
  Wire.begin();
  EEPROM.begin(sizeof(settings));
  loadSettings();

  pinMode(RE_CLK, INPUT_PULLUP);
  pinMode(RE_DT, INPUT_PULLUP);
  pinMode(RE_SW, INPUT_PULLUP);
  pinMode(MOSFET_PIN, OUTPUT);
  analogWriteFreq(25000);

  SPI1.setTX(TFT_MOSI);
  SPI1.setSCK(TFT_SCK);

  tft.init(170, 320);

  if (aht.begin()) tempSensorWorking = true;
  bmp.begin(0x77);
  LED_RGB.begin();

  while (!Serial && millis() < 2000);
  Serial.println("Started!");

  writeFilamentProperties();
  writecolorTheme();
  settings.selectedcolorTheme = 2;

  tft.setRotation(1);

  tft.setTextSize(2);
  canvas.setTextSize(2);
  canvas1.setTextSize(2);
  canvasMMButton.setTextSize(2);
  canvasBackButton.setTextSize(2);
  canvasStartButton.setTextSize(2);
  canvasTHBox.setTextSize(2);

  canvas.setTextWrap(false);
  canvas1.setTextWrap(false);
  canvasMMButton.setTextWrap(false);
  canvasBackButton.setTextWrap(false);
  canvasStartButton.setTextWrap(false);
  canvasTHBox.setTextWrap(false);

  tft.setTextColor(colorTheme[settings.selectedcolorTheme].textColor);
  canvasBackButton.setTextColor(colorTheme[settings.selectedcolorTheme].textColor);
  canvasMMButton.setTextColor(colorTheme[settings.selectedcolorTheme].textColor);
  canvasStartButton.setTextColor(colorTheme[settings.selectedcolorTheme].textColor);

  canvasStartButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  canvasBackButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  tft.setCursor(0, 0);
  drawMainMenu();


}

void loop() {
static int16_t pos = 0;
static bool selectingValue = false;
static bool selectingFilament = false;
static uint8_t selectedTemp = 0;
static uint8_t selectedTime = 0;
static bool drewDryingMenu = false;
static bool drewSettingsMenu = false;
static bool drewAppearanceMenu = false;
encoder.tick();

if (currentMenu == 0) {
  limitEncoderPos(pos, 0, 2, true);
  if (pos == 0 && selectedPos != 0) {
    canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 6), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Drying");
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 9), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Annealing");
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 8), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Settings");
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    selectedPos = 0;
  } else if (pos == 1 && selectedPos != 1) {
    canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 6), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Drying");
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 9), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Annealing");
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 8), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Settings");
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
    selectedPos = 1;
  } else if (pos == 2 && selectedPos != 2){

    canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 6), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Drying");
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 9), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Annealing");
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 8), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Settings");
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
    selectedPos = 2;
  }

if (debounceButton(RE_SW, pressed) == false && pressed == true /*&& tempSensorWorking*/) { //BUTTON PRESSED
  pressed = false;
  selectedPos = 0;
  if (pos == 0){
    currentMenu = 1;
    drewFSMenu = false;
  }
  else if (pos == 1) currentMenu = 3;
  else if (pos == 2) currentMenu = 4;
  encoder.setPosition(0);
}


} else if (currentMenu == 1) {
  if (drewFSMenu == false) {
    drawFilamentSelectionMenu();
    drewFSMenu = true;
  }
  if (!selectingFilament && !selectingTemp && !selectingTime) limitEncoderPos(pos, 0, 2, true);
  else if (selectingFilament) limitEncoderPos(pos, 0, filamentQty - 1, false);
  else if (selectingTemp) limitEncoderPos(pos, 30, 80, false);
  else if (selectingTime) limitEncoderPos(pos, 1, 240, false);
  
  if (debounceButton(RE_SW, pressed) == false && pressed == true) { //BUTTON PRESSED
    pressed = false;

    if (selectingFilament) { //stop selecting filament
      selectingFilament = false;

      drawFilament(selectedFilament, 1);

      if (selectedFilament == 0) {
        selectingTemp = true;
        pos = filament[0].dryingTemperature;
        encoder.setPosition(pos);
        drawFilamentProperties(selectedFilament); //Highlighting the temperature
      }
    } else if (selectingTemp) {
      selectingTemp = false;
      selectingTime = true;

      pos = filament[0].dryingTime / hour;
      encoder.setPosition(pos);
      drawFilamentProperties(selectedFilament); //Highlighting the drying time

    } else if (selectingTime) {
      selectingTime = false;

      pos = 0;
      encoder.setPosition(pos);

      Serial.println("TIME CONFIRMED"); //DEBUG
    }

    else {
      if (selectedPos == 0){
        drawMainMenu();

      } else if (selectedPos == 1){
        selectingFilament = true;

        drawFilament(selectedFilament, 2);  //Entering drawFilamentProp 1
        encoder.setPosition(selectedFilament);
        pos=selectedFilament;

        Serial.println("Selecting filament!"); //DEBUG
      } else if (selectedPos == 2) {
        currentMenu = 2;
        startDryingTime = millis();
        pos = 0;
        encoder.setPosition(pos);
        drewFSMenu = false;
      }
      
    }
  } 
  
  //SELECTING CUSTOM FILAMENT
  if (selectingFilament) {
    if (pos != selectedFilament) {
      selectedFilament = pos;
      drawFilament(selectedFilament, 2);
    } 
  } else if (selectingTemp) {
    if (pos != filament[0].dryingTemperature) {
      filament[0].dryingTemperature = pos;
      drawFilamentProperties(selectedFilament);
    }
  } else if (selectingTime){
    if (pos != filament[0].dryingTime / hour) {
      filament[0].dryingTime = pos*hour;
      drawFilamentProperties(selectedFilament);
    }

  } else { //

    if (pos == 0 && selectedPos != 0) { //Hovering back button
      selectedPos = 0;
      //118, 138
      canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor); //unhighlighting other options
      canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
      canvasStartButton.print("START");
      tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());
      drawFilament(selectedFilament, 0);

      canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
      canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);//0x18C3
      canvasBackButton.print("Back");
      tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());
      

    } else if (pos == 1 && selectedPos != 1) { //Hovering filament selection
      selectedPos = 1;

      canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
      canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
      canvasBackButton.print("Back");
      tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());

      canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
      canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
      canvasStartButton.print("START");
      tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());

      drawFilament(selectedFilament, 1); //highlighting filament selection bar

    } else if (pos == 2 && selectedPos != 2){
      selectedPos = 2;

      canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
      canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
      canvasBackButton.print("Back");
      tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());
      drawFilament(selectedFilament, 0);

      canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
      canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
      canvasStartButton.print("START");
      tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());
    }
  }
  
} else if (currentMenu == 2) {
  static bool paused = false;
  static uint32_t timeWhenPaused = 0;
  if (!drewDryingMenu) {
    drawDryingMenu();
    drewDryingMenu = true;
  }
  limitEncoderPos(pos, 0, 1, true);
  if (millis()-timeSinceLastTimeUpdate > 1000){
    aht.getEvent(&hum, &temp);
    temperature = temp.temperature;
    humidity = hum.relative_humidity;

    updateTimeLeft();
    updateTemperature();
    updateHumidity();

    // HEATER BEHAVIOUR
    if (!paused){
      if (filament[selectedFilament].dryingTemperature - temperature > 5){
        digitalWrite(MOSFET_PIN, HIGH);
        tft.fillRect(0, 165, 320, 5, colorTheme[settings.selectedcolorTheme].accentColor);

      } else {
        uint8_t heaterPower = constrain(map(temperature*10, filament[selectedFilament].dryingTemperature*10-50, filament[selectedFilament].dryingTemperature*10, 255, 0), 0, 255);
        analogWrite(MOSFET_PIN, heaterPower); //PWM on the heater
      
        uint16_t powerBarWidth = constrain(map(temperature*10, filament[selectedFilament].dryingTemperature*10-50, filament[selectedFilament].dryingTemperature*10, 320, 0), 0, 320);
        canvasPowerBar.fillScreen(0x34FF);
        canvasPowerBar.fillRect(0, 0, powerBarWidth, 5, colorTheme[settings.selectedcolorTheme].accentColor);
        tft.drawRGBBitmap(0, 165, canvasPowerBar.getBuffer(), canvasPowerBar.width(), canvasPowerBar.height());
      }
    } else {
      digitalWrite(MOSFET_PIN, LOW);
      canvasPowerBar.fillScreen(0x34FF);
      tft.drawRGBBitmap(0, 165, canvasPowerBar.getBuffer(), canvasPowerBar.width(), canvasPowerBar.height());
    }
  } 

  // BUTTON BEHAVIOUR
  if (pos == 0 && selectedPos != 0){
    selectedPos=0;

    canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(170, 6, 80, 26, 9, 0xF987); //0xFB8E
    canvasMMButton.setCursor(getCenteredTextX(170, 80, 4), getCenteredTextY(6, 26, 1));
    canvasMMButton.print("STOP");

    canvasMMButton.fillRoundRect(70, 6, 80, 26, 9, 0xFED4);
    if (!paused) {
      canvasMMButton.setCursor(getCenteredTextX(70, 80, 5), getCenteredTextY(6, 26, 1));
      canvasMMButton.print("PAUSE");
    } else {
      canvasMMButton.setCursor(getCenteredTextX(70, 80, 6), getCenteredTextY(6, 26, 1));
      canvasMMButton.print("RESUME");
    }
    tft.drawRGBBitmap(0, 123, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
    

  } else if (pos == 1 && selectedPos != 1){
    selectedPos=1;

    canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(170, 6, 80, 26, 9, 0xFD34);
    canvasMMButton.setCursor(getCenteredTextX(170, 80, 4), getCenteredTextY(6, 26, 1));
    canvasMMButton.print("STOP");

    canvasMMButton.fillRoundRect(70, 6, 80, 26, 9, 0xFD67);
    if (!paused) {
      canvasMMButton.setCursor(getCenteredTextX(70, 80, 5), getCenteredTextY(6, 26, 1));
      canvasMMButton.print("PAUSE");
    } else {
      canvasMMButton.setCursor(getCenteredTextX(70, 80, 6), getCenteredTextY(6, 26, 1));
      canvasMMButton.print("RESUME");
    }
    tft.drawRGBBitmap(0, 123, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

  }

  if (paused){
    pauseTime = millis()-timeWhenPaused;
  }
  
  if (debounceButton(RE_SW, pressed) == false && pressed == true) { //BUTTON PRESSED
    pressed = false;
    if (selectedPos == 0){
      if (!paused){
        //STOP THE HEATER
        paused = true;
        timeWhenPaused = millis()-pauseTime;
        tft.fillRoundRect(70, 129, 80, 26, 9, 0xFE2E);
        tft.setCursor(getCenteredTextX(70, 80, 6), getCenteredTextY(129, 26, 1));
        tft.print("RESUME");
      } else {
        paused = false;
        tft.fillRoundRect(70, 129, 80, 26, 9, 0xFE2E);
        tft.setCursor(getCenteredTextX(70, 80, 5), getCenteredTextY(129, 26, 1));
        tft.print("PAUSE");
      }
    } else if (selectedPos == 1){
      digitalWrite(MOSFET_PIN, LOW);
      pauseTime = 0;
      currentMenu = 0;
      drewDryingMenu = false;
      paused = false;
      drawMainMenu();
    }
  }
} else if (currentMenu == 3){

} else if (currentMenu == 4){
  if (!drewSettingsMenu){
    drawSettingsMenu();
    drewSettingsMenu = true;
  }
  limitEncoderPos(pos, 0, 1, true);
  
  if (pos == 0 && selectedPos != 0) {
    canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 10), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Appearance");
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 4), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Save");
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    selectedPos = 0;
  }

  
  if (debounceButton(RE_SW, pressed) == false && pressed == true) { //BUTTON PRESSED
  pressed = false;
  selectedPos = 0;
  if (pos == 0){
    currentMenu = 5;
    drewAppearanceMenu = false;
    drewSettingsMenu = false;
  } else if (pos == 1){
    currentMenu = 0;
    drewAppearanceMenu = false;
    drewSettingsMenu = false;
    //saveSettings();
  }
  selectedPos = 0;
  pos = 0;
  encoder.setPosition(0);

  }
} else if (currentMenu == 5){
  //tft.setTextSize(1);
  limitEncoderPos(pos, 0, 11, true);
  if (!drewAppearanceMenu){
    drawAppearanceMenu();
    drewAppearanceMenu = true;
  }
  /*
  if (pos != selectedPos){
    
    uint16_t xStart = (pos%4)*70+5;
    uint8_t yStart = (pos/4)*50+5;
    drawColorThBox(xStart, yStart, pos, true);
    if (pos < selectedPos){
      if (pos != 0) drawColorThBox( ((pos-1)%4)*70+5, ((pos-1)/4)*50+5, pos-1, false);
      else drawColorThBox( ((11)%4)*70+5, ((11)/4)*50+5, 11, false);
    } 
    if (pos > selectedPos){
      if (pos != 11) drawColorThBox( ((pos+1)%4)*70+5, ((pos+1)/4)*50+5, pos+1, false);
      else drawColorThBox( ((0)%4)*70+5, ((0)/4)*50+5, 0, false);
    } 
    //drawColorThBox((pos-1%4)*70+5, (pos-1/4)*50+5, pos, false);
    //drawColorThBox((pos+1%4)*70+5, (pos+1/4)*50+5, pos, false);
    selectedPos = pos;
  }*/
  tft.setTextSize(2);
}




if (debounceButton(RE_SW, pressed) == true && pressed == false)  pressed = true; //BUTTON RELEASED
}


bool debounceButton(int pin, bool pressed) {
  bool currentlyPressed = digitalRead(pin);
  if (pressed != currentlyPressed) {
    delay(10);
    currentlyPressed = digitalRead(pin);
  }
  return currentlyPressed;
}


void drawMainMenu() {
  uint16_t width = 320;
  uint8_t height = 170;
  uint8_t mainMenuButtonWidth = width-(padding*2);

  aht.getEvent(&hum, &temp);
  temperature = temp.temperature;
  humidity = hum.relative_humidity;

  tft.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  tft.fillRoundRect(padding, 12, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor); //Drying button BG
  tft.fillRoundRect(padding, 56, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor); //Annealing button BG
  tft.fillRoundRect(padding, 100, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor); //Settings button BG
  tft.setCursor(getCenteredTextX(padding, 260, 6), getCenteredTextY(12, 32, 1));
  tft.print("Drying");
  tft.setCursor(getCenteredTextX(padding, 260, 9), getCenteredTextY(56, 32, 1));
  tft.print("Annealing");
  tft.setCursor(getCenteredTextX(padding, 260, 8), getCenteredTextY(100, 32, 1));
  tft.print("Settings");
  tft.setCursor(0,140);
  if (tempSensorWorking) {
  tft.println("Temperature: ");
  tft.print("R. Humidity: ");
  canvas.fillScreen(0);
  canvas.setCursor(0, 0);
  canvas.println(temperature);
  canvas.print(humidity);
  canvas.setCursor(0, 0);
  tft.drawBitmap(144, 112, canvas.getBuffer(), canvas.width(), canvas.height(), colorTheme[settings.selectedcolorTheme].textColor, colorTheme[settings.selectedcolorTheme].backgroundColor);
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("CHECK TEMPERATURE SENSOR!");
    tft.println("Init. FAILED, NO TEMP DATA");
    tft.setTextColor(colorTheme[settings.selectedcolorTheme].textColor);
  }

  selectedPos = 0;
  currentMenu = 0;
}

void drawFilamentSelectionMenu(){
  tft.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  tft.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
  tft.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
  tft.print("Back");
  tft.setCursor(getCenteredTextX(0, 320, 15),0);
  tft.print("Select filament");
  drawFilament(selectedFilament, 0);
  tft.fillRoundRect(118, 138, 84, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor); //0x77FF
  tft.setCursor(getCenteredTextX(118, 84, 5), getCenteredTextY(138, 32, 1));
  tft.print("START");
}

void limitEncoderPos(int16_t &pos, int16_t minPos, int16_t maxPos, bool wraping) {
  int16_t newPos = encoder.getPosition();
  if (pos != newPos) {
    pos = newPos;
    if (pos > maxPos){
      if (wraping){
        encoder.setPosition(minPos);
        pos = minPos;
      } else {
        encoder.setPosition(maxPos);
        pos = maxPos;
      }
    }
    else if (pos < minPos){
      if (wraping) {
        encoder.setPosition(maxPos);
        pos = maxPos;
      } else {
        encoder.setPosition(minPos);
        pos=minPos;
      }
    } 
  }
}

void drawFilament(uint8_t type, uint8_t highlight){
  canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  canvasMMButton.fillRoundRect(0, 0, 320, 32, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
  if (type > 0) {
    canvasMMButton.setCursor(12,8);
    canvasMMButton.print(filament[type-1].name);
  }
  if (highlight == 1) canvasMMButton.fillRoundRect(115, 0, 90, 32, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
  if (highlight == 2) {
    canvasMMButton.fillRoundRect(115, 0, 90, 32, 9, colorTheme[settings.selectedcolorTheme].accentColor);
    canvasMMButton.setCursor(getCenteredTextX(0, 320, filament[type].name.length()), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedcolorTheme].flipTextColorOnAccentBg) canvasMMButton.setTextColor(colorTheme[settings.selectedcolorTheme].backgroundColor);
    else canvasMMButton.setTextColor(0);
    canvasMMButton.print(filament[type].name);
    canvasMMButton.setTextColor(colorTheme[settings.selectedcolorTheme].textColor);
  } else if (highlight == 0 || highlight == 1) {
    canvasMMButton.drawRoundRect(115, 0, 90, 32, 9, colorTheme[settings.selectedcolorTheme].accentColor);
    canvasMMButton.setCursor(getCenteredTextX(0, 320, filament[type].name.length()), getCenteredTextY(0, 32, 1));
    canvasMMButton.print(filament[type].name);
  }
  if (type + 1 < filamentQty) {
    uint16_t xEnd = 304 - filament[type+1].name.length()*10;
    canvasMMButton.setCursor(xEnd, 9);
    canvasMMButton.print(filament[type+1].name);
  }
  tft.drawRGBBitmap(0, 26, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

  drawFilamentProperties(type);
}

void drawFilamentProperties(uint8_t type){
  Serial.println("ENTER drawFilamentProperties"); //DEBUG
  canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  Serial.println("AFTER fillScreen"); //DEBUG
  if (type == 0){
    uint8_t timeLenght = 38;
    if (filament[type].dryingTime/hour > 9 && filament[type].dryingTime/hour < 100) timeLenght+=12;
    if (filament[type].dryingTime/hour > 99) timeLenght+=24;
    if (selectingTemp == false && selectingTime == false){
      canvasMMButton.fillRoundRect(160, 0, 60, 15, 3, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
      canvasMMButton.fillRoundRect(65, 16, timeLenght, 15, 3, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    } else if (selectingTemp == true) {
      canvasMMButton.fillRoundRect(160, 0, 60, 15, 3, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
      canvasMMButton.fillRoundRect(65, 16, timeLenght, 15, 3, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    } else if (selectingTime == true) {
      canvasMMButton.fillRoundRect(160, 0, 60, 15, 3, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
      canvasMMButton.fillRoundRect(65, 16, timeLenght, 15, 3, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
    }
  }
  canvasMMButton.setCursor(0, 0);
  canvasMMButton.print("Drying temp.: ");
  canvasMMButton.print(filament[type].dryingTemperature);
  canvasMMButton.drawRect(195, 0, 4, 4, colorTheme[settings.selectedcolorTheme].textColor);
  canvasMMButton.setCursor(202, 0);
  canvasMMButton.println("C");
  canvasMMButton.print("Time: ");
  canvasMMButton.print(filament[type].dryingTime / hour);
  canvasMMButton.print("h");

  Serial.println("Before drawRGBBitmap"); //Debug
  tft.drawRGBBitmap(0, 70, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
  Serial.println("After drawRGBBitmap"); //Debug
}

void writeFilamentProperties(){
  filament[0].name="Custom";
  filament[0].dryingTemperature=50;
  filament[0].dryingTime=hour*4;

  filament[1].name="PLA";
  filament[1].dryingTemperature=50;
  filament[1].dryingTime=hour*6;

  filament[2].name="PETG";
  filament[2].dryingTemperature=65;
  filament[2].dryingTime=hour*6;

  filament[3].name="ABS";
  filament[3].dryingTemperature=65;
  filament[3].dryingTime=hour*6;

  filament[4].name="ASA";
  filament[4].dryingTemperature=65;
  filament[4].dryingTime=hour*8;

  filament[5].name="TPU";
  filament[5].dryingTemperature=55;
  filament[5].dryingTime=hour*8;

  filament[6].name="PC";
  filament[6].dryingTemperature=75;
  filament[6].dryingTime=hour*12;

  filament[7].name="PA6xF";
  filament[7].dryingTemperature=75;
  filament[7].dryingTime=hour*24;

  filament[8].name="PA12xF";
  filament[8].dryingTemperature=75;
  filament[8].dryingTime=hour*24;

  filament[9].name="PCTG";
  filament[9].dryingTemperature=65;
  filament[9].dryingTime=hour*6;

  filament[10].name="WOODFL";
  filament[10].dryingTemperature=55;
  filament[10].dryingTime=hour*6;

  filament[11].name="PVB";
  filament[11].dryingTemperature=55;
  filament[11].dryingTime=hour*6;
  
  filament[12].name="PVA";
  filament[12].dryingTemperature=45;
  filament[12].dryingTime=hour*16;

  filament[13].name="HIPS";
  filament[13].dryingTemperature=65;
  filament[13].dryingTime=hour*8;

  filament[14].name="PP";
  filament[14].dryingTemperature=55;
  filament[14].dryingTime=hour*6;

  filament[15].name="DESIC";
  filament[15].dryingTemperature=65;
  filament[15].dryingTime=hour*12;

  //Data mostly from https://www.reddit.com/r/3Dprinting/wiki/filament_drying/
  //https://wiki.flashforge.com/en/filaments_and_accessories/Drying_Temperature_and_Time
  //And other settings recomended by manufacturers
}

void writecolorTheme(){
  colorTheme[0].name = "Classic dark";
  colorTheme[0].backgroundColor = 0;
  colorTheme[0].buttonUnselectedColor = 0x18C3;
  colorTheme[0].buttonSelectedColor = 0x4A69;
  colorTheme[0].accentColor = 0x77FF;
  colorTheme[0].textColor = 0xFFFF;

  colorTheme[1].name = "Classic light"; //https://www.realtimecolors.com/?colors=000000-FFFFFF-6acbfb-040599-501ef8&fonts=Inter-Inter
  colorTheme[1].backgroundColor = 0xFFFF;
  colorTheme[1].buttonUnselectedColor = 0xEF5D;
  colorTheme[1].buttonSelectedColor = 0xB59C;
  colorTheme[1].accentColor = 0x77FF;
  colorTheme[1].textColor = 0x0000;
  colorTheme[1].flipTextColorOnAccentBg = true;

  colorTheme[2].name = "Icy dark"; //https://www.realtimecolors.com/?colors=ddf6fe-000c0f-6acbfb-040599-501ef8&fonts=Inter-Inter   //?
  colorTheme[2].backgroundColor = 0x00E4;
  colorTheme[2].buttonUnselectedColor = 0x0167;
  colorTheme[2].buttonSelectedColor = 0x0AAE;
  colorTheme[2].accentColor = 0x7E9D;
  colorTheme[2].textColor = 0xDFBF;

  colorTheme[3].name = "Icy light"; //https://www.realtimecolors.com/?colors=041a20-daf2fb-106584-8777ee-5518bf&fonts=Inter-Inter
  colorTheme[3].backgroundColor = 0xDF9F;
  colorTheme[3].buttonUnselectedColor = 0xC6FC;
  colorTheme[3].buttonSelectedColor = 0x1330;
  colorTheme[3].accentColor = 0x83BD;
  colorTheme[3].textColor = 0x00C4;
  colorTheme[3].flipTextColorOnAccentBg = true;

  colorTheme[4].name = "Cherry grove";
  colorTheme[4].backgroundColor = 0x1842;
  colorTheme[4].buttonUnselectedColor = 0x3863;
  colorTheme[4].buttonSelectedColor = 0x68E6;
  colorTheme[4].accentColor = 0xF12C;
  colorTheme[4].textColor = 0xF77D;

  colorTheme[5].name = "Cherry yoghurt"; //https://www.realtimecolors.com/?colors=120d0e-f4e7ec-651f35-e87d9f-d90d4d&fonts=Inter-Inter
  colorTheme[5].backgroundColor = 0xF73D;
  colorTheme[5].buttonUnselectedColor = 0xF63A;
  colorTheme[5].buttonSelectedColor = 0xEBF3;
  colorTheme[5].accentColor = 0xD869;
  colorTheme[5].textColor = 0x1061;
  colorTheme[5].flipTextColorOnAccentBg = true;

  colorTheme[6].name = "Neon / Waprowave"; //https://www.realtimecolors.com/?colors=e8fcf7-010404-18d8b1-0e2b81-2213a4&fonts=Inter-Inter
  colorTheme[6].backgroundColor = 0x0020;
  colorTheme[6].buttonUnselectedColor = 0x0085;
  colorTheme[6].buttonSelectedColor = 0x092B;
  colorTheme[6].accentColor = 0x1ED6;
  colorTheme[6].textColor = 0xEFFE;

  colorTheme[7].name = "Carrot cake"; //https://www.realtimecolors.com/?colors=fce9d9-1c0e02-f5b277-56920b-42ef1c&fonts=Inter-Inter
  colorTheme[7].backgroundColor = 0x1860;
  colorTheme[7].buttonUnselectedColor = 0x29A0;
  colorTheme[7].buttonSelectedColor = 0x5B41;
  colorTheme[7].accentColor = 0xF58E;//0x4763
  colorTheme[7].textColor = 0xFF5B;

  colorTheme[8].name = "Vibrant purple"; //https://www.realtimecolors.com/?colors=e8d9fc-0f0321-a171f4-930b76-ee1b81&fonts=Inter-Inter
  colorTheme[8].backgroundColor = 0x0804;
  colorTheme[8].buttonUnselectedColor = 0x1826;
  colorTheme[8].buttonSelectedColor = 0x284D;
  colorTheme[8].accentColor = 0xE8D0;
  colorTheme[8].textColor = 0xEEDF;
  //ADD LIGHT TOO

  colorTheme[9].name = "Green banana / Matcha"; //https://www.realtimecolors.com/?colors=1f220c-f6f8ec-596124-95d4a7-75c6ac&fonts=Inter-Inter
  colorTheme[9].backgroundColor = 0xF7DD;
  colorTheme[9].buttonUnselectedColor = 0xDF7A;
  colorTheme[9].buttonSelectedColor = 0x5B04;
  colorTheme[9].accentColor = 0x7635;
  colorTheme[9].textColor = 0x1901;
  colorTheme[9].flipTextColorOnAccentBg = true;

  colorTheme[10].name = "Laser"; //https://www.realtimecolors.com/?colors=f6e5e5-0c0303-be272c-851418-ad1015&fonts=Inter-Inter
  colorTheme[10].backgroundColor = 0x0800;
  colorTheme[10].buttonUnselectedColor = 0x3041;
  colorTheme[10].buttonSelectedColor = 0x6082;
  colorTheme[10].accentColor = 0xE166;
  colorTheme[10].textColor = 0xF73C;

  colorTheme[11].name = "Jungle"; //https://www.realtimecolors.com/docs/selecting-colors?colors=eafcd9-080e01-baf480-0ea04f-19ebb3&fonts=Inter-Inter
  colorTheme[11].backgroundColor = 0x0860;
  colorTheme[11].buttonUnselectedColor = 0x09C3;
  colorTheme[11].buttonSelectedColor = 0x1365;
  colorTheme[11].accentColor = 0x1F56;
  colorTheme[11].textColor = 0xEFFB;

  colorTheme[12].name = "Dark pastel"; //https://www.realtimecolors.com/?colors=e4f2e3-081208-a0d4a0-375e7b-686dbb&fonts=Inter-Inter
  colorTheme[12].backgroundColor = 0x0881;
  colorTheme[12].buttonUnselectedColor = 0x1145;
  colorTheme[12].buttonSelectedColor = 0x32CB;
  colorTheme[12].accentColor = 0x6B77;
  colorTheme[12].textColor = 0xE79C;
}

void drawDryingMenu(){
  aht.getEvent(&hum, &temp);
  temperature = temp.temperature;
  humidity = hum.relative_humidity;

  tft.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  tft.setCursor(getCenteredTextX(0, 320, 6), 0);
  tft.print("DRYING");
  tft.setCursor(0, 26);
  tft.println("Current temp.: ");
  updateTemperature();
  
  tft.println("R. humidity: ");
  updateHumidity();

  tft.print("Target temp.: ");
  tft.print(filament[selectedFilament].dryingTemperature);
  tft.drawRect(195, 58, 4, 4, colorTheme[settings.selectedcolorTheme].textColor);
  tft.setCursor(201, 58);
  tft.println("C");
  

  tft.print("Time left: ");
  updateTimeLeft();

  //BUTTONS
  tft.fillRoundRect(70, 129, 80, 26, 9, 0xFE2E);
  tft.setCursor(getCenteredTextX(70, 80, 5), getCenteredTextY(129, 26, 1));
  tft.print("PAUSE");

  tft.fillRoundRect(170, 129, 80, 26, 9, 0xF987);
  tft.setCursor(getCenteredTextX(170, 80, 4), getCenteredTextY(129, 26, 1));
  tft.print("STOP");

  //POWER BAR
  tft.fillRect(0, 165, 320, 5, 0x34FF);

}

void updateTimeLeft() {
  timeLeft = startDryingTime+filament[selectedFilament].dryingTime-millis()+pauseTime;
  
  canvas.fillScreen(0);
  canvas.setCursor(0, 0);
  canvas.print(timeLeft / hour);
  canvas.print("h ");
  canvas.print((timeLeft % hour) / 60000);
  canvas.print("m ");
  canvas.print((timeLeft % 60000) / 1000);
  canvas.print("s");
  tft.drawBitmap(130, 74, canvas.getBuffer(), canvas.width(), canvas.height(), colorTheme[settings.selectedcolorTheme].textColor, colorTheme[settings.selectedcolorTheme].backgroundColor);

  timeSinceLastTimeUpdate = millis();
}

void updateTemperature(){
  canvas1.fillScreen(0);
  canvas1.setCursor(0, 0);
  canvas1.printf("%04.1f" , temperature);
  canvas1.drawRect(50, 0, 4, 4, 0xFFFF);
  canvas1.setCursor(56, 0);
  canvas1.print("C");
  tft.drawBitmap(172, 26, canvas1.getBuffer(), canvas1.width(), canvas1.height(), colorTheme[settings.selectedcolorTheme].textColor, colorTheme[settings.selectedcolorTheme].backgroundColor);
}

void updateHumidity(){
  canvas1.fillScreen(0);
  canvas1.setCursor(0, 0);
  canvas1.printf("%2.1f" , humidity);
  canvas1.print("%");
  tft.drawBitmap(152, 42, canvas1.getBuffer(), canvas1.width(), canvas1.height(), colorTheme[settings.selectedcolorTheme].textColor, colorTheme[settings.selectedcolorTheme].backgroundColor);
}

uint16_t getCenteredTextX(uint16_t xStart, uint16_t width, uint8_t textLenght) {
    return xStart+(width/2)-textLenght*6+1;
}

uint8_t getCenteredTextY (uint8_t yStart, uint8_t height, uint8_t lines){
  return yStart+(height/2)-8*lines+1;
}

void loadSettings(){
  EEPROM.get(0, settings);

  if (settings.selectedcolorTheme == 255){ //Restoring defaults if none set
    settings.selectedcolorTheme = 0;
  }
}

void saveSettings(){
  EEPROM.get(0, currentStored);
  if (memcmp(&settings, &currentStored, sizeof(settings)) != 0){
    EEPROM.put(0, settings);
    EEPROM.commit();
  }
}

void drawAnnealingMenu(){
  //tft.fillScreen();

}

void drawSettingsMenu(){
  tft.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  //canvasMMButton.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  tft.fillRoundRect(padding, 12, 260, 32, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor); //Appearance button BG
  tft.setCursor(getCenteredTextX(padding, 260, 10), getCenteredTextY(12, 32, 1));
  tft.print("Appearance");

}

void drawAppearanceMenu(){
  /*
  for (uint8_t counter = 0; counter <=11; counter++){
    uint16_t xStart = (counter%4)*70+5;
    uint8_t yStart = (counter/4)*50+5;
    drawColorThBox(xStart, yStart, counter, counter==encoder.getPosition());
  }
    */
  NEWdrawColorThBox(1);
}

void drawColorThBox(int16_t xStart, int8_t yStart, uint8_t color, bool selected){
  if (selected) tft.drawRoundRect(xStart, yStart, 70, 45, 9, 0xF800);
  else tft.drawRoundRect(xStart, yStart, 70, 45, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
  tft.fillRoundRect(xStart+1, yStart+1, 68, 43, 9, colorTheme[color].backgroundColor);
  tft.setCursor(getCenteredTextX(xStart+5, 60, colorTheme[color].name.length()), yStart+5);
  tft.setTextColor(colorTheme[color].textColor);
  tft.print(colorTheme[color].name);
  tft.setTextColor(colorTheme[settings.selectedcolorTheme].textColor);
  tft.fillRoundRect(xStart+5, yStart+15, 60, 10, 5, colorTheme[color].buttonSelectedColor);
  tft.fillRoundRect(xStart+5, yStart+30, 60, 10, 5, colorTheme[color].buttonSelectedColor);
}

void NEWdrawColorThBox(uint8_t color){
  tft.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  canvasTHBox.fillScreen(colorTheme[settings.selectedcolorTheme].backgroundColor);
  canvasTHBox.setTextColor(colorTheme[color].textColor);
  bool selected = true;
  if (color == settings.selectedcolorTheme) selected = true;
  //Scroll bar
  tft.fillRect(310, 0, 10, 170, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
  tft.fillRect(310, map(color, 0, colorThemesQty, 0, 300), 10, 20, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);


  tft.setCursor(getCenteredTextX(0, 310, colorTheme[color].name.length()), 0);
  tft.print(colorTheme[color].name);
  //Outline 
  if (selected) {
    canvasTHBox.fillRoundRect(0, 0, 270, 130, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
    //canvasTHBox.drawRoundRect(1, 1, 268, 128, 9, colorTheme[settings.selectedcolorTheme].buttonSelectedColor);
  }
  else{
    canvasTHBox.fillRoundRect(0, 0, 270, 130, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
    //canvasTHBox.drawRoundRect(1, 1, 268, 128, 9, colorTheme[settings.selectedcolorTheme].buttonUnselectedColor);
  }
  //buttons
  canvasTHBox.fillRoundRect(2, 2, 266, 126, 9, colorTheme[color].backgroundColor);
  canvasTHBox.fillRoundRect(11, 11, 248, 32, 9, colorTheme[color].buttonSelectedColor);
  canvasTHBox.fillRoundRect(11, 55, 248, 32, 9, colorTheme[color].buttonUnselectedColor);
  canvasTHBox.setCursor(getCenteredTextX(11, 248, 8), getCenteredTextY(11, 32, 1));
  canvasTHBox.print("selected");
  canvasTHBox.setCursor(getCenteredTextX(11, 248, 10), getCenteredTextY(55, 32, 1));
  canvasTHBox.print("unselected");

  //Accent bar


  tft.drawRGBBitmap(20, 20, canvasTHBox.getBuffer(), canvasTHBox.width(), canvasTHBox.height());

}
// put function definitions here:

//RIGHT -> padding
//Center -> 160-5*(characters+1)
//LEFT -> 320-10*characters

//CHANGELOG:
/*
Made the time and temp sensor selection tile highlight before changing
Added safety feature, warning when the temperature sensor doesn't work
Made the time pass correctly
Made it possible to pause the time
Fixed an issue where pausing would make time go by in slow motion
Fixed some of the buttons flickering
Added Colors!



*/