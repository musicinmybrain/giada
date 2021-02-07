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
namespace
{
/* getIter_
Returns an iterator of an element from list 'list' with given ID. */

template<typename L>
auto getIter_(L& list, ID id)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	auto it = std::find_if(list.begin(), list.end(), [&](auto* t)
	{
		return t->id == id;
	});
	assert(it != list.end());
	return it;
}


/* -------------------------------------------------------------------------- */

/* onSwapByIndex_
Swaps i-th element from list with a new one and applies a function f to it. */

template<typename L>
void onSwapByIndex_(L& list, std::size_t i, std::function<void(typename L::value_type&)> f)
{
	std::unique_ptr<typename L::value_type> o = list.clone(i);
	f(*o.get());
	list.swap(std::move(o), i);
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


struct Clock
{	
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
	bool hasSolos = false;    
	bool inToOut  = false;
};


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


struct Actions
{
	Actions() = default;
	Actions(const Actions& o);

	recorder::ActionMap map;
};


using ClockLock    = RCUList<Clock>::Lock;
using MixerLock    = RCUList<Mixer>::Lock;
using RecorderLock = RCUList<Recorder>::Lock;
using MidiInLock   = RCUList<MidiIn>::Lock;
using ActionsLock  = RCUList<Actions>::Lock;
using ChannelsLock = RCUList<Channel>::Lock;
using WavesLock    = RCUList<Wave>::Lock;
#ifdef WITH_VST
using PluginsLock  = RCUList<Plugin>::Lock;
#endif

extern RCUList<Clock>    clock;
extern RCUList<Mixer>    mixer;
extern RCUList<Recorder> recorder;
extern RCUList<MidiIn>   midiIn;
extern RCUList<Actions>  actions;
extern RCUList<Channel>  channels;
extern RCUList<Wave>     waves;
#ifdef WITH_VST
extern RCUList<Plugin>   plugins;
#endif


/* -------------------------------------------------------------------------- */


template<typename L>
bool exists(L& list, ID id)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");	
	typename L::Lock l(list);
	auto it = std::find_if(list.begin(), list.end(), [&](auto* t)
	{
		return t->id == id;
	});
	return it != list.end();
}


/* -------------------------------------------------------------------------- */


/* getIndex (thread safe)
Returns the index of element with ID from a list. */

template<typename L>
std::size_t getIndex(L& list, ID id)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	typename L::Lock l(list);
	return std::distance(list.begin(), getIter_(list, id));
}


/* -------------------------------------------------------------------------- */


/* getIndex (thread safe)
Returns the element ID of the i-th element of a list. */

template<typename L>
ID getId(L& list, std::size_t i)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	typename L::Lock l(list);
	return list.get(i)->id;
}


/* -------------------------------------------------------------------------- */


template<typename L>
typename L::value_type& get(L& list, ID id)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	return **getIter_(list, id);
}


/* -------------------------------------------------------------------------- */


/* onGet (1) (thread safe)
Utility function for reading ID-based things from a RCUList. */

template<typename L>
void onGet(L& list, ID id, std::function<void(typename L::value_type&)> f, bool rebuild=false)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	typename L::Lock l(list);
	f(**getIter_(list, id));
	if (rebuild)
		list.changed.store(true);
}


/* onGet (2) (thread safe)
Same as (1), for non-ID-based things. */

template<typename L>
void onGet(L& list, std::function<void(typename L::value_type&)> f)
{
	static_assert(!has_id<typename L::value_type>(), "This type has ID");
	typename L::Lock l(list);
	f(*list.get());
}


/* ---------------------------------------------------------------------------*/



































struct Clock_NEW
{	
	struct State
	{
		std::atomic<int> currentFrameWait{0};// TODO - weakatomic
		std::atomic<int> currentFrame{0};// TODO - weakatomic
		std::atomic<int> currentBeat{0};// TODO - weakatomic
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

struct Mixer_NEW
{
	struct State
	{
		std::atomic<bool>  processing{false};
		std::atomic<bool>  active{false};
		std::atomic<float> peakOut{0.0f}; // TODO - weakatomic
		std::atomic<float> peakIn{0.0f}; // TODO - weakatomic
	};

	State* state    = nullptr;
	bool   hasSolos = false;    
	bool   inToOut  = false;
};

struct Layout
{
	channel::Data&       getChannel(ID id); 
	const channel::Data& getChannel(ID id) const;

	Clock_NEW clock;
	Mixer_NEW mixer;
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


class ChannelDataLock
{
public:

	ChannelDataLock(channel::Data& ch);
	ChannelDataLock(ID channelId);
	~ChannelDataLock();

	ID                   channelId;
	Wave*                wave;
	std::vector<Plugin*> plugins;
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
std::size_t getIndex(ID id);

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
