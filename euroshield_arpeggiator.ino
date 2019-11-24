/*
Euroshield MIDI Arpeggiator
Written By Nick Shaver - nick.shaver@gmail.com
Copyright (C) 2019 Nick Shaver

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

Instructions:
1. Possess a 1010 Music Eurosheild with a Teensy 3.2, 3.5, or 3.6 installed
2. Connect MIDI out from a keyboard to MIDI in on the Euroshield
3. Connect MIDI out from the Euroshield to MIDI in on a sound module
4. press the Eurosheild button to cycle through the arpeggiator state (off, non-latched, latched)
5. with arp in non-latched or latched mode, play a chord on the keyboard on the MIDI channel defined below in the variable midi_channel
6. use the top Euroshield potentiometer to modify the direction (up, up+down, up+down with repeat at top/bottom)
7. use the bottom Euroshield potentiometer to modify whether or not to release the previous arp note before or after the next note is played.
   this feature is important if you want to use portamento. for portamento to work the note needs to be released AFTER the next note is played.
   if your monosynth doesn't seem to be sustaining notes, try modifying this setting. the notes might be decaying because your decay/sustain
   are in effect because the new notes are being considered as part of the old notes (release is after the next note is played).
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
*/

// uncomment the next line to turn on serial debugging
//#define DEBUG

#ifdef DEBUG
	#define DEBUG_PRINT(x) Serial.println(x)
#else
	#define DEBUG_PRINT(x)
#endif

#include <Audio.h>
#include <MIDI.h>

////////////////////////////////////////
// button
#include <Bounce2.h>
Bounce debouncer = Bounce();
#define BUTTON_PIN 2
////////////////////////////////////////

////////////////////////////////////////
// potentiometer
#define UPPER_POT_PIN 20														// euroshield upper potentiometer pin
#define LOWER_POT_PIN 21														// euroshield lower potentiometer pin
int this_potvalue;																	// stores current pot value
int last_upper_pot=0;																// keeps track of upper pot value
int last_lower_pot=0;																// keeps track of lower pot value
unsigned long pot_read_ms=0;												// next time to read pots, so they're not read every loop
////////////////////////////////////////

////////////////////////////////////////
// midi
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
byte midi_channel=1;																// midi channel on which to listen and send on
////////////////////////////////////////

////////////////////////////////////////
// CV input for tempo
AudioInputI2S            i2s1;											// setup audio input
AudioAnalyzePeak         peak1;											// analyze amplitude to detect CV high/low
AudioConnection          patchCord11(i2s1, 0, peak1, 0);
AudioControlSGTL5000     sgtl5000_1;

int last_trigger_1=0;
float cv_trigger_rise_level=0.40;										// sets minimum threshold (probably somewhere around 0.4 or 0.5)
float cv_trigger_fall_level=0.30;										// sets level for assuming trigger has fallen (somewhere around 0.3)
unsigned long cv_trigger_delay_ms=100;							// sets minimum ms between triggers. I was getting false triggers so I created this variable/logic
////////////////////////////////////////

// GUItool: end automatically generated code
////////////////////////////////////////
// leds
int led_pins[] = {6, 5, 4, 3};											// pins of the four euroshield LEDs
////////////////////////////////////////

////////////////////////////////////////
// misc variables
////////////////////////////////////////
int arp_notes[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};	// keeps track of notes in arpeggio chord
int held_notes[127]={};															// keeps track of notes that are currently pressed
int arp_state=0;																		// arpeggiator state: 0=off, 1=nonlatched, 2=latched
int arp_direction=1;   															// arpeggiator direction: 0=up, 1=updown, 2=updown with repeat at end
int arp_incr_decr=1;																// for updown direction, keeps up with current direction (up or down)
bool arp_release_before_play=false;									// keeps up with whether to release notes before or after next note is played.
																										// this is very important if portamento is desired. for portamento to work, the
																										// notes will need to be released after the next note is played, rather than before.
int current_arpnote=0;															// stores current arp
unsigned long last_loop_ms=0;												// keeps up with exactly when the current loop started
unsigned long tempo_ms=125;													// current tempo, should be enhanced to function via MIDI clock, potentiomenter,
																										// CV trigger in, tap temop, etc...
																										// might also consider a variable to set whether to play on 16th notes, 8th notes, etc...
unsigned long next_beat_ms=0;												// keeps up with when the next beat will arrive
byte last_arpnote=0;																// keeps up with the last note that was played
int tempo_mode=1;																		// how to control tempo: 0=hardcoded via tempo_ms, 1=CV trigger input
////////////////////////////////////////

