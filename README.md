# Smart Pill Bottle Cap

An IoT-based smart pill bottle system designed to monitor medication usage and help users take medicines on time.

The system uses an **ESP32-S3**, **Hall Effect sensor**, and **load cell with HX711** to detect bottle opening and monitor changes in medicine weight. The ESP32-S3 processes the sensor data and communicates medication status to a mobile device using **Bluetooth Low Energy (BLE)**.

---

## Project Overview

Many people forget to take their medicines on time, especially elderly patients and people with busy schedules. The Smart Pill Bottle Cap is designed to provide a simple monitoring system for medication adherence.

The system monitors:

- Bottle cap opening and closing
- Bottle weight
- Tablet count
- Tablet removal
- Medication taken or missed status
- Event timestamp
- BLE communication with a mobile device

The project is implemented using an **ESP32-S3** and the **NimBLE BLE stack**.

---

## Features

- Bottle cap open/close detection
- Load-cell-based weight measurement
- HX711-based load cell interfacing
- Tablet count estimation
- Tablet removal detection
- Tablet taken/missed decision
- BLE advertising and connection
- BLE notifications to a connected phone
- Weight and tablet count reporting
- Event timestamp generation
- Weight filtering and stability detection

---

## System Architecture

The system consists of four major parts:

```text
                    SMART PILL BOTTLE CAP
                           |
          +----------------+----------------+
          |                                 |
     Hall Sensor                       Load Cell
          |                                 |
          |                              HX711
          |                                 |
          +---------------+-----------------+
                          |
                       ESP32-S3
                          |
                   BLE Communication
                          |
                    Mobile Device
