
# Adaptive Multi-Cell Battery Management System (ESP32)

An integrated, non-blocking embedded battery management system built on ESP32, combining six functional layers into a single Wokwi project: sensing & analytics, safety protection, embedded HMI, fault-tolerant runtime, cloud telemetry, and an executive risk dashboard.

##  Live Simulation
**Wokwi Project:** (https://wokwi.com/projects/469158982357556225)

##  Dashboard
**Blynk Dashboard:** (https://blynk.cloud/dashboard/721688/templates/1513878/info)

##  System Architecture

Battery/Safety/Runtime state feeds into HMI, Telemetry, and Dashboard modules — all sharing sensor data through getter functions, never touching each other's internals. Full diagram in `/docs` and in the project report.

##  Modules

| File | Responsibility |
|---|---|
| `sketch.ino` | Core loop, cell sensing, pack analytics, health classification |
| `safety_kernel.h/.cpp` | Event-driven fault detection, relay cutoff, anti-chatter, recovery FSM |
| `hmi_kernel.h/.cpp` | Flicker-free rotating LCD diagnostics, fault-priority override |
| `runtime_kernel.h/.cpp` | Hardware/software fault isolation, staged operational-mode FSM, fault log |
| `telemetry_kernel.h/.cpp` | Event-driven Blynk cloud sync, WiFi reconnect handling, signal monitoring |
| `dashboard_kernel.h/.cpp` | Risk scoring (0–100), severity tiers, operator recommendations |

## Hardware (Wokwi simulated)

- ESP32 DevKit-C V4
- 4× Potentiometer (cell voltage simulation)
- 3× LED (health status)
- Relay module (pack cutoff)
- Buzzer (audible alert)
- LCD1602 I2C (diagnostic display)

##  Getting Started

1. Open the Wokwi project link above (or clone this repo and import into Wokwi).
2. Install the **Blynk** library via Wokwi's Library Manager (already listed in `libraries.txt`).
3. Fill in your own Blynk credentials in `telemetry_kernel.cpp`:
   - `BLYNK_TEMPLATE_ID`
   - `BLYNK_TEMPLATE_NAME`
   - `BLYNK_AUTH`
4. Run the simulation — Serial Monitor will show live pack + safety + runtime status.

##  Documentation

- Full project report: (https://1drv.ms/w/c/eaff95a649487188/IQA0Ryz5vm4pToEfhH145GYvAXg3K9uXIXacOKgrtbR-woU?e=S6PrNG)


##  Author

**Yashitha D**
