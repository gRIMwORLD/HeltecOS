# HeltecOS

An embedded micro-operating system for the **Heltec WiFi Kit 32 (V3)** featuring dual-core **FreeRTOS** multitasking, an interactive **128x64 OLED GUI**, a full-featured **USB Serial CLI Shell**, real-time **LiPo battery monitoring**, and **BME280 / BMP280 climate telemetry**.

---

## 🛠 Hardware Specifications

* **Microcontroller:** ESP32-S3 (Xtensa® dual-core 32-bit LX7 @ 240 MHz)
* **Flash & SRAM:** 8 MB embedded SPI Flash, 512 KB SRAM
* **Integrated Display:** 0.96-inch monochrome OLED (128×64 SSD1306)
* **Battery Interface:** SH1.25 2-pin LiPo battery connector with onboard TP4054 charger
* **Connectivity:** 2.4 GHz Wi-Fi 802.11 b/g/n, Bluetooth 5 (LE)
* **Controls:** PRG / Boot button (`GPIO 0`), User LED (`GPIO 35`)

---

## 🔌 Wiring & Breadboard Pinout

With the Heltec WiFi Kit 32 (V3) seated on a breadboard straddling the center divider and the **USB-C connector at Row 1 (Top)**:

```text
                      [ USB-C Port ]
             (Left JP2)            (Right JP3)
  Row 1:     GND                   GND  -----------------> [BME280 GND]
  Row 2:     5V                    3V3  -----------------> [BME280 VIN]
  Row 3:     Ve (Switched 3.3V)    3V3
  Row 4:     Ve                    GPIO 26
  Row 5:     RX (GPIO 44)          GPIO 48
  Row 6:     TX (GPIO 43)          GPIO 47
  Row 7:     RST                   GPIO 33
  Row 8:     GPIO 0 (PRG Button)   GPIO 34
  Row 9:     GPIO 21 (OLED RST)    GPIO 35 (User LED)
  Row 10:    GPIO 20               GPIO 36 (Vext Ctrl)
  Row 11:    GPIO 19               GPIO 37 (Battery ADC Ctrl)
  Row 12:    GPIO 7                GPIO 38
  Row 13:    GPIO 6                GPIO 39
  Row 14:    GPIO 5 <------------- | --------------------> [BME280 SCL]
  Row 15:    GPIO 4 <------------- | --------------------> [BME280 SDA]
  Row 16:    GPIO 3                GPIO 42
  Row 17:    GPIO 2                GPIO 45
  Row 18:    GPIO 1 (Battery ADC)  GPIO 46
```

### BME/BMP280 Sensor Connections:
| Sensor Pin | Heltec V3 Header Pin | Breadboard Row | Notes |
| :--- | :--- | :--- | :--- |
| **VIN** | **3V3** | **Right Row 2** | Regulated 3.3V power (Do not use 5V) |
| **GND** | **GND** | **Right Row 1** | Ground reference |
| **SCL** | **GPIO 5** | **Left Row 14** | External I²C Clock |
| **SDA** | **GPIO 4** | **Left Row 15** | External I²C Data |

*Note: The built-in OLED uses internal GPIO 17 (SDA) and GPIO 18 (SCL), keeping the sensor bus completely independent.*

---

## 🔋 Battery Telemetry

* **Voltage Sensing:** `GPIO 1` (`ADC1_CH0`) via an internal 390k / 100k resistor divider.
* **Power Gating:** `GPIO 37` (`ADC_Ctrl`). Driven LOW momentarily during reads to eliminate parasitic battery drain.
* **Auto Power Path:** Unplugging USB smoothly transfers to LiPo power without interrupting system tasks.

---

## 🖥 Software Architecture

```
                 [ FreeRTOS Kernel ]
            /                           \
      [ Core 0 ]                     [ Core 1 ]
          |                              |
     +-----------+             +-------------------+
     | CLI Task  |             |     GUI Task      | (10 FPS OLED Render,
     +-----------+             +-------------------+  PRG Button Debounce)
     (115200 Baud                        |
      ANSI Shell)              +-------------------+
                               |    Sensor Task    | (1 Hz Telemetry Read,
                               +-------------------+  Bosch Forced Mode)
```

### OLED Screen Navigation (PRG Button / `GPIO 0`)
1. **PAGE 1/5: Dashboard** — Uptime, Battery Voltage & %, Free RAM, CPU Clock.
2. **PAGE 2/5: Environmental Sensor** — Live Temperature (°C), Humidity (%RH), Pressure (hPa), Altitude, and live pulsing heartbeat dot.
3. **PAGE 3/5: WiFi Network** — Connection state, IP address, and signal strength.
4. **PAGE 4/5: Tasks Monitor** — Active FreeRTOS task table and core mapping.
5. **PAGE 5/5: About** — Firmware version, hardware specs, and serial baud rate.

---

## 💻 Interactive Serial CLI Shell

Connect using any serial monitor at **115200 baud**:

```bash
pio device monitor -p COM7 -b 115200
```

### Available Shell Commands:
| Command | Description |
| :--- | :--- |
| `help` | List all available shell commands |
| `sysinfo` | Detailed hardware specs, clock, heap memory, and flash size |
| `sensor` | Print raw and calibrated temperature, humidity, and pressure |
| `battery` | Display live battery voltage, charge state, and battery % |
| `offset <val>` | Set live temperature calibration offset in °C (e.g. `offset -1.0`) |
| `i2c scan` | Scan external I²C bus on GPIO 4 / GPIO 5 |
| `ps` | Inspect FreeRTOS tasks and system memory |
| `screen <0..4>` | Switch OLED screen directly from computer terminal |
| `led on/off/t` | Turn onboard user LED (`GPIO 35`) on, off, or toggle |
| `wifi scan` | Scan nearby 2.4 GHz WiFi networks with RSSI and channel info |
| `clear` | Clear terminal screen |
| `reboot` | Perform software restart of the ESP32-S3 |

---

## 🚀 Building & Flashing

### Requirements
* [PlatformIO Core](https://platformio.org/)

### Commands
```bash
# Compile firmware
pio run

# Flash to device via USB
pio run -t upload

# Open interactive terminal
pio device monitor
```
