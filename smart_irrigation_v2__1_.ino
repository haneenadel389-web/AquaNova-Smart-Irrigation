#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <EEPROM.h>
#include <string.h>

// ================= Pins =================
#define DHTPIN        7
#define DHTTYPE       DHT11
#define SOIL_PIN      A0
#define LDR_PIN       4
#define RELAY_PIN     9
#define FLOW_PIN      2

// ================= Constants =================
const float  LITERS_PER_PULSE  = 0.25 / 25000.0;
const float  TARGET_VOLUME     = 0.25;

const int    DRY_THRESHOLD     = 700;
const int    WET_THRESHOLD     = 500;

const unsigned long PUMP_TIMEOUT_MS = 60000UL;
const unsigned long COOLDOWN_MS     = 300000UL;
const unsigned long SOAKING_MS      = 15000UL;
const unsigned long READ_INTERVAL   = 500UL;

// ================= EEPROM =================
#define EEPROM_ADDR 0

// ================= Objects =================
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= State =================
volatile unsigned long pulseCount         = 0;
bool                   soilWasDry         = false;
unsigned long          lastIrrigationTime = 0;
float                  totalLiters        = 0.0;
float                  savedLiters        = 0.0;

// ================= Last Values  =================
int   lastMoisture = -1;
int   lastLight    = -1;
float lastTemp     = -999;
float lastHum      = -999;

// ================= LCD Buffers =================
char line1[17];
char line2[17];

// ================= Interrupt =================
void flowISR() {
  pulseCount++;
}

// ================= Safe Pulse Read =================
unsigned long safeReadPulses() {
  noInterrupts();
  unsigned long val = pulseCount;
  interrupts();
  return val;
}

// ================= LCD Helper =================
void lcdPrint(const char* l1, const char* l2) {
  lcd.setCursor(0, 0);
  lcd.print(l1);
  for (int i = strlen(l1); i < 16; i++) lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print(l2);
  for (int i = strlen(l2); i < 16; i++) lcd.print(" ");
}

// ================= DHT Read with Retry =================
bool readDHT(float &temp, float &hum) {
  for (int attempt = 0; attempt < 3; attempt++) {
    temp = dht.readTemperature();
    hum  = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) return true;
    delay(500);
  }
  return false;
}

// ================= Update Sensor Screen =================
void updateSensorScreen(int moisture, int light, float temp, float hum, bool dhtOK) {

  bool changed = (moisture != lastMoisture)  ||
                 (light    != lastLight)      ||
                 (dhtOK && (abs(temp - lastTemp) >= 1.0 ||
                            abs(hum  - lastHum)  >= 1.0));

  if (!changed) return;

  snprintf(line1, sizeof(line1), "S:%d%% %s",
           moisture, light ? "Dark" : "Lite");

  if (!dhtOK) {
    snprintf(line2, sizeof(line2), "DHT Error!");
  } else {
    char tBuff[5], hBuff[5];
    dtostrf(temp, 3, 0, tBuff);
    dtostrf(hum,  3, 0, hBuff);
    snprintf(line2, sizeof(line2), "T:%sC H:%s%%", tBuff, hBuff);
  }

  lcdPrint(line1, line2);

  lastMoisture = moisture;
  lastLight    = light;
  if (dhtOK) { lastTemp = temp; lastHum = hum; }
}

// ================= Serial Print =================
void printSerial(int soilRaw, int moisture, int light, float temp, float hum, bool dhtOK) {
  Serial.println("====================");
  Serial.print("Soil Raw: ");    Serial.println(soilRaw);
  Serial.print("Moisture: ");    Serial.print(moisture);  Serial.println("%");
  Serial.print("Light: ");       Serial.println(light ? "Dark" : "Bright");
  if (dhtOK) {
    Serial.print("Temp: ");      Serial.print(temp);      Serial.println(" C");
    Serial.print("Humidity: ");  Serial.print(hum);       Serial.println(" %");
  } else {
    Serial.println("DHT: Failed after 3 attempts");
  }
  Serial.print("Total Water: "); Serial.print(totalLiters, 2); Serial.println(" L");
}

