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


#include <functional>
#include "core/model/model.h"
#include "utils/log.h"
#include "core/worker.h"
#include "core/clock.h"
#include "core/sequencer.h"
#include "eventDispatcher.h"


namespace giada::m::eventDispatcher
{
namespace
{
Worker worker_;

/* eventBuffer_
Buffer of events sent to channels for event parsing. This is filled with Events
coming from the two event queues.*/

EventBuffer eventBuffer_;


/* -------------------------------------------------------------------------- */


bool hasChannelEvents_()
{
	return u::vector::has(eventBuffer_, [] (const Event& e) { return e.channelId != 0; });
}


/* -------------------------------------------------------------------------- */


void processChannels_()
{
	std::vector<channel::Data> channels = model::get().channels;

	for (channel::Data& ch : channels)
		channel::react(ch, eventBuffer_, true); // TODO audible
	
	model::swap([&channels] (model::Layout& l) { l.channels = channels; }, model::SwapType::SOFT);
}


/* -------------------------------------------------------------------------- */


void processSequencer_()
{
	sequencer::react(eventBuffer_);
}


/* -------------------------------------------------------------------------- */


void process_()
{
	eventBuffer_.clear();

	Event e;
	while (UIevents.pop(e))   eventBuffer_.push_back(e);
	while (MidiEvents.pop(e)) eventBuffer_.push_back(e);

	if (eventBuffer_.size() == 0)
		return;
		
#ifdef G_DEBUG_MODE
	for (const Event& e : eventBuffer_)
		G_DEBUG("Event type=" << (int) e.type << ", delta=" << e.delta <<
		        ", globalFrame=" << clock::getCurrentFrame());
#endif

	if (hasChannelEvents_())
		processChannels_();
	processSequencer_();
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


Queue<Event, G_MAX_DISPATCHER_EVENTS> UIevents;
Queue<Event, G_MAX_DISPATCHER_EVENTS> MidiEvents;


/* -------------------------------------------------------------------------- */


void init()
{
    worker_.start(process_, /*sleep=*/5); // TODO - <= than audio thread speed
}


/* -------------------------------------------------------------------------- */


void pumpEvent(Event e)
{
	UIevents.push(e); // The actual queue doesn't matter
}
} // giada::m