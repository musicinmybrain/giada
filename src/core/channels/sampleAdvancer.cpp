/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2020 Giovanni A. Zuliani | Monocasual

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
#include "core/clock.h"
#include "core/channels/channel.h"
#include "sampleAdvancer.h"


namespace giada::m 
{
SampleAdvancer::SampleAdvancer(Channel_NEW& c, SamplePlayer_NEW& s)
: m_channel     (c)
, m_samplePlayer(s) 
{
}


/* -------------------------------------------------------------------------- */


void SampleAdvancer::advance(const sequencer::Event& e) const
{
	switch (e.type) {

		case sequencer::EventType::FIRST_BEAT:
			onFirstBeat(e.delta);
			break;

		case sequencer::EventType::BAR:
			onBar(e.delta);
			break;

		case sequencer::EventType::REWIND:
			rewind(e.delta);
			break;

		case sequencer::EventType::ACTIONS:
			if (m_channel.readActions)
				parseActions(*e.actions, e.delta);
			break;

		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void SampleAdvancer::onFirstBeat(Frame localFrame) const
{
G_DEBUG("onFirstBeat ch=" << m_channel.id << ", localFrame=" << localFrame);

	ChannelStatus playStatus = m_channel.playStatus;
	bool          isLoop     = m_samplePlayer.isAnyLoopMode();

	switch (playStatus) { 

		case ChannelStatus::PLAY:
			if (isLoop) 
				rewind(localFrame); 
			break;
		
		case ChannelStatus::WAIT:
			pumpChannelFunction(m_channel.id, [] (Channel_NEW& ch)
			{
				ch.playStatus = ChannelStatus::PLAY;
			});
			break;

		case ChannelStatus::ENDING:
			if (isLoop) 
				stop(localFrame);
			break;
		
		default: break;
	}
}



/* -------------------------------------------------------------------------- */


void SampleAdvancer::onBar(Frame localFrame) const
{
	G_DEBUG("onBar ch=" << m_channel.id);

	ChannelStatus    playStatus = m_channel.playStatus;
	SamplePlayerMode mode       = m_samplePlayer.mode;

	if (playStatus == ChannelStatus::PLAY && (mode == SamplePlayerMode::LOOP_REPEAT || 
											  mode == SamplePlayerMode::LOOP_ONCE_BAR))
		rewind(localFrame);
	else
	if (playStatus == ChannelStatus::WAIT && mode == SamplePlayerMode::LOOP_ONCE_BAR) {
		pumpChannelFunction(m_channel.id, [] (Channel_NEW& ch)
		{
			ch.playStatus = ChannelStatus::PLAY;
		});
	}
}


/* -------------------------------------------------------------------------- */


void SampleAdvancer::parseActions(const std::vector<Action>& as, Frame localFrame) const
{
	bool isLoop = m_samplePlayer.isAnyLoopMode();

	for (const Action& a : as) {

		switch (a.event.getStatus()) {
			
			case MidiEvent::NOTE_ON:
				//if (!isLoop)
				//	press(localFrame, /*velocity=*/G_MAX_VELOCITY, /*manual=*/false);
				break;
			case MidiEvent::NOTE_OFF:
				//if (!isLoop)
				//	release(localFrame);
				break;
			case MidiEvent::NOTE_KILL:
				//if (!isLoop)
				//	stop(localFrame);
				break;
			case MidiEvent::ENVELOPE:
				//calcVolumeEnv_(ch, a); TODO
				break;
		}
	}
}


/* -------------------------------------------------------------------------- */


void SampleAdvancer::onLastFrame() const
{
	ChannelStatus    playStatus = m_channel.playStatus;
	SamplePlayerMode mode       = m_samplePlayer.mode;
	bool             running    = clock::isRunning();
	
	if (playStatus == ChannelStatus::PLAY) {
		/* Stop LOOP_* when the sequencer is off, or SINGLE_* except for
		SINGLE_ENDLESS, which runs forever unless it's in ENDING mode. 
		Other loop once modes are put in wait mode. */
		if ((mode == SamplePlayerMode::SINGLE_BASIC   || 
			 mode == SamplePlayerMode::SINGLE_PRESS   ||
			 mode == SamplePlayerMode::SINGLE_RETRIG) || 
			(m_samplePlayer.isAnyLoopMode() && !running))
			playStatus = ChannelStatus::OFF;
		else
		if (mode == SamplePlayerMode::LOOP_ONCE || mode == SamplePlayerMode::LOOP_ONCE_BAR)
			playStatus = ChannelStatus::WAIT;
	}
	else
	if (playStatus == ChannelStatus::ENDING)
		playStatus = ChannelStatus::OFF;

	pumpChannelFunction(m_channel.id, [playStatus] (Channel_NEW& ch)
	{
		ch.playStatus = playStatus;
	});
}


/* -------------------------------------------------------------------------- */


void SampleAdvancer::rewind(Frame localFrame) const
{
	pumpChannelFunction(m_channel.id, [localFrame] (Channel_NEW& ch)
	{
		ch.state->rewinding = true;
		ch.state->offset    = localFrame;
	});
}


/* -------------------------------------------------------------------------- */


void SampleAdvancer::stop(Frame localFrame) const
{
	pumpChannelFunction(m_channel.id, [begin = m_samplePlayer.begin] (Channel_NEW& ch)
	{
		ch.playStatus = ChannelStatus::OFF;
		ch.state->tracker.store(begin);
		ch.samplePlayer->quantizing = false;
	});

	/*  Clear data in range [localFrame, (buffer.size)) if the event occurs in
	the middle of the buffer. TODO - samplePlayer should be responsible for this*/
/*
	if (localFrame != 0)
		m_channel->data.audioBuffer.clear(localFrame);*/
}
} // giada::m::
















namespace giada::m::sampleAdvancer
{
namespace
{
void rewind_(const channel::Data& ch, Frame localFrame)
{
    ch.state->rewinding = true;
    ch.state->offset    = localFrame;
}


/* -------------------------------------------------------------------------- */


void stop_(const channel::Data& ch, Frame localFrame)
{
	ch.state->playStatus.store(ChannelStatus::OFF);
	ch.state->tracker.store(ch.samplePlayer->begin);
	// TODO ? ch.samplePlayer->quantizing = false;

    /*  Clear data in range [localFrame, (buffer.size)) if the event occurs in
    the middle of the buffer. TODO - samplePlayer should be responsible for this*/
/*
if (localFrame != 0)
    m_channel->data.audioBuffer.clear(localFrame);*/
}


/* -------------------------------------------------------------------------- */


void onFirstBeat_(const channel::Data& ch, Frame localFrame)
{
G_DEBUG("onFirstBeat ch=" << ch.id << ", localFrame=" << localFrame);

	ChannelStatus playStatus = ch.state->playStatus.load();
	bool          isLoop     = ch.samplePlayer->isAnyLoopMode();

	switch (playStatus) { 

		case ChannelStatus::PLAY:
			if (isLoop) 
				rewind_(ch, localFrame);
			break;
		
		case ChannelStatus::WAIT:
		    ch.state->playStatus.store(ChannelStatus::PLAY);
			break;

		case ChannelStatus::ENDING:
			if (isLoop) 
				stop_(ch, localFrame);
			break;
		
		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void onBar_(const channel::Data& ch, Frame localFrame)
{
	G_DEBUG("onBar ch=" << ch.id);

	ChannelStatus    playStatus = ch.state->playStatus.load();;
	SamplePlayerMode mode       = ch.samplePlayer->mode;

	if (playStatus == ChannelStatus::PLAY && (mode == SamplePlayerMode::LOOP_REPEAT || 
											  mode == SamplePlayerMode::LOOP_ONCE_BAR))
		rewind_(ch, localFrame);
	else
	if (playStatus == ChannelStatus::WAIT && mode == SamplePlayerMode::LOOP_ONCE_BAR)
	    ch.state->playStatus.store(ChannelStatus::PLAY);
}


/* -------------------------------------------------------------------------- */


void parseActions_(const channel::Data& ch, const std::vector<Action>& as, Frame localFrame)
{
	bool isLoop = ch.samplePlayer->isAnyLoopMode();

	for (const Action& a : as) {

		switch (a.event.getStatus()) {
			
			case MidiEvent::NOTE_ON:
				//if (!isLoop)
				//	press(localFrame, /*velocity=*/G_MAX_VELOCITY, /*manual=*/false);
				break;
			case MidiEvent::NOTE_OFF:
				//if (!isLoop)
				//	release(localFrame);
				break;
			case MidiEvent::NOTE_KILL:
				//if (!isLoop)
				//	stop(localFrame);
				break;
			case MidiEvent::ENVELOPE:
				//calcVolumeEnv_(ch, a); TODO
				break;
		}
	}
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void onLastFrame(const channel::Data& ch)
{
	ChannelStatus    playStatus = ch.state->playStatus.load();
	SamplePlayerMode mode       = ch.samplePlayer->mode;
	bool             isLoop     = ch.samplePlayer->isAnyLoopMode();
	bool             running    = clock::isRunning();
	
	if (playStatus == ChannelStatus::PLAY) {
		/* Stop LOOP_* when the sequencer is off, or SINGLE_* except for
		SINGLE_ENDLESS, which runs forever unless it's in ENDING mode. 
		Other loop once modes are put in wait mode. */
		if ((mode == SamplePlayerMode::SINGLE_BASIC   || 
			 mode == SamplePlayerMode::SINGLE_PRESS   ||
			 mode == SamplePlayerMode::SINGLE_RETRIG) || 
			(isLoop && !running))
			playStatus = ChannelStatus::OFF;
		else
		if (mode == SamplePlayerMode::LOOP_ONCE || mode == SamplePlayerMode::LOOP_ONCE_BAR)
			playStatus = ChannelStatus::WAIT;
	}
	else
	if (playStatus == ChannelStatus::ENDING)
		playStatus = ChannelStatus::OFF;

	ch.state->playStatus.store(playStatus);
}


/* -------------------------------------------------------------------------- */


void advance(const channel::Data& ch, const sequencer::Event& e)
{
	switch (e.type) {

		case sequencer::EventType::FIRST_BEAT:
			onFirstBeat_(ch, e.delta);
			break;

		case sequencer::EventType::BAR:
			onBar_(ch, e.delta);
			break;

		case sequencer::EventType::REWIND:
			rewind_(ch, e.delta);
			break;

		case sequencer::EventType::ACTIONS:
			if (ch.readActions)
				parseActions_(ch, *e.actions, e.delta);
			break;

		default: break;
	}
}
}