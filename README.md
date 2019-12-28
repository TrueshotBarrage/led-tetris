# LED Tetris
Programmed with C/C++ for Arduino Mega 2560.

## What you need
* A microcontroller (I personally used a Mega 2560.)
* An analog joystick
* 8x8 LED grid (MAX7219)
* A few M-M & M-F wires & breadboard

Schematics coming soon...

## Functionality
~~Planned development until school starts again (around late January 2020).
Will finish before then, hopefully.~~

Finished way faster than expected. (Took around 50 hours total, in a single consecutive 7-day period.)

Full Tetris game! Other than a minor bug (listed below), everything works pretty nicely. 
Pins are configurable. Feel free to modify the code to your liking to let it run on your hardware. 
I dunno why I made the input device a joystick instead of something more conventional. 
But at least it's kinda cool and old-school vibey(?)

## Changelog
* December 27th, 2019
  * Finished
* December 26th, 2019
  * Bare-bones structure in place.
  * FSM finalized with the transitions, as well as some hardcoded conditions/inputs
  * Tons of bugs but at least the joystick works somewhat...
  * Can shift block left or right, but the blocks go off-screen and never come back :(
* December 21st, 2019
  * Added pins for joystick and LED matrix.
  * Thought of a rough FSM sketch.
