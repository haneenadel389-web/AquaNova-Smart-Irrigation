# AquaNova — Arduino-Based Smart Irrigation System

An automated precision-irrigation system that monitors soil moisture, temperature, humidity, and light in real time, and waters plants only when needed — reducing water waste and preventing over/under-watering.

## Features
- **Hysteresis control** (DRY=700 / WET=500) to prevent relay flickering
- **Daytime-only irrigation** using an LDR to reduce fungal disease risk
- **Volumetric watering**: pump auto-stops at 0.25 L, measured via flow-sensor interrupt (FALLING edge, 25,000 pulses/L)
- **Safety guards**: 60s pump timeout, 5-min cooldown, 15s soaking delay
- **Persistent water-usage log** saved to EEPROM across resets
- **Real-time status** on a 16x2 I2C LCD + serial logging

## Hardware
| Component | Arduino Pin | Mode |
|---|---|---|
| Soil Moisture Sensor | A0 | Analog IN |
| DHT11 Temp/Humidity | D7 | Digital IN |
| LDR Light Sensor | D4 | Digital IN |
| Water Flow Sensor (YF-S201) | D2 | INT0 FALLING |
| Relay Module | D9 | Digital OUT |
| LCD 16x2 | SDA/SCL | I2C (0x27) |

### Wiring Notes
- Soil Moisture → A0 (analog)
- DHT11 → D7 (digital, with 3-attempt retry)
- LDR → D4 (digital: 0=Day, 1=Night)
- Flow Sensor YF-S201 → D2 (INT0, FALLING edge)
- Relay Module → D9 (HIGH=OFF, LOW=ON)
- LCD 16x2 → SDA/SCL, I2C address 0x27

## Files
- `smart_irrigation_v2__1_.ino` — Main Arduino firmware
- `AquaNova_Smart_Irrigation.pdf` — Project presentation
- `AquaNova_Prototype.jpg` — Final prototype photo

## Team
- Hager Mohamed El Azzazy
- Shahd Mohamed Hatab
- Nada Yousry Elalfy
- **Haneen Adel Attia — Hardware & Wiring Engineer**
- Menna Tarek Elfar

**Supervisor:** Dr. Samar Elbedwehy  
**Teaching Assistant:** Eng. Omnia Nagm

## My Contribution
Designed and implemented the full hardware wiring and circuit assembly, including:
- Interfacing soil moisture, DHT11, LDR, and YF-S201 flow sensors with the Arduino UNO
- Wiring the relay module for pump control (active-LOW logic)
- Connecting the 16x2 I2C LCD (SDA/SCL) and verifying address 0x27
- Power distribution, grounding, and safe routing of all connections
- Bench testing each sensor individually before full-system integration
