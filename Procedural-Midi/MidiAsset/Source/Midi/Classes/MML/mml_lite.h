//©2018 ArcheAge MML Library - reverse engineered Python code

#ifndef MML_SIMPLE_H
#define MML_SIMPLE_H

#include "./MidiTrack.h"
#include "./Event/Meta/Tempo.h"
#include "./Event/ProgramChange.h"
#include "./Event/Controller.h"
#include "./Event/NoteOn.h"
#include "./Event/NoteOff.h"
#include "./Util/MidiUtil.h"

#include "ntlibc.h"

#include <string.h>
#include <vector>

struct MML_LITE {
	unsigned int delay = 0; // play one note every quarter second
	unsigned int note = 0; // the MIDI note
	unsigned int numNotes = 12;
	int defaultLength = 4;
	unsigned int index = 0;
	string value = "";
	unsigned int track = 0;
	float startPos = 1;
	float curPos = startPos;
	float speed = 1.0f / 4.0f;
	unsigned int octave = 5;
	int tempo = 100;
	unsigned int volume = 127;
	string * source = NULL;
	bool skipNext = false;

	MidiTrack* _track;
	vector<MidiTrack*> _tracks;

	static int get_note_value(char c)
	{
		switch (ntlibc_toupper(c)) {
		case 'C':
			return 0;
		case 'D':
			return 2;
		case 'E':
			return 4;
		case 'F':
			return 5;
		case 'G':
			return 7;
		case 'A':
			return 9;
		case 'B':
			return 11;
		}

		return -1;
	}

	static string get_char_type(char c)
	{
		switch (ntlibc_toupper(c)) {
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'A':
		case 'B':
			return "n";
		case 'R':
			return "rest";
		case '#':
		case '+':
		case '-':
		case '&':
			return "mod";
		case ',':
			return "trk";
		case 'L':
			return "len";
		case 'V':
			return "vol";
		case 'T':
			return "tempo";
		case 'O':
		case '>':
		case '<':
			return "oct";
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return "val";
		case '.':
			return "stop";
		}
		return "";
	}

	void playNote(int note, float speed, int* mod) {

		_track->insertEvent(new NoteOn(curPos * 1000, track, note, volume));

		if (!mod[and])
			_track->insertEvent(new NoteOff((curPos + speed) * 1000, track, note, 0));
	};

	void moveTime(float speed) {
		float timeIndex = speed * (240.0f / tempo);
		curPos += timeIndex;
	};
	enum mods {
		_value, halfStep, stop, and
	};
	int* parseModifiers() {
		string value = "";

		static int modifiers[4] = { 0 };
		memset(modifiers, 0, sizeof modifiers);
		int i;
		for (i = index; i < source->length(); i++) {
			char cur = source->at(i);

			if (get_char_type(cur) != ""
				&& get_char_type(cur) != "stop"
				&& get_char_type(cur) != "mod"
				&& get_char_type(cur) != "val") {
				index = i;
				return modifiers;
			}

			// Read in complete values if current is the start of one
			if (get_char_type(cur) == "val") {
				string v = "";
				while (i < source->length() && get_char_type(source->at(i)) == "val") {
					v += source->at(i++);
				}
				i--;
				modifiers[mods::_value] = ntlibc_atoi(v.c_str());
			}
			else if (get_char_type(cur) == "mod") {
				// ÃŸ, #
				if (cur == '+' || cur == '#')
					modifiers[halfStep] = 1;
				else if (cur == '&')
					modifiers[and] = true;
				else
					modifiers[halfStep] = -1;
			}
			else if (get_char_type(cur) == "stop") {
				modifiers[stop] = 1;
			}
		}
		index = i;
		return modifiers;
	};

	void parseNote() {
		while (index < source->length()) {
			int i = index;
			char cur = source->at(i);

			if (get_char_type(cur) == "n") {
				// note
				index++;
				auto mod = parseModifiers();
				auto speed = 1.0f / defaultLength;
				if (mod[mods::_value])
					speed = 1.0f / mod[mods::_value];
				if (mod[stop])
					speed *= 1.5f;

				this->speed = speed;
				this->note = this->octave * this->numNotes + get_note_value(cur) + mod[halfStep];
				if (this->skipNext)
					this->skipNext = false;
				else
					this->playNote(note, speed, mod);

				this->moveTime(speed);

				if (mod[and])
					this->skipNext = true;
			}
			else if (get_char_type(cur) == "trk") {
				index++;
				track++;
				curPos = startPos;

				_track = new MidiTrack();
				_tracks.push_back(_track);
			}
			else if (get_char_type(cur) == "oct") {
				index++;
				// octave change command
				if (cur == '>') {
					octave++;
				}
				else if (cur == '<') {
					octave--;
				}
				else if (cur == 'o') {
					auto mod = parseModifiers();
					octave = mod[mods::_value];
				}
			}
			else if (get_char_type(cur) == "len") {
				// length
				index++;
				auto mod = parseModifiers();
				defaultLength = mod[mods::_value];
			}
			else if (get_char_type(cur) == "vol") {
				index++;
				auto mod = parseModifiers();
				_track->insertEvent(new Controller(curPos * 1000, track, 7, volume));
			}
			else if (get_char_type(cur) == "rest") {
				// rest
				index++;
				auto mod = parseModifiers();
				auto speed = 1.0f / defaultLength;
				if (mod[mods::_value]) {
					speed = 1.0f / mod[mods::_value];
				}
				moveTime(speed);
			}
			else if (get_char_type(cur) == "tempo") {
				// tempo
				index++;
				auto mod = parseModifiers();
				tempo = mod[mods::_value];
			}
			else {
				index++;
			}
		}
	};

	void parse(string data) {
		source = &data;
		curPos = startPos;
		index = 0;
		track = 0;

		_track = new MidiTrack();
		_tracks.push_back(_track);

		reset();

		parseNote();
	};

	void reset() {
		speed = 1.0f / 4.0f;
		octave = 5;
		tempo = 100;
		defaultLength = 4;
	};
};

#endif