// ================= Save EEPROM if Changed =================
void saveIfChanged() {
  if (totalLiters != savedLiters) {
    EEPROM.put(EEPROM_ADDR, totalLiters);
    savedLiters = totalLiters;
    Serial.println("EEPROM saved.");
  }
}

// ================= Setup =================
void setup() {
  Serial.begin(9600);

  pinMode(LDR_PIN,   INPUT);
  pinMode(FLOW_PIN,  INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, FALLING);

  dht.begin();
  lcd.init();
  lcd.backlight();

  EEPROM.get(EEPROM_ADDR, totalLiters);
  if (isnan(totalLiters) || totalLiters < 0 || totalLiters > 9999) {
    totalLiters = 0.0;
    EEPROM.put(EEPROM_ADDR, totalLiters);
  }
  savedLiters = totalLiters;

  lcdPrint("Smart Irrigation", "System Starting");
  Serial.println("=== SYSTEM START ===");
  Serial.print("Saved Total: "); Serial.print(totalLiters, 2); Serial.println(" L");

  delay(2000);
}

// ================= Main Loop =================
void loop() {

  // ======= 1.sensor reading =======
  long total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(SOIL_PIN);
    delay(5);
  }
  int soilRaw  = constrain(total / 10, 0, 1023);
  int moisture = constrain(map(soilRaw, 1023, 0, 0, 100), 0, 100);

  int   light = digitalRead(LDR_PIN);
  float temp  = 0, hum = 0;
  bool  dhtOK = readDHT(temp, hum);

  // ======= 2. Serial =======
  printSerial(soilRaw, moisture, light, temp, hum, dhtOK);

  // ======= 3. update screen =======
  updateSensorScreen(moisture, light, temp, hum, dhtOK);

  // ======= 4. Hysteresis =======
  if (soilRaw > DRY_THRESHOLD) soilWasDry = true;
  if (soilRaw < WET_THRESHOLD)  soilWasDry = false;

  // ======= 5. Cooldown =======
  bool cooldownOK = (lastIrrigationTime == 0) ||
                    (millis() - lastIrrigationTime >= COOLDOWN_MS);

  // ======= 6. LDR =======
  bool isDaytime = (light == 0);

  // ======= 7. =======
  if (soilWasDry && isDaytime && cooldownOK) {

    
    float prTemp = 0, prHum = 0;
    bool  prOK   = readDHT(prTemp, prHum);

    snprintf(line1, sizeof(line1), "Soil DRY!");
    if (prOK) {
      char tBuff[5], hBuff[5];
      dtostrf(prTemp, 3, 0, tBuff);
      dtostrf(prHum,  3, 0, hBuff);
      snprintf(line2, sizeof(line2), "T:%sC H:%s%%", tBuff, hBuff);
    } else {
      snprintf(line2, sizeof(line2), "DHT Error!");
    }
    lcdPrint(line1, line2);
    Serial.println("Soil DRY -> Need Water!");
    delay(2000);

    
    Serial.println("Pump ON -> Irrigating");

    noInterrupts();
    pulseCount = 0;
    interrupts();

    digitalWrite(RELAY_PIN, LOW);

    unsigned long pumpStart = millis();
    bool  timedOut = false;
    float liters   = 0.0;

    while (liters < TARGET_VOLUME) {

      if (millis() - pumpStart >= PUMP_TIMEOUT_MS) {
        timedOut = true;
        break;
      }

      unsigned long pulses = safeReadPulses();
      liters = pulses * LITERS_PER_PULSE;

      Serial.print("Pulses: "); Serial.print(pulses);
      Serial.print(" | Liters: "); Serial.println(liters, 3);

      char lBuff[5], tgBuff[5];
      dtostrf(liters,        4, 2, lBuff);
      dtostrf(TARGET_VOLUME, 4, 2, tgBuff);
      snprintf(line1, sizeof(line1), "Pump ON");
      snprintf(line2, sizeof(line2), "W:%s/%sL", lBuff, tgBuff);
      lcdPrint(line1, line2);

      delay(200);
    }

    
    digitalWrite(RELAY_PIN, HIGH);
    soilWasDry = false;

    totalLiters += liters;
    saveIfChanged();
    lastIrrigationTime = millis();

    lastMoisture = -1;
    lastTemp     = -999;
    lastHum      = -999;

    
    float poTemp = 0, poHum = 0;
    bool  poOK   = readDHT(poTemp, poHum);

    char tBuff[5], hBuff[5];
    if (poOK) {
      dtostrf(poTemp, 3, 0, tBuff);
      dtostrf(poHum,  3, 0, hBuff);
    }

    if (timedOut) {
      Serial.println("TIMEOUT -> Pump OFF");
      snprintf(line1, sizeof(line1), "TIMEOUT!");
    } else {
      Serial.println("TARGET REACHED -> Pump OFF");
      snprintf(line1, sizeof(line1), "Target Done!");
    }
    if (poOK) snprintf(line2, sizeof(line2), "T:%sC H:%s%%", tBuff, hBuff);
    else      snprintf(line2, sizeof(line2), "DHT Error!");
    lcdPrint(line1, line2);

    Serial.print("Total Water: "); Serial.print(totalLiters, 2); Serial.println(" L");

    delay(3000);

    // --- Soaking---
    unsigned long soakStart  = millis();
    bool          showSoak   = true;
    unsigned long lastSwitch = millis();

    while (millis() - soakStart < SOAKING_MS) {

      unsigned long remaining = (SOAKING_MS - (millis() - soakStart)) / 1000;

      if (showSoak) {
        snprintf(line1, sizeof(line1), "Water Soaking...");
        snprintf(line2, sizeof(line2), "Wait: %lus", remaining);
      } else {
        float sTemp = 0, sHum = 0;
        bool  sOK   = readDHT(sTemp, sHum);
        if (sOK) {
          char sBuff[5], uBuff[5];
          dtostrf(sTemp, 3, 0, sBuff);
          dtostrf(sHum,  3, 0, uBuff);
          snprintf(line1, sizeof(line1), "T:%sC H:%s%%", sBuff, uBuff);
        } else {
          snprintf(line1, sizeof(line1), "DHT Error!");
        }
        snprintf(line2, sizeof(line2), "Wait: %lus", remaining);
      }

      lcdPrint(line1, line2);

      if (millis() - lastSwitch >= 3000) {
        showSoak   = !showSoak;
        lastSwitch = millis();
      }

      delay(500);
    }

  } else {

    digitalWrite(RELAY_PIN, HIGH);

    
    char tBuff[5], hBuff[5];
    if (dhtOK) {
      dtostrf(temp, 3, 0, tBuff);
      dtostrf(hum,  3, 0, hBuff);
    }

    if (!isDaytime) {
      Serial.println("Night Mode -> Pump OFF");
      snprintf(line1, sizeof(line1), "Night S:%d%%", moisture);
      if (dhtOK) snprintf(line2, sizeof(line2), "T:%sC H:%s%%", tBuff, hBuff);
      else       snprintf(line2, sizeof(line2), "DHT Error!");
      lcdPrint(line1, line2);

    } else if (!cooldownOK) {
      Serial.println("Cooldown Active -> Pump OFF");
      snprintf(line1, sizeof(line1), "Cooldown S:%d%%", moisture);
      if (dhtOK) snprintf(line2, sizeof(line2), "T:%sC H:%s%%", tBuff, hBuff);
      else       snprintf(line2, sizeof(line2), "DHT Error!");
      lcdPrint(line1, line2);

    } else {
      Serial.println("Soil Wet -> Pump OFF");
      snprintf(line1, sizeof(line1), "Fine! S:%d%%", moisture);
      if (dhtOK) snprintf(line2, sizeof(line2), "T:%sC H:%s%%", tBuff, hBuff);
      else       snprintf(line2, sizeof(line2), "DHT Error!");
      lcdPrint(line1, line2);
    }

    delay(3000);
  }

  delay(READ_INTERVAL);
}