# Qwartr

**An experimental Arduino OS.** Aims to be faster than [Chika](https://github.com/phunanon/Chika).

Apps I want to write, and implement in a handheld device called qPC:
 - terminal
 - text editor
 - Telegram/Discord via bot
 - Wikipedia
 - alarms
 - file sync
 
 Components:
 - [BB Q10 Keyboard PMOD](https://www.tindie.com/products/arturo182/bb-q10-keyboard-pmod/)
 - [WeMos D1 Mini ESP8266 Arduino](https://makersportal.com/blog/2019/6/12/wemos-d1-mini-esp8266-arduino-wifi-board)
 - An SSD1306 128x64 OLED display
   - Or a more advanced display (LCD) through a slave chip
   - Driver written in Qwarter for either SSD1306 I²C or slave chip
