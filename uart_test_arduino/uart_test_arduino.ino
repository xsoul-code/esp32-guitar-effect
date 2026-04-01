// UART test — ESP32 tester -> ESP32 Guitar Effect
// Tester TX (GPIO17) -> Guitar Effect RX (GPIO16)
// GND -> GND

#define TX_PIN 17
#define RX_PIN 16

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
}

void loop() {
  Serial2.println("HELLO_ESP32");
  Serial.println("Sent: HELLO_ESP32");
  delay(2000);
}
