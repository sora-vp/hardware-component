int keys[] = { 2, 3, 4, 5, 6 };

uint32_t previousmillis;
int keypressed = 0;

void setup() {
  // Init serial
  Serial.begin(9600);

  // Init pin registered on keys
  for (int i = 0; i < 5; i++) pinMode(keys[i], INPUT);
}

void loop() {
  for (int i = 0; i < 5; i++) {
    if (digitalRead(keys[i]) == HIGH) {
      if (millis() - previousmillis > 200) {
        previousmillis = millis();

        doAction(keys[i]);
      }
    }
  }
}

void doAction(int pin) {
  switch (pin) {
    case 2:
      Serial.println("SORA-KEYBIND-ESC");
      break;
    case 3:
      Serial.println("SORA-KEYBIND-1");
      break;
    case 4:
      Serial.println("SORA-KEYBIND-2");
      break;
    case 5:
      Serial.println("SORA-KEYBIND-3");
      break;
    case 6:
      Serial.println("SORA-KEYBIND-ENTER");
      break;
  }
}