void setup() {
#ifdef DEBUG
  Serial.begin(57600);
#endif

	// setup audio in for CV trigger
  AudioMemory(12);
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_1.lineInLevel(0,0);

	// setup leds
  for (int i=0; i<(sizeof(led_pins)/sizeof(led_pins[0])); i++) pinMode(led_pins[i], OUTPUT);

	// setup midi
	DEBUG_PRINT(String("Euroshield Started"));
  MIDI.begin();
  MIDI.setHandleNoteOn(NoteOnHandler);
  MIDI.setHandleNoteOff(NoteOffHandler);
  MIDI.turnThruOff();

	// setup debounced button
	debouncer.attach(BUTTON_PIN, INPUT_PULLUP);
	debouncer.interval(25);
}

void loop() {
	last_loop_ms=millis();

	bool trigger_arp_now=false;
	if (tempo_mode==0){
		if (next_beat_ms<=last_loop_ms){
			trigger_arp_now=true;
		}
	} else if (tempo_mode==1){
		if (peak1.available()) {
			float peak_value_1 = peak1.read();
			//DEBUG_PRINT(String("input1 peak: ") + peak_value_1);
			if (peak_value_1>cv_trigger_rise_level && last_trigger_1==0 && next_beat_ms<=last_loop_ms){
				// just went high
				DEBUG_PRINT(String("trigger on, input1 freq: ") + peak_value_1);
				last_trigger_1=1;

				// blink LED
				digitalWrite(led_pins[2], HIGH);

				// step to next arp note
				trigger_arp_now=true;
			} else if (peak_value_1<cv_trigger_fall_level && last_trigger_1==1){
				// just went low
				DEBUG_PRINT(String("trigger off, input1 freq: ") + peak_value_1);
				last_trigger_1=0;

				// unblink LED
				digitalWrite(led_pins[2], LOW);
			}
		}
	}

	if (trigger_arp_now){
		// time for another arp note
		if (tempo_mode==0){
			next_beat_ms=last_loop_ms+tempo_ms;
		} else if (tempo_mode==1){
			// ignore false readings from CV input, expect at least x ms between clicks
			next_beat_ms=last_loop_ms+cv_trigger_delay_ms;
		}

		if (arp_state>0){
			// determine next arpnote
			if (arp_direction==0){
				// forward
				if (current_arpnote>=(sizeof(arp_notes)/sizeof(arp_notes[0]))) {
					// start over at first note
					current_arpnote=0;
				} else {
					if (arp_notes[current_arpnote+1]==0){
						// on last note, restart at first note
						current_arpnote=0;
					} else {
						current_arpnote++;
					}
				}
			} else if (arp_direction==1){
				// updown, but no repeat
				if (current_arpnote>=(sizeof(arp_notes)/sizeof(arp_notes[0]))) {
					// at top, turn around now
					arp_incr_decr=-1;
					current_arpnote+=arp_incr_decr;
				} else {
					if (arp_incr_decr>0){
						// on the way up
						if (arp_notes[current_arpnote+1]>0){
							// next note up is set, use it
							current_arpnote++;
						} else {
							// next note is not set, turn around
							arp_incr_decr=-1;
							if (current_arpnote>0){
								if (arp_notes[current_arpnote-1]>0){
									current_arpnote--;
								} else {
									// previous note not set, just start over at 0 and then go back up
									current_arpnote=0;
									arp_incr_decr=1;
								}
							} else {
								// must only be one note
								current_arpnote=0;
							}
						}
					} else {
						// on the way down
						if (current_arpnote<=0){
							// at start, go back up
							arp_incr_decr=1;
							if (arp_notes[current_arpnote+1]>0){
								current_arpnote=1;
							} else {
								// must only be one note
								current_arpnote=0;
							}
						} else {
							// somewhere in middle of notes
							if (arp_notes[current_arpnote-1]>0){
								current_arpnote--;
							} else {
								// must only be one note
								current_arpnote=0;
							}
						}
					}
				}
			} else if (arp_direction==2){
				// updown, with repeat
				if (current_arpnote>=(sizeof(arp_notes)/sizeof(arp_notes[0]))) {
					// at top
					if (arp_incr_decr==1){
						// just got to top, don't turn around yet, repeat top note
						arp_incr_decr=-1;
					} else {
						// turn around now
						arp_incr_decr=-1;
						current_arpnote+=arp_incr_decr;
					}
				} else {
					if (arp_incr_decr>0){
						// on the way up
						if (arp_notes[current_arpnote+1]>0){
							// next note up is set, use it
							current_arpnote++;
						} else {
							// next note is not set
							if (arp_incr_decr==1){
								// need to repeat this last note
								arp_incr_decr=-1;
							} else {
								// turn around
								arp_incr_decr=-1;
								if (current_arpnote>0){
									if (arp_notes[current_arpnote-1]>0){
										current_arpnote--;
									} else {
										// previous note not set, just start over at 0 and then go back up
										current_arpnote=0;
										arp_incr_decr=1;
									}
								} else {
									// must only be one note
									current_arpnote=0;
								}
							}
						}
					} else {
						// on the way down
						if (current_arpnote<=0){
							// at start, go back up
							if (arp_incr_decr==-1){
								// just got to bottom, just turn around but repeat this note
								arp_incr_decr=1;
							} else {
								if (arp_notes[current_arpnote+1]>0){
									current_arpnote=1;
								} else {
									// must only be one note
									current_arpnote=0;
								}
							}
						} else {
							// somewhere in middle of notes
							if (arp_notes[current_arpnote-1]>0){
								current_arpnote--;
							} else {
								// must only be one note
								current_arpnote=0;
							}
						}
					}
				}
			}

			// release last note before playing new note
			byte arp_release_note=last_arpnote;
			if (arp_release_note>0 && arp_release_before_play){
				MIDI.sendNoteOff(arp_release_note, 0, midi_channel);
			}

			// play note
			if (arp_notes[current_arpnote]>0){
				MIDI.sendNoteOn(arp_notes[current_arpnote], 127, midi_channel);
				last_arpnote=arp_notes[current_arpnote];
			} else {
				// no note to play
				last_arpnote=0;
				current_arpnote=0;
			}

			// release last note after playing new note
			if (arp_release_note>0 && arp_release_before_play==false){
				MIDI.sendNoteOff(arp_release_note, 0, midi_channel);
			}
		}
	}

	if (MIDI.read()){
		// misc midi received
	}
	detect_button();

	if (last_loop_ms>=pot_read_ms){
		get_pot(0);
		get_pot(1);

		// only read pots every 250ms
		pot_read_ms=last_loop_ms+250;
	}
}

