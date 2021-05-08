#include <Arduino.h>
#include <SoftwareSerial.h>

#define DEBUG 1

const int pinDebugRx = 12;
const int pinDebugTx = 13;
SoftwareSerial SerialDebug(pinDebugRx, pinDebugTx); // RX, TX

const int pinLatch = 10;  // LOW = Load, HIGH = shift 
const int pinClk = 11;

const int pinData[] = {2, 3};  // list of digital pins
const int registerCount = sizeof(pinData) / sizeof(int);

uint16_t registerState[registerCount]; // the current reading from the input pin
uint16_t lastRegisterState[registerCount];   // the previous reading from the registers

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime[registerCount];  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

void setup() {
  if (DEBUG) {
    SerialDebug.begin(38400);
    SerialDebug.println("Starting up");
  }
  // data link
  Serial.begin(115200);

  pinMode(pinLatch, OUTPUT);
  pinMode(pinClk, OUTPUT);
  digitalWrite(pinLatch, HIGH);
  digitalWrite(pinClk, LOW);

  for(int i=0; i<registerCount; ++i) {
    pinMode(pinData[i], INPUT);
    registerState[i] = lastRegisterState[i] = lastDebounceTime[i] = 0;
  }
}

void loop() {
  bool send = false;
  digitalWrite(pinLatch, LOW);  // load register
  digitalWrite(pinLatch, HIGH);  // shift register

  uint16_t incoming[registerCount];
  for(uint8_t i=0; i<registerCount; ++i) {
    incoming[i] = 0;
  }

  for (uint8_t j = 0; j < 16; ++j) {
      digitalWrite(pinClk, LOW);
      for(uint8_t i=0; i<registerCount; ++i) {
        incoming[i] |= digitalRead(pinData[i]) << (15 - j);
      }
      digitalWrite(pinClk, HIGH);
  }

  for(uint8_t i=0; i<registerCount; ++i) {
    if (incoming[i] != lastRegisterState[i]) {
      // reset the debouncing timer
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (incoming[i] != registerState[i]) {
        registerState[i] = incoming[i];
        send = true;
      }
    }
    lastRegisterState[i] = incoming[i];
  }

  if (send) {
    if (DEBUG) SerialDebug.print('[');
    Serial.print('[');
    for(int i=0; i<registerCount; ++i) {
      char dataString[4] = {0};
      sprintf(dataString, "%04X", registerState[i]);
      if (DEBUG) SerialDebug.print(dataString);
      Serial.print(dataString);
    }
    Serial.print(']');
    if (DEBUG) SerialDebug.print(']');
    send = false;
  }
}
