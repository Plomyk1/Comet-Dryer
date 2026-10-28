#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <RotaryEncoder.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <EEPROM.h>

/* TODO: 
- MAIN MENU                               DONE
- PRESETS FOR COMMON FILAMENTS            DONE
- COLOR SELECTION MENU                    DONE
- CONNECT THE HEATER                      DONE
- DESIGN 3D PRINTED CASE                  DONE
- ADD SPOOL ROLLERS AND ROTATOR MOTOR     SPOOL ROLLERS DONE, NOT USING MOTOR
Motor has been currently cancelled, as it was taking up too much space to implement.
Hopefully it doesen't have too much of an effect on the preformance of the dryer
- ADD POWER SWITCH (?)                    DONE
- ADD TEMP. AND HUM. SENSOR               DONE
- MAKE 3D PRINTABLE PROTOTYPE PCB (?)     CANCELLED (FOR NOW)
- MAKE IT RUN OFF 1 PSU                   DONE
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

#define HEATER_PIN 2
#define FAN_PIN 3
//#define MOTOR_PIN 4 Commenting out motor code, as it is not currently used

//SET THIS TO TRUE IF YOUR POWER SUPPLY IS UNDERPOWERED AND CANNOT CONSTANTLY DELIVER 12V WHEN AT FULL LOAD      ! ! !
bool underpoweredPSU = false;



uint32_t timeSinceTHData = 0;
bool released = false;
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
uint32_t startAnnealingTime;
uint32_t timeSinceLastTimeUpdate = 0;
uint32_t timeLeft;
uint32_t pauseTime = 0;
uint32_t annealingTime = 45;
uint32_t stressReliefTime = 45;
uint8_t vicatTemperature = 50;
bool tempSensorWorking = false;
bool stressReliefFinished = false;
float temperatureBeforeAnnealing = 20;


const uint8_t filamentQty = 16;
struct {
  String name;
  uint8_t dryingTemperature;
  uint32_t dryingTime;
} filament[filamentQty];

struct {
  uint8_t selectedColorTheme;
  uint8_t fanSpeed;
} settings, currentStored;

const uint8_t colorThemesQty = 13;
struct {
  String name;
  uint16_t backgroundColor;
  uint16_t buttonUnselectedColor;
  uint16_t buttonSelectedColor;
  uint16_t accentColor;
  uint16_t textColor;
  bool flipTextColorOnSelectedBg = false;
  bool flipTextColorOnAccentBg = false;
} colorTheme[colorThemesQty];

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
sensors_event_t hum, temp;

bool debounceButton(int pin, bool released);
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
void writeColorThemes();
void drawAnnealingSelectionMenu();
void drawSettingsMenu();
void drawColorThBox(uint8_t color);
void drawAnnealingMenu();
void updateAnnealingTimeLeft();
void drawCoolDownMenu();

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
  digitalWrite(HEATER_PIN, LOW);
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);
  pinMode(FAN_PIN, OUTPUT);
  //pinMode(MOTOR_PIN, OUTPUT);
  analogWriteFreq(25000);

  SPI1.setTX(TFT_MOSI);
  SPI1.setSCK(TFT_SCK);

  tft.init(170, 320);

  if (aht.begin()) tempSensorWorking = true;

  while (!Serial && millis() < 2000);
  Serial.println("Started!");

  writeFilamentProperties();
  writeColorThemes();

  tft.setRotation(3);

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

  tft.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  canvasBackButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  canvasStartButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);

  canvasStartButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasBackButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  tft.setCursor(0, 0);
  drawMainMenu();


}

void loop() {

static int16_t pos = 0;
//static uint32_t timeSinceLastMotorSpin = 0;
static bool selectingValue = false;
static bool selectingFilament = false;
static bool selectingFanSpeed = false;
static bool drewMainMenu = true;
static bool drewDryingMenu = false;
static bool drewAnnealingMenu = false;
static bool drewSettingsMenu = false;
static bool drewAppearanceMenu = false;
static bool drewAnnealingSelectionMenu = false;
static bool drewCoolDownMenu = false;
encoder.tick();

if (currentMenu == 0) /* Main */ {
  limitEncoderPos(pos, 0, 2, true);
  if (!drewMainMenu){
    drawMainMenu();
    drewMainMenu = true; 
  }
  if (pos == 0 && selectedPos != 0) {
    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 6), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print("Drying");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 9), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Annealing");
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 8), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Settings");
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    selectedPos = 0;
  } else if (pos == 1 && selectedPos != 1) {
    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 6), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Drying");
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 9), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print("Annealing");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 8), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Settings");
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
    selectedPos = 1;
  } else if (pos == 2 && selectedPos != 2){

    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 6), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Drying");
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 9), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Annealing");
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 8), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print("Settings");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
    selectedPos = 2;
  }

