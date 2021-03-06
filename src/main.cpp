#include <Arduino.h>
#include <KerbalSimpit.h>

#define DEBUG 1

const int pinLatch = 10;  // LOW = Load, HIGH = shift 
const int pinClk = 11;

const int pinData[] = {3};  // list of digital pins
const int registerCount = sizeof(pinData) / sizeof(int);

uint16_t registerState[registerCount]; // the current reading from the input pin
uint16_t lastRegisterState[registerCount];   // the previous reading from the registers

unsigned long lastDebounceTime[registerCount];  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

KerbalSimpit mySimpit(Serial);

bool initialized = false;

bool staging = false;

void messageHandler(byte messageType, byte msg[], byte msgSize);

void setup() {
  Serial.begin(115200);

  while (!mySimpit.init()) {
    delay(100);
  }
  mySimpit.printToKSP("Switches connected", PRINT_TO_SCREEN);
  mySimpit.inboundHandler(messageHandler);
  mySimpit.registerChannel(CAGSTATUS_MESSAGE);

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
  mySimpit.update();

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
              case 100:
                if (value) mySimpit.deactivateCAG(243);
              break;
              case 101:
                if (value) mySimpit.activateCAG(243);
              break;
              case 0:
                if (value) mySimpit.activateAction(GEAR_ACTION);
                else mySimpit.deactivateAction(GEAR_ACTION);
              break;
              case 1:
                if (value) mySimpit.activateCAG(1);
                else mySimpit.deactivateCAG(1);
              break;
              case 2:
                if (value) mySimpit.activateCAG(3);
                else mySimpit.deactivateCAG(3);
              break;
              case 3: // staging toggle
                if (value) mySimpit.activateCAG(242);
                else mySimpit.deactivateCAG(242);
              break;
              case 6:
                if (value) mySimpit.activateAction(BRAKES_ACTION);
                else mySimpit.deactivateAction(BRAKES_ACTION);
              break;
              case 7:
                if (value && staging) mySimpit.toggleAction(STAGE_ACTION);
              break;
              case 10:
                if (value) mySimpit.toggleAction(SAS_ACTION);
              break;
              case 11:
                if (value) mySimpit.toggleAction(RCS_ACTION);
              break;
              case 12:
                if (value) mySimpit.toggleAction(LIGHT_ACTION);
              break;
              case 8: // ascent mode
                if (value) {
                  mySimpit.deactivateCAG(240);
                  mySimpit.deactivateCAG(241);
                }
              break;
              case 14: // orbit mode
                if (value) {
                  mySimpit.activateCAG(240);
                  mySimpit.deactivateCAG(241);
                }
              break;
              case 15: // descent mode
                if (value) {
                  mySimpit.deactivateCAG(240);
                  mySimpit.activateCAG(241);
                }
              break;
              case 4: // docking mode
                if (value) {
                  mySimpit.activateCAG(240);
                  mySimpit.activateCAG(241);
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

void messageHandler(byte messageType, byte msg[], byte msgSize) {
  switch(messageType) {
    case CAGSTATUS_MESSAGE:
      if (msgSize == sizeof(cagStatusMessage)) {
        cagStatusMessage status = parseCAGStatusMessage(msg);
        staging = status.is_action_activated(242);
      }
    break;
  }
}
