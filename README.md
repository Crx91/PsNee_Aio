# PsNee_Aio
The ultimate mod based on ![PsNee](https://github.com/kalymos/psnee), my ![OneNee](https://github.com/Crx91/OneNee_Ch32v003) and  my latest ![Jap_Bios_Unlocker](https://github.com/Crx91/Ps1_Jap_Bios_Unlocker).                   
Unleash the power of your Ps1!

Coded for the "10 cent" MCU CH32V003 using the powerful ch32fun development environment,                                                                                         
PSNee_Aio is the ultimate mod for the Playstation 1 console!

![PCB3d](https://github.com/Crx91/PsNee_Aio/blob/main/PCB/3D_ch32v003f4p6.jpg)

Features:
- Fully compatible and stealth with all PS1 models and BIOS!
- Unlock JAP bios, allowing boot of PAL and USA games;
- Unlock PAL PSone (SCPH-102) bios, allowing boot of USA and JAP games;
- Allows boot backup games and backup VCD on SCPH-5903 consoles;
- It uses an alternate and universal method of JAP bios patching “on the fly”. No more bios crash, no more timing problems and no more need to recompile the code for each Jap bios version;
- Jumper settable, so you can move the chip from various models or change the settings\features without recompiling the code again!;
- Precompiled BIN image ready to flash on ch32v00f4p6, no need to mess with Arduino IDE and compilers!;
- Ws2812b RGB output led to know the status of the chip!;
- Fully open-source. (If you see this mod sold online, I am not getting a dime!);
- Easily upgradeable onboard using a single wire (SDI) through the SWIO pin;
- Original PsNee code by ramapcsx2 with kalymos updates and optimizations;
- VCD detection code made by RepairBox;
- Jap_Bios_Unlocker code made by Crx91 (me).

## Supported MCU:
-	Ch32v003f4p6
-	Ch32v003f4u6 (PCB and schematics not available at the moment)

## Prerequisites:
- [WCH-LinkE](https://github.com/carmax91/PsNee-CH32V003/blob/main/Imgs/WCH-LinkEPrg.jpg) programmer (pay attention to the E).
-	WCH LinkUtility software.

## HowTo:
-	Download this repository.
-	Follow [PsNee_Aio Wiki](https://github.com/Crx91/PsNee_Aio/wiki) or [PsNee_Aio.pdf](https://github.com/Crx91/PsNee_Aio/blob/main/PsNee_Aio.pdf) instructions.

In the [PCB](https://github.com/Crx91/PsNee_Aio/tree/main/PCB) folder, you can find the Gerber file and schematic for the Ch32v003f4p6 variant.

## Thanks to:
ramapcsx2, kalymos, SpenceKonde, oldcrow, mayumi, arduino community, ch32fun community, Infrid and lots of people that can't remember now.
