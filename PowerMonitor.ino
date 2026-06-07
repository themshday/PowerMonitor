/*
 * =============================================================================
 * INA226 Power Monitor – Direct Register I2C Version
 * =============================================================================
 * Version: 1.0.0
 * 
 * Features:
 *   - Real‑time voltage (V), current (A), power (W) with 4‑decimal precision
 *   - Max values recorded since start
 *   - Total charge (mAh) and total energy (mWh) accumulation
 *   - Runtime display with day rollover (e.g. "1D 13:53:43")
 *   - OLED 128x64 I2C display (SSD1306)
 *   - Serial output for monitoring & debugging
 *   - Configurable over‑current alert with passive buzzer
 *   - Buzzer test beep on startup
 *   - No external INA226 library required (Wire only)
 *   - All configurable parameters in one section
 * 
 * Hardware:
 *   - Arduino Nano (ATmega328, 5V logic)
 *   - INA226 module with R100 shunt (0.1 Ω)
 *   - 0.96" SSD1306 128×64 OLED I2C display
 *   - Passive piezo buzzer (with series resistor)
 * 
 * Author: themshday
 * License: MIT
 * =============================================================================
 */

 #include <Wire.h>
 #include <Adafruit_GFX.h>
 #include <Adafruit_SSD1306.h>
 
 // ==================== CONFIGURATION ====================
 #define FIRMWARE_VERSION "1.0.0"
 
 // I2C addresses
 #define INA226_ADDRESS   0x40
 #define OLED_ADDRESS     0x3C
 
 // OLED settings
 #define SCREEN_WIDTH     128
 #define SCREEN_HEIGHT    64
 #define OLED_RESET       -1
 
 // Shunt resistor (ohms)
 #define SHUNT_RESISTANCE 0.1f
 
 // Maximum expected current (amps) – used for calibration
 #define MAX_EXPECTED_CURRENT 1.0f
 
 // Alert & buzzer
 #define ALERT_PIN               2
 #define BUZZER_PIN              3
 #define OVERCURRENT_THRESHOLD_A 1.000f  // alert if current exceeds 1000 mA
 #define ALERT_COOLDOWN_MS       2000
 #define BUZZER_FREQUENCY        1000
 #define BUZZER_BEEP_MS          100
 #define STARTUP_BEEP_COUNT      3
 
 // Serial & display
 #define SERIAL_BAUD             115200
 #define DISPLAY_UPDATE_INTERVAL 200
 
 // INA226 registers (do not change)
 #define INA226_REG_CONFIG      0x00
 #define INA226_REG_SHUNT_VOLT  0x01
 #define INA226_REG_BUS_VOLT    0x02
 #define INA226_REG_POWER       0x03
 #define INA226_REG_CURRENT     0x04
 #define INA226_REG_CALIBRATION 0x05
 #define INA226_REG_MASK_ENABLE 0x06
 #define INA226_REG_ALERT_LIMIT 0x07
 #define INA226_CONFIG_DEFAULT  0x4127
 
 // ==================== GLOBALS ====================
 Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
 // Instantaneous values (volts, amps, watts)
 float voltage_now = 0;   // V
 float current_now = 0;   // A
 float power_now   = 0;   // W
 
 // Maximums
 float voltage_max = 0;
 float current_max = 0;
 float power_max   = 0;
 
 // Totals (mAh and mWh)
 float total_mAh = 0;
 float total_mWh = 0;
 
 // Runtime
 unsigned long start_time = 0;
 unsigned long last_display_update = 0;
 unsigned long last_alert_time = 0;
 volatile bool alert_triggered = false;
 char runtime_str[20];
 
 uint16_t calibration_value = 0;
 float current_LSB = 0;   // A per LSB
 
 // ----- I²C helpers -----
 void writeRegister(uint8_t reg, uint16_t value) {
   Wire.beginTransmission(INA226_ADDRESS);
   Wire.write(reg);
   Wire.write((value >> 8) & 0xFF);
   Wire.write(value & 0xFF);
   Wire.endTransmission();
 }
 
 uint16_t readRegister(uint8_t reg) {
   Wire.beginTransmission(INA226_ADDRESS);
   Wire.write(reg);
   Wire.endTransmission();
   Wire.requestFrom((uint8_t)INA226_ADDRESS, (uint8_t)2);
   uint16_t value = 0;
   if (Wire.available() == 2) {
     value = Wire.read() << 8;
     value |= Wire.read();
   }
   return value;
 }
 
 // ----- Buzzer -----
 void beep(int count, int duration_ms) {
   for (int i = 0; i < count; i++) {
     tone(BUZZER_PIN, BUZZER_FREQUENCY, duration_ms);
     delay(duration_ms + 50);
   }
 }
 
 // ----- Runtime formatting -----
 void formatRuntime(unsigned long total_sec, char* buffer) {
   if (total_sec < 86400) {
     unsigned int h = total_sec / 3600;
     unsigned int m = (total_sec % 3600) / 60;
     unsigned int s = total_sec % 60;
     sprintf(buffer, "%02u:%02u:%02u", h, m, s);
   } else {
     unsigned int days = total_sec / 86400;
     unsigned int remainder = total_sec % 86400;
     unsigned int h = remainder / 3600;
     unsigned int m = (remainder % 3600) / 60;
     unsigned int s = remainder % 60;
     sprintf(buffer, "%uD %02u:%02u:%02u", days, h, m, s);
   }
 }
 
 // ----- OLED update -----
 void updateOLED() {
   display.clearDisplay();
   display.setTextSize(1);
   display.setTextColor(SSD1306_WHITE);
 
   // Voltage: V:12.1234 M:12.1234
   display.setCursor(0, 0);
   display.print("V:"); display.print(voltage_now, 4);
   display.print(" M:"); display.print(voltage_max, 4);
 
   // Current: A:0.1234 M:0.1234
   display.setCursor(0, 12);
   display.print("A:"); display.print(current_now, 4);
   display.print(" M:"); display.print(current_max, 4);
 
   // Power: W:0.4567 M:0.4567
   display.setCursor(0, 24);
   display.print("W:"); display.print(power_now, 4);
   display.print(" M:"); display.print(power_max, 4);
 
   // Totals: mAh:123.45 mWh:123.45
   display.setCursor(0, 36);
   display.print("mAh:"); display.print(total_mAh, 2);
   display.setCursor(64, 36);
   display.print("mWh:"); display.print(total_mWh, 2);
 
   // Runtime
   display.setCursor(0, 48);
   unsigned long elapsed = (millis() - start_time) / 1000;
   formatRuntime(elapsed, runtime_str);
   display.print("Run:"); display.print(runtime_str);
 
   display.display();
 }
 
 // ----- Serial output -----
 void printSerialData(float shunt_mV, uint16_t current_reg) {
   Serial.println(F("-------------------------------"));
   Serial.print(F("Voltage: ")); Serial.print(voltage_now, 4); Serial.print(F(" V, Max: ")); Serial.println(voltage_max, 4);
   Serial.print(F("Current: ")); Serial.print(current_now, 4); Serial.print(F(" A, Max: ")); Serial.println(current_max, 4);
   Serial.print(F("  [Raw Shunt: ")); Serial.print(shunt_mV, 4); Serial.print(F(" mV, Raw Cur Reg: ")); Serial.print(current_reg); Serial.println("]");
   Serial.print(F("Power:   ")); Serial.print(power_now, 4); Serial.print(F(" W, Max: ")); Serial.println(power_max, 4);
   Serial.print(F("Total:   ")); Serial.print(total_mAh, 2); Serial.print(F(" mAh, ")); Serial.print(total_mWh, 2); Serial.println(F(" mWh"));
   unsigned long elapsed = (millis() - start_time) / 1000;
   formatRuntime(elapsed, runtime_str);
   Serial.print(F("Runtime: ")); Serial.println(runtime_str);
   if (alert_triggered) { Serial.println(F("*** ALERT TRIGGERED ***")); alert_triggered = false; }
 }
 
 // ==================== SETUP ====================
 void setup() {
   Serial.begin(SERIAL_BAUD);
   while (!Serial);
   Serial.println(F("INA226 Power Monitor v" FIRMWARE_VERSION));
 
   Wire.begin();
   Wire.setClock(400000);
 
   // OLED
   if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
     Serial.println(F("OLED not found"));
     for(;;);
   }
   display.clearDisplay();
   display.setTextSize(1);
   display.setTextColor(SSD1306_WHITE);
   display.setCursor(0,0);
   display.println(F("INA226 Monitor"));
   display.print(F("v" FIRMWARE_VERSION));
   display.setCursor(0,24);
   display.println(F("Booting..."));
   display.display();
 
   // INA226 check
   uint16_t config_check = readRegister(INA226_REG_CONFIG);
   if (config_check == 0xFFFF || config_check == 0) {
     Serial.println(F("INA226 not detected."));
     display.clearDisplay();
     display.setCursor(0,0);
     display.println(F("INA226 not found!"));
     display.display();
     while (1);
   }
   writeRegister(INA226_REG_CONFIG, INA226_CONFIG_DEFAULT);
   delay(1);
 
   current_LSB = MAX_EXPECTED_CURRENT / 32768.0f;
   calibration_value = (uint16_t)(0.00512f / (current_LSB * SHUNT_RESISTANCE));
   writeRegister(INA226_REG_CALIBRATION, calibration_value);
 
   // Pins
   pinMode(ALERT_PIN, INPUT_PULLUP);
   pinMode(BUZZER_PIN, OUTPUT);
   digitalWrite(BUZZER_PIN, LOW);
 
   // Startup beep
   if (STARTUP_BEEP_COUNT > 0) {
     beep(STARTUP_BEEP_COUNT, BUZZER_BEEP_MS);
   }
 
   start_time = millis();
   last_display_update = millis();
   Serial.println(F("Ready."));
 }
 
 // ==================== LOOP ====================
 void loop() {
   uint16_t shunt_raw  = readRegister(INA226_REG_SHUNT_VOLT);
   uint16_t bus_raw    = readRegister(INA226_REG_BUS_VOLT);
   uint16_t current_raw = readRegister(INA226_REG_CURRENT);
   uint16_t power_raw   = readRegister(INA226_REG_POWER);
 
   // Convert to float
   int16_t shunt_s = (int16_t)shunt_raw;
   float shunt_voltage_mV = shunt_s * 0.0025f;
 
   int16_t bus_s = (int16_t)bus_raw;
   float bus_voltage_V = bus_s * 0.00125f;   // 1.25 mV per LSB -> volts
 
   int16_t cur_s = (int16_t)current_raw;
   float current_A = cur_s * current_LSB;
 
   int16_t pow_s = (int16_t)power_raw;
   float power_W = pow_s * 25.0f * current_LSB;
 
   // Store in volts, amps, watts
   voltage_now = bus_voltage_V;
   current_now = current_A;
   power_now   = power_W;
 
   // Update max values
   if (voltage_now > voltage_max) voltage_max = voltage_now;
   if (current_now > current_max) current_max = current_now;
   if (power_now   > power_max)   power_max   = power_now;
 
   // Accumulate totals (mAh, mWh)
   static unsigned long last_measure = millis();
   unsigned long now = millis();
   float delta_h = (now - last_measure) / 3600000.0f;
   total_mAh += (current_A * 1000.0f) * delta_h;   // mA * hours
   total_mWh += (power_W * 1000.0f)   * delta_h;   // mW * hours
   last_measure = now;
 
   // Alert & buzzer
   if (digitalRead(ALERT_PIN) == LOW) alert_triggered = true;
   if (current_A > OVERCURRENT_THRESHOLD_A) {
     if (millis() - last_alert_time > ALERT_COOLDOWN_MS) {
       beep(1, BUZZER_BEEP_MS);
       last_alert_time = millis();
     }
   }
 
   // OLED refresh
   if (millis() - last_display_update >= DISPLAY_UPDATE_INTERVAL) {
     updateOLED();
     last_display_update = millis();
   }
 
   // Serial output
   static unsigned long last_serial = 0;
   if (millis() - last_serial >= 1000) {
     printSerialData(shunt_voltage_mV, current_raw);
     last_serial = millis();
   }
 }