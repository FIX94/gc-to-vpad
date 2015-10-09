# GC to VPAD
A WiiU patcher to replace the gamepad input with the gamecube adapter.

# Requirements
A WiiU capable of running the current browser and kernel exploit.  

# Usage
Copy the the content of the "htmls" folder into a new folder on your webserver, run OSDriver for kernel access, start GC to VPAD, start a game and enjoy!  
It just works like you would expect for the most part except for Z, holding down Z will switch L to gamepad L, R to gamepad R and start to gamepad minus. This is done for convenience purposes.

# Limitations
This only patches VPADRead so not every game is supported.  
As of right now the installer only supports 3.1.0 and 5.3.2. If you happen to have another firmware and want it ported you might open up a new issue on this page.  
The way I internally share memory can break newer WiiU games (Splatoon).  
The gamepad needs to be on at all times, also screen savers and console shutdowns will happen because the gamecube inputs are not detected officially by the system.  
To get around the screen saver and console shutdown make sure to disable burn-in reduction and auto shutdown in your wiiu settings.

# Manual Compiling
Go into the gc_reader and vpad_patch folders respectively with a cmd and run make.  
Copy the contents of both "bin" folders into gc-to-vpad-installer/src.  
Copy gc-to-vpad-installer into a libwiiu example folder and use the libwiiu build.py to create the html files.  
Copy the www/gc-to-vpad-installer folder to your webserver, run OSDriver for kernel access, start GC to VPAD, start a game and enjoy!  
