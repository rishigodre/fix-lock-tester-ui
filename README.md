**Make sure to edit (uncomment required lines of) the libraries/TFT_eSPI/User_Setup.h
with the correct pin numbers for your respective setup.**

```
// ###### EDIT THE PIN NUMBERS IN THE LINES FOLLOWING TO SUIT YOUR ESP32 SETUP   ######

// For ESP32 Dev board (only tested with ILI9341 display)
// The hardware SPI can be mapped to any pins

#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15  // Chip select control pin
#define TFT_DC   2  // Data Command control pin
#define TFT_RST  4  // Reset pin (could connect to RST pin)
//#define TFT_RST  -1  // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST
```
1. DO THE ABOVE CHANGES.
2. Power everything with 3.3V.
3. Short the SLEEP and RESET pins of A4988.
4. Do the connections as follows.

| ILI9341   | ESP32   |
| --------- | ------- |
| VCC       | 3.3V    |
| GND       | GND     |
| CS        | GPIO 15 |
| RESET     | GPIO 4  |
| DC        | GPIO 2  |
| SDI(MOSI) | GPIO 23 |
| SCK       | GPIO 18 |
| LED       | 3.3V    |
| SDO(MISO) | GPIO 19 |

| Touch       | ESP32   |
| ----------- | ------- |
| T_CLK(SCK)  | GPIO 18 |
| T_CS        | GPIO 5  |
| T_DIN(MOSI) | GPIO 23 |
| T_DO(MISO)  | GPIO 19 |
| T_IRQ       | GPIO 27 |

| A4988 | ESP32 |
| ----- | ----- |
| DIR   | 25    |
| SETP  | 26    |
| EN    | 33    |

| A4988  | STEPPER MOTOR |
| ------ | ------------- |
| COIL 1 | 1A, 1B        |
| COIL 2 | 2A, 2B        |

| A4988 | 12V PSU |
| ----- | ------- |
| VMOT  | 12V     |
| GND   | GND     |



**For Windows do the following steps**
1. Install Arduino IDE.
2. Follow the steps in this - [https://forum.arduino.cc/t/downloading-esp32-3-3-5-fails/1420739/7](https://forum.arduino.cc/t/downloading-esp32-3-3-5-fails/1420739/7).
3. Download the CP210x driver for Windows using this guide - [https://randomnerdtutorials.com/install-esp32-esp8266-usb-drivers-cp210x-windows/](https://randomnerdtutorials.com/install-esp32-esp8266-usb-drivers-cp210x-windows/).
4. Install the `TFT_eSPI` and `XPT2046_Touchscreen` libraries from the library manager.
5. Install 2 the ESP32 related board managers by `Arduino` and `Espressif Systems`.
6. Select the correct `COM` port and the board - `ESP32 DEV MODULE`.
7. Press the upload button and let it rip!!!.


