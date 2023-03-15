int keys[] = {2,3,4};

void setup() {
  // Init serial
  Serial.begin(9600);

  // Init pin registered on keys
  for (int i = 2; i < 5; i++) pinMode(i, INPUT);
}

void loop() {
  for (int i = 2; i < 5; i++) {
    if (readButton(i)) doAction(i);
  }
}

boolean readButton(int pin) {
  if (digitalRead(pin) == HIGH) {
    delay(10); // debounce

    if (digitalRead(pin) == HIGH) return true;
  }

  return false;
}

void doAction(int pin) {
  switch (pin) {
    case 2:
      Serial.println(1);
      delay(100);
      break;
    case 3:
      Serial.println(2);
      delay(100);
      break;
    case 4:
      Serial.println(3);
      delay(100);
      break;
  }
}