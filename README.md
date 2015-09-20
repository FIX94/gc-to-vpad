# gc-to-vpad
A WiiU patcher to replace the gamepad input with the gamecube adapter

# Installation
Go into the gc_reader and vpad_patch folders respectively with a cmd and run make.
Copy the contents of both "bin" folders into gc-to-vpad-installer.
Copy gc-to-vpad-installer into a libwiiu example folder and use the libwiiu build.py.
Copy the folder to your webserver, start it, start a game and enjoy!

# Limitations
This only patches VPADRead so not every game is supported.
As of right now the installer only supports 3.1.0 and 5.3.2.
The way I internally share memory can break newer WiiU games (Splatoon).
