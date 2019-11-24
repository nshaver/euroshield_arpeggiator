# euroshield_arpeggiator
MIDI Arpeggiator using 1010 Music Euroshield and Teensy

1. Possess a 1010 Music Eurosheild with a Teensy 3.2, 3.5, or 3.6 installed

2. Connect MIDI out from a keyboard to MIDI in on the Euroshield

3. Connect MIDI out from the Euroshield to MIDI in on a sound module

4. press the Eurosheild button to cycle through the arpeggiator state (off, non-latched, latched)

5. with arp in non-latched or latched mode, play a chord on the keyboard on the MIDI channel defined below in the variable midi_channel

6. use the top Euroshield potentiometer to modify the direction (up, up+down, up+down with repeat at top/bottom)

7. use the bottom Euroshield potentiometer to modify whether or not to release the previous arp note before or after the next note is played.

this feature is important if you want to use portamento. for portamento to work the note needs to be released AFTER the next note is played.

if your monosynth doesn't seem to be sustaining notes, try modifying this setting. the notes might be decaying because your decay/sustain are in effect because the new notes are being considered as part of the old notes (release is after the next note is played).
