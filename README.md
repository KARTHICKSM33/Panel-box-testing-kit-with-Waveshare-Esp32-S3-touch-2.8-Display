# Panel-box-testing-kit-with-Waveshare-Eso32-S3-touch-2.8-Display
With this testing kit we can see the issue in the Panel box 

*Requirement* 
1. Esp32 S3 waveshare lcd 2.8
2. Breakout -7semi (rs485 communication)
3. Powermonitore (I2C)
4. NFC Reader PN532 (I2C) -NFC -sticker use pannuvom 
5. 7semi dc - dc converter 5volt 


*LIBRARY Arduino*
1. Esp32 by espressif system -3.0.7 (board Manager)
2. Lvgl by kisvegabor -8.3.10 
3. Adafruit_INA260 -1.5.3 (Power monitor)
4. Adafruit_PN532 By Adafruit -1.3.4
5. Modbusmaster by doc walker -2.0.1 (communication for rs485)

*I2C PIN*
1. SDA_PIN 11 
2. SCL_PIN 10

*PLC/RS485 CONFIG*
1. RXD2    44
2. TXD2    43
3. EN485  18

*VOLTAGE condition 1*

1. VOLT_MIN   11.90
2. VOLT_MAX  12.90
