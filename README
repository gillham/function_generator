Primitive function generator for Arduino with LCD keypad shield
===============================================================

LCD Keypad shield version.  Supports "DF Robot" v1.0 boards.
These shields are available for around $10 shipped on eBay and
other places.  The newer versions use different resistance values
for the keys, check their sample code.
I will try to add a #define for the new boards so you dont' have
to edit the values.  I don't have a newer board for testing yet.

The select button will start / stop the generator at any time.
Use the up/down buttons to move between function/rate/start.
Use the left/right buttons to change the values.  On the start menu
press the right button to start.

Depending on the function you select, the valid rates will change.
The square wave uses Arduino clock (divided in half) directly so it 
supports up to 8MHz on a 16MHz Arduino.
The patterns are done via an ISR and max out at 100KHz.  The bit rate
is 100KHz and it sends 8 bits so the cycle rate between two bits toggling
is only 50KHz.  I might update the rates later to be more accurate, but
for now this is how it is setup.

The square wave is quite precise as it is just hardware driven directly off
your clock.  If you have an accurate crystal, you have accurate output.
The pattern generation is accurate down to 500Hz.  Below 500Hz the clock
division error skews it slightly.  You can see it using a logic analyzer.

Square wave -- pin is toggled on/off the same amount of time.
Narrow pulse -- pin is on 1 bit, off 7.
Wide pulse -- pin is on 7 bits, off 1.
Directional -- Not sure what to call this, but basically it is a pattern
that will tell you if your scope or logic analyzer is presenting the data
backwards or something.  Mostly useful for my logic_analyzer development.

