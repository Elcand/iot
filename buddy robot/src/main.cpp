#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Bounce2.h>

// ---------------- Display config ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- Sensors & input ----------------
Adafruit_MPU6050 mpu;

#define BUTTON_PIN 15
#define TOUCH_PIN 4 // TTP223 touch module OUT (red board). HIGH = touched.

Bounce2::Button button = Bounce2::Button();

// ---------------- Clock config (realtime) ----------------
// In Wokwi simulator this open network works. On real hardware replace
// with your own SSID / password.
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASS = "";
const long GMT_OFFSET_SEC = 7 * 3600; // GMT+7 Jakarta
const int DAYLIGHT_OFFSET_SEC = 0;
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";
const char *NTP_SERVER_3 = "id.pool.ntp.org";

const char *DAY_NAMES_EN[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

bool ntpAvailable = false;
unsigned long softwareClockBase = 0; // millis() when software clock started
int softwareStartHour = 10;
int softwareStartMin = 0;
int softwareStartSec = 0;
int softwareStartWday = 1; // 0=Sunday, 1=Monday

// ---------------- Emotion state machine ----------------
enum EmotionState
{
  EMO_NORMAL,
  EMO_HAPPY,
  EMO_SAD,
  EMO_DIZZY,
  EMO_CONFUSED,
  EMO_ANGRY,
  EMO_LOOK_LEFT,
  EMO_LOOK_RIGHT,
  EMO_CLOCK
};

EmotionState currentState = EMO_NORMAL;
EmotionState stateBeforeClock = EMO_NORMAL;

unsigned long lastBlinkTime = 0;
unsigned long blinkUntil = 0;
unsigned long stateTimer = 0;        // generic state timeout
unsigned long lastInteraction = 0;   // last time user interacted
unsigned long lastEyeShift = 0;
int lookAroundDir = 0;

// Thresholds
const float SHAKE_DIZZY_ACCEL = 17.0;     // m/s^2, fast spin / hard shake
const float SHAKE_CONFUSED_ACCEL = 13.0;  // m/s^2, light shake
const float SPIN_DIZZY_GYRO = 250.0;      // deg/s, sudden rotation
const float TILT_LOOK_THRESHOLD = 4.5;    // m/s^2 on X axis
const float UPSIDE_DOWN_Z = -5.0;         // m/s^2 on Z axis
const unsigned long SAD_TIMEOUT_MS = 15000;
const unsigned long DIZZY_DURATION_MS = 3000;
const unsigned long ANGRY_DURATION_MS = 2500;
const unsigned long HAPPY_DURATION_MS = 2000;
const unsigned long CONFUSED_DURATION_MS = 1800;

// ---------------- Forward declarations ----------------
bool isPettedByTouch();
void updateClockSource();
bool getCurrentTime(int &h12, int &minute, int &second, bool &isPM, int &wday, int &day, int &month, int &year);
void drawClockMode();
void drawEyesBasic(int w, int h, int xOffset, int yOffset);
void drawHappy();
void drawSad();
void drawAngry();
void drawConfused();
void drawDizzy();
void drawLook(bool toLeft);
void drawNormal(bool blink);
void setEmotion(EmotionState next, unsigned long durationMs = 0);

// ================================================================
// SETUP
// ================================================================
void setup()
{
  Serial.begin(115200);
  Serial.println(F("Buddy Robot starting..."));

  button.attach(BUTTON_PIN, INPUT_PULLUP);
  button.interval(25);
  button.setPressedState(LOW);

  // TTP223 touch module: idle LOW, touched HIGH.
  // In Wokwi the red button emulates the touch pad (press T).
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("Failed to init SSD1306 OLED!"));
    for (;;)
      ;
  }

  if (!mpu.begin())
  {
    Serial.println(F("Failed to find MPU6050! Check wiring."));
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 25);
    display.print(F("MPU6050 missing!"));
    display.display();
    // Do not halt: clock + eyes can still run without IMU.
    delay(1500);
  }
  else
  {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(25, 15);
  display.print(F("Hello!"));
  display.setTextSize(1);
  display.setCursor(20, 40);
  display.print(F("Warming up..."));
  display.display();

  // Try WiFi + NTP for realtime clock (non-blocking, short timeout)
  updateClockSource();
  softwareClockBase = millis();

  lastBlinkTime = millis();
  lastInteraction = millis();
  stateTimer = millis();

  delay(1200);
  Serial.println(F("Setup done."));
}

