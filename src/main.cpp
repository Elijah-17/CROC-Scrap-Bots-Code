#include <Arduino.h>
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

XboxSeriesXControllerESP32_asukiaaa::Core xboxController;

/* ========= CONTROLLER LOCK ========= */
#define ALLOWED_CONTROLLER "c4:12:bf:d5:0a:00"
//#define ALLOWED_CONTROLLER "ff:ff:ff:ff:ff:ff"

/* ========= MOTOR PINS (DRV8833) ========= */
#define LA 12
#define LB 13
#define RA 27
#define RB 26
#define WA 32
#define WB 33

#define LED_PIN 2

/* ========= PWM ========= */
#define PWM_FREQ 20000
#define PWM_RES 8

#define LA_CH 0
#define LB_CH 1
#define RA_CH 2
#define RB_CH 3
#define WA_CH 4
#define WB_CH 5

/* ========= STATE ========= */
bool robotEnabled = false;

bool driveReversed = false;
bool weaponReversed = false;

bool toggleArmed = false;

bool startPrev = false;
bool selectPrev = false;

unsigned long ledTimer = 0;
bool ledState = false;
bool firstValidInputReceived = false;

/* 🔥 NEW: connection debug flags */
bool connectedPrinted = false;
bool firstInputPrinted = false;
unsigned long lastNotConnectedPrint = 0;

/* ========= MOTOR FUNCTION ========= */
void setMotorDRV(int pinA, int chA, int pinB, int chB, float speed) {
  speed = constrain(speed, -1.0, 1.0);

  const int MIN_PWM = 60;
  int pwm = abs(speed) * (255 - MIN_PWM) + MIN_PWM;

  if (speed > 0) {
    ledcWrite(chA, pwm);
    ledcWrite(chB, 0);
  }
  else if (speed < 0) {
    ledcWrite(chA, 0);
    ledcWrite(chB, pwm);
  }
  else {
    ledcWrite(chA, 0);
    ledcWrite(chB, 0);
  }
}

/* ========= DRIVE ========= */
void driveFromJoystick(float x, float y) {
  const float deadzone = 0.01;

  if (fabs(x) < deadzone) x = 0;
  if (fabs(y) < deadzone) y = 0;

  if (x != 0)
    x = (fabs(x) - deadzone) / (1.0 - deadzone) * (x > 0 ? 1 : -1);

  if (y != 0)
    y = (fabs(y) - deadzone) / (1.0 - deadzone) * (y > 0 ? 1 : -1);

  float expo = 0.6;
  x = (1 - expo) * x + expo * x * x * x;
  y = (1 - expo) * y + expo * y * y * y;

  float left  = y + x;
  float right = y - x;

  float maxVal = max(fabs(left), fabs(right));
  if (maxVal > 1.0) {
    left  /= maxVal;
    right /= maxVal;
  }

  left  *= 0.80;
  right *= 0.80;

  if (x == 0 && y == 0) {
    left = 0;
    right = 0;
  }

  if (driveReversed) {
    left = -left;
    right = -right;
  }

  /*Serial.print("X: "); Serial.print(x, 3);
  Serial.print(" Y: "); Serial.print(y, 3);
  Serial.print(" | L: "); Serial.print(left, 3);
  Serial.print(" R: "); Serial.println(right, zzzzzzaaaaaaa3);*/

  if(robotEnabled){
    setMotorDRV(LA, LA_CH, LB, LB_CH, left);
    setMotorDRV(RA, RA_CH, RB, RB_CH, right);
  }
}

/* ========= SETUP ========= */
void setup() {
  Serial.begin(115200);
  Serial.println("Starting NimBLE Client");

  xboxController.begin();

  pinMode(LED_PIN, OUTPUT);

  ledcSetup(LA_CH, PWM_FREQ, PWM_RES);
  ledcSetup(LB_CH, PWM_FREQ, PWM_RES);
  ledcSetup(RA_CH, PWM_FREQ, PWM_RES);
  ledcSetup(RB_CH, PWM_FREQ, PWM_RES);
  ledcSetup(WA_CH, PWM_FREQ, PWM_RES);
  ledcSetup(WB_CH, PWM_FREQ, PWM_RES);

  ledcAttachPin(LA, LA_CH);
  ledcAttachPin(LB, LB_CH);
  ledcAttachPin(RA, RA_CH);
  ledcAttachPin(RB, RB_CH);
  ledcAttachPin(WA, WA_CH);
  ledcAttachPin(WB, WB_CH);

  Serial.println("DRV8833 Ready");
}

/* ========= LOOP ========= */
void loop() {
  xboxController.onLoop();

  /* ===== NOT CONNECTED ===== */
  if (!xboxController.isConnected()) {
    connectedPrinted = false;
    firstInputPrinted = false;
    robotEnabled = false;

    if (millis() - ledTimer > 150) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      ledTimer = millis();
    }

    if (millis() - lastNotConnectedPrint > 2000) {
      Serial.println("not connected");
      lastNotConnectedPrint = millis();
    }

    if (xboxController.getCountFailedConnection() > 2) {
      ESP.restart();
    }

    return;
  }

  /* ===== CONNECTED ===== */
  if (!connectedPrinted) {
    String addr = xboxController.buildDeviceAddressStr();

    Serial.println("CONNECTED");
    Serial.println("Controller Address: " + addr);

    /*
    if (addr != ALLOWED_CONTROLLER) {
      Serial.println("WRONG CONTROLLER - RESTARTING ESP");
      delay(500);
      ESP.restart();
    }*/

    Serial.println("Correct controller verified");
    connectedPrinted = true;
  }

  if (!firstValidInputReceived) {
    if (millis() - ledTimer > 500) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      ledTimer = millis();
    }
  } else {
    digitalWrite(LED_PIN, HIGH);
  }

  if (xboxController.isWaitingForFirstNotification()) return;

  if (!firstInputPrinted) {
    Serial.println("First controller input received");
    firstInputPrinted = true;
    firstValidInputReceived = true;
  }

  auto notif = xboxController.xboxNotif;

  float joyX = (notif.joyRHori - 32768) / 32768.0;
  float joyY = -(notif.joyRVert - 32768) / 32768.0;

  float rightTrigger = notif.trigRT / 1023.0;

  bool lb = notif.btnLB;
  bool rb = notif.btnRB;

  if (lb && rb) toggleArmed = true;

  if (toggleArmed && !lb && !rb) {
    robotEnabled = !robotEnabled;
    toggleArmed = false;
    Serial.println(robotEnabled ? "ENABLED" : "DISABLED");
  }

  bool startBtn = notif.btnStart;
  bool selectBtn = notif.btnSelect;

  if (startBtn && !startPrev) {
    driveReversed = !driveReversed;
    Serial.println(driveReversed ? "Drive Reversed" : "Drive Normal");
  }

  if (selectBtn && !selectPrev) {
    weaponReversed = !weaponReversed;
    Serial.println(weaponReversed ? "Weapon Reversed" : "Weapon Normal");
  }

  startPrev = startBtn;
  selectPrev = selectBtn;

  if (robotEnabled) {
    driveFromJoystick(joyX, joyY);

    float weaponSpeed = rightTrigger;
    if (weaponReversed) weaponSpeed *= -1;

    setMotorDRV(WA, WA_CH, WB, WB_CH, weaponSpeed);
  }
  else {
    driveFromJoystick(0, 0);
    setMotorDRV(WA, WA_CH, WB, WB_CH, 0);
  }
}