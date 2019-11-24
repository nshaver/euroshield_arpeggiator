# euroshield_arpeggiator
MIDI Arpeggiator using 1010 Music Euroshield and Teensy

Instructions:
1. Possess a 1010 Music Eurosheild with a Teensy 3.2, 3.5, or 3.6 installed

2. Connect MIDI out from a keyboard to MIDI in on the Euroshield

3. Connect MIDI out from the Euroshield to MIDI in on a sound module

4. press the Eurosheild button to cycle through the arpeggiator state (off, non-latched, latched)

5. with arp in non-latched or latched mode, play a chord on the keyboard on the MIDI channel defined below in the variable midi_channel

6. use the top Euroshield potentiometer to modify the direction:
     - 0=up
     - 1=up+down, no repeat at top/bottom, for Stranger Things theme: C2 E2 G2 B3 C3
     - 2=up+down with repeat at top/bottom
     - 3=follow note played order, for Pink Floyd On The Run: E2 G2 A2 G2 D3 C3 D3 E3

7. use the bottom Euroshield potentiometer to modify whether or not to release the previous arp note before or after the next note is played.

this feature is important if you want to use portamento. for portamento to work the note needs to be released AFTER the next note is played.

if your monosynth doesn't seem to be sustaining notes, try modifying this setting. the notes might be decaying because your decay/sustain are in effect because the new notes are being considered as part of the old notes (release is after the next note is played).

8. use hardcoded variable tempo_mode to set whether tempo is from hard-coded tempo_ms variable, or from CV trigger into audio input 1.
  
   to use hardcoded tempo, find and set the following variables:
     - tempo_mode=0
     - tempo_ms=125 where 125=ms between arp beats
  
   to use CV input, find and set the following variables:
     - tempo_mode=1 - sets CV input mode      
     - cv_trigger_rise_level - sets minimum threshold (probably somewhere around 0.4 or 0.5)
     - cv_trigger_fall_level - sets level for assuming trigger has fallen (somewhere around 0.3)
     - cv_trigger_delay_ms - sets minimum ms between triggers. I was getting false triggers so I created this variable/logic
  
   if using CV trigger input, adjust those last three variables to accommodate your CV input level. It's a little tricky because we're
   using an audio library to analyze the input level, rather than simply reading an input voltage. I don't know of another way to read the
   voltage via a Euroshield DAC audio input. to find the correct value, turn on serial debugging (uncomment the define DEBUG line below) and
   watch the serial log to see what level is triggering the input, and how many ms in between triggers.

   if the triggering isn't working well, just keep at it. it can work perfectly once you get the rise and fall and delay variables set right
   for whatever you're using for your trigger. My values are from a eurorack clock module I built that uses a Teensy 3.2 and it's simply doing
   a digitalWrite HIGH and LOW for the trigger on/off.

   if you want to get fancy, you could control the tempo from the lower pot rather than controlling the release-before/after variable. That
   ought to be very easy to code up. I have no intention of doing that, but I would if someone really wanted it and bought me a beer somehow.
   for a few beers I'd code up a way to use the button to select the variable, a pot to adjust that variable, and an LED to flash x times
   indicating which variable you're currently adjusting. That would allow for setting lots of variables via the very limited controls on
   the Euroshield.