// Try to connect to WiFi and configure NTP. Falls back to software clock.
void updateClockSource()
{
  if (strlen(WIFI_SSID) == 0)
  {
    ntpAvailable = false;
    Serial.println(F("No WiFi SSID set, using software clock."));
    return;
  }

  Serial.print(F("Connecting WiFi: "));
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000)
  {
    delay(250);
    Serial.print(F("."));
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F("WiFi connected, configuring NTP..."));
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC,
               NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    struct tm tmp;
    if (getLocalTime(&tmp, 5000))
    {
      ntpAvailable = true;
      Serial.println(F("NTP time synced."));
    }
    else
    {
      ntpAvailable = false;
      Serial.println(F("NTP sync failed, using software clock."));
    }
  }
  else
  {
    ntpAvailable = false;
    Serial.println(F("WiFi failed, using software clock."));
  }
}

// Returns 12-hour time + date. True = NTP time, false = software fallback.
bool getCurrentTime(int &h12, int &minute, int &second, bool &isPM,
                    int &wday, int &day, int &month, int &year)
{
  struct tm ti;
  if (ntpAvailable && getLocalTime(&ti, 1000))
  {
    int h24 = ti.tm_hour;
    isPM = (h24 >= 12);
    h12 = h24 % 12;
    if (h12 == 0)
      h12 = 12;
    minute = ti.tm_min;
    second = ti.tm_sec;
    wday = ti.tm_wday;
    day = ti.tm_mday;
    month = ti.tm_mon + 1;
    year = ti.tm_year + 1900;
    return true;
  }

  // Software clock fallback: advance from start values using millis()
  unsigned long elapsed = (millis() - softwareClockBase) / 1000;
  long totalSec = softwareStartHour * 3600L + softwareStartMin * 60L +
                  softwareStartSec + (long)elapsed;
  int h24 = (totalSec / 3600) % 24;
  minute = (totalSec / 60) % 60;
  second = totalSec % 60;
  isPM = (h24 >= 12);
  h12 = h24 % 12;
  if (h12 == 0)
    h12 = 12;
  int dayOffset = (softwareStartHour * 3600L + softwareStartMin * 60L +
                   softwareStartSec + (long)elapsed) /
                  86400L;
  wday = (softwareStartWday + dayOffset) % 7;
  day = 1;
  month = 1;
  year = 2026;
  return false;
}

// ================================================================
// SENSORS
// ================================================================
// TTP223 red touch module gives digital HIGH while touched.
bool isPettedByTouch()
{
  return digitalRead(TOUCH_PIN) == HIGH;
}

void setEmotion(EmotionState next, unsigned long durationMs)
{
  if (currentState == EMO_CLOCK && next != EMO_CLOCK)
    stateBeforeClock = next; // remember for later, not used now
  if (currentState != next)
  {
    currentState = next;
    stateTimer = millis();
    Serial.print(F("Emotion -> "));
    Serial.println((int)next);
  }
  if (durationMs > 0)
    stateTimer = millis(); // restart timeout window
}

// ================================================================
// DRAW HELPERS
// ================================================================
void drawEyesBasic(int w, int h, int xOffset, int yOffset)
{
  display.clearDisplay();
  display.fillRoundRect(32 - w / 2 + xOffset, 30 - h / 2 + yOffset, w, h, 8, SSD1306_WHITE);
  display.fillRoundRect(96 - w / 2 + xOffset, 30 - h / 2 + yOffset, w, h, 8, SSD1306_WHITE);
  display.display();
}

