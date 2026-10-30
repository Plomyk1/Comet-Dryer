# Assembly instructions
## 1.Picking some of the components:
- If you cannot purchase the exact container I have, get one that can handle high temperature (≥80°C) containers made out of PP (Polypropylene) or PC (Polycarbonate) should be suitable. Verify that your spool(s) will fit inside with ~ 8cm of space in front of the spools for the heater and fan assembly
- You should probably avoid cheap 12V brick laptop power supplies from aliexpress, the one I purchased turned out to incapable of delivering even 4A at 12V due to overheating. If you end up choosing a different PSU from me, I recomend you get one rated for at least 8A, since that is the heaters initial current draw

**The images in this guide might be slightly inacurate, as they were from the 1.1 version of the PCB! Most connections should stay the same though**
## 2. Adjust the voltage after stepping it down
If your step down converter (the LM2596S) is adjustable, connect it to your 12v PSU, and adjust the output voltage to 5v by turning the knob on the potentiometer. The potentiometer is located here:

<img width="388" height="208" alt="Potentiometer location" src="https://github.com/user-attachments/assets/4e22ff0b-4c7c-41be-a2f6-01ce6dc3ca20" />

## 3. Soldering the components to the PCB:
Before soldering, connect the RP2040-Zero devboard to your computer. Hold the boot button and press the reset one. The devboard should now be detected as an external drive. [Download the .uf2 file from the firmware folder](/firmware/firmware.uf2), and drop it to the main directory of the drive. The firmware is now uploaded to the microcontroller

### 3.1 Soldering to the front of the PCB:
- Remove the pins soldered onto the Rotary Encoder
- Solder the display and rotary encoder board to the front of the PCB

After soldering the front should look like this:
<img width="4000" height="1800" alt="Front after soldering" src="https://github.com/user-attachments/assets/c5612ea4-65d7-429d-af7e-263bb460e4d9" />

### 3.2 Soldering the back of the PCB:

- Solder the RP-2040 Zero devboard to the PCB. The buttons should be exposed if the devboard is facing the correct way
- Solder all of the diodes and resistors. The ones next to the motor are not necessary, as the project currently does not use one
- Solder the LM2596S module to the board
- Solder the MOSFETs, the one next to the motor connections is optional
- Solder wires the temperature and humidity sensor (AHT20) footprint. The wires should be about 8cm long. **Remember which wire is connected to what, or write it down**
- Solder the PSU and HEATER wires. They need to be **thicker than** 0.5mm² / 20 AWG. The heater wires should be around 5cm long. The positive wire of the psu should be ~2.5cm long. The negative should be ~5cm long

After soldering, the back of the PCB should look something like this (the wires are not connected on the picture, and the location of the resistors is slightly different, as this is a previous revision of the PCB)
<img width="4000" height="1800" alt="Back after soldering" src="https://github.com/user-attachments/assets/35649c20-17e4-4223-8552-9097750f3433" />

## 4. 3D printing

The enclosure can be printed out of any filament, preferably PETG or higher vicat / HDT temperature. The parts going inside the dryer (the mounting bracket and optionally the stand if you print it) should be printed out of ASA/ABS or filament with a higher vicat / HDT temperature 

**Remember to split the STL files into multiple objects in your slicer!** Print the enclosure laying on the side with openings for the display and rotary encoder

## 5. Mounting the PCB, Power switch and barrel jack to the enclosure
- Press heatset inserts into the round holes in the enclosure with your soldering iron. The temperature should be 50°C higher than the temperature at which you printed the enclosure
- Put the PCB into place. The holes should line up with the heatset inserts
- Screw down the PCB, **with the screws going in from the back of the PCB**. Use the M3x10 screws
- Push the power switch and barrel jack socket into place. Optionally, secure the socket with a nut if one came with it
- Solder the positive PSU wire to one of the metal tabs on the power switch
- Solder a wire from the positive (shorter) leg of the barrel jack socket to the other metal tab of the switch
- Solder the negative wire to the longer leg of the socket
- Attach the fan to the mounting bracket with M4 screws. The back side of the fan should be facing the inside of the box. Mount the fan to the opposite side from the rods going out

After everything, the inside of the enclosure should look simmiliar to this:
<img width="4000" height="2256" alt="Inside the enclosure" src="https://github.com/user-attachments/assets/ff984654-e2d0-4653-8313-1957d81171a4" />


## 6. Make holes in the box
- Mark where the heatset inserts on the back of the enclosure are, when you place the enclosure in the position that you want to mount it in
- Use the mounting bracket to determine where you need to mark holes for the M5 screws
- Mark where you are going to make holes for the heater, fan and temperature sensor wires
- Make the holes in your box. I used a dremel with a small tip to make them.

## 7. Screw down the enclosure and bracket to the box
I am not really sure how to describe this, so here are pictures of how everything should be mounted:

Do not screw the enclosure to the box just yet
<img width="4000" height="2256" alt="Enclosure mounting screws" src="https://github.com/user-attachments/assets/36efd243-d63b-4718-ad13-dee8350b13e7" />
<img width="4000" height="2256" alt="Outside m5 screws" src="https://github.com/user-attachments/assets/a945eede-0be8-4562-b489-abb469310a3f" />
<img width="4000" height="2256" alt="heater mounting" src="https://github.com/user-attachments/assets/8457dc6a-758a-464a-940e-d24681f9ecc5" />
<img width="564" height="1000" alt="Heater topdown view" src="https://github.com/user-attachments/assets/e1227e12-6351-4ec8-8d06-98d641b723dc" />

## 8. Solder the wires to the components inside the box
- Push the heater wires through the holes you made. Solder them to the wires going from the PCB. Polarity does not matter
- Push the fan wires through the hole and solder them to the wires going from the PCB.
- Pull the wires from enclosure to the box and solder them to the temperature sensor.

**AND ITS DONE**

*I proabably detailed some of the steps a bit too much, while others may be a bit confusing, so if you have any questions or feedback you can message me on slack (@Plomyk) or on discord (plomyk.)*