if (debounceButton(RE_SW, released) == false && released == true) { //BUTTON PRESSED
  released = false;
  selectedPos = 0;
  if (pos == 0 && tempSensorWorking){ //Drying
    currentMenu = 1;
    drewFSMenu = false;
  }
  else if (pos == 1 && tempSensorWorking) currentMenu = 3; //Annealing
  else if (pos == 2){ //Settings
    currentMenu = 4;
    pos == 0;
    selectedPos == 0;
  }
  encoder.setPosition(0);
}


} else if (currentMenu == 1) /* DryingSEL */ {
  if (drewFSMenu == false) {
    drawFilamentSelectionMenu();
    drewFSMenu = true;
  }
  if (!selectingFilament && !selectingTemp && !selectingTime) limitEncoderPos(pos, 0, 2, true);
  else if (selectingFilament) limitEncoderPos(pos, 0, filamentQty - 1, false);
  else if (selectingTemp) limitEncoderPos(pos, 30, 80, false);
  else if (selectingTime) limitEncoderPos(pos, 1, 240, false);
  
  if (debounceButton(RE_SW, released) == false && released == true) { //BUTTON PRESSED
    released = false;

    if (selectingFilament) {
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
    }

    else {
      if (selectedPos == 0){
        drawMainMenu();

      } else if (selectedPos == 1){
        selectingFilament = true;

        drawFilament(selectedFilament, 2);
        encoder.setPosition(selectedFilament);
        pos=selectedFilament;
      } else if (selectedPos == 2) {
        currentMenu = 2;
        startDryingTime = millis();
        //startDryingTime = millis()-filament[selectedFilament].dryingTime+10000; Debug feature to test if it Stops when time runs out
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

  } else {
    if (pos == 0 && selectedPos != 0) { //Hovering back button
      selectedPos = 0;
      canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //unhighlighting other options
      canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
      canvasStartButton.print("START");
      tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());
      drawFilament(selectedFilament, 0);

      canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
      canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);//0x18C3
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasBackButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
      canvasBackButton.print("Back");
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasBackButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
      
      tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());
      

    } else if (pos == 1 && selectedPos != 1) { //Hovering filament selection
      selectedPos = 1;

      canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
      canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasBackButton.print("Back");
      tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());

      canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
      canvasStartButton.print("START");
      tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());

      drawFilament(selectedFilament, 1); //highlighting filament selection bar

    } else if (pos == 2 && selectedPos != 2){
      selectedPos = 2;

      canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
      canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasBackButton.print("Back");
      tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());
      drawFilament(selectedFilament, 0);

      canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
      canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasStartButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
      canvasStartButton.print("START");
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasStartButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
      tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());
    }
  }
  
} else if (currentMenu == 2) /* Drying */ {
  static bool paused = false;
  static uint32_t timeWhenPaused = 0;
  if (!drewDryingMenu) {
    drawDryingMenu();
    drewDryingMenu = true;
  }
  limitEncoderPos(pos, 0, 1, true);
  if (millis()-timeSinceLastTimeUpdate > 700){
    aht.getEvent(&hum, &temp);
    temperature = temp.temperature;
    humidity = hum.relative_humidity;

    updateTimeLeft();
    updateTemperature();
    updateHumidity();

    // HEATER, FAN AND MOTOR BEHAVIOUR
    uint8_t fanPower = constrain(map(settings.fanSpeed, 0, 100, 40, 255), 0, 255);
    if (!paused){
      analogWrite(FAN_PIN, fanPower);
/*
      if (millis()-timeSinceLastMotorSpin > hour/2){ //MOTOR
        digitalWrite(MOTOR_PIN, HIGH);
        timeSinceLastMotorSpin = millis();
      } else digitalWrite(MOTOR_PIN, LOW);
*/
      if (filament[selectedFilament].dryingTemperature - temperature > 5){
        if (!underpoweredPSU) digitalWrite(HEATER_PIN, HIGH);
        else analogWrite(HEATER_PIN, 150);
        tft.fillRect(0, 165, 320, 5, colorTheme[settings.selectedColorTheme].accentColor);

      } else {
        uint8_t heaterPower = constrain(map(temperature*10, filament[selectedFilament].dryingTemperature*10-50, filament[selectedFilament].dryingTemperature*10, 255, 0), 0, 255);
        if (underpoweredPSU) heaterPower = constrain(map(temperature*10, filament[selectedFilament].dryingTemperature*10-50, filament[selectedFilament].dryingTemperature*10, 150, 0), 0, 150);
        analogWrite(HEATER_PIN, heaterPower); //PWM on the heater
      
        uint16_t powerBarWidth = constrain(map(temperature*10, filament[selectedFilament].dryingTemperature*10-50, filament[selectedFilament].dryingTemperature*10, 320, 0), 0, 320);
        canvasPowerBar.fillScreen(colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
        canvasPowerBar.fillRect(0, 0, powerBarWidth, 5, colorTheme[settings.selectedColorTheme].accentColor);
        tft.drawRGBBitmap(0, 165, canvasPowerBar.getBuffer(), canvasPowerBar.width(), canvasPowerBar.height());
      }
    } else {
      digitalWrite(HEATER_PIN, LOW);
      digitalWrite(FAN_PIN, LOW);
      canvasPowerBar.fillScreen(0x34FF);
      tft.drawRGBBitmap(0, 165, canvasPowerBar.getBuffer(), canvasPowerBar.width(), canvasPowerBar.height());
    }
    timeSinceLastTimeUpdate = millis();
  } 

  // BUTTON BEHAVIOUR
  canvasMMButton.setTextColor(0);
  if (pos == 0 && selectedPos != 0){
    selectedPos=0;
    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(170, 6, 80, 26, 9, 0xF987);
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

    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
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
  canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);

  if (paused){
    pauseTime = millis()-timeWhenPaused;
  }
  
  if (debounceButton(RE_SW, released) == false && released == true) { //BUTTON PRESSED
    released = false;
    if (selectedPos == 0){
      tft.setTextColor(0);
      if (!paused){
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
      tft.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    } else if (selectedPos == 1){
      digitalWrite(HEATER_PIN, LOW);
      digitalWrite(FAN_PIN, LOW);
      pauseTime = 0;
      currentMenu = 0;
      drewDryingMenu = false;
      paused = false;
      pos = 0;
      selectedPos = 1;
      encoder.setPosition(0);
      drawMainMenu();
    }
    }
    if (timeLeft < 1000 || timeLeft > hour*240) {
      digitalWrite(HEATER_PIN, LOW);
      digitalWrite(FAN_PIN, LOW);
      pauseTime = 0;
      currentMenu = 0;
      drewDryingMenu = false;
      paused = false;
      pos = 0;
      selectedPos = 1;
      encoder.setPosition(0);
      drawMainMenu();
  }
} else if (currentMenu == 3) /* AnnealingSEL */ {
  if (!drewAnnealingSelectionMenu) {
    drawAnnealingSelectionMenu();
    drewAnnealingSelectionMenu=true;
  }
  if (!selectingTime && !selectingTemp) limitEncoderPos(pos, 0, 3, true);
  if (selectingTime) limitEncoderPos(pos, 0, 120, false);
  if (selectingTemp) limitEncoderPos(pos, 40, 80, false);

  if (debounceButton(RE_SW, released) == false && released == true) {
    released = false;
    if (selectingTime) {
      annealingTime = pos;
      selectingTime = false;
      pos = 1;
      selectedPos = 2;
      encoder.setPosition(1);
    } else {
      if (selectingTemp){
      vicatTemperature = pos;
      selectingTemp = false;
      pos = 2;
      selectedPos = 3;
      encoder.setPosition(2);
      } else {
      if (pos == 0) {
      currentMenu = 0;
      pos = 1;
      selectedPos = 0;
      canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
      drewMainMenu = false;
      drewAnnealingSelectionMenu = false;
    } else if (pos == 1) {
      selectingTime = true;
      pos = annealingTime;
      encoder.setPosition(annealingTime);
      selectedPos = annealingTime;
    } else if (pos == 2) {
      selectingTemp = true;
      encoder.setPosition(vicatTemperature);
      selectedPos = vicatTemperature;
    } else if (pos == 3) {
      drewAnnealingSelectionMenu = false;
      pos = 0;
      selectedPos = 0;
      encoder.setPosition(0);
      currentMenu = 6;
    }
    }
  }
  }

if (!selectingTime && !selectingTemp){
  if (pos == 0 && selectedPos != 0){
    canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
    canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor); //BACK
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasBackButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasBackButton.print("Back");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasBackButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());

    canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //START
    canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
    canvasStartButton.print("START");
    tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());

    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    if (annealingTime < 10) canvasMMButton.fillRoundRect(184, 0, 38, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    if (annealingTime < 100 && annealingTime >= 10) canvasMMButton.fillRoundRect(184, 0, 50, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    if (annealingTime >= 100) canvasMMButton.fillRoundRect(184, 0, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.fillRoundRect(148, 16, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //Vicat temp
    
    canvasMMButton.setCursor(0, 0);
    canvasMMButton.print("Annealing time: ");
    canvasMMButton.print(annealingTime);
    canvasMMButton.println("m");
    canvasMMButton.print("Vicat temp.: ");
    canvasMMButton.print(vicatTemperature);
    canvasMMButton.drawRect(184, 16, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
    canvasMMButton.setCursor(194, 16);
    canvasMMButton.println("C");

    tft.drawRGBBitmap(0, 51, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height()); // Annealing time and vicat temp

    selectedPos = pos;
  } if (pos == 1 && selectedPos != 1){
    canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
    canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //BACK
    canvasBackButton.print("Back");
    tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());

    canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //START
    canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
    canvasStartButton.print("START");
    tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());

    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    if (annealingTime < 10) canvasMMButton.fillRoundRect(184, 0, 38, 16, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    if (annealingTime < 100 && annealingTime >= 10) canvasMMButton.fillRoundRect(184, 0, 50, 16, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    if (annealingTime >= 100) canvasMMButton.fillRoundRect(184, 0, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.fillRoundRect(148, 16, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //Vicat temp
    
    canvasMMButton.setCursor(0, 0);
    canvasMMButton.print("Annealing time: ");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print(annealingTime);
    canvasMMButton.println("m");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    canvasMMButton.print("Vicat temp.: ");
    canvasMMButton.print(vicatTemperature);
    canvasMMButton.drawRect(184, 16, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
    canvasMMButton.setCursor(194, 16);
    canvasMMButton.println("C");

    tft.drawRGBBitmap(0, 51, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height()); // Annealing time and vicat temp

    selectedPos = pos;
  } if (pos == 2 && selectedPos != 2){
    canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
    canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //BACK
    canvasBackButton.print("Back");
    tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());

    canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //START
    canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
    canvasStartButton.print("START");
    tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());

    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    if (annealingTime < 10) canvasMMButton.fillRoundRect(184, 0, 38, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    if (annealingTime < 100 && annealingTime >= 10) canvasMMButton.fillRoundRect(184, 0, 50, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    if (annealingTime >= 100) canvasMMButton.fillRoundRect(184, 0, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.fillRoundRect(148, 16, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor); //Vicat temp
    
    canvasMMButton.setCursor(0, 0);
    canvasMMButton.print("Annealing time: ");
    canvasMMButton.print(annealingTime);
    canvasMMButton.println("m");
    canvasMMButton.print("Vicat temp.: ");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg){
      canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
      canvasMMButton.drawRect(184, 16, 4, 4, colorTheme[settings.selectedColorTheme].backgroundColor);
    } else canvasMMButton.drawRect(184, 16, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
    canvasMMButton.print(vicatTemperature);
    canvasMMButton.setCursor(194, 16);
    canvasMMButton.println("C");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);

    tft.drawRGBBitmap(0, 51, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height()); // Annealing time and vicat temp

    selectedPos = pos;
  } if (pos == 3 && selectedPos != 3){
    canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
    canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //BACK
    canvasBackButton.print("Back");
    tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height());

    canvasStartButton.fillRoundRect(0, 0, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor); //START
    canvasStartButton.setCursor(getCenteredTextX(0, 84, 5), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasStartButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasStartButton.print("START");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasStartButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    tft.drawRGBBitmap(118, 138, canvasStartButton.getBuffer(), canvasStartButton.width(), canvasStartButton.height());

    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    if (annealingTime < 10) canvasMMButton.fillRoundRect(184, 0, 38, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    if (annealingTime < 100 && annealingTime >= 10) canvasMMButton.fillRoundRect(184, 0, 50, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    if (annealingTime >= 100) canvasMMButton.fillRoundRect(184, 0, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.fillRoundRect(148, 16, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    
    canvasMMButton.setCursor(0, 0);
    canvasMMButton.print("Annealing time: ");
    canvasMMButton.print(annealingTime);
    canvasMMButton.println("m");
    canvasMMButton.print("Vicat temp.: ");
    canvasMMButton.print(vicatTemperature);
    canvasMMButton.drawRect(184, 16, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
    canvasMMButton.setCursor(194, 16);
    canvasMMButton.println("C");

    tft.drawRGBBitmap(0, 51, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height()); // Annealing time and vicat temp

    selectedPos = pos;
  }
} else if (selectingTime) {
      canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
      if (pos < 10) canvasMMButton.fillRoundRect(184, 0, 38, 16, 9, colorTheme[settings.selectedColorTheme].accentColor);
      if (pos < 100 && pos >= 10) canvasMMButton.fillRoundRect(184, 0, 50, 16, 9, colorTheme[settings.selectedColorTheme].accentColor);
      if (pos >= 100) canvasMMButton.fillRoundRect(184, 0, 62, 16, 9, colorTheme[settings.selectedColorTheme].accentColor);
      canvasMMButton.fillRoundRect(148, 16, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //Vicat temp
      
      canvasMMButton.setCursor(0, 0);
      canvasMMButton.print("Annealing time: ");
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnAccentBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
      canvasMMButton.print(pos);
      canvasMMButton.println("m");
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnAccentBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
      canvasMMButton.print("Vicat temp.: ");
      canvasMMButton.print(vicatTemperature);
      canvasMMButton.drawRect(184, 16, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
      canvasMMButton.setCursor(194, 16);
      canvasMMButton.println("C");

      tft.drawRGBBitmap(0, 51, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height()); // Annealing time and vicat temp
} else if (selectingTemp) {
      canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
      if (pos < 10) canvasMMButton.fillRoundRect(184, 0, 38, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      if (pos < 100 && pos >= 10) canvasMMButton.fillRoundRect(184, 0, 50, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      if (pos >= 100) canvasMMButton.fillRoundRect(184, 0, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);

      canvasMMButton.fillRoundRect(148, 16, 62, 16, 9, colorTheme[settings.selectedColorTheme].accentColor); //Vicat temp
      
      canvasMMButton.setCursor(0, 0);
      canvasMMButton.print("Annealing time: ");
      canvasMMButton.print(annealingTime);
      canvasMMButton.println("m");

      canvasMMButton.print("Vicat temp.: ");
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnAccentBg) {
        canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
        canvasMMButton.drawRect(184, 16, 4, 4, colorTheme[settings.selectedColorTheme].backgroundColor);
      } else canvasMMButton.drawRect(184, 16, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
      canvasMMButton.print(pos);
      canvasMMButton.setCursor(194, 16);
      canvasMMButton.println("C");
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnAccentBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);

      tft.drawRGBBitmap(0, 51, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height()); // Annealing time and vicat temp
}
  
} else if (currentMenu == 4) /* Settings */{
  if (!selectingFanSpeed) limitEncoderPos(pos, 0, 2, true);
  if (!drewSettingsMenu){
    drawSettingsMenu();
    drewSettingsMenu = true;
  }
  if (selectingFanSpeed){
    limitEncoderPos(pos, 0, 10, false);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 7), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Fan ");
    if (pos == 0) {
      canvasMMButton.fillRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
      canvasMMButton.drawRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (pos < 10 && pos != 0) {
      canvasMMButton.fillRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
      canvasMMButton.drawRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (pos == 10) {
      canvasMMButton.fillRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
      canvasMMButton.drawRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print(pos*10);
    canvasMMButton.print("%");
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

  } else if (pos == 0 && selectedPos != 0) {
    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 10), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print("Appearance"); //Highlighting Appearance
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 7), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Fan "); //Unhighlighting Fan speed
    if (settings.fanSpeed == 0) {
      canvasMMButton.fillRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (settings.fanSpeed < 100 && settings.fanSpeed != 0) {
      canvasMMButton.fillRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (settings.fanSpeed == 100) {
      canvasMMButton.fillRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    canvasMMButton.print(settings.fanSpeed);
    canvasMMButton.print("%");
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 4), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Save"); //Unhighlighting Save
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    selectedPos = 0;
  } else if (pos == 1 && selectedPos != 1) {
    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 10), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Appearance"); //Unhighlighting Appearance
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 7), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print("Fan "); //Highlighting Fan
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    if (settings.fanSpeed == 0) {
      canvasMMButton.fillRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (settings.fanSpeed < 100 && settings.fanSpeed != 0) {
      canvasMMButton.fillRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (settings.fanSpeed == 100) {
      canvasMMButton.fillRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    canvasMMButton.print(settings.fanSpeed);
    canvasMMButton.print("%");
    
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 7, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 4), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Save"); //Unhighlighting Save
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    selectedPos = 1;
  } else if (pos == 2 && selectedPos != 2) {
    selectedPos = 2;

    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 10), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Appearance"); //Unhighlighting Appearance
    tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 7), getCenteredTextY(0, 32, 1));
    canvasMMButton.print("Fan "); //Unhighlighting Fan
    if (settings.fanSpeed == 0) {
      canvasMMButton.fillRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (settings.fanSpeed < 100 && settings.fanSpeed != 0) {
      canvasMMButton.fillRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    if (settings.fanSpeed == 100) {
      canvasMMButton.fillRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.drawRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    }
    canvasMMButton.print(settings.fanSpeed);
    canvasMMButton.print("%");
    tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
    

    canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.setCursor(getCenteredTextX(padding, 260, 4), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print("Save"); //Highlighting Save
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
  }

  
  if (debounceButton(RE_SW, released) == false && released == true) { //BUTTON PRESSED
    released = false;
    selectedPos = 0;
    if (selectingFanSpeed) {
      selectingFanSpeed = false;
      settings.fanSpeed = pos*10;
      pos = 1;
      selectedPos = 1;
      encoder.setPosition(1);

      canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
      canvasMMButton.setCursor(getCenteredTextX(padding, 260, 7), getCenteredTextY(0, 32, 1));
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
      canvasMMButton.print("Fan ");
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
      if (settings.fanSpeed == 0) {
        canvasMMButton.fillRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
        canvasMMButton.drawRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
      }
      if (settings.fanSpeed < 100 && settings.fanSpeed != 0) {
        canvasMMButton.fillRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
        canvasMMButton.drawRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
      }
      if (settings.fanSpeed == 100) {
        canvasMMButton.fillRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
        canvasMMButton.drawRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
      }
      canvasMMButton.print(settings.fanSpeed);
      canvasMMButton.print("%");
      tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

    } else if (pos == 0){
      currentMenu = 5;
      drewAppearanceMenu = false;
      drewSettingsMenu = false;
      pos = settings.selectedColorTheme;
      selectedPos = settings.selectedColorTheme;
      encoder.setPosition(settings.selectedColorTheme);
    } else if (pos == 1){
      canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
      canvasMMButton.setCursor(getCenteredTextX(padding, 260, 7), getCenteredTextY(0, 32, 1));
      
      canvasMMButton.print("Fan ");
      if (settings.fanSpeed == 0) {
        canvasMMButton.fillRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
        canvasMMButton.drawRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
      }
      if (settings.fanSpeed < 100 && settings.fanSpeed != 0) {
        canvasMMButton.fillRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
        canvasMMButton.drawRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
      }
      if (settings.fanSpeed == 100) {
        canvasMMButton.fillRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
        canvasMMButton.drawRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
      }
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
      canvasMMButton.print(settings.fanSpeed);
      canvasMMButton.print("%");
      tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
      selectingFanSpeed = true;
      encoder.setPosition(settings.fanSpeed/10);
      pos = settings.fanSpeed/10;
      if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    } else if (pos == 2){
      currentMenu = 0;
      drewAppearanceMenu = false;
      drewSettingsMenu = false;
      drewMainMenu = false;
      saveSettings();
    } 
  }
} else if (currentMenu == 5) /* Appearance */{
  limitEncoderPos(pos, 0, 12, true);
  if (!drewAppearanceMenu){
    drawColorThBox(settings.selectedColorTheme);
    drewAppearanceMenu = true;
  }
  
  if (pos != selectedPos){
    drawColorThBox(pos);
    selectedPos = pos;
  }
  if (debounceButton(RE_SW, released) == false && released == true) {
    released = false;

    settings.selectedColorTheme = pos;
    tft.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    canvasBackButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    canvasStartButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
    canvasBackButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasStartButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);

    currentMenu = 4;
    drewAppearanceMenu = false;

    selectedPos = 0;
    pos = 0;
    encoder.setPosition(0);
  }


} else if (currentMenu == 6) /* Annealing */{
static uint32_t timeWhenPaused = 0;
if (!drewAnnealingMenu) {
  drawAnnealingMenu();
  updateTemperature();
  temperatureBeforeAnnealing = temperature;
  drewAnnealingMenu = true;
  startAnnealingTime = millis();
}
limitEncoderPos(pos, 0, 1, true);
if (millis()-timeSinceLastTimeUpdate > 1000){
  aht.getEvent(&hum, &temp);
  temperature = temp.temperature;
  humidity = hum.relative_humidity;

  updateAnnealingTimeLeft();
  updateTemperature();
  updateHumidity();

  // HEATER AND FAN BEHAVIOUR
  uint8_t fanPower = constrain(map(settings.fanSpeed, 0, 100, 40, 255), 0, 255);
  analogWrite(FAN_PIN, fanPower);

  if (filament[selectedFilament].dryingTemperature - temperature > 5){
    digitalWrite(HEATER_PIN, HIGH);
    tft.fillRect(0, 165, 320, 5, colorTheme[settings.selectedColorTheme].accentColor);
  } else {
    uint8_t heaterPower;
    uint16_t powerBarWidth;
    if (!stressReliefFinished){
      heaterPower = constrain(map(temperature*10, (vicatTemperature-5)*10-50, (vicatTemperature-5)*10, 255, 0), 0, 255);
      powerBarWidth = constrain(map(temperature*10, (vicatTemperature-5)*10-50, (vicatTemperature-5)*10-50, 320, 0), 0, 320);
    } else {
      heaterPower = constrain(map(temperature*10, 800-50, 800, 255, 0), 0, 255);
      powerBarWidth = constrain(map(temperature*10, 800-50, 800, 320, 0), 0, 320);
    }
    analogWrite(HEATER_PIN, heaterPower); //PWM on the heater
    
    canvasPowerBar.fillScreen(colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasPowerBar.fillRect(0, 0, powerBarWidth, 5, colorTheme[settings.selectedColorTheme].accentColor);
    tft.drawRGBBitmap(0, 165, canvasPowerBar.getBuffer(), canvasPowerBar.width(), canvasPowerBar.height());
  }
  timeSinceLastTimeUpdate = millis();
} 

if (debounceButton(RE_SW, released) == false && released == true) { //BUTTON PRESSED
  released = false;
  digitalWrite(HEATER_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  currentMenu = 0;
  drewAnnealingMenu = false;
  pos = 0;
  selectedPos = 1;
  encoder.setPosition(0);
  drawMainMenu();
  stressReliefFinished = false;
  }

if ((timeLeft < 1000 || timeLeft > hour*240 ) && stressReliefFinished == true) {
  digitalWrite(HEATER_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  drewAnnealingMenu = false;
  pos = 0;
  selectedPos = 1;
  encoder.setPosition(0);
  canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  currentMenu = 7;
  stressReliefFinished = false;
}
} else if (currentMenu == 7) /* Cooling down */ {
  if (!drewCoolDownMenu) {
    drawCoolDownMenu();
    drewCoolDownMenu = true;
  }
  if (timeSinceLastTimeUpdate-millis() > 1000){
  aht.getEvent(&hum, &temp);
  temperature = temp.temperature;
  humidity = hum.relative_humidity;

  if (temperature <= temperatureBeforeAnnealing+5){
    currentMenu = 0;
    drawMainMenu();
    stressReliefFinished = false;
    canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  } else {
    canvasMMButton.fillRect(0, 0, 48, 16, colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.setCursor(0, 0);
    canvasMMButton.print(temperature);
    canvasMMButton.drawRect(64, 0, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
    canvasMMButton.setCursor(74, 0);
    canvasMMButton.println("C");
    tft.drawRGBBitmap(180, 48, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
  }
  timeSinceLastTimeUpdate = millis();
}
}

if (debounceButton(RE_SW, released) == true && released == false) released = true; //BUTTON RELEASED
}


bool debounceButton(int pin, bool released) {
  bool currentlyPressed = digitalRead(pin);
  if (released != currentlyPressed) {
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

  tft.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  tft.fillRoundRect(padding, 12, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor); //Drying button BG
  tft.fillRoundRect(padding, 56, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //Annealing button BG
  tft.fillRoundRect(padding, 100, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //Settings button BG
  tft.setCursor(getCenteredTextX(padding, 260, 6), getCenteredTextY(12, 32, 1));
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) tft.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
  tft.print("Drying");
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) tft.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
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
  tft.drawBitmap(144, 140, canvas.getBuffer(), canvas.width(), canvas.height(), colorTheme[settings.selectedColorTheme].textColor, colorTheme[settings.selectedColorTheme].backgroundColor);
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("CHECK TEMPERATURE SENSOR!");
    tft.println("Init. FAILED, NO TEMP DATA");
    tft.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  }

  selectedPos = 0;
  currentMenu = 0;
}

void drawFilamentSelectionMenu(){
  tft.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  tft.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
  tft.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) tft.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
  tft.print("Back");
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) tft.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  tft.setCursor(getCenteredTextX(0, 320, 15),0);
  tft.print("Select filament");
  drawFilament(selectedFilament, 0);
  tft.fillRoundRect(118, 138, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
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
  canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasMMButton.fillRoundRect(0, 0, 320, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
  if (type > 0) {
    canvasMMButton.setCursor(12,8);
    canvasMMButton.print(filament[type-1].name);
  }
  if (highlight == 1) {
    canvasMMButton.fillRoundRect(115, 0, 90, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    canvasMMButton.drawRoundRect(115, 0, 90, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    canvasMMButton.setCursor(getCenteredTextX(0, 320, filament[type].name.length()), getCenteredTextY(0, 32, 1));
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print(filament[type].name);
    if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  }
  if (highlight == 2) {
    canvasMMButton.fillRoundRect(115, 0, 90, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
    canvasMMButton.setCursor(getCenteredTextX(0, 320, filament[type].name.length()), getCenteredTextY(0, 32, 1));
    if (!colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.print(filament[type].name);
    if (!colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  } else if (highlight == 0) {
    canvasMMButton.drawRoundRect(115, 0, 90, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
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
  canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  if (type == 0){
    uint8_t timeLenght = 38;
    if (filament[type].dryingTime/hour > 9 && filament[type].dryingTime/hour < 100) timeLenght+=12;
    if (filament[type].dryingTime/hour > 99) timeLenght+=24;
    if (selectingTemp == false && selectingTime == false){
      canvasMMButton.fillRoundRect(160, 0, 60, 15, 3, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.fillRoundRect(65, 16, timeLenght, 15, 3, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    } else if (selectingTemp == true) {
      canvasMMButton.fillRoundRect(160, 0, 60, 15, 3, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
      canvasMMButton.fillRoundRect(65, 16, timeLenght, 15, 3, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    } else if (selectingTime == true) {
      canvasMMButton.fillRoundRect(160, 0, 60, 15, 3, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
      canvasMMButton.fillRoundRect(65, 16, timeLenght, 15, 3, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
    }
  }
  canvasMMButton.setCursor(0, 0);
  canvasMMButton.print("Drying temp.: ");
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg && selectingTemp){
    canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
    canvasMMButton.drawRect(195, 0, 4, 4, colorTheme[settings.selectedColorTheme].backgroundColor);
  } else canvasMMButton.drawRect(195, 0, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
  canvasMMButton.print(filament[type].dryingTemperature);  
  canvasMMButton.setCursor(202, 0);
  canvasMMButton.println("C");
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg && selectingTemp) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  canvasMMButton.print("Time: ");
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg && selectingTime) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasMMButton.print(filament[type].dryingTime / hour);
  canvasMMButton.print("h");
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg && selectingTime) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor); 
  
  tft.drawRGBBitmap(0, 70, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());
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

void writeColorThemes(){
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
  colorTheme[1].accentColor = 0x3D7C;
  colorTheme[1].textColor = 0x0000;
  colorTheme[1].flipTextColorOnSelectedBg = true;
  colorTheme[1].flipTextColorOnAccentBg = true;

  colorTheme[2].name = "Icy dark"; //https://www.realtimecolors.com/?colors=ddf6fe-000c0f-6acbfb-040599-501ef8&fonts=Inter-Inter
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
  colorTheme[3].flipTextColorOnSelectedBg = true;
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
  colorTheme[5].flipTextColorOnSelectedBg = true;

  colorTheme[6].name = "Northern Lights"; //https://www.realtimecolors.com/?colors=e8fcf7-010404-18d8b1-0e2b81-2213a4&fonts=Inter-Inter
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

  colorTheme[9].name = "Green banana"; //https://www.realtimecolors.com/?colors=1f220c-f6f8ec-596124-95d4a7-75c6ac&fonts=Inter-Inter
  colorTheme[9].backgroundColor = 0xF7DD;
  colorTheme[9].buttonUnselectedColor = 0xDF7A;
  colorTheme[9].buttonSelectedColor = 0x5B04;
  colorTheme[9].accentColor = 0x7635;
  colorTheme[9].textColor = 0x1901;
  colorTheme[9].flipTextColorOnSelectedBg = true;
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

  //Thanks to realtimecolors.com for making the process of selecting matching colors so much easier
}

void drawDryingMenu(){
  aht.getEvent(&hum, &temp);
  temperature = temp.temperature;
  humidity = hum.relative_humidity;

  tft.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  tft.setCursor(getCenteredTextX(0, 320, 6), 0);
  tft.print("DRYING");
  tft.setCursor(0, 26);
  tft.println("Current temp.: ");
  updateTemperature();
  
  tft.println("R. humidity: ");
  updateHumidity();

  tft.print("Target temp.: ");
  tft.print(filament[selectedFilament].dryingTemperature);
  tft.drawRect(195, 58, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
  tft.setCursor(201, 58);
  tft.println("C");
  

  tft.print("Time left: ");
  updateTimeLeft();

  //BUTTONS
  tft.setTextColor(0);
  tft.fillRoundRect(70, 129, 80, 26, 9, 0xFE2E);
  tft.setCursor(getCenteredTextX(70, 80, 5), getCenteredTextY(129, 26, 1));
  tft.print("PAUSE");

  tft.fillRoundRect(170, 129, 80, 26, 9, 0xF987);
  tft.setCursor(getCenteredTextX(170, 80, 4), getCenteredTextY(129, 26, 1));
  tft.print("STOP");
  tft.setTextColor(colorTheme[settings.selectedColorTheme].textColor);

  //POWER BAR
  tft.fillRect(0, 165, 320, 5, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);

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
  tft.drawBitmap(130, 74, canvas.getBuffer(), canvas.width(), canvas.height(), colorTheme[settings.selectedColorTheme].textColor, colorTheme[settings.selectedColorTheme].backgroundColor);

}

void updateTemperature(){
  canvas1.fillScreen(0);
  canvas1.setCursor(0, 0);
  canvas1.printf("%04.1f" , temperature);
  canvas1.drawRect(50, 0, 4, 4, 0xFFFF);
  canvas1.setCursor(56, 0);
  canvas1.print("C");
  tft.drawBitmap(172, 26, canvas1.getBuffer(), canvas1.width(), canvas1.height(), colorTheme[settings.selectedColorTheme].textColor, colorTheme[settings.selectedColorTheme].backgroundColor);
}

void updateHumidity(){
  canvas1.fillScreen(0);
  canvas1.setCursor(0, 0);
  canvas1.printf("%2.1f" , humidity);
  canvas1.print("%");
  tft.drawBitmap(152, 42, canvas1.getBuffer(), canvas1.width(), canvas1.height(), colorTheme[settings.selectedColorTheme].textColor, colorTheme[settings.selectedColorTheme].backgroundColor);
}

uint16_t getCenteredTextX(uint16_t xStart, uint16_t width, uint8_t textLenght) {
    return xStart+(width/2)-textLenght*6+1;
}

uint8_t getCenteredTextY (uint8_t yStart, uint8_t height, uint8_t lines){
  return yStart+(height/2)-8*lines+1;
}

void loadSettings(){
  EEPROM.get(0, settings);

  if (settings.selectedColorTheme == 255){ //Restoring defaults if none set
    settings.selectedColorTheme = 0;
    settings.fanSpeed = 100;
  }
}

void saveSettings(){
  EEPROM.get(0, currentStored);
  if (memcmp(&settings, &currentStored, sizeof(settings)) != 0){
    EEPROM.put(0, settings);
    EEPROM.commit();
  }
}

void drawAnnealingSelectionMenu(){
  
  tft.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);

  canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  if (annealingTime < 10) canvasMMButton.fillRoundRect(184, 0, 38, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
  if (annealingTime < 100 && annealingTime >= 10) canvasMMButton.fillRoundRect(184, 0, 50, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
  if (annealingTime >= 100) canvasMMButton.fillRoundRect(184, 0, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
  tft.drawRGBBitmap(0, 50, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height()); // Annealing time

  tft.fillRoundRect(148, 67, 62, 16, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); // Vicat Temperature

  canvasBackButton.setCursor(getCenteredTextX(0, 62, 4), getCenteredTextY(0, 23, 1));
  canvasBackButton.fillRoundRect(0, 0, 62, 23, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasBackButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasBackButton.print("Back");
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasBackButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  tft.drawRGBBitmap(0, 0, canvasBackButton.getBuffer(), canvasBackButton.width(), canvasBackButton.height()); //Back button

  tft.setCursor(0, 35);
  tft.print("Anl. temperature: 80");
  tft.drawRect(244, 35, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
  tft.setCursor(254, 35);
  tft.println("C");
  tft.print("Annealing time: ");
  tft.print(annealingTime);
  tft.println("m");
  tft.print("Vicat temp.: ");
  tft.print(vicatTemperature);
  tft.drawRect(184, 67, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
  tft.setCursor(194, 67);
  tft.println("C");
  tft.print("Stress relief time: ");
  tft.print(stressReliefTime);
  tft.println("m");
  


  tft.fillRoundRect(118, 138, 84, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor); //Start button
  tft.setCursor(getCenteredTextX(118, 84, 5), getCenteredTextY(138, 32, 1));
  tft.print("START");


}

void drawSettingsMenu(){
  tft.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasMMButton.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonSelectedColor);
  canvasMMButton.setCursor(getCenteredTextX(padding, 260, 10), getCenteredTextY(0, 32, 1));
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasMMButton.print("Appearance");  
  if (colorTheme[settings.selectedColorTheme].flipTextColorOnSelectedBg) canvasMMButton.setTextColor(colorTheme[settings.selectedColorTheme].textColor);
  tft.drawRGBBitmap(0, 12, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

  canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
  canvasMMButton.setCursor(getCenteredTextX(padding, 260, 7), getCenteredTextY(0, 32, 1));
  canvasMMButton.print("Fan ");
  if (settings.fanSpeed == 0) {
    canvasMMButton.fillRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.drawRoundRect(160, 0, 38, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
  }
  if (settings.fanSpeed < 100 && settings.fanSpeed != 0) {
    canvasMMButton.fillRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.drawRoundRect(160, 0, 50, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
  }
  if (settings.fanSpeed == 100) {
    canvasMMButton.fillRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
    canvasMMButton.drawRoundRect(160, 0, 62, 32, 9, colorTheme[settings.selectedColorTheme].accentColor);
  }
  canvasMMButton.print(settings.fanSpeed);
  canvasMMButton.print("%");
  tft.drawRGBBitmap(0, 56, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

  canvasMMButton.fillRoundRect(padding, 0, 260, 32, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
  canvasMMButton.setCursor(getCenteredTextX(padding, 260, 4), getCenteredTextY(0, 32, 1));
  canvasMMButton.print("Save");
  tft.drawRGBBitmap(0, 100, canvasMMButton.getBuffer(), canvasMMButton.width(), canvasMMButton.height());

}

void drawColorThBox(uint8_t color){
  tft.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasTHBox.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  canvasTHBox.setTextColor(colorTheme[color].textColor);
  bool selected = false;
  if (color == settings.selectedColorTheme) selected = true;
  //Scroll bar
  tft.fillRect(310, 0, 10, 170, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
  tft.fillRect(310, map(color, 0, colorThemesQty-1, 0, 150), 10, 20, colorTheme[settings.selectedColorTheme].buttonSelectedColor);


  tft.setCursor(getCenteredTextX(0, 310, colorTheme[color].name.length()), 0);
  tft.print(colorTheme[color].name);
  //Outline 
  if (selected) {
    canvasTHBox.fillRoundRect(0, 0, 270, 130, 9, colorTheme[settings.selectedColorTheme].accentColor);
  }
  else{
    canvasTHBox.fillRoundRect(0, 0, 270, 130, 9, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
  }
  //buttons
  canvasTHBox.fillRoundRect(2, 2, 266, 126, 9, colorTheme[color].backgroundColor);
  canvasTHBox.fillRoundRect(11, 11, 248, 32, 9, colorTheme[color].buttonSelectedColor);
  canvasTHBox.fillRoundRect(11, 55, 248, 32, 9, colorTheme[color].buttonUnselectedColor);
  canvasTHBox.setCursor(getCenteredTextX(11, 248, 8), getCenteredTextY(11, 32, 1));
  if (colorTheme[color].flipTextColorOnSelectedBg) canvasTHBox.setTextColor(colorTheme[color].backgroundColor);
  canvasTHBox.print("selected");
  if (colorTheme[color].flipTextColorOnSelectedBg) canvasTHBox.setTextColor(colorTheme[color].textColor);
  canvasTHBox.setCursor(getCenteredTextX(11, 248, 10), getCenteredTextY(55, 32, 1));
  canvasTHBox.print("unselected");

  //Accent bar
  canvasTHBox.fillRect(11, 99, 248, 10, colorTheme[color].buttonUnselectedColor);
  canvasTHBox.fillRect(11, 99, 165, 10, colorTheme[color].accentColor);


  tft.drawRGBBitmap(20, 20, canvasTHBox.getBuffer(), canvasTHBox.width(), canvasTHBox.height());

}

void drawAnnealingMenu(){
  aht.getEvent(&hum, &temp);
  temperature = temp.temperature;
  humidity = hum.relative_humidity;

  tft.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  tft.setCursor(getCenteredTextX(0, 320, 13), 0);
  tft.print("STRESS-RELIEF");
  tft.setCursor(0, 26);
  tft.println("Current temp.: ");
  updateTemperature();
  
  tft.println("R. humidity: ");
  updateHumidity();

  tft.print("Target temp.: ");
  if (!stressReliefFinished) tft.print(vicatTemperature-5);
  else if (stressReliefFinished) tft.print(80);
  tft.drawRect(195, 58, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
  tft.setCursor(201, 58);
  tft.println("C");
  

  tft.print("Time left: ");
  startAnnealingTime = millis(); //without this line annealing breaks when run more than once per boot
  updateAnnealingTimeLeft();

  //BUTTON
  tft.setTextColor(0);
  tft.fillRoundRect(120, 129, 80, 26, 9, 0xF987);
  tft.setCursor(getCenteredTextX(120, 80, 4), getCenteredTextY(129, 26, 1));
  tft.print("STOP");
  tft.setTextColor(colorTheme[settings.selectedColorTheme].textColor);

  //POWER BAR
  tft.fillRect(0, 165, 320, 5, colorTheme[settings.selectedColorTheme].buttonUnselectedColor);
}

void updateAnnealingTimeLeft() {
  if (!stressReliefFinished){
    timeLeft = startAnnealingTime+(hour/60)*45-millis(); //The 45 is the Stress relief time
    if (timeLeft > 2 * hour || timeLeft < 1000){
      stressReliefFinished = true;
      startAnnealingTime = millis();
      tft.fillRect(0, 0, 320, 16, colorTheme[settings.selectedColorTheme].backgroundColor);
      tft.setCursor(getCenteredTextX(0, 320, 9), 0);
      tft.print("ANNEALING");
      tft.fillRect(168, 58, 24, 16, colorTheme[settings.selectedColorTheme].backgroundColor);
      tft.setCursor(168, 58);
      tft.print("80");
  }
  } if (stressReliefFinished) {
    timeLeft = startAnnealingTime+(annealingTime*hour/60)-millis(); 
  }

  canvas.fillScreen(0);
  canvas.setCursor(0, 0);
  canvas.print(timeLeft / hour);
  canvas.print("h ");
  canvas.print((timeLeft % hour) / 60000);
  canvas.print("m ");
  canvas.print((timeLeft % 60000) / 1000);
  canvas.print("s");
  tft.drawBitmap(130, 74, canvas.getBuffer(), canvas.width(), canvas.height(), colorTheme[settings.selectedColorTheme].textColor, colorTheme[settings.selectedColorTheme].backgroundColor);

}

void drawCoolDownMenu() {
  updateTemperature();
  tft.fillScreen(colorTheme[settings.selectedColorTheme].backgroundColor);
  tft.setCursor(0, 0);
  tft.println("Please wait for the part");
  tft.println("to cool down to room temp.");
  tft.print("Temp. before: ");
  tft.println(temperatureBeforeAnnealing);
  tft.drawRect(232, 32, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
  tft.setCursor(242, 32);
  tft.println("C");
  
  tft.setCursor(0, 48);
  tft.print("Current temp.: ");
  tft.print(temperature);
  tft.drawRect(184, 48, 4, 4, colorTheme[settings.selectedColorTheme].textColor);
  tft.setCursor(194, 48);
  tft.println("C");
}

//RIGHT -> padding
//Center -> 160-5*(characters+1)
//LEFT -> 320-10*characters

//CHANGELOG:
/*
Commented out motor code
*/

/* KNOWN ISSUES:
The motor does not fit within the box
*/