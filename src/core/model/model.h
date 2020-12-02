/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2020 Giovanni A. Zuliani | Monocasual
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
#include "core/rcuArray.h"
#include "core/recorder.h"


namespace giada {
namespace m {
namespace model
{
namespace
{
/* getIter_
Returns an iterator of an element from list 'list' with given ID. */

template<typename L>
auto getIter_(L& list, ID id)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	auto it = std::find_if(list.begin(), list.end(), [&](auto& t)
	{
		return t.id == id;
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
	typename L::value_type o = list.clone(i);
	f(o);
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


using ClockLock    = RCUArray<Clock>::Lock;
using MixerLock    = RCUArray<Mixer>::Lock;
using KernelLock   = RCUArray<Kernel>::Lock;
using RecorderLock = RCUArray<Recorder>::Lock;
using MidiInLock   = RCUArray<MidiIn>::Lock;
using ActionsLock  = RCUArray<Actions>::Lock;
using ChannelsLock = RCUArray<Channel, 1024>::Lock;
using WavesLock    = RCUArray<Wave, 1024>::Lock;
#ifdef WITH_VST
using PluginsLock  = RCUArray<Plugin, 1024>::Lock;
#endif

extern RCUArray<Clock>    clock;
extern RCUArray<Mixer>    mixer;
extern RCUArray<Kernel>   kernel;
extern RCUArray<Recorder> recorder;
extern RCUArray<MidiIn>   midiIn;
extern RCUArray<Actions>  actions;
extern RCUArray<Channel, 1024>  channels;
extern RCUArray<Wave, 1024>     waves;
#ifdef WITH_VST
extern RCUArray<Plugin, 1024>   plugins;
#endif


/* -------------------------------------------------------------------------- */


template<typename L>
bool exists(L& list, ID id)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");	
	typename L::Lock l(list);
	auto it = std::find_if(list.begin(), list.end(), [&](auto& t)
	{
		return t.id == id;
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
	return list.get(i).id;
}


/* -------------------------------------------------------------------------- */


template<typename L>
typename L::value_type& get(L& list, ID id)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	return *getIter_(list, id);
}


/* -------------------------------------------------------------------------- */


/* onGet (1) (thread safe)
Utility function for reading ID-based things from a RCUList. */

template<typename L>
void onGet(L& list, ID id, std::function<void(typename L::value_type&)> f, bool rebuild=false)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	typename L::Lock l(list);
	f(*getIter_(list, id));
	if (rebuild)
		list.resetChanged(true);
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


/* onSwap (1) (thread safe)
Utility function for swapping ID-based things in a RCUList. */

template<typename L>
void onSwap(L& list, ID id, std::function<void(typename L::value_type&)> f)
{
	static_assert(has_id<typename L::value_type>(), "This type has no ID");
	onSwapByIndex_(list, getIndex(list, id), f); 
}


/* onSwap (2) (thread safe)
Utility function for swapping things in a RCUList when the list contains only
a single element (and so with no ID). */

template<typename L>
void onSwap(L& list, std::function<void(typename L::value_type&)> f)
{
	static_assert(!has_id<typename L::value_type>(), "This type has ID");
	onSwapByIndex_(list, 0, f); 
}


/* ---------------------------------------------------------------------------*/


#ifdef G_DEBUG_MODE

void debug();

#endif
}}} // giada::m::model::


#endif
