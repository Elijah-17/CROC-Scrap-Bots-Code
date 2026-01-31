#include <Arduino.h>
#include <NimBLEDevice.h>

// Motor PWM pins
#define MOTOR_L 4
#define MOTOR_R 5
#define LED_PIN 8

// PWM config
const int freq = 5000;
const int resolution = 8; // 0-255
const int channelL = 0;
const int channelR = 1;

// Xbox controller BLE service/characteristics UUIDs
#define XBOX_SERVICE_UUID       "0000ffe0-0000-1000-8000-00805f9b34fb" // placeholder
#define XBOX_REPORT_CHAR_UUID   "0000ffe1-0000-1000-8000-00805f9b34fb" // placeholder

NimBLEAdvertisedDevice* xboxDevice = nullptr;
NimBLEClient* pClient = nullptr;
NimBLERemoteCharacteristic* pReportChar = nullptr;

bool connected = false;

void notifyCallback(NimBLERemoteCharacteristic* pRemoteChar,
                    uint8_t* pData, size_t length, bool isNotify) {
    if (length < 6) return; // ensure enough bytes

    // Parse joystick axes (example mapping, adjust per controller)
    int16_t lx = (int16_t)((pData[1] << 8) | pData[0]); // left stick X
    int16_t ly = (int16_t)((pData[3] << 8) | pData[2]); // left stick Y
    bool aButton = pData[4] & 0x10;

    // Map from -32768→32767 to 0→255
    int pwmL = map(lx, -32768, 32767, 0, 255);
    int pwmR = map(ly, -32768, 32767, 0, 255);

    // Output PWM
    ledcWrite(channelL, pwmL);
    ledcWrite(channelR, pwmR);

    // Optional LED
    digitalWrite(LED_PIN, aButton);

    Serial.printf("LX:%d LY:%d PWM L:%d PWM R:%d A:%d\n", lx, ly, pwmL, pwmR, aButton);
}

class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->haveServiceUUID() &&
            advertisedDevice->isAdvertisingService(NimBLEUUID(XBOX_SERVICE_UUID))) {
            Serial.printf("Found Xbox controller: %s\n", advertisedDevice->getName().c_str());
            xboxDevice = advertisedDevice;
            NimBLEDevice::getScan()->stop();
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(LED_PIN, OUTPUT);

    // PWM setup
    ledcSetup(channelL, freq, resolution);
    ledcAttachPin(MOTOR_L, channelL);

    ledcSetup(channelR, freq, resolution);
    ledcAttachPin(MOTOR_R, channelR);

    // Initialize BLE
    NimBLEDevice::init("ESP32_S3");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pScan->setActiveScan(true);
    pScan->start(10, false); // scan for 10 seconds
}

void loop() {
    if (xboxDevice && !connected) {
        Serial.println("Connecting to Xbox controller...");
        pClient = NimBLEDevice::createClient();
        if (pClient->connect(xboxDevice)) {
            Serial.println("Connected!");
            connected = true;

            pReportChar = pClient->getService(NimBLEUUID(XBOX_SERVICE_UUID))
                                  ->getCharacteristic(NimBLEUUID(XBOX_REPORT_CHAR_UUID));

            if (pReportChar) {
                pReportChar->subscribe(true, notifyCallback);
            } else {
                Serial.println("Failed to find report characteristic");
            }
        } else {
            Serial.println("Failed to connect");
            xboxDevice = nullptr; // try scanning again
        }
    }

    delay(50);
}
