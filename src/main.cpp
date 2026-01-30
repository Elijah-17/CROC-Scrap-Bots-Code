#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(5000);  // allow USB UART to initialize
  Serial.println("Serial test successful!");
}

void loop() {
  Serial.println("Tick");
  delay(1000);
}
