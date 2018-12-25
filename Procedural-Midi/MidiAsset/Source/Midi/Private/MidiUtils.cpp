// Copyright -> Scott Bishel

#include "MidiUtils.h"
#include "MidiPrivatePCH.h"

#include <math.h>

static const float MIN_FREQUENCY = (440.0f / 32.0f); // MIDI Note Number 9

float UMidiUtils::DefaultNoteToFrequency(TEnumAsByte<ENoteEnum> note) {
	return NoteToFrequency((uint8)note + 60);
}

float UMidiUtils::NoteToFrequency(uint8 note) {
	//	Hertz = 440.0 * pow(2.0, (midi note - 69) / 12);
	// Diatonic Scale
	float Frequency = MIN_FREQUENCY * powf(2.0f, (note - 9) / 12);
	return Frequency;
}

uint8 UMidiUtils::FrequencyToNote(float Frequency) {
	// midi note = log(Hertz/440.0)/log(2) * 12 + 69;
	int32 MidiNote = logf(Frequency / MIN_FREQUENCY ) / logf(2) * 12 + 9;
	return MidiNote;
}

float UMidiUtils::SemitoneToFrequency(int32 semitone) {
	// frequency = 2^(semitone/12) * 440 Hz
	float Frequency = powf(2.0f, semitone / 12) * MIN_FREQUENCY;
	return Frequency;
}

int32 UMidiUtils::FrequencyToSemitone(float Frequency) {
	// semi tone = 12*(log (Frequency/440) / log(2)) 
	int32 Semitone = logf(Frequency / MIN_FREQUENCY) / logf(2.0f) * 12;
	return Semitone;
}

int32 UMidiUtils::FrequencyToCent(float Frequency) {
	// cent = log(Hertz/440.0)/log(2) * 1200;
	int32 Cent = logf(Frequency / MIN_FREQUENCY) / logf(2.0f) * 1200;
	return Cent;
}

int32 UMidiUtils::FrequencyToOctave(float Frequency) {
	// octave = log(Hertz/440.0)/log(2);
	int32 Octave = logf(Frequency / MIN_FREQUENCY) / logf(2.0f);
	return Octave;
}

ENoteEnum UMidiUtils::NoteToChord(uint8 note) {
	return (ENoteEnum)(note % 12);
}

int32 UMidiUtils::GetPitchBendAmount(uint8 data1, uint8 data2) {
	int y = ((int)data2 & 0x7F) << 7;
	int x = ((int)data1);

	return y + x;
}

void UMidiUtils::ConvertPitchBendAmount(int32 amount, uint8& data1, uint8& data2) {
	amount = amount & 0x3FFF;
	data1 = (amount & 0x7F);
	data2 = amount >> 7;
}