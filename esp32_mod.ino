#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

const int relayPin1 = 4;
const int relayPin2 = 5;
const int relayPin3 = 6;
const int relayPin4 = 7;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_BT"); // Bluetooth device name

  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  pinMode(relayPin4, OUTPUT);

  // Ensure that the relays are initially in the OFF state
  digitalWrite(relayPin1, LOW);
  digitalWrite(relayPin2, LOW);
  digitalWrite(relayPin3, LOW);
  digitalWrite(relayPin4, LOW);
}

void loop() {
  if (SerialBT.available()) {
    char command = SerialBT.read();
    executeCommand(command);
  }
}

void executeCommand(char command) {
  switch (command) {
    case '1':
      digitalWrite(relayPin1, !digitalRead(relayPin1));
      break;
    case '2':
      digitalWrite(relayPin2, !digitalRead(relayPin2));
      break;
    case '3':
      digitalWrite(relayPin3, !digitalRead(relayPin3));
      break;
    case '4':
      digitalWrite(relayPin4, !digitalRead(relayPin4));
      break;
    // To turn off the relays, use '5', '6', '7', and '8'
    case '5':
      digitalWrite(relayPin1, LOW);
      break;
    case '6':
      digitalWrite(relayPin2, LOW);
      break;
    case '7':
      digitalWrite(relayPin3, LOW);
      break;
    case '8':
      digitalWrite(relayPin4, LOW);
      break;
    default:
      break;
  }
}
