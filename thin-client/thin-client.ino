#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

// Pin definitions and button address
#define IR_SENSOR_PIN 16
#define ONBOARD_LED_PIN 2
#define PN532_RESET_TRANSISTOR_PIN 4
#define BUTTON_HARDWARE_ADDRESS 0x1F

// Create PN532 instance
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

// Task handles
TaskHandle_t irTaskHandle;
TaskHandle_t nfcTaskHandle;
TaskHandle_t queueTaskHandle;
TaskHandle_t buttonTaskHandle;
TaskHandle_t serialTaskHandle;

// Mutex handle
SemaphoreHandle_t stateMutex;

// Handle string queue of handler 
QueueHandle_t stringQueue;

// Define states as an enum
enum MachineState {
  UNAVAIL_STATE,
  AVAIL_STATE
};

unsigned short failedAttempts = 0;
volatile MachineState currentState = UNAVAIL_STATE;  // Default state
String cardUID = "";  // Stores the UID when in AVAIL_STATE

bool connectToCard() {
  Serial.print("Failed attempts: ");
  Serial.print(failedAttempts);
  Serial.println();

  if (failedAttempts >= 3) {
    digitalWrite(PN532_RESET_TRANSISTOR_PIN, HIGH);
    delay(1000);
    digitalWrite(PN532_RESET_TRANSISTOR_PIN, LOW);
    delay(1000);
    digitalWrite(PN532_RESET_TRANSISTOR_PIN, HIGH);
  }

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 card module not found!");
    return false;
  }

  Serial.println("Please tap your card to the sensor.");
  Serial.println();

  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();
  return true;
}

void setup() {
  Serial.begin(115200);

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  pinMode(PN532_RESET_TRANSISTOR_PIN, OUTPUT);

  digitalWrite(PN532_RESET_TRANSISTOR_PIN, LOW);

  Wire.setClock(10000);

  // Create the mutex
  stateMutex = xSemaphoreCreateMutex();

  if (stateMutex == NULL) {
    Serial.println("Failed to create mutex!");
    while (true);  // Halt system if mutex creation fails
  }

  // Create the queue
  stringQueue = xQueueCreate(100, 55);  // Adjust queue length and item size as needed

  if (stringQueue == NULL) {
    Serial.println("Failed to create queue!");
    while (true);
  }

  digitalWrite(PN532_RESET_TRANSISTOR_PIN, HIGH);

  while (!connectToCard()) {
    failedAttempts++;
    Serial.println("Retrying to connect to card module...");
    delay(850);
  }

  Serial.println("SYS OK!");

  // Create tasks
  xTaskCreatePinnedToCore(irTask, "IR Sensor Task", 5000, NULL, 1, &irTaskHandle, 1);
  xTaskCreatePinnedToCore(nfcTask, "NFC Reader Task", 3500, NULL, 1, &nfcTaskHandle, 1);
  xTaskCreatePinnedToCore(buttonTask, "External Button Hardware Task", 2000, NULL, 1, &buttonTaskHandle, 1);
  xTaskCreatePinnedToCore(queueManagerTask, "Queue Manager Task", 2000, NULL, 1, &queueTaskHandle, 0);
  xTaskCreatePinnedToCore(serialTask, "Serial Task", 2000, NULL, 1, &serialTaskHandle, 0);
}

void irTask(void *parameter) {
  while (true) {
    bool objectDetected = digitalRead(IR_SENSOR_PIN) == LOW;

    if (objectDetected) {
      vTaskResume(nfcTaskHandle);  // Wake NFC task
    } else {
      vTaskSuspend(nfcTaskHandle);

      // Lock the mutex to update shared state
      if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
        currentState = UNAVAIL_STATE;
        cardUID = "";  // Clear the UID

        xSemaphoreGive(stateMutex);  // Release the mutex
      }
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);  // Poll every 100 ms

    // UBaseType_t stackLeft = uxTaskGetStackHighWaterMark(irTaskHandle);
    // Serial.println(stackLeft);
  }
}

void nfcTask(void *parameter) {
  while (true) {
    uint8_t uidBuffer[7];
    uint8_t uidLength;

    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidBuffer, &uidLength)) {
      // Construct UID string
      String uid = "";
      for (uint8_t i = 0; i < uidLength; i++) {
        uid += "0x" + String(uidBuffer[i], HEX);
      }
      uid.toUpperCase();

      vTaskSuspend(irTaskHandle);  // Suspend IR task
      vTaskDelay(25 / portTICK_PERIOD_MS); // A small delay

      // Lock the mutex to update shared state
      if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
        currentState = AVAIL_STATE;
        cardUID = uid;
        xSemaphoreGive(stateMutex);  // Release the mutex
      }

      vTaskDelay(25 / portTICK_PERIOD_MS); // A small delay
      vTaskResume(irTaskHandle);  // Resume IR task
    } else {
      Serial.println("Timeout");
    }

    // Small delay to avoid immediate re-trigger
    vTaskDelay(650 / portTICK_PERIOD_MS);
  }

  vTaskSuspend(nfcTaskHandle);  // Should never reach here
}

void buttonTask(void *param) {
  char buffer[32];  // Buffer to store the response from the slave

  while (true) {
    // Request data from the slave
    Wire.requestFrom(BUTTON_HARDWARE_ADDRESS, sizeof(buffer) - 1);

    // Read the response
    int i = 0;

    while (Wire.available() && i < sizeof(buffer) - 1) {
      char c = Wire.read();

      // Filter out non-printable characters
      if (isPrintable(c) && c != '\0') {
        buffer[i++] = c;
      }
    }

    buffer[i] = '\0';  // Null-terminate the string

    // Enqueue the received message if it's valid
    if (strlen(buffer) > 0) {
      if (xQueueSend(stringQueue, buffer, pdMS_TO_TICKS(100)) != pdPASS) {
        Serial.println("Queue is full, message dropped!");
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);  // Adjust polling interval as needed
  }

}

void queueManagerTask(void *parameter) {
  while (true) {
    MachineState localState;
    String localUID;

    // Lock the mutex to read shared state
    if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
      localState = currentState;
      localUID = cardUID;
      xSemaphoreGive(stateMutex);  // Release the mutex
    }

    // Use local copies of the state to avoid holding the mutex too long
    String message;
    if (localState == UNAVAIL_STATE) {
      message = "<SORA-THIN-CLIENT-DATA-UNAVAIL>";
      digitalWrite(ONBOARD_LED_PIN, LOW);
    } else if (localState == AVAIL_STATE) {
      message = "<SORA-THIN-CLIENT-DATA-" + localUID + ">";
      digitalWrite(ONBOARD_LED_PIN, HIGH);
    }

    // Send the message to the queue
    if (xQueueSend(stringQueue, message.c_str(), pdMS_TO_TICKS(100)) != pdPASS) {
      Serial.println("Queue is full, message dropped!");
    }

    vTaskDelay(localState == UNAVAIL_STATE ? 100 / portTICK_PERIOD_MS : 200 / portTICK_PERIOD_MS);  // Adjust delay as needed
  }
}

void serialTask(void *parameter) {
  char buffer[32];  // Buffer for dequeued message
  
  while (true) {
    if (xQueueReceive(stringQueue, buffer, portMAX_DELAY)) {
      Serial.println(buffer);
    }

    vTaskDelay(25 / portTICK_PERIOD_MS);  // Small delay to avoid busy-waiting
  }
}

void loop() {
  // Leave this empty
  delay(1000);
}
