# Comet Dryer
[![View PCB on KiCanvas](https://hack.club/pcb-badge)](https://kicanvas.org/?repo=https://github.com/Plomyk1/Comet-Dryer/tree/master/PCB)
![Hackatime Badge](https://hackatime-badge.hackclub.com/63466/Filament%20dryer?color=18d8b1&label=Time%20spent)
<img width="4000" height="2256" alt="Comet Dryer hero image" src="https://github.com/user-attachments/assets/23dd8f50-0512-4d2f-a694-731ce3c8126b" />

Comet Dryer is a 3D printer filament dryer, with support for drying 2 spools at once!

I made this project, because I felt like trying to make something more complicated than all my previous projects. Decided on a filament dryer, since it was something I wanted to have, and the dryers i could buy seemed quite expensive for essentially just heating air inside a box

## Features:
- 50w Heater
- Fan for even distrubution of hot air
- A colorful 320x170 display
- Rotary encoder for navigating the menus
- UI fully written in C++ with the Adafruit GFX Library
- 13 Color themes
- 15 presets for drying different materials and dessicant

## CAD Model: 
<img width="1632" height="1060" alt="Full cad assembly" src="https://github.com/user-attachments/assets/06346f8b-10e3-42ed-8547-7a5c327fedb6" />
<img width="395" height="313" alt="Enclosure" src="https://github.com/user-attachments/assets/eeb7b067-1f8e-40eb-8d51-9e18d7856c82" />
<img width="421" height="317" alt="Heater and fan bracket" src="https://github.com/user-attachments/assets/06b5785c-835b-40e6-aa25-2c6cc7f3eaff" />

The PCB and enclosure is put together with 5 M3x10 screws and heatset insters. Both the barrel jack and power switch is press fit into the enclosure. The heater and fan addembly is mounted with 2 M5x80 screws. The fan is screwed the the 3D printed bracket with 4 M4x20 screws

You can find all the STL and STEP files in the [CAD directory](/CAD)

## PCB:
<img width="438" height="295" alt="Back side of the PCB" src="https://github.com/user-attachments/assets/f8e54675-fa5f-4ee3-8c4a-4a358eae7e4c" />
<img width="432" height="298" alt="Front side of the PCB" src="https://github.com/user-attachments/assets/bde594e8-baa9-4403-aea3-c5139a86edee" />

The PCB has been designed to be manufactured by PCBWay. It may also fit the requirements of other manufacturers, but that is currently untested!
You can view the PCB wiring on KiCanvas with the button at the top of the readme

<img width="500" height="202" alt="PCB power wiring diagram" src="https://github.com/user-attachments/assets/d81146d8-a7e4-4c42-8633-f00a64d7998a" />

Diagram showing how the PCB is connected to power to the power supply




### If you want to make this dryer yourself, have a look at the assembly instructions in the [assembly.md](assembly.md) file!

## Credits:
Libraries I used: 
- [Adafruit ST7789](https://github.com/adafruit/Adafruit-ST7735-Library) by Adafruit Industries
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) by Adafruit Industries
- [Adafruit AHTX0](https://github.com/adafruit/Adafruit_AHTX0) by Adafruit Industries
- [RotaryEncoder](https://github.com/mathertel/RotaryEncoder) by Matthias Hertel

AI use:

I have only used AI for debugging, and minor got help with electrical issues. All code in firmware.cpp was written by me
