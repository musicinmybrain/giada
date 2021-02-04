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
#include "core/action.h"
#include "core/eventDispatcher.h"
#include "core/clock.h"
#include "core/conf.h"
#include "core/mixer.h"
#include "core/recorderHandler.h"
#include "core/recManager.h"
#include "core/channels/state.h"
#include "core/channels/channel.h"
#include "sampleActionRecorder.h"


namespace giada {
namespace m
{
SampleActionRecorder::SampleActionRecorder(ChannelState* c, SamplePlayerState* sc)
: m_channelState(c)
, m_samplePlayerState(sc)
{
}


/* -------------------------------------------------------------------------- */


SampleActionRecorder::SampleActionRecorder(const SampleActionRecorder& /*o*/, 
	ChannelState* c, SamplePlayerState* sc)
: SampleActionRecorder(c, sc)
{
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::parse(const mixer::Event& e) const
{
	assert(m_channelState != nullptr);

	switch (e.type) {

		case mixer::EventType::KEY_PRESS:
			onKeyPress(); break;

		/* Record a stop event only if channel is SINGLE_PRESS. For any other 
		mode the key release event is meaningless. */

		case mixer::EventType::KEY_RELEASE:
			if (canRecord() && m_samplePlayerState->mode.load() == SamplePlayerMode::SINGLE_PRESS) 
				record(MidiEvent::NOTE_OFF);
			break;

		case mixer::EventType::KEY_KILL:
			if (canRecord()) 
				record(MidiEvent::NOTE_KILL);
			break;
		
		case mixer::EventType::SEQUENCER_FIRST_BEAT:
			onFirstBeat(); break;

		case mixer::EventType::CHANNEL_TOGGLE_READ_ACTIONS:	
			toggleReadActions(); break;        

		case mixer::EventType::CHANNEL_KILL_READ_ACTIONS:
			killReadActions(); break;      
		
		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::record(int note) const
{
	recorderHandler::liveRec(m_channelState->id, MidiEvent(note, 0, 0), 
		clock::quantize(clock::getCurrentFrame()));

	m_channelState->hasActions = true;
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::startReadActions() const
{
	if (conf::conf.treatRecsAsLoops) 
		m_channelState->recStatus.store(ChannelStatus::WAIT);
	else {
		m_channelState->recStatus.store(ChannelStatus::PLAY);
		m_channelState->readActions.store(true);
	}
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::stopReadActions(ChannelStatus curRecStatus) const
{
	/* First of all, if the clock is not running or treatRecsAsLoops is off, 
	just stop and disable everything. Otherwise make sure a channel with actions
	behave like a dynamic one. */

	if (!clock::isRunning() || !conf::conf.treatRecsAsLoops) {
		m_channelState->recStatus.store(ChannelStatus::OFF);
	    m_channelState->readActions.store(false);
	}
	else
	if (curRecStatus == ChannelStatus::WAIT)
		m_channelState->recStatus.store(ChannelStatus::OFF);
	else
	if (curRecStatus == ChannelStatus::ENDING)
		m_channelState->recStatus.store(ChannelStatus::PLAY);
	else
		m_channelState->recStatus.store(ChannelStatus::ENDING);
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::toggleReadActions() const
{
	/* When you start reading actions while conf::treatRecsAsLoops is true, the
	value ch.state->readActions actually is not set to true immediately, because
	the channel is in wait mode (REC_WAITING). readActions will become true on
	the next first beat. So a 'stop rec' command should occur also when
	readActions is false but the channel is in wait mode; this check will
	handle the case of when you press 'R', the channel goes into REC_WAITING and
	then you press 'R' again to undo the status. */

	if (!m_channelState->hasActions)
		return;

	bool          readActions = m_channelState->readActions.load();
	ChannelStatus recStatus   = m_channelState->recStatus.load();

	if (readActions || (!readActions && recStatus == ChannelStatus::WAIT))
		stopReadActions(recStatus);
	else
		startReadActions();
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::killReadActions() const
{
	/* Killing Read Actions, i.e. shift + click on 'R' button is meaninful only 
	when the conf::treatRecsAsLoops is true. */

	if (!conf::conf.treatRecsAsLoops)
		return;
	m_channelState->recStatus.store(ChannelStatus::OFF);
	m_channelState->readActions.store(false);
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::onKeyPress() const
{
	if (!canRecord()) 
		return;
	record(MidiEvent::NOTE_ON);

	/* Skip reading actions when recording on ChannelMode::SINGLE_PRESS to 
	prevent	existing actions to interfere with the keypress/keyrel combo. */

	if (m_samplePlayerState->mode.load() == SamplePlayerMode::SINGLE_PRESS)
		m_channelState->readActions = false;
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::onKeyRelease() const
{
	if (canRecord() && m_samplePlayerState->mode.load() == SamplePlayerMode::SINGLE_PRESS) {
		record(MidiEvent::NOTE_OFF);
		m_channelState->readActions = true;
	}
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder::onFirstBeat() const
{
	ChannelStatus recStatus = m_channelState->recStatus.load();

	switch (recStatus) { 

		case ChannelStatus::ENDING:
            m_channelState->recStatus.store(ChannelStatus::OFF);
			m_channelState->readActions = false;
			break;

		case ChannelStatus::WAIT:
            m_channelState->recStatus.store(ChannelStatus::PLAY);
			m_channelState->readActions = true;
			break;

		default: break;
	}	
}


/* -------------------------------------------------------------------------- */


bool SampleActionRecorder::canRecord() const
{
	return recManager::isRecordingAction() && 
	       clock::isRunning()              && 
	       !recManager::isRecordingInput() &&
		   !m_samplePlayerState->isAnyLoopMode();
}






































SampleActionRecorder_NEW::SampleActionRecorder_NEW(Channel_NEW& c, SamplePlayer_NEW& s)
: m_channel     (c)
, m_samplePlayer(s)
{
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::react(const eventDispatcher::Event& e)
{
	switch (e.type) {

		case eventDispatcher::EventType::KEY_PRESS:
			onKeyPress(); break;

		/* Record a stop event only if channel is SINGLE_PRESS. For any other 
		mode the key release event is meaningless. */

		case eventDispatcher::EventType::KEY_RELEASE:
			if (canRecord() && m_samplePlayer.mode == SamplePlayerMode::SINGLE_PRESS) 
				record(MidiEvent::NOTE_OFF);
			break;

		case eventDispatcher::EventType::KEY_KILL:
			if (canRecord()) 
				record(MidiEvent::NOTE_KILL);
			break;

		case eventDispatcher::EventType::CHANNEL_TOGGLE_READ_ACTIONS:	
			toggleReadActions(); break;        

		case eventDispatcher::EventType::CHANNEL_KILL_READ_ACTIONS:
			killReadActions(); break;      
		
		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::record(int note)
{
	recorderHandler::liveRec(m_channel.id, MidiEvent(note, 0, 0), 
		clock::quantize(clock::getCurrentFrame()));

	m_channel.hasActions = true;
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::startReadActions()
{
	if (conf::conf.treatRecsAsLoops)
		m_channel.recStatus = ChannelStatus::WAIT;
	else {
    	m_channel.recStatus = ChannelStatus::PLAY;
		m_channel.readActions = true;
	}
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::stopReadActions(ChannelStatus curRecStatus)
{
	/* First of all, if the clock is not running or treatRecsAsLoops is off, 
	just stop and disable everything. Otherwise make sure a channel with actions
	behave like a dynamic one. */

	if (!clock::isRunning() || !conf::conf.treatRecsAsLoops) {
        m_channel.recStatus = ChannelStatus::OFF;
	    m_channel.readActions = false;
	}
	else
	if (curRecStatus == ChannelStatus::WAIT)
        m_channel.recStatus = ChannelStatus::OFF;
	else
	if (curRecStatus == ChannelStatus::ENDING)
        m_channel.recStatus = ChannelStatus::PLAY;
	else
        m_channel.recStatus = ChannelStatus::ENDING;
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::toggleReadActions()
{
	/* When you start reading actions while conf::treatRecsAsLoops is true, the
	value ch.state->readActions actually is not set to true immediately, because
	the channel is in wait mode (REC_WAITING). readActions will become true on
	the next first beat. So a 'stop rec' command should occur also when
	readActions is false but the channel is in wait mode; this check will
	handle the case of when you press 'R', the channel goes into REC_WAITING and
	then you press 'R' again to undo the status. */

	if (!m_channel.hasActions)
		return;

	bool          readActions = m_channel.readActions;
	ChannelStatus recStatus   = m_channel.recStatus;

	if (readActions || (!readActions && recStatus == ChannelStatus::WAIT))
		stopReadActions(recStatus);
	else
		startReadActions();
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::killReadActions()
{
	/* Killing Read Actions, i.e. shift + click on 'R' button is meaninful only 
	when the conf::treatRecsAsLoops is true. */

	if (!conf::conf.treatRecsAsLoops)
		return;
    m_channel.recStatus = ChannelStatus::OFF;
	m_channel.readActions = false;
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::onKeyPress()
{
	if (!canRecord()) 
		return;
	record(MidiEvent::NOTE_ON);

	/* Skip reading actions when recording on ChannelMode::SINGLE_PRESS to 
	prevent	existing actions to interfere with the keypress/keyrel combo. */

	if (m_samplePlayer.mode == SamplePlayerMode::SINGLE_PRESS)
		m_channel.readActions = false;
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::onKeyRelease()
{
	if (canRecord() && m_samplePlayer.mode == SamplePlayerMode::SINGLE_PRESS) {
		record(MidiEvent::NOTE_OFF);
		m_channel.readActions = true;
	}
}


/* -------------------------------------------------------------------------- */

/*
void SampleActionRecorder_NEW::onFirstBeat() const
{
	ChannelStatus recStatus = m_channel.recStatus;

	switch (recStatus) { 

		case ChannelStatus::ENDING:
            pumpRecStatus(ChannelStatus::OFF);
			m_channel.readActions = false;
			break;

		case ChannelStatus::WAIT:
			pumpRecStatus(ChannelStatus::OFF);
			m_channel.readActions = true;
			break;

		default: break;
	}	
}
*/

/* -------------------------------------------------------------------------- */


bool SampleActionRecorder_NEW::canRecord() const
{
	return recManager::isRecordingAction() && 
	       clock::isRunning()              && 
	       !recManager::isRecordingInput() &&
		   !m_samplePlayer.isAnyLoopMode();
}


/* -------------------------------------------------------------------------- */


void SampleActionRecorder_NEW::pumpRecStatus(ChannelStatus s) const
{
    assert(false);
    /*
	pumpChannelFunction(m_channel.id, [s] (channel::Data& c)
	{
		c.recStatus = s;
	});*/
}
}} // giada::m::












namespace giada::m::sampleActionRecorder
{
namespace
{
void record_           (channel::Data& ch, int note);
void onKeyPress_       (channel::Data& ch);
void onKeyRelease_     (channel::Data& ch);
void toggleReadActions_(channel::Data& ch);
void startReadActions_ (channel::Data& ch);
void stopReadActions_  (channel::Data& ch, ChannelStatus curRecStatus);
void killReadActions_  (channel::Data& ch);
bool canRecord_        (const channel::Data& ch);
void pumpRecStatus_    (channel::Data& ch, ChannelStatus s);


/* -------------------------------------------------------------------------- */


bool canRecord_(const channel::Data& ch)
{
	return recManager::isRecordingAction() && 
	       clock::isRunning()              && 
	       !recManager::isRecordingInput() &&
		   !ch.samplePlayer->isAnyLoopMode();
}


/* -------------------------------------------------------------------------- */


void onKeyPress_(channel::Data& ch)
{
	if (!canRecord_(ch)) 
		return;
	record_(ch, MidiEvent::NOTE_ON);

	/* Skip reading actions when recording on ChannelMode::SINGLE_PRESS to 
	prevent	existing actions to interfere with the keypress/keyrel combo. */

	if (ch.samplePlayer->mode == SamplePlayerMode::SINGLE_PRESS)
		ch.readActions = false;
}


/* -------------------------------------------------------------------------- */


void pumpRecStatus_(ChannelStatus s)
{

}

/* -------------------------------------------------------------------------- */


void record_(channel::Data& ch, int note)
{
	recorderHandler::liveRec(ch.id, MidiEvent(note, 0, 0), 
		clock::quantize(clock::getCurrentFrame()));

	ch.hasActions = true;
}


/* -------------------------------------------------------------------------- */


void toggleReadActions_(channel::Data& ch)
{
	/* When you start reading actions while conf::treatRecsAsLoops is true, the
	value ch.state->readActions actually is not set to true immediately, because
	the channel is in wait mode (REC_WAITING). readActions will become true on
	the next first beat. So a 'stop rec' command should occur also when
	readActions is false but the channel is in wait mode; this check will
	handle the case of when you press 'R', the channel goes into REC_WAITING and
	then you press 'R' again to undo the status. */

	if (!ch.hasActions)
		return;

	bool          readActions = ch.readActions;
	ChannelStatus recStatus   = ch.recStatus;

	if (readActions || (!readActions && recStatus == ChannelStatus::WAIT))
		stopReadActions_(ch, recStatus);
	else
		startReadActions_(ch);
}


/* -------------------------------------------------------------------------- */


void startReadActions_(channel::Data& ch)
{
	if (conf::conf.treatRecsAsLoops)
		ch.recStatus = ChannelStatus::WAIT;
	else {
    	ch.recStatus = ChannelStatus::PLAY;
		ch.readActions = true;
	}
}


/* -------------------------------------------------------------------------- */


void stopReadActions_(channel::Data& ch, ChannelStatus curRecStatus)
{
	/* First of all, if the clock is not running or treatRecsAsLoops is off, 
	just stop and disable everything. Otherwise make sure a channel with actions
	behave like a dynamic one. */

	if (!clock::isRunning() || !conf::conf.treatRecsAsLoops) {
        ch.recStatus = ChannelStatus::OFF;
	    ch.readActions = false;
	}
	else
	if (curRecStatus == ChannelStatus::WAIT)
        ch.recStatus = ChannelStatus::OFF;
	else
	if (curRecStatus == ChannelStatus::ENDING)
        ch.recStatus = ChannelStatus::PLAY;
	else
        ch.recStatus = ChannelStatus::ENDING;
}


/* -------------------------------------------------------------------------- */


void killReadActions_(channel::Data& ch)
{
	/* Killing Read Actions, i.e. shift + click on 'R' button is meaninful only 
	when the conf::treatRecsAsLoops is true. */

	if (!conf::conf.treatRecsAsLoops)
		return;
    ch.recStatus = ChannelStatus::OFF;
	ch.readActions = false;
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void react(channel::Data& ch, const eventDispatcher::Event& e)
{
	switch (e.type) {

		case eventDispatcher::EventType::KEY_PRESS:
			onKeyPress_(ch); break;

		/* Record a stop event only if channel is SINGLE_PRESS. For any other 
		mode the key release event is meaningless. */

		case eventDispatcher::EventType::KEY_RELEASE:
			if (canRecord_(ch) && ch.samplePlayer->mode == SamplePlayerMode::SINGLE_PRESS) 
				record_(ch, MidiEvent::NOTE_OFF);
			break;

		case eventDispatcher::EventType::KEY_KILL:
			if (canRecord_(ch)) 
				record_(ch, MidiEvent::NOTE_KILL);
			break;

		case eventDispatcher::EventType::CHANNEL_TOGGLE_READ_ACTIONS:	
			toggleReadActions_(ch); break;        

		case eventDispatcher::EventType::CHANNEL_KILL_READ_ACTIONS:
			killReadActions_(ch); break;      
		
		default: break;
	}
}
}