void drawHappy()
{
  // Happy: tall rounded eyes + smile curve
  display.clearDisplay();
  display.fillRoundRect(32 - 15, 30 - 19, 30, 38, 10, SSD1306_WHITE);
  display.fillRoundRect(96 - 15, 30 - 19, 30, 38, 10, SSD1306_WHITE);
  // Smile
  display.drawCircle(64, 38, 14, SSD1306_WHITE);
  display.fillRect(50, 30, 28, 12, SSD1306_BLACK); // mask top half -> smile arc
  display.fillRect(0, 50, 128, 14, SSD1306_BLACK); // clean below
  display.display();
}

void drawSad()
{
  // Sad: droopy half eyes + sad arc + tear drop
  display.clearDisplay();
  display.fillRoundRect(32 - 14, 30 - 10, 28, 20, 7, SSD1306_WHITE);
  display.fillRoundRect(96 - 14, 30 - 10, 28, 20, 7, SSD1306_WHITE);
  display.fillRect(0, 0, 128, 20, SSD1306_BLACK); // droopy eyelids
  display.fillRect(18, 20, 28, 4, SSD1306_BLACK);
  display.fillRect(82, 20, 28, 4, SSD1306_BLACK);
  // Sad mouth (inverted arc)
  display.drawCircle(64, 58, 10, SSD1306_WHITE);
  display.fillRect(54, 52, 20, 12, SSD1306_BLACK);
  // Tear under right eye
  display.fillCircle(104, 46, 2, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(8, 55);
  display.print(F("Lonely..."));
  display.display();
}

void drawAngry()
{
  // Angry: slanted brows + narrow eyes
  display.clearDisplay();
  display.fillRoundRect(32 - 15, 30 - 8, 30, 16, 4, SSD1306_WHITE);
  display.fillRoundRect(96 - 15, 30 - 8, 30, 16, 4, SSD1306_WHITE);
  // Brows: left falling to center, right falling to center
  display.fillTriangle(14, 8, 50, 20, 14, 16, SSD1306_WHITE);
  display.fillTriangle(114, 8, 78, 20, 114, 16, SSD1306_WHITE);
  // Frown
  display.drawCircle(64, 60, 12, SSD1306_WHITE);
  display.fillRect(52, 54, 24, 10, SSD1306_BLACK);
  display.display();
}

void drawConfused()
{
  // Confused: one big eye, one small eye + question mark
  display.clearDisplay();
  display.fillRoundRect(32 - 16, 30 - 19, 32, 38, 10, SSD1306_WHITE);
  display.fillRoundRect(96 - 9, 30 - 11, 18, 22, 6, SSD1306_WHITE);
  // Tilted mouth
  display.drawLine(52, 52, 78, 48, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(58, 8);
  display.print(F("?"));
  display.display();
}

void drawDizzy()
{
  display.clearDisplay();
  // Left X eye
  display.drawLine(20, 18, 44, 42, SSD1306_WHITE);
  display.drawLine(44, 18, 20, 42, SSD1306_WHITE);
  display.drawLine(21, 18, 45, 42, SSD1306_WHITE);
  // Right X eye
  display.drawLine(84, 18, 108, 42, SSD1306_WHITE);
  display.drawLine(108, 18, 84, 42, SSD1306_WHITE);
  display.drawLine(85, 18, 109, 42, SSD1306_WHITE);
  // Wavy mouth
  for (int x = 48; x < 80; x += 4)
    display.drawPixel(x, 52 + ((x % 8 == 0) ? 2 : -2), SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(42, 54);
  display.print(F("Dizzy!"));
  display.display();
}

void drawLook(bool toLeft)
{
  // Glance: white frame stays, pupils shift left/right
  int shift = toLeft ? -10 : 10;
  display.clearDisplay();
  // Eye whites
  display.drawRoundRect(32 - 16, 30 - 18, 32, 36, 10, SSD1306_WHITE);
  display.drawRoundRect(96 - 16, 30 - 18, 32, 36, 10, SSD1306_WHITE);
  // Pupils shifted
  display.fillCircle(32 + shift, 30, 9, SSD1306_WHITE);
  display.fillCircle(96 + shift, 30, 9, SSD1306_WHITE);
  display.display();
}

void drawNormal(bool blink)
{
  if (blink)
    drawEyesBasic(30, 4, 0, 0);
  else
    drawEyesBasic(30, 36, 0, 0);
}

// Realtime 12-hour clock with day text below
void drawClockMode()
{
  int h12, minute, second, wday, day, month, year;
  bool isPM;
  bool realtime = getCurrentTime(h12, minute, second, isPM, wday, day, month, year);

  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", h12, minute);

  char secBuf[16];
  snprintf(secBuf, sizeof(secBuf), ":%02d %s", second, isPM ? "PM" : "AM");

  display.clearDisplay();

  // Time (large, centered)
  display.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(timeBuf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int)w) / 2, 6);
  display.print(timeBuf);

  // Seconds + AM/PM
  display.setTextSize(1);
  display.getTextBounds(secBuf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int)w) / 2, 34);
  display.print(secBuf);

  // Day text below clock
  const char *dayStr = DAY_NAMES_EN[wday % 7];
  display.setTextSize(2);
  display.getTextBounds(dayStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int)w) / 2, 46);
  display.print(dayStr);

  if (!realtime)
  {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("No NTP"));
  }

  display.display();
}

