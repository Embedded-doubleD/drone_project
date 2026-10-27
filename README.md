# Embedded Drone Telemetry & Black-Box Monitoring System
Course: Embedded Platforms and Communications for IoT (EPC4IoT)
Project: Embedded Subsystem for Drone Tracking and Telemetry
Target Hardware: NUCLEO-WL55JC Development Board
Operating System: Zephyr Real-Time Operating System (RTOS)

## 1. Executive Summary & System Overview

This repository contains the complete firmware and devicetree configuration for an embedded Drone Telemetry and Black-Box Monitoring Subsystem built on Zephyr RTOS for the STM32WL55JC microcontroller.

The node functions as an independent observation module mounted on a mobile UAV/drone platform. It continuously acquires multi-sensor metrics (6-axis inertial motion, GPS position and velocity, ambient light, temperature, and relative humidity), evaluates operational drone states (IDLE, FLYING, PROBLEM), drives a visual RGB status indicator, and streams structured telemetry over a serial interface at 10 Hz to feed an external flight simulator or ground control workstation.

### Key System Characteristics
* Non-Interference Policy: Strictly performs monitoring and logging; does not control flight actuators or motors.
* Event-Driven RTOS Architecture: Multi-threaded execution model using Zephyr primitives (K_THREAD_DEFINE, k_msgq, k_timer, GPIO ISRs).
* High-Rate Telemetry: Fixed 10 Hz machine-readable $SIMU ASCII output protocol with XOR checksum verification.
* Ultra-Low Power Optimizations: On-demand GPIO sensor power gating for the LDR voltage divider and STOP2 power management substate policy locking.

---

## 2. Complete Hardware Interfacing & Pinout Specification

The hardware system integrates five main sensor/actuator peripherals connected to the NUCLEO-WL55JC board via I2C, UART, ADC, and GPIO interfaces.

### 2.1 Exhaustive Pin Mapping Table

| Peripheral / Sensor | Function / Signal | MCU Port & Pin | Arduino Header Pin Label | Electrical / Interface Details | Devicetree Node & Alias |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **I2C1 Bus** | Serial Clock (`scl`) | **`PA9`** | **`D9`** (CN5 Pin 10) | Shared I2C Clock Bus (100 kHz) | `&i2c1` / `i2c-bus` |
| | Serial Data (`sda`) | **`PA10`** | **`A2`** (CN8 Pin 3) | Shared I2C Data Bus (100 kHz) | `&i2c1` / `i2c-bus` |
| **MPU6050 IMU** | I2C Address Select | `GND` | - | Tied to GND (Fixes I2C Address `0x68`) | `mpu6050@68` |
| | Interrupt Signal | `PA6` | `D12` | *Unused / Disconnected* (Polled via I2C) | - |
| **Si7021 Sensor** | Temp & Humidity | - | - | Connected to shared I2C Bus (`0x40`) | `si7021@40` |
| **Ultimate GPS PA6H** | MCU RX <- GPS TX | **`PA3`** | **`D0`** (CN9 Pin 1) | UART RX Signal (9600 bps NMEA) | `&usart2` / `gps` |
| | MCU TX -> GPS RX | **`PA2`** | **`D1`** (CN9 Pin 2) | UART TX Signal (9600 bps NMEA) | `&usart2` / `gps` |
| **LDR Light Sensor** | ADC Analog Input | **`PB1`** | **`A0`** (CN8 Pin 1) | Channel `ADC1_IN5` (12-bit, 0–4095) | `&adc1 channel@5` / `analog-in` |
| | Power Supply Control | **`PB2`** | **`A1`** (CN8 Pin 2) | GPIO Output (Gated 3.3V Power Supply) | `ldr_pwr_node` / `ldr-pwr` |
| **Common Anode RGB LED** | Red Cathode | **`PB3`** | **`D3`** (CN9 Pin 4) | Active Low (`GPIO_ACTIVE_LOW`), 220 Ohm Resistor | `red_led_node` / `led-red` |
| | Green Cathode | **`PC2`** | **`D8`** (CN5 Pin 1) | Active Low (`GPIO_ACTIVE_LOW`), 1.2 kOhm Resistor | `green_led_node` / `led-green` |
| | Blue Cathode | **`PA4`** | **`D10`** (CN5 Pin 3) | Active Low (`GPIO_ACTIVE_LOW`), 220 Ohm Resistor | `blue_led_node` / `led-blue` |
| **User Button B2** | User Mode Switch | **`PA1`** | **Onboard B2** | Active Low with Pull-Up (`sw0`) | `user_button_2` / `sw0` |

---

### 2.2 Detailed Connection Instructions

#### 1. Shared I2C Bus (MPU6050 & Si7021)
* Connect MCU **`PA9`** (`D9`) to the breadboard **SCL rail**. Connect SCL of both MPU6050 and Si7021 to this rail.
* Connect MCU **`PA10`** (`A2`) to the breadboard **SDA rail**. Connect SDA of both MPU6050 and Si7021 to this rail.
* Connect `VDD` of both sensors to **+3.3V** and `GND` to common **GND**.
* Connect the **`AD0`** pin of the MPU6050 to **GND** (sets hardware I2C address to `0x68`). The `INT` pin is left unconnected as data is acquired via polling.

#### 2. GPS Module PA6H (USART2)
* Connect GPS **`TX`** to MCU **`PA3`** (`D0` / RX2).
* Connect GPS **`RX`** to MCU **`PA2`** (`D1` / TX2).
* Power the GPS module via **+3.3V** (or +5V) and **GND**.

#### 3. LDR Ambient Light Sensor Circuit (On-Demand Power)
* Form a voltage divider using the LDR photoresistor and a **10 kOhm** fixed resistor in series.
* **Top Rail (Power):** Connect the free terminal of the LDR to MCU pin **`PB2`** (labeled **`A1`** on the board header). This GPIO acts as an on-demand 3.3V power supply.
* **Mid-Node (Signal):** Connect the junction between the LDR and the 10 kOhm resistor to MCU pin **`PB1`** (labeled **`A0`** on the board header, `ADC1_IN5`).
* **Bottom Rail (Ground):** Connect the free terminal of the 10 kOhm resistor to **GND**.

#### 4. Common-Anode RGB LED
* **Anode (Longest Pin):** Connect directly to the **+3.3V** rail.
* **Red Cathode:** Connect via a **220 Ohm** resistor to MCU pin **`PB3`** (`D3`).
* **Green Cathode:** Connect via a **1.2 kOhm** resistor (attenuates green brightness for proper yellow color mixing) to MCU pin **`PC2`** (`D8`).
* **Blue Cathode:** Connect via a **220 Ohm** resistor to MCU pin **`PA4`** (`D10`).

---