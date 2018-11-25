// Copyright -> Scott Bishel

#include "MidiComponent.h"
#include "MidiPrivatePCH.h"

// Unreal Engine Files
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"

#include "MidiUtils.h"
#include "MidiFile.h"
#include "Util/MidiUtil.h"

#include "Event/ChannelAftertouch.h"
#include "Event/ChannelEvent.h"
#include "Event/Controller.h"
#include "Event/NoteAfterTouch.h"
#include "Event/NoteOff.h"
#include "Event/NoteOn.h"
#include "Event/PitchBend.h"
#include "Event/ProgramChange.h"
#include "Event/SystemExclusiveEvent.h"
#include "Event/Meta/Tempo.h"

#include "MidiAsset.h"

#include "Util/MidiProcessor.h"

#include "MML/LabMidiSong.h"
#include "Thread/MidiThread.h"

#include <algorithm>    // std::sort

// Sets default values for this component's properties
UMidiComponent::UMidiComponent() : PlaySpeed(1.0), mWorker(NULL)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.000f;
	// ...
	mProcessor.setListener(this);
	mMidiFile = NULL;
}

UMidiComponent::~UMidiComponent() {
	mProcessor.stop();

	if (mMidiFile)
		delete mMidiFile;
	mMidiFile = NULL;
}


// Called when the game starts
void UMidiComponent::BeginPlay()
{
	Super::BeginPlay();
	// ...
}

// Called every frame
void UMidiComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	if (mProcessor.PlayRate != PlaySpeed)
		mProcessor.PlayRate = PlaySpeed;

	if (!InBackground) {
		// update time
		if (isGameTime) {
			mProcessor.update(GetWorld()->TimeSeconds * 1000.0f);
		}
		else
			mProcessor.update(FPlatformTime::Cycles());
	}

	else
		while (!mQueue.IsEmpty()) {
			MidiCallbackMessage _midiEvent;
			mQueue.Dequeue(_midiEvent);
			OnMidiEvent.Broadcast(_midiEvent.Event, _midiEvent.ms, _midiEvent.trackID);
		}
}

bool UMidiComponent::canInit() {
	if (isRunning()) {
		UE_LOG(LogTemp, Warning, TEXT("Unable to load MIDI while another is playing"));
		return false;
	}

	if (mMidiFile)
		delete mMidiFile;
	mMidiFile = NULL;

	return true;
}

struct membuf : std::streambuf
{
	membuf(char* begin, char* end) {
		this->setg(begin, begin, end);
	}
};

void UMidiComponent::LoadAsset(UMidiAsset* MidiAsset) {
	if (!canInit()) return;
	if (!MidiAsset) return;

	const TArray<uint8>& data = MidiAsset->Data;
	if (data.Num() == 0)
		return;

	char* ptr = (char*)data.GetData();

	membuf sbuf(ptr, ptr + data.Num());
	std::istream in(&sbuf);

	mMidiFile = new MidiFile(in);
	mProcessor.load(*mMidiFile);
}

void UMidiComponent::LoadFile(FString path) {
	if (!canInit()) return;

	TArray<uint8> data;
	bool result = FFileHelper::LoadFileToArray(data, path.GetCharArray().GetData());
	if (result == 0 || data.Num() == 0)
		return;

	char* ptr = (char*)data.GetData();

	membuf sbuf(ptr, ptr + data.Num());
	std::istream in(&sbuf);

	mMidiFile = new MidiFile(in);
	mProcessor.load(*mMidiFile);
}

void UMidiComponent::LoadMML(FString path) {
	if (!canInit()) return;
	std::string MyStdString(TCHAR_TO_UTF8(*path));

	Lab::MidiSong song;
	song.trackNumber = 0;
	song.LoadString(MyStdString);
	mMidiFile = new MidiFile();
	mMidiFile->addTrack(song.track);
	mProcessor.load(*mMidiFile);
}

void UMidiComponent::onEvent(MidiEvent* _event, long ms) {
	// Channel Event
	if (_event->getType() >= ChannelEvent::NOTE_OFF && _event->getType() <= ChannelEvent::PITCH_BEND) {
		ChannelEvent* channelEvent = static_cast<ChannelEvent*>(_event);
		FMidiEvent _midiEvent;

		_midiEvent.Type = static_cast<EMidiTypeEnum>(_event->getType() & 0X0F);
		_midiEvent.Channel = channelEvent->getChannel() & 0x0F;
		_midiEvent.Data1 = channelEvent->getValue1() & 0xFF;
		_midiEvent.Data2 = channelEvent->getValue2() & 0xFF;
		
		if (SimplifyNote) {
			// Running Status Event [Improved Midi Performance]
			if (_event->getType() == ChannelEvent::NOTE_OFF) {
				_midiEvent.Type = static_cast<EMidiTypeEnum>(ChannelEvent::NOTE_ON & 0X0F);
				_midiEvent.Data2 = 0 & 0XFF;
			}
		}
		if (InBackground) {
			MidiCallbackMessage msg = { _midiEvent, ms, mProcessor._trackID };
			mQueue.Enqueue(msg);
		}
		else
			OnMidiEvent.Broadcast(_midiEvent, ms, mProcessor._trackID);
	}
}

