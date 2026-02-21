#include <Arduino.h>
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>

// Any Xbox controller
XboxSeriesXControllerESP32_asukiaaa::Core xboxController;

/* ========= MOTOR PINS ========= */
// Left drive motor
#define LA 12
#define LB 13
#define LPWM 14

// Right drive motor
#define RA 27
#define RB 26
#define RPWM 25

// Weapon motor (temporary)
#define WA 18
#define WB 19
#define WPWM 20

/* ========= PWM SETTINGS ========= */
#define PWM_FREQ 20000
#define PWM_RES 8   // 0–255 PWM

// PWM CHANNELS (ESP32 uses channels, not pins)
#define L_CHANNEL 0
#define R_CHANNEL 1
#define W_CHANNEL 2

/* ========= STATE FLAGS ========= */
bool robotEnabled = false;
bool connectedPrinted = false;
bool firstInputPrinted = false;

bool lbPressed = false;
bool rbPressed = false;
bool lbReleased = false;
bool rbReleased = false;

unsigned long lastNotConnectedPrint = 0;

/* ========= HELPER FUNCTIONS ========= */

// speed range: -1.0 → 1.0
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

// Arcade drive (RIGHT STICK)
// x = turning, y = forward/back
void driveFromJoystick(float x, float y) {

  // standard differential mixing
  float left = y + x;
  float right = y - x;

  // normalize to keep range -1 to 1
  float maxVal = max(abs(left), abs(right));
  if (maxVal > 1.0) {
    left /= maxVal;
    right /= maxVal;
  }

  // BOTH motors inverted (your drivetrain requires this)
  setMotor(LA, LB, L_CHANNEL, -left);
  setMotor(RA, RB, R_CHANNEL, -right);
}

/* ========= SETUP ========= */

void setup() {
  Serial.begin(115200);
  Serial.println("Starting NimBLE Client");

  xboxController.begin();

  // Direction pins
  pinMode(LA, OUTPUT);
  pinMode(LB, OUTPUT);
  pinMode(RA, OUTPUT);
  pinMode(RB, OUTPUT);
  pinMode(WA, OUTPUT);
  pinMode(WB, OUTPUT);

  // ===== ESP32 PWM SETUP =====

  // Configure PWM channels
  ledcSetup(L_CHANNEL, PWM_FREQ, PWM_RES);
  ledcSetup(R_CHANNEL, PWM_FREQ, PWM_RES);
  ledcSetup(W_CHANNEL, PWM_FREQ, PWM_RES);

  // Attach pins to channels
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

    // stop motors immediately
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

  /* ===== CONNECTED (print once) ===== */
  if (!connectedPrinted) {
    Serial.println("CONNECTED");
    Serial.println("Controller Address: " +
                   xboxController.buildDeviceAddressStr());
    connectedPrinted = true;
  }

  /* ===== WAIT FIRST INPUT ===== */
  if (xboxController.isWaitingForFirstNotification()) {
    return;
  }

  if (!firstInputPrinted) {
    Serial.println("First controller input received");
    firstInputPrinted = true;
  }

  auto notif = xboxController.xboxNotif;

  /* ===== CONVERT JOYSTICK VALUES ===== */
  // Library returns:
  //   0 = positive
  //   32767 = center
  //   65535 = negative

  float joyX = -(32767.0 - notif.joyRHori) / 32767.0;  // turning
  float joyY = (32767.0 - notif.joyRVert) / 32767.0;   // forward/back

  joyX = constrain(joyX, -1.0, 1.0);
  joyY = constrain(joyY, -1.0, 1.0);

  /* ===== JOYSTICK DEADBAND ===== */
  if (abs(joyX) < 0.08) joyX = 0;
  if (abs(joyY) < 0.08) joyY = 0;

  /* ===== TRIGGER ===== */
  float trigMax = XboxControllerNotificationParser::maxTrig;
  float leftTrigger = notif.trigLT / trigMax;

  /* ===== ENABLE ROBOT ===== */

  if (notif.btnLB) lbPressed = true;
  if (notif.btnRB) rbPressed = true;

  if (!notif.btnLB && lbPressed) lbReleased = true;
  if (!notif.btnRB && rbPressed) rbReleased = true;

  if (!robotEnabled && lbReleased && rbReleased) {
    robotEnabled = true;
    Serial.println("ROBOT ENABLED");
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