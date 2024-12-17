#include <Wire.h>

#define I2C_ADDRESS 0x1F

const int debounceDelay = 50;  // in ms
const int NUM_KEYS = 5;

const int keys[NUM_KEYS] = { 2, 3, 4, 5, 6 };
int buttonState[NUM_KEYS];
int lastButtonState[NUM_KEYS];
unsigned long lastDebounceTime[NUM_KEYS];

// Buffer to store the state to be sent to the master
String currentMessage = "";

void setup() {
  // Init I2C
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(sendData);  // Set callback for I2C requests

  // Init serial
  Serial.begin(9600);
  Serial.println("I2C slave initialized!");

  // Init pin states
  for (int i = 0; i < NUM_KEYS; i++) {
    pinMode(keys[i], INPUT_PULLUP);
    lastButtonState[i] = digitalRead(keys[i]);
  }
}

void loop() {
  for (int i = 0; i < NUM_KEYS; i++) {
    int reading = digitalRead(keys[i]);

    if (reading != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != buttonState[i]) {
        buttonState[i] = reading;

        if (buttonState[i] == LOW) {
          currentMessage = generateMessage(keys[i]);
          Serial.println(currentMessage);
        }
      }
    }

    lastButtonState[i] = reading;
  }
}

String generateMessage(int pin) {
  switch (pin) {
    case 2: return "<SORA-KEYBIND-ESC>";
    case 3: return "<SORA-KEYBIND-1>";
    case 4: return "<SORA-KEYBIND-2>";
    case 5: return "<SORA-KEYBIND-3>";
    case 6: return "<SORA-KEYBIND-ENTER>";
    default: return "UNKNOWN";
  }
}

// Send the current message over I2C when requested
void sendData() {
  Serial.println("Minta!");
  if (currentMessage != "") {
    Wire.write(currentMessage.c_str());
    currentMessage = "";
    Serial.println("Dibales!");
  }
}
