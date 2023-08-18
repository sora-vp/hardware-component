const int debounceDelay = 50;  // in ms
const int NUM_KEYS = 8;

const int keys[NUM_KEYS] = { 2, 3, 4, 5, 6, 7, 8, 9 };
int buttonState[NUM_KEYS];
int lastButtonState[NUM_KEYS];
unsigned long lastDebounceTime[NUM_KEYS];

const int ESC_PIN = 4;
const int KEYBIND_1_PIN = 9;
const int KEYBIND_2_PIN = 8;
const int KEYBIND_3_PIN = 7;
const int KEYBIND_4_PIN = 6;
const int KEYBIND_5_PIN = 5;
const int ENTER_PIN = 3;
const int RELOAD_PIN = 2;

void setup() {
  // Init serial
  Serial.begin(9600);

  // Init pin registered on keys
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
          doAction(keys[i]);
        }
      }
    }

    lastButtonState[i] = reading;
  }
}

void doAction(int pin) {
  switch (pin) {
    case ESC_PIN:
      Serial.println("SORA-KEYBIND-ESC");
      break;
    case RELOAD_PIN:
      Serial.println("SORA-KEYBIND-RELOAD");
      break;
    case KEYBIND_1_PIN:
      Serial.println("SORA-KEYBIND-1");
      break;
    case KEYBIND_2_PIN:
      Serial.println("SORA-KEYBIND-2");
      break;
    case KEYBIND_3_PIN:
      Serial.println("SORA-KEYBIND-3");
      break;
    case KEYBIND_4_PIN:
      Serial.println("SORA-KEYBIND-4");
      break;
    case KEYBIND_5_PIN:
      Serial.println("SORA-KEYBIND-5");
      break;
    case ENTER_PIN:
      Serial.println("SORA-KEYBIND-ENTER");
      break;
  }
}