void NoteOnHandler(byte channel, byte pitch, byte velocity) {
	if (channel!=midi_channel) {
		// not the midi channel we want to use for arpeggiator.
		// just forward to midi out.
		MIDI.sendNoteOn(pitch, velocity, channel);
		return;
	}

	held_notes[pitch]=1;
	if (arp_state==0){
		MIDI.sendNoteOn(pitch, velocity, midi_channel);
	} else if (arp_state==1) {
		// find first available arp note
		for (int i=0; i<(sizeof(arp_notes)/sizeof(arp_notes[0])); i++){
			if (arp_notes[i]==0){
				// this spot is available
				arp_notes[i]=pitch;
				resort_arp_notes();
				break;
			} else if (arp_notes[i]==pitch){
				// this note is already in arp_notes, ignore
			}
		}
	} else if (arp_state==2) {
		// see if this is the only note pressed
		bool start_arp_over=true;
		for (int i=0; i<(sizeof(held_notes)/sizeof(held_notes[0])); i++){
			if (i<12 || i>=108) continue;
			if (held_notes[i]>0 && pitch!=i){
				// this note is still held, do not start over
				DEBUG_PRINT(String("Still held: ") + i);
				start_arp_over=false;
				break;
			}
		}

		if (start_arp_over==true){
			// just played the first note of a new chord, empty out arp_notes
			DEBUG_PRINT(String("empty arp_notes"));
			for (int i=0; i<(sizeof(arp_notes)/sizeof(arp_notes[0])); i++){
				arp_notes[i]=0;
			}
		}
		
		// find first available arp note
		for (int i=0; i<(sizeof(arp_notes)/sizeof(arp_notes[0])); i++){
			if (arp_notes[i]==0){
				// this spot is available
				arp_notes[i]=pitch;
				resort_arp_notes();
				break;
			} else if (arp_notes[i]==pitch){
				// this note is already in arp_notes, ignore
			}
		}
	}

  digitalWrite(led_pins[3], HIGH);
	DEBUG_PRINT(String("Note On, C=") + channel + String(" N=") + pitch + String(" V=") + velocity);
}

