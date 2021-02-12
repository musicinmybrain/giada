/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2021 Giovanni A. Zuliani | Monocasual
 *
 * This file is part of Giada - Your Hardcore Loopmachine.
 *
 * Giada - Your Hardcore Loopmachine is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Giada - Your Hardcore Loopmachine is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Giada - Your Hardcore Loopmachine. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- */


#ifndef G_RENDER_MODEL_H
#define G_RENDER_MODEL_H


#include <algorithm>
#include "core/model/traits.h"
#include "core/channels/channel.h"
#include "core/channels/state.h"
#include "core/const.h"
#include "core/wave.h"
#include "core/plugins/plugin.h"
#include "core/rcuList.h"
#include "core/recorder.h"
#include "core/swapper.h"
#include "utils/vector.h"


namespace giada::m::model
{
struct Kernel
{
	bool audioReady = false;
	bool midiReady  = false;
};

struct Recorder
{
	bool isRecordingAction = false;
	bool isRecordingInput  = false;
};

struct MidiIn
{
	bool     enabled    = false;
	int      filter     = -1;
	uint32_t rewind     = 0x0;
	uint32_t startStop  = 0x0;
	uint32_t actionRec  = 0x0;
	uint32_t inputRec   = 0x0;
	uint32_t volumeIn   = 0x0;
	uint32_t volumeOut  = 0x0;
	uint32_t beatDouble = 0x0;
	uint32_t beatHalf   = 0x0;
	uint32_t metronome  = 0x0;	
};

struct Clock
{	
	struct State
	{
		WeakAtomic<int> currentFrameWait = 0;
		WeakAtomic<int> currentFrame     = 0;
		WeakAtomic<int> currentBeat      = 0;
	};

	State*      state        = nullptr;
	ClockStatus status       = ClockStatus::STOPPED;
	int         framesInLoop = 0;
	int         framesInBar  = 0;
	int         framesInBeat = 0;
	int         framesInSeq  = 0;
	int         bars         = G_DEFAULT_BARS;
	int         beats        = G_DEFAULT_BEATS;
	float       bpm          = G_DEFAULT_BPM;
	int         quantize     = G_DEFAULT_QUANTIZE;
};

struct Mixer
{
	struct State
	{
		std::atomic<bool> processing = false;
		std::atomic<bool> active     = false;
		WeakAtomic<float> peakOut    = 0.0f;
		WeakAtomic<float> peakIn     = 0.0f;
	};

	State* state    = nullptr;
	bool   hasSolos = false;    
	bool   inToOut  = false;
};

struct Layout
{
	channel::Data&       getChannel(ID id); 
	const channel::Data& getChannel(ID id) const;

	Clock    clock;
	Mixer    mixer;
	Kernel   kernel;
	Recorder recorder;
	MidiIn   midiIn;

	std::vector<channel::Data> channels;
	recorder::ActionMap        actions;
};

/* Lock
Alias for a REALTIME scoped lock provided by the Swapper class. Use this in the
real-time thread to lock the Layout. */

using Lock = Swapper<Layout>::RtLock;

/* SwapType
Type of Layout change. 
	Hard: the structure has changed (e.g. add a new channel);
	Soft: a property has changed (e.g. change volume);
	None: something has changed but we don't care. 
Used by model listeners to determine the type of change that occured in the 
layout. */

enum class SwapType { HARD, SOFT, NONE };


/* -------------------------------------------------------------------------- */


class DataLock
{
public:

	DataLock(const channel::Data& ch, SwapType t=SwapType::HARD);
	DataLock(ID channelId, SwapType t=SwapType::HARD);
	~DataLock();

	channel::Data channel;

private:

	std::size_t m_index;
	SwapType    m_swapType;
};


/* -------------------------------------------------------------------------- */


/* init
Initializes the internal layout. */

void init();

/* get
Returns a reference to the NON-REALTIME layout structure. */

Layout& get();

/* get_RT
Returns a Lock object for REALTIME processing. Access layout by calling 
Lock::get() method (returns ready-only Layout). */

Lock get_RT();

/* swap
Swap non-rt layout with the rt one. See 'SwapType' notes above. */

void swap(SwapType t);

/* onSwap
Registers an optional callback fired when the layout has been swapped. Useful 
for listening to model changes. */

void onSwap(std::function<void(SwapType)> f);


/* -------------------------------------------------------------------------- */

/* Model utilities */

// TODO - are ID-based objects still necessary?

template <typename T> 
std::vector<std::unique_ptr<T>>& getAll();

/* find
Finds something (Plugins or Waves) given an ID. Returns nullptr if the object is
not found. */

template <typename T> 
T* find(ID id);

template <typename T> 
T& add(std::unique_ptr<T>);

template <typename T> 
void remove(const T&);

template <typename T> 
void clear();

#ifdef G_DEBUG_MODE
void debug();
#endif
} // giada::m::model::


#endif
