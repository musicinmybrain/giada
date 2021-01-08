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


#include <cassert>
#include "core/conf.h"
#include "core/channels/channel.h"
#include "core/channels/state.h"
#include "midiController.h"


namespace giada::m 
{
MidiController::MidiController(ChannelState* c)
: m_channelState(c)
{
}


/* -------------------------------------------------------------------------- */


MidiController::MidiController(const MidiController& /*o*/, ChannelState* c)
: m_channelState(c)
{
}


/* -------------------------------------------------------------------------- */


void MidiController::parse(const mixer::Event& e) const
{
	assert(m_channelState != nullptr);

	switch (e.type) {

		case mixer::EventType::KEY_PRESS:
			press(); break;

		case mixer::EventType::KEY_KILL:
		case mixer::EventType::SEQUENCER_STOP:
			kill();	break;

		case mixer::EventType::SEQUENCER_FIRST_BEAT:
		case mixer::EventType::SEQUENCER_REWIND:
			onFirstBeat();	

		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void MidiController::press() const
{
    ChannelStatus playStatus = m_channelState->playStatus.load();

	switch (playStatus) {
		case ChannelStatus::PLAY:
			playStatus = ChannelStatus::ENDING; break;

		case ChannelStatus::ENDING:
		case ChannelStatus::WAIT:
			playStatus = ChannelStatus::OFF; break;

		case ChannelStatus::OFF:
			playStatus = ChannelStatus::WAIT; break;

		default: break;
	}	
	
	m_channelState->playStatus.store(playStatus);
}


/* -------------------------------------------------------------------------- */


void MidiController::kill() const
{
	m_channelState->playStatus.store(ChannelStatus::OFF);
}


/* -------------------------------------------------------------------------- */


void MidiController::onFirstBeat() const
{
	ChannelStatus playStatus = m_channelState->playStatus.load();

	if (playStatus == ChannelStatus::ENDING)
		playStatus = ChannelStatus::OFF;
	else
	if (playStatus == ChannelStatus::WAIT)
		playStatus = ChannelStatus::PLAY;
	
	m_channelState->playStatus.store(playStatus);
}



















MidiController_NEW::MidiController_NEW(Channel_NEW& c)
: m_channel(c)
{
}


/* -------------------------------------------------------------------------- */


void MidiController_NEW::react(const eventDispatcher::Event& e)
{
	switch (e.type) {

		case eventDispatcher::EventType::KEY_PRESS:
			press(); break;

		case eventDispatcher::EventType::KEY_KILL:
		case eventDispatcher::EventType::SEQUENCER_STOP:
			kill();	break;

		case eventDispatcher::EventType::SEQUENCER_REWIND:
			m_channel.playStatus = onFirstBeat();

		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void MidiController_NEW::advance(const sequencer::Event& e) const
{
	if (e.type == sequencer::EventType::FIRST_BEAT)
		pumpChannelFunction(m_channel.id, [status = onFirstBeat()](Channel_NEW& c)
		{
			c.playStatus = status;
		});
}


/* -------------------------------------------------------------------------- */


void MidiController_NEW::press()
{
    ChannelStatus playStatus = m_channel.playStatus;

	switch (playStatus) {
		case ChannelStatus::PLAY:
			playStatus = ChannelStatus::ENDING; break;

		case ChannelStatus::ENDING:
		case ChannelStatus::WAIT:
			playStatus = ChannelStatus::OFF; break;

		case ChannelStatus::OFF:
			playStatus = ChannelStatus::WAIT; break;

		default: break;
	}

	m_channel.playStatus = playStatus;
}


/* -------------------------------------------------------------------------- */


void MidiController_NEW::kill()
{
	m_channel.playStatus = ChannelStatus::OFF;
}


/* -------------------------------------------------------------------------- */


ChannelStatus MidiController_NEW::onFirstBeat() const
{
	ChannelStatus playStatus = m_channel.playStatus;

	if (playStatus == ChannelStatus::ENDING)
		playStatus = ChannelStatus::OFF;
	else
	if (playStatus == ChannelStatus::WAIT)
		playStatus = ChannelStatus::PLAY;

	return playStatus;
}
} // giada::m::



















namespace giada::m::midiController
{
namespace
{
ChannelStatus onFirstBeat_(const channel::Data& ch)
{
	ChannelStatus playStatus = ch.playStatus;

	if (playStatus == ChannelStatus::ENDING)
		playStatus = ChannelStatus::OFF;
	else
	if (playStatus == ChannelStatus::WAIT)
		playStatus = ChannelStatus::PLAY;

	return playStatus;
}


/* -------------------------------------------------------------------------- */


ChannelStatus press_(const channel::Data& ch)
{
    ChannelStatus playStatus = ch.playStatus;

	switch (playStatus) {
		case ChannelStatus::PLAY:
			playStatus = ChannelStatus::ENDING; break;

		case ChannelStatus::ENDING:
		case ChannelStatus::WAIT:
			playStatus = ChannelStatus::OFF; break;

		case ChannelStatus::OFF:
			playStatus = ChannelStatus::WAIT; break;

		default: break;
	}

	return playStatus;	
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void react(channel::Data& ch, const eventDispatcher::Event& e)
{
	switch (e.type) {

		case eventDispatcher::EventType::KEY_PRESS:
			ch.playStatus = press_(ch); break;

		case eventDispatcher::EventType::KEY_KILL:
		case eventDispatcher::EventType::SEQUENCER_STOP:
			ch.playStatus = ChannelStatus::OFF; break;

		case eventDispatcher::EventType::SEQUENCER_REWIND:
			ch.playStatus = onFirstBeat_(ch);

		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void advance(const channel::Data& ch, const sequencer::Event& e)
{
	if (e.type != sequencer::EventType::FIRST_BEAT)
		return;
	assert(false);
	/*
	pumpChannelFunction(ch.id, [status = onFirstBeat_(ch)](channel::Data& ch)
	{
		ch.playStatus = status;
	});*/
}
}
