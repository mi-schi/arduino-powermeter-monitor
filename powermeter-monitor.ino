#include <LiquidCrystal.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_MCP3008.h>
#include <LowPower.h>
#include <RTClib.h>

RTC_DS1307 rtc;
LiquidCrystal lcd(A0, A1, A2, A3, A6, A7);
Adafruit_MCP3008 adc[2];
const float ARDUINO_VOLT = 4.7;
const byte SENSOR_LOOP = 5;

const byte LCD_POWER_PIN = 4;
volatile bool lcdPower = false;
volatile byte lcdView = 0;
volatile unsigned long lcdDebounceTime = 0;

const byte ACS_POWER_PIN = 5;
const byte ACS_COUNT = 13;
const float ACS_20A = 0.1;
const float ACS_5A = 0.185;
const byte ACS_BATTERY_COUNT = 4;

struct ACS {
  byte adcNumber;
  byte pin;
  float voltPerAmpere;
  int midPointAvg;
};

ACS acss[ACS_COUNT] = {
  {0, 3, ACS_5A, 508},
  {0, 2, ACS_5A, 505},
  {0, 1, ACS_20A, 509},
  {1, 0, ACS_20A, 508},
  {1, 1, ACS_20A, 508},
  {1, 2, ACS_20A, 508},
  {1, 3, ACS_5A, 505},
  {1, 4, ACS_5A, 505},
  {1, 5, ACS_5A, 506},
  {0, 4, ACS_5A, 509},
  {0, 5, ACS_5A, 509},
  {0, 6, ACS_5A, 508},
  {0, 7, ACS_5A, 507},
};

const int APPEND_INTERVAL = 900;
int appendCounter = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("set input pullup, output pin and interrrupt");
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  
  pinMode(LCD_POWER_PIN, OUTPUT);
  digitalWrite(LCD_POWER_PIN, lcdPower);
  pinMode(ACS_POWER_PIN, OUTPUT);
  digitalWrite(ACS_POWER_PIN, LOW);

  attachInterrupt(0, changeLCDPower, CHANGE);
  attachInterrupt(1, changeLCDView, CHANGE);

  delay(100);
  
  Serial.println("begin RTC");
  rtc.begin();
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  Serial.println("set up LCD display");
  lcd.begin(16, 2);
  
  Serial.println("begin adc MCP3008");
  adc[0].begin(10);
  adc[1].begin(8);

  Serial.println("initializing SD card");
  if (!SD.begin(9)) {
    while (true);
  }

  Serial.println("---");
  delay(1000);    
}

void loop() {
  bool shouldAppend = appendCounter >= APPEND_INTERVAL;
  
  if (shouldAppend || lcdPower) {
    digitalWrite(ACS_POWER_PIN, HIGH);
    delay(100);
    
    float volt = getVolt();
    float amperes[ACS_COUNT];
    for (byte a = 0; a < ACS_COUNT; a++) {
      amperes[a] = getAmpere(acss[a]);
    }

    digitalWrite(ACS_POWER_PIN, LOW);

    if (lcdPower) {
      showLCD(volt, amperes);
      delay(1000);
      appendCounter++;
    }

    if (shouldAppend) {
      appendPowerCSV(volt, amperes);
      appendCounter = 0;
    }    
  } else {
    delay(100);   
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    appendCounter = appendCounter + 8;
  }
}

void changeLCDView() {
  if (millis() - lcdDebounceTime > 500) {
    lcdView = lcdView + 2;
  
    if (lcdView > ACS_COUNT) {
      lcdView = 0;
    }

    lcdDebounceTime = millis();
  }
}

void changeLCDPower() {
  if (millis() - lcdDebounceTime > 500) {
    lcdPower = !lcdPower;
    digitalWrite(LCD_POWER_PIN, lcdPower);
    lcdDebounceTime = millis();
  }
}

float getVolt() {
  return getSensorVolt(getSensorAvg(0, 0) * ((29900.0 + 7520.0) / 7520.0));
}

float getAmpere(ACS acs) {
  return getSensorVolt(getSensorAvg(acs.adcNumber, acs.pin) - acs.midPointAvg) / acs.voltPerAmpere;
}

float getSensorVolt(int sensorValue) {
  return sensorValue * ARDUINO_VOLT / 1023;
}

int getSensorAvg(byte adcNumber, byte pin) {
  unsigned long sensorRaw = 0;
  
  for (byte i = 0; i < SENSOR_LOOP; i++) {
    sensorRaw += adc[adcNumber].readADC(pin);      
  }

  return sensorRaw / SENSOR_LOOP;
}

void showLCD(float volt, float amperes[ACS_COUNT]) {
  lcd.clear();

  if (lcdView == 0) {
    float batteryAmpere = 0;
    for (byte b = 0; b < ACS_BATTERY_COUNT; b++) {
      batteryAmpere += amperes[b];
    }
  
    float pvAmpere = 0, loadAmpere = 0;
    for (byte a = ACS_BATTERY_COUNT; a < ACS_COUNT; a++) {
      float ampere = amperes[a];

      if (ampere < 0) {
        pvAmpere -= ampere;
      }
      if (ampere > 0) {
        loadAmpere += ampere;
      }
    }
   
    lcd.print(volt, 2);
    lcd.print("V  PV ");
    lcd.print(volt * pvAmpere, 0);
    lcd.print("W");
  
    lcd.setCursor(0, 1);
    lcd.print("BAT ");
    lcd.print(volt * batteryAmpere, 0);
    lcd.print("W  LD ");
    lcd.print(volt * loadAmpere, 0);
    lcd.print("W");
  } else{
    for (byte r = 0; r < 2; r++) {
      byte acsNumber = lcdView - 1 + r;
      lcd.setCursor(0, r);
      lcd.print(acsNumber);
      lcd.print(": ");
      lcd.print(volt * amperes[acsNumber - 1], 1);
      lcd.print("W ");
      lcd.print(amperes[acsNumber - 1], 1);
      lcd.print("A"); 
    }
  }
}

void appendPowerCSV(float volt, float amperes[ACS_COUNT]) {
  File powerFile = SD.open("power.csv", FILE_WRITE);

  if (powerFile) {
    DateTime now = rtc.now();

    char dateTime[19] = {
      now.day(), ".", now.month(), ".", now.year(), " ",
      now.hour(), ":", now.minute(), ":", now.second()
    };

    Serial.print(dateTime);
    powerFile.print(dateTime);
    
    Serial.print(";");
    Serial.print(volt);
    
    powerFile.print(";");
    powerFile.print(volt);

    for (byte s = 0; s < ACS_COUNT; s++) {
      Serial.print(";");
      Serial.print(amperes[s], 2);
      
      powerFile.print(";");
      powerFile.print(amperes[s], 2);
    }

    Serial.print("\n");
    powerFile.print("\n");
    powerFile.close();
    
    return;
  }

  Serial.println("ERROR: opening power.csv");
}