void UMidiComponent::onStart(bool fromBeginning) { 
	OnStart.Broadcast(fromBeginning); 
}
void UMidiComponent::onStop(bool finish) { 
	OnStop.Broadcast(finish);

		// MultiThread
	if (mWorker) {
		mWorker->Stop();
		delete mWorker;
	}
	mWorker = NULL;
	mQueue.Empty();
}

//-----------------------------------

void UMidiComponent::start(bool background, bool UseGameTime) {
	if (!isRunning()) {
		InBackground = background;
		this->isGameTime = UseGameTime;


		if (UseGameTime) {
			mProcessor.milliFunction = NULL;
			if (!InBackground)
				mProcessor.start(GetWorld()->TimeSeconds * 1000.0f);
		}
		else {
			mProcessor.milliFunction = FPlatformTime::ToMilliseconds;
			if (!InBackground)
				mProcessor.start(FPlatformTime::Cycles());
		}

		// MultiThread
		if (InBackground) {
			mWorker = new FMidiProcessorWorker(&mProcessor, this->isGameTime);
		}
	}
}

void UMidiComponent::stop() {
	mProcessor.stop();
}

void UMidiComponent::reset() {
	mProcessor.stop();
	mProcessor.reset();
}

bool UMidiComponent::isStarted() {
	return mProcessor.isStarted();
}
bool UMidiComponent::isRunning() {
	return mProcessor.isRunning();
}

int UMidiComponent::GetResolution()
{
	if (mMidiFile)
	{
		return mMidiFile->getResolution();
	}
	else
	{
		return 0;
	}
}

// sort MIDI predicate
inline static bool _ConstPredicate(const MidiEvent* ip1, const MidiEvent* ip2)
{
	int value = ((MidiEvent*)ip1)->compareTo((MidiEvent*)ip2);

	// somehow value should be less then else it flips the MIDI file
	return value < 0;
}

float UMidiComponent::GetDuration()
{

	// TODO find a better solution
	if (mMidiFile)
	{
		vector<vector<MidiEvent*>::iterator > mCurrEvents;
		vector<vector<MidiEvent*>::iterator > mCurrEventsEnd;

		vector<MidiTrack*>& tracks = mMidiFile->getTracks();
		for (int i = 0; i < (int)tracks.size(); i++) {
			mCurrEvents.push_back(tracks[i]->getEvents().begin());
			mCurrEventsEnd.push_back(tracks[i]->getEvents().end());
		}

		double msElapsed = 0;
		const int& mPPQ = mMidiFile->getResolution();
		int mMPQN = Tempo::DEFAULT_MPQN;

		vector<Tempo*> tempoTicks;
		for (int i = 0; i < (int)mCurrEvents.size(); i++) {
			while (mCurrEvents[i] != mCurrEventsEnd[i]) {
				MidiEvent * _event = *mCurrEvents[i];
				// Tempo and Time Signature events are always needed by the processor
				if (_event->getType() == MetaEvent::TEMPO) {
					tempoTicks.push_back(static_cast<Tempo*>(_event));
				}
				++mCurrEvents[i];
			}
		}

		std::sort(tempoTicks.begin(), tempoTicks.end(), _ConstPredicate);

		double ticksElapsed = 0;
		for (int i = 0; i < (int)tempoTicks.size(); i++) {
			Tempo * tempo = tempoTicks[i];

			if (i > 0)
				ticksElapsed = tempoTicks[i - 1]->getTick();

			msElapsed += MidiUtil::ticksToMs(tempo->getTick() - ticksElapsed, mMPQN, mPPQ);
			mMPQN = tempo->getMpqn();
		}

		if (tempoTicks.size() > 0)
			ticksElapsed = tempoTicks[(int)tempoTicks.size() - 1]->getTick();

		msElapsed += MidiUtil::ticksToMs(mMidiFile->getLengthInTicks() - ticksElapsed, mMPQN, mPPQ);

		return  msElapsed / 1000.0;
	}
	else
	{
		return 0;
	}
}
