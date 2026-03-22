#include <Arduino.h>
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

XboxSeriesXControllerESP32_asukiaaa::Core xboxController;

/* ========= CONTROLLER LOCK ========= */
#define ALLOWED_CONTROLLER "c4:12:bf:d5:0a:00"

/* ========= MOTOR PINS (DRV8833) ========= */
// Left drive
#define LA 12
#define LB 14

// Right drive
#define RA 27
#define RB 26

// Weapon
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

/* ========= MOTOR FUNCTION ========= */
void setMotorDRV(int pinA, int chA, int pinB, int chB, float speed) {
  speed = constrain(speed, -1.0, 1.0);

  const int MIN_PWM = 60; // 🔥 NEW: minimum PWM to overcome motor deadzone
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

  // --- Deadzone ---
  if (fabs(x) < deadzone) x = 0;
  if (fabs(y) < deadzone) y = 0;

  // --- Rescale ---
  if (x != 0)
    x = (fabs(x) - deadzone) / (1.0 - deadzone) * (x > 0 ? 1 : -1);

  if (y != 0)
    y = (fabs(y) - deadzone) / (1.0 - deadzone) * (y > 0 ? 1 : -1);

  // --- EXPO CURVE ---
  float expo = 0.6;
  x = (1 - expo) * x + expo * x * x * x;
  y = (1 - expo) * y + expo * y * y * y;

  // --- Arcade mix ---
  float left  = y + x;
  float right = y - x;

  // --- Normalize ---
  float maxVal = max(fabs(left), fabs(right));
  if (maxVal > 1.0) {
    left  /= maxVal;
    right /= maxVal;
  }

  // ===== 🔥 NEW: SPEED LIMIT =====
  left  *= 0.80;
  right *= 0.80;

  // --- HARD STOP ---
  if (x == 0 && y == 0) {
    left = 0;
    right = 0;
  }

  // --- Reverse toggle ---
  if (driveReversed) {
    left = -left;
    right = -right;
  }

  // --- DEBUG PRINT ---
  Serial.print("X: "); Serial.print(x, 3);
  Serial.print(" Y: "); Serial.print(y, 3);
  Serial.print(" | L: "); Serial.print(left, 3);
  Serial.print(" R: "); Serial.println(right, 3);

  // --- Output ---
  if(robotEnabled){
    setMotorDRV(LA, LA_CH, LB, LB_CH, left);
    setMotorDRV(RA, RA_CH, RB, RB_CH, right);
  }
}

/* ========= SETUP ========= */
void setup() {
  Serial.begin(115200);
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
  setMotorDRV(LA, LA_CH, LB, LB_CH, 0);
  setMotorDRV(RA, RA_CH, RB, RB_CH, 0);
}

/* ========= LOOP ========= */
void loop() {
  xboxController.onLoop();

  if (!xboxController.isConnected()) {
    if (millis() - ledTimer > 150) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      ledTimer = millis();
    }
    return;
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

  auto notif = xboxController.xboxNotif;
  firstValidInputReceived = true;

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