void NoteOffHandler(byte channel, byte pitch, byte velocity) {
	if (channel!=midi_channel) {
		// not the midi channel we want to use for arpeggiator.
		// just forward to midi out.
		MIDI.sendNoteOff(pitch, velocity, channel);
		return;
	}

	held_notes[pitch]=0;
	if (arp_state==0){
		MIDI.sendNoteOff(pitch, velocity, midi_channel);
	} else if (arp_state==1) {
		// see if this note is in arp_notes. if so, remove it
		for (int i=0; i<(sizeof(arp_notes)/sizeof(arp_notes[0])); i++){
			if (arp_notes[i]==pitch){
				// this note is being played, release it
				arp_notes[i]=0;
			}
		}
		resort_arp_notes();
	} else if (arp_state==2) {
		// in latched mode, do not remove arp_notes when released
	}

  digitalWrite(led_pins[3], LOW);
}

void reorder_arp_notes(){
	// omit any empty spots, reorder
	// if you want to play notes in the order that they were received, use this instaed of resort_arp_notes
	int tmparp_notes[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int next_tmp_spot=0;
	for (int i=0; i<(sizeof(tmparp_notes)/sizeof(tmparp_notes[0])); i++){
		if (arp_notes[i]>0){
			tmparp_notes[next_tmp_spot]=arp_notes[i];
			next_tmp_spot++;
		}
	}

	for (int i=0; i<(sizeof(tmparp_notes)/sizeof(tmparp_notes[0])); i++){
		arp_notes[i]=tmparp_notes[i];
	}
}

void resort_arp_notes(){
	sort(arp_notes, sizeof(arp_notes)/sizeof(arp_notes[0]));

	// all zeros will be at start, need to move them to end
	reorder_arp_notes();
}

void detect_button(){
	// detect button pushes
	debouncer.update();
	if (debouncer.fell()){
		int last_arp_state=arp_state;
		arp_state++;
		if (arp_state>2) arp_state=0;
		if (arp_state==1){
			// arp on, non latch
  		digitalWrite(led_pins[0], HIGH);
  		digitalWrite(led_pins[1], LOW);
			all_notes_off(1);
		} else if (arp_state==2){
			// arp on, latch
  		digitalWrite(led_pins[0], HIGH);
  		digitalWrite(led_pins[1], HIGH);
			if (last_arp_state==1){
				// do not stop current chord if already playing from non-latched mode
			} else {
				all_notes_off(1);
			}
		} else {
			// arp off
  		digitalWrite(led_pins[0], LOW);
  		digitalWrite(led_pins[1], LOW);
			all_notes_off(1);
		}
	}
}

void all_notes_off(byte channel){
	// clear out arp_notes
	for (int i=0; i<(sizeof(arp_notes)/sizeof(arp_notes[0])); i++){
		arp_notes[i]=0;
	}

	// send note off for all notes
	for (int i=0; i<(sizeof(held_notes)/sizeof(held_notes[0])); i++){
		if (i>=12 && i<108) {
			MIDI.sendNoteOff(i, 0, midi_channel);
			held_notes[i]=0;
		}
	}
}

void sort(int a[], int size) {
	for(int i=0; i<(size-1); i++) {
		for(int o=0; o<(size-(i+1)); o++) {
			if(a[o] > a[o+1]) {
				int t = a[o];
				a[o] = a[o+1];
				a[o+1] = t;
			}
		}
	}
}

void get_pot(int whichpot){
	switch (whichpot){
		case 0:
			// make adjustments from upper pot
			this_potvalue = analogRead(UPPER_POT_PIN);
			if (this_potvalue != last_upper_pot){
				set_arp_direction(this_potvalue);
				last_upper_pot=this_potvalue;
			}
			break;
		case 1:
			// make adjustments from lower pot
			this_potvalue = analogRead(LOWER_POT_PIN);
			if (this_potvalue != last_lower_pot){
				set_arp_release_before_play(this_potvalue);
				last_lower_pot=this_potvalue;
			}
			break;
	}
}

void set_arp_direction(int potvalue){
	if (potvalue<=400 && arp_direction!=0){
		arp_direction=0;
		DEBUG_PRINT(String("arp_direction=up"));
	} else if (potvalue>400 && potvalue<800 && arp_direction!=1){
		arp_direction=1;
		DEBUG_PRINT(String("arp_direction=up/down"));
	} else if (potvalue>=800 && arp_direction!=2){
		arp_direction=2;
		DEBUG_PRINT(String("arp_direction=up/down with repeat"));
	}
}

void set_arp_release_before_play(int potvalue){
	if (potvalue<=600){
		arp_release_before_play=false;
	} else {
		arp_release_before_play=true;
	}
}
