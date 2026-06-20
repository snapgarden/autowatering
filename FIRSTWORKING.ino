/*
 ╔══════════════════════════════════════════╗
 ║        Auto Water System v1.0            ║
 ║   Arduino UNO R4 WiFi + I2C LCD 1602    ║
 ╚══════════════════════════════════════════╝

 WIRING SUMMARY:
   A0       → Bottom water sensor (S)
   A1       → Top water sensor    (S)
   A4       → LCD SDA
   A5       → LCD SCL
   D7       → Relay IN
   Sensor + → Breadboard 5V rail
   Sensor - → Breadboard GND rail
   LCD VDD  → Breadboard 5V rail
   LCD GND  → Breadboard GND rail
   Relay DC+→ Breadboard 5V rail
   Relay DC-→ Breadboard GND rail
   Relay NO → Pump red wire
   Pump blk → Breadboard GND rail

 ⚠️  IMPORTANT - MISSING CONNECTION:
   Relay COM must be connected to Breadboard 5V rail
   to complete the pump power circuit!
   Without this the pump will never run.

 REQUIRED LIBRARY (install via Library Manager):
   "LiquidCrystal I2C" by Frank de Brabander
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ─── LCD ──────────────────────────────────────────────────────────────────────
// If the LCD stays blank after uploading, change 0x27 to 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── Pin Definitions ──────────────────────────────────────────────────────────
const int PIN_BOTTOM = A0;   // Bottom water sensor signal
const int PIN_TOP    = A1;   // Top water sensor signal
const int PIN_RELAY  = 7;    // Relay IN

// ─── Configuration ────────────────────────────────────────────────────────────
const int  THRESHOLD = 100;  // Analog water threshold (0–1023)
                              // Raise if getting false triggers; lower if not detecting
const int  SAMPLES   = 5;    // Reads averaged per sensor per cycle (noise reduction)
const long RATE_MS   = 50;   // 50 ms = 20 sensor checks per second

// Relay polarity:
//   true  → HIGH signal activates relay  (jumper on "H" / High Level Trigger)
//   false → LOW  signal activates relay  (jumper on "L" / Low Level Trigger)
// If the pump runs backwards (on when it should be off), flip this value.
const bool ACTIVE_HIGH_RELAY = true;
const int  PUMP_ON  = ACTIVE_HIGH_RELAY ? HIGH : LOW;
const int  PUMP_OFF = ACTIVE_HIGH_RELAY ? LOW  : HIGH;

// ─── State Tracking ───────────────────────────────────────────────────────────
enum WaterState { BOOT, EMPTY, FILLING, FULL };
WaterState lastState  = BOOT;
unsigned long lastCheck = 0;

// ─── Helper: Average multiple analog reads for stable sensor value ─────────────
int readSensor(int pin) {
  long total = 0;
  for (int i = 0; i < SAMPLES; i++) {
    total += analogRead(pin);
    delayMicroseconds(200);  // Brief pause between reads
  }
  return (int)(total / SAMPLES);
}

// ─── Helper: Write a left-padded 16-char line to the LCD ─────────────────────
void lcdLine(int row, const char* text) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

// ─── Helper: Control the pump via relay ──────────────────────────────────────
void setPump(bool on) {
  digitalWrite(PIN_RELAY, on ? PUMP_ON : PUMP_OFF);
}

// ─── Helper: Typing animation for startup screen ─────────────────────────────
void typeOnLCD(int col, int row, const char* text, int charDelay = 80) {
  lcd.setCursor(col, row);
  for (int i = 0; text[i] != '\0'; i++) {
    lcd.print(text[i]);
    delay(charDelay);
  }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  pinMode(PIN_RELAY, OUTPUT);
  setPump(false);   // Ensure pump is OFF at power-on

  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Startup animation — types title character by character, holds 3 s total
  //   "Auto Water" = 10 chars × 80 ms = 800 ms
  //   "System"     =  6 chars × 80 ms = 480 ms
  //   Remaining delay                 = 1720 ms
  //   Total                           = 3000 ms  ✓
  typeOnLCD(3, 0, "Auto Water");
  typeOnLCD(5, 1, "System");
  delay(1720);
  lcd.clear();
}

// ─── Main Loop ────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // Only run at 20 Hz
  if (now - lastCheck < (unsigned long)RATE_MS) return;
  lastCheck = now;

  // Read sensors
  bool bottomWet = (readSensor(PIN_BOTTOM) >= THRESHOLD);
  bool topWet    = (readSensor(PIN_TOP)    >= THRESHOLD);

  // Determine current water state
  WaterState newState;
  if      (!bottomWet && !topWet) newState = EMPTY;    // Both dry
  else if ( bottomWet && !topWet) newState = FILLING;  // Bottom only
  else                            newState = FULL;      // Both wet (or edge case: top only)

  // Only update relay + LCD when state actually changes (prevents flicker)
  if (newState == lastState) return;
  lastState = newState;

  switch (newState) {

    case EMPTY:
      // Neither sensor sees water → tank empty → pump on
      setPump(true);
      lcdLine(0, "Status:  EMPTY");
      lcdLine(1, "Pump:    ON");
      break;

    case FILLING:
      // Bottom sensor sees water, top doesn't → still filling → pump on
      setPump(true);
      lcdLine(0, "Status: FILLING");
      lcdLine(1, "Pump:    ON");
      break;

    case FULL:
      // Both sensors see water → tank full → pump off
      setPump(false);
      lcdLine(0, "Status:  FULL");
      lcdLine(1, "Pump:    OFF");
      break;

    default:
      break;
  }
}
