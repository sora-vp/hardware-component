#include <Wire.h>
#include <Vector.h>

#define I2C_ADDRESS 0x1F

const short i2cIdleTolerantMS = 950;
const short debounceDelay = 50; // in ms
const int NUM_KEYS = 5;

const int keys[NUM_KEYS] = {2, 3, 4, 5, 6};
int buttonState[NUM_KEYS];
int lastButtonState[NUM_KEYS];
unsigned long lastDebounceTime[NUM_KEYS];
unsigned long idleMillisNoI2C = 0;

// Buffer to store the state to be sent to the master
String allMessages[10];
Vector<String> outcomingMessages;

void setup()
{
  // Init I2C
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(sendData); // Set callback for I2C requests

  // Init serial
  Serial.begin(9600);
  Serial.println("I2C slave initialized!");

  // Init pin states
  for (int i = 0; i < NUM_KEYS; i++)
  {
    pinMode(keys[i], INPUT_PULLUP);
    lastButtonState[i] = digitalRead(keys[i]);
  }

  outcomingMessages.setStorage(allMessages);
}

void loop()
{
  unsigned long currentMillis = millis();

  if ((currentMillis - idleMillisNoI2C) >= i2cIdleTolerantMS)
  {
    idleMillisNoI2C = currentMillis;

    outcomingMessages.clear();
  }

  for (int i = 0; i < NUM_KEYS; i++)
  {
    int reading = digitalRead(keys[i]);

    if (reading != lastButtonState[i])
    {
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay)
    {
      if (reading != buttonState[i])
      {
        buttonState[i] = reading;

        if (buttonState[i] == LOW)
        {
          String currentMessage = generateMessage(keys[i]);

          if (outcomingMessages.size() > 9)
            outcomingMessages.clear();
          else
            outcomingMessages.push_back(currentMessage);

          Serial.println(currentMessage);
        }
      }
    }

    lastButtonState[i] = reading;
  }
}

String generateMessage(int pin)
{
  switch (pin)
  {
  case 2:
    return "<SORA-KEYBIND-ESC>";
  case 3:
    return "<SORA-KEYBIND-1>";
  case 4:
    return "<SORA-KEYBIND-2>";
  case 5:
    return "<SORA-KEYBIND-3>";
  case 6:
    return "<SORA-KEYBIND-ENTER>";
  default:
    return "UNKNOWN";
  }
}

// Send the current message over I2C when requested
void sendData()
{
  Serial.println("Minta!");

  if (outcomingMessages.size() > 0)
  {
    Wire.write(outcomingMessages.front().c_str());
    outcomingMessages.remove(0);
    Serial.println("Dibales!");
  }
}