// ================================================================
// MAIN LOOP
// ================================================================
void loop()
{
  button.update();

  // ---- Button: short press toggles clock, long press = angry ----
  static unsigned long pressStart = 0;
  static bool longFired = false;

  if (button.pressed())
  {
    pressStart = millis();
    longFired = false;
  }
  if (button.isPressed() && !longFired && millis() - pressStart > 1000)
  {
    longFired = true;
    if (currentState == EMO_CLOCK)
    {
      currentState = stateBeforeClock;
    }
    setEmotion(EMO_ANGRY, ANGRY_DURATION_MS);
    lastInteraction = millis();
    Serial.println(F("Long press: angry"));
  }
  if (button.released())
  {
    if (!longFired)
    {
      if (currentState == EMO_CLOCK)
      {
        currentState = EMO_NORMAL;
        Serial.println(F("Exit clock mode"));
      }
      else
      {
        stateBeforeClock = currentState;
        currentState = EMO_CLOCK;
        Serial.println(F("Enter clock mode"));
      }
      delay(50);
    }
  }

  if (currentState == EMO_CLOCK)
  {
    drawClockMode();
    delay(200); // 5 FPS is enough for a clock, saves flicker
    return;
  }

  // ---- Read IMU ----
  float totalAccel = 9.8;
  float gyroDeg = 0;
  float ax = 0, ay = 0, az = 9.8;
  bool imuOk = false;

  sensors_event_t a, g, temp;
  if (mpu.getEvent(&a, &g, &temp))
  {
    imuOk = true;
    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
    totalAccel = sqrt(ax * ax + ay * ay + az * az);
    float gyroRad = sqrt(g.gyro.x * g.gyro.x + g.gyro.y * g.gyro.y + g.gyro.z * g.gyro.z);
    gyroDeg = gyroRad * 57.2958;
  }

  // ---- Read touch pad (petted = HIGH) ----
  bool pettedTouch = isPettedByTouch();

  unsigned long now = millis();

  // Priority 1: sudden spin / hard shake -> DIZZY
  if (imuOk && (gyroDeg > SPIN_DIZZY_GYRO || totalAccel > SHAKE_DIZZY_ACCEL))
  {
    setEmotion(EMO_DIZZY, DIZZY_DURATION_MS);
    lastInteraction = now;
  }
  // Priority 2: upside down -> ANGRY
  else if (imuOk && az < UPSIDE_DOWN_Z)
  {
    setEmotion(EMO_ANGRY, ANGRY_DURATION_MS);
    lastInteraction = now;
  }
  // Priority 3: petted on touch pad -> HAPPY
  else if (pettedTouch)
  {
    setEmotion(EMO_HAPPY, HAPPY_DURATION_MS);
    lastInteraction = now;
  }
  // Priority 4: tilt sideways -> glance left / right
  else if (imuOk && totalAccel < SHAKE_CONFUSED_ACCEL)
  {
    if (ax > TILT_LOOK_THRESHOLD)
    {
      setEmotion(EMO_LOOK_RIGHT, 0);
      lastInteraction = now;
    }
    else if (ax < -TILT_LOOK_THRESHOLD)
    {
      setEmotion(EMO_LOOK_LEFT, 0);
      lastInteraction = now;
    }
  }

  // Priority 5: light shake -> CONFUSED (only if not locked in timed emotion)
  bool inTimedEmotion =
      (currentState == EMO_DIZZY && now - stateTimer < DIZZY_DURATION_MS) ||
      (currentState == EMO_ANGRY && now - stateTimer < ANGRY_DURATION_MS) ||
      (currentState == EMO_HAPPY && now - stateTimer < HAPPY_DURATION_MS) ||
      (currentState == EMO_CONFUSED && now - stateTimer < CONFUSED_DURATION_MS);

  if (!inTimedEmotion && imuOk && totalAccel > SHAKE_CONFUSED_ACCEL)
  {
    setEmotion(EMO_CONFUSED, CONFUSED_DURATION_MS);
    lastInteraction = now;
    inTimedEmotion = true;
  }

  // Touch activity keeps SAD away
  if (pettedTouch)
    lastInteraction = now;

  // Priority 6: timeouts -> back to normal, or SAD if lonely too long
  if (currentState == EMO_DIZZY && now - stateTimer > DIZZY_DURATION_MS)
    setEmotion(EMO_NORMAL, 0);
  else if (currentState == EMO_ANGRY && now - stateTimer > ANGRY_DURATION_MS)
    setEmotion(EMO_NORMAL, 0);
  else if (currentState == EMO_HAPPY && now - stateTimer > HAPPY_DURATION_MS)
    setEmotion(EMO_NORMAL, 0);
  else if (currentState == EMO_CONFUSED && now - stateTimer > CONFUSED_DURATION_MS)
    setEmotion(EMO_NORMAL, 0);

  if (now - lastInteraction > SAD_TIMEOUT_MS)
  {
    if (currentState != EMO_SAD)
      setEmotion(EMO_SAD, 0);
  }
  else if (currentState == EMO_SAD)
  {
    setEmotion(EMO_NORMAL, 0); // someone came back
  }

  // ---- Render ----
  switch (currentState)
  {
  case EMO_HAPPY:
    drawHappy();
    break;
  case EMO_SAD:
    // Slow blink while sad
    if (now - lastBlinkTime > 4000)
    {
      drawEyesBasic(28, 4, 0, 4);
      if (now - lastBlinkTime > 4200)
        lastBlinkTime = now;
    }
    else
    {
      drawSad();
    }
    break;
  case EMO_DIZZY:
    drawDizzy();
    break;
  case EMO_CONFUSED:
    drawConfused();
    break;
  case EMO_ANGRY:
    drawAngry();
    break;
  case EMO_LOOK_LEFT:
    drawLook(true);
    // Auto return to normal when device is upright again
    if (imuOk && ax > -2.0)
      setEmotion(EMO_NORMAL, 0);
    break;
  case EMO_LOOK_RIGHT:
    drawLook(false);
    if (imuOk && ax < 2.0)
      setEmotion(EMO_NORMAL, 0);
    break;
  case EMO_CLOCK:
    drawClockMode();
    break;
  case EMO_NORMAL:
  default:
    // Periodic blink + occasional look-around when idle
    if (now - lastBlinkTime > 3000)
    {
      drawNormal(true);
      if (now - lastBlinkTime > 3180)
      {
        lastBlinkTime = now;
        // Small chance to glance around while idle
        if (now - lastEyeShift > 9000 && now - lastInteraction > 5000)
        {
          lastEyeShift = now;
          lookAroundDir = (lookAroundDir == 0) ? 1 : 0;
          if (lookAroundDir == 1)
            setEmotion(lookAroundDir % 2 ? EMO_LOOK_LEFT : EMO_LOOK_RIGHT, 0);
        }
      }
    }
    else
    {
      drawNormal(false);
    }
    break;
  }

  delay(50);
}
