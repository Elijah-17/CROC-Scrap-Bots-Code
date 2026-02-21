#include <Arduino.h>
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

// Any Xbox controller
XboxSeriesXControllerESP32_asukiaaa::Core xboxController;

/* ========= CONTROLLER LOCK ========= */
#define ALLOWED_CONTROLLER "c4:12:bf:d5:0a:00"   // <<< CHANGE THIS

/* ========= MOTOR PINS ========= */
// Left drive motor
#define LA 12
#define LB 13
#define LPWM 14

// Right drive motor
#define RA 27
#define RB 26
#define RPWM 25

// Weapon motor
#define WA 18
#define WB 19
#define WPWM 20

/* ========= PWM SETTINGS ========= */
#define PWM_FREQ 20000
#define PWM_RES 8

// PWM CHANNELS
#define L_CHANNEL 0
#define R_CHANNEL 1
#define W_CHANNEL 2

/* ========= STATE FLAGS ========= */
bool robotEnabled = false;
bool connectedPrinted = false;
bool firstInputPrinted = false;

bool lbWasPressed = false;
bool rbWasPressed = false;
bool toggleArmed = false;

unsigned long lastNotConnectedPrint = 0;

/* ========= HELPER FUNCTIONS ========= */

void setMotor(int aPin, int bPin, int pwmChannel, float speed) {
  speed = constrain(speed, -1.0, 1.0);
  int pwmVal = abs(speed) * 255;

  if (speed > 0) {
    digitalWrite(aPin, HIGH);
    digitalWrite(bPin, LOW);
  }
  else if (speed < 0) {
    digitalWrite(aPin, LOW);
    digitalWrite(bPin, HIGH);
  }
  else {
    digitalWrite(aPin, LOW);
    digitalWrite(bPin, LOW);
  }

  ledcWrite(pwmChannel, pwmVal);
}

void driveFromJoystick(float x, float y) {
  float left = y + x;
  float right = y - x;

  float maxVal = max(abs(left), abs(right));
  if (maxVal > 1.0) {
    left /= maxVal;
    right /= maxVal;
  }

  setMotor(LA, LB, L_CHANNEL, -left);
  setMotor(RA, RB, R_CHANNEL, -right);
}

/* ========= SETUP ========= */

void setup() {
  Serial.begin(115200);
  Serial.println("Starting NimBLE Client");

  xboxController.begin();

  pinMode(LA, OUTPUT);
  pinMode(LB, OUTPUT);
  pinMode(RA, OUTPUT);
  pinMode(RB, OUTPUT);
  pinMode(WA, OUTPUT);
  pinMode(WB, OUTPUT);

  ledcSetup(L_CHANNEL, PWM_FREQ, PWM_RES);
  ledcSetup(R_CHANNEL, PWM_FREQ, PWM_RES);
  ledcSetup(W_CHANNEL, PWM_FREQ, PWM_RES);

  ledcAttachPin(LPWM, L_CHANNEL);
  ledcAttachPin(RPWM, R_CHANNEL);
  ledcAttachPin(WPWM, W_CHANNEL);

  Serial.println("PWM initialized");
}

/* ========= LOOP ========= */

void loop() {
  xboxController.onLoop();

  /* ===== NOT CONNECTED ===== */
  if (!xboxController.isConnected()) {
    connectedPrinted = false;
    firstInputPrinted = false;
    robotEnabled = false;

    driveFromJoystick(0, 0);
    setMotor(WA, WB, W_CHANNEL, 0);

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

    // Check MAC address
    if (addr != ALLOWED_CONTROLLER) {
      Serial.println("WRONG CONTROLLER - RESTARTING ESP");
      delay(500);
      ESP.restart();   // Hard reset instead of disconnect
    }

    Serial.println("Correct controller verified");
    connectedPrinted = true;
}

  if (xboxController.isWaitingForFirstNotification()) return;

  if (!firstInputPrinted) {
    Serial.println("First controller input received");
    firstInputPrinted = true;
  }

  auto notif = xboxController.xboxNotif;

  /* ===== JOYSTICK CONVERSION ===== */
  float joyX = -(32767.0 - notif.joyRHori) / 32767.0;
  float joyY =  (32767.0 - notif.joyRVert) / 32767.0;

  joyX = constrain(joyX, -1.0, 1.0);
  joyY = constrain(joyY, -1.0, 1.0);

  if (abs(joyX) < 0.08) joyX = 0;
  if (abs(joyY) < 0.08) joyY = 0;

  float trigMax = XboxControllerNotificationParser::maxTrig;
  float leftTrigger = notif.trigLT / trigMax;

  /* ===== ENABLE / DISABLE TOGGLE ===== */

  bool lb = notif.btnLB;
  bool rb = notif.btnRB;

  // detect both pressed
  if (lb && rb) {
    toggleArmed = true;
  }

  // detect release after both were pressed
  if (toggleArmed && !lb && !rb) {
    robotEnabled = !robotEnabled;  // TOGGLE STATE
    toggleArmed = false;

    if (robotEnabled) {
      Serial.println("ROBOT ENABLED");
    } else {
      Serial.println("ROBOT DISABLED");
      driveFromJoystick(0, 0);
      setMotor(WA, WB, W_CHANNEL, 0);
    }
  }

  /* ===== DRIVE ===== */

  if (robotEnabled) {
    driveFromJoystick(joyX, joyY);
    setMotor(WA, WB, W_CHANNEL, leftTrigger);
  }
  else {
    driveFromJoystick(0, 0);
    setMotor(WA, WB, W_CHANNEL, 0);
  }
}