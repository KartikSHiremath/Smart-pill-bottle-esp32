
# Smart Pill Bottle Cap

An IoT-based smart pill bottle cap designed to monitor medication usage and help users take medicines on time.

## Overview

The Smart Pill Bottle Cap uses an ESP32-S3 microcontroller along with a Hall Effect sensor and a load cell with an HX711 amplifier.

The Hall sensor detects whether the bottle cap is opened or closed, while the load cell measures changes in bottle weight to estimate tablet removal.

The ESP32-S3 processes the sensor data and communicates the medication status to a mobile application using Bluetooth Low Energy (BLE).

## Features

- Bottle cap open/close detection
- Bottle weight measurement
- Tablet removal detection
- Medication taken/missed detection
- Bluetooth Low Energy (BLE) communication
- Medication status notifications
- Low-power operation

## Hardware Components

- ESP32-S3
- Hall Effect Sensor
- Load Cell
- HX711 Load Cell Amplifier
- Li-ion/Li-Po Battery
- Pill Bottle Mechanism

## Working Principle

1. The ESP32-S3 initializes the sensors and BLE.
2. The Hall sensor detects bottle cap opening and closing.
3. The load cell measures the bottle weight.
4. The ESP32 processes the weight measurements.
5. Weight changes are used to identify tablet removal.
6. The system determines the medication status.
7. The status is transmitted to the mobile application through BLE.

## Software Architecture

The software consists of:

- Application Layer
- Driver Layer
- Hardware Abstraction Layer (HAL)
- Hardware Layer

## Communication

Bluetooth Low Energy (BLE) is used for communication between the ESP32-S3 and the mobile application.

## Project Team

| Name | Roll No. |
|---|---:|
| Srujan H | 644 |
| Sachin | 650 |
| Kartik S H | 651 |
| Ravi Sanju H | 657 |

## Institution

KLE Technological University  
School of Electronics and Communication Engineering

## Project Type

Course Project – Advanced IoT Systems
