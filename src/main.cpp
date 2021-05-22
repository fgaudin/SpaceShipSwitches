#include <Arduino.h>
#include <KerbalSimpit.h>

#define DEBUG 1

const int pinLatch = 10;  // LOW = Load, HIGH = shift 
const int pinClk = 11;

const int pinData[] = {2, 3};  // list of digital pins
const int registerCount = sizeof(pinData) / sizeof(int);

uint16_t registerState[registerCount]; // the current reading from the input pin
uint16_t lastRegisterState[registerCount];   // the previous reading from the registers

unsigned long lastDebounceTime[registerCount];  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

KerbalSimpit mySimpit(Serial);

bool initialized = false;

void setup() {
  Serial.begin(115200);

  while (!mySimpit.init()) {
    delay(100);
  }
  mySimpit.printToKSP("Switches connected", PRINT_TO_SCREEN);

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

  uint16_t changed;
  uint16_t mask;
  bool value;

  for(uint8_t i=0; i<registerCount; ++i) {
    if (incoming[i] != lastRegisterState[i]) {
      // reset the debouncing timer
      lastDebounceTime[i] = millis();
    }

    mask = 1;

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (incoming[i] != registerState[i]) {
        changed = incoming[i] ^ registerState[i];
        for (uint8_t j = 0; j < 16; ++j) {
          if (changed & mask || !initialized) {
            value = ((incoming[i] & mask) > 0);
            #if DEBUG
            char debug[12] = "switch ";
            itoa(j + i * 16, &debug[7], 10);
            mySimpit.printToKSP(debug);
            #endif
            switch (j + i * 16)
            {
              case 16:
                if (value) mySimpit.activateAction(GEAR_ACTION);
                else mySimpit.deactivateAction(GEAR_ACTION);
              break;
              case 17:
                if (value) mySimpit.activateCAG(1);
                else mySimpit.deactivateCAG(1);
              break;
              case 18:
                if (value) mySimpit.activateCAG(3);
                else mySimpit.deactivateCAG(3);
              break;
              case 19:
                if (value) mySimpit.activateCAG(2);
                else mySimpit.deactivateCAG(2);
              break;
              case 22:
                if (value) mySimpit.activateAction(BRAKES_ACTION);
                else mySimpit.deactivateAction(BRAKES_ACTION);
              break;
              case 23:
                if (value) mySimpit.toggleAction(STAGE_ACTION);
              break;
              case 26:
                if (value) mySimpit.toggleAction(SAS_ACTION);
              break;
              case 27:
                if (value) mySimpit.toggleAction(RCS_ACTION);
              break;
              case 28:
                if (value) mySimpit.toggleAction(LIGHT_ACTION);
              break;
              case 24: // ascent mode
                if (value) {
                  mySimpit.deactivateCAG(9);
                  mySimpit.deactivateCAG(10);
                }
              break;
              case 30: // orbit mode
                if (value) {
                  mySimpit.activateCAG(9);
                  mySimpit.deactivateCAG(10);
                }
              break;
              case 31: // descent mode
                if (value) {
                  mySimpit.deactivateCAG(9);
                  mySimpit.activateCAG(10);
                }
              break;
              case 20: // docking mode
                if (value) {
                  mySimpit.activateCAG(9);
                  mySimpit.activateCAG(10);
                }
              break;
            }
          }
          mask = mask << 1;
        }
        registerState[i] = incoming[i];
      }
    }
    lastRegisterState[i] = incoming[i];
  }
  initialized = true;
}
