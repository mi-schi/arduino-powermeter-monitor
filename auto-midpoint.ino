#include <SPI.h>
#include <Adafruit_MCP3008.h>

Adafruit_MCP3008 adc[2];

const byte ACS_COUNT = 13;
const int SENSOR_LOOP = 5;

struct ACS {
  byte adcNumber;
  byte pin;
  String mVperA;
};

ACS acss[ACS_COUNT] = {
  {0, 3, "ACS_5A"},
  {0, 2, "ACS_5A"},
  {0, 1, "ACS_20A"},
  {1, 0, "ACS_20A"},
  {1, 1, "ACS_20A"},
  {1, 2, "ACS_20A"},
  {1, 3, "ACS_5A"},
  {1, 4, "ACS_5A"},
  {1, 5, "ACS_5A"},
  {0, 4, "ACS_5A"},
  {0, 5, "ACS_5A"},
  {0, 6, "ACS_5A"},
  {0, 7, "ACS_5A"},
};

void setup() {
  Serial.begin(9600);
  while (!Serial); 
  adc[0].begin(10);
  adc[1].begin(8);
}

void loop() {
  for (byte a = 0; a < ACS_COUNT; a++) {
    Serial.print("{");
    Serial.print(acss[a].adcNumber);
    Serial.print(", ");
    Serial.print(acss[a].pin);
    Serial.print(", ");
    Serial.print(acss[a].mVperA);
    Serial.print(", ");
    Serial.print(getSensorAvg(acss[a].adcNumber, acss[a].pin), 0);
    Serial.println("},");
  }
  
  Serial.println("---");
  delay(5000);
}

float getSensorAvg(byte adcNumber, byte pin) {
  unsigned long sensorRaw = 0;
  
  for (int i = 0; i < SENSOR_LOOP; i++) {
    sensorRaw += adc[adcNumber].readADC(pin);
    delay(10);
  }

  float sensorAvg = sensorRaw / float(SENSOR_LOOP);
  
  return sensorAvg;
}
