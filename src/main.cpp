#include <Arduino.h>
void setup() {
  Serial.begin(115200);      // use Serial for USB/UART
  delay(3000);               // wait for Serial Monitor to attach
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("ESP32-C3 test board ready!");
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("LED ON");    // print status to serial
  delay(500);
  
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("LED OFF");   // print status to serial
  delay(500);
}

