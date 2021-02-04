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


#include "core/channels/state.h"
#include "core/channels/channel.h"
#include "audioReceiver.h"


namespace giada {
namespace m 
{
AudioReceiver::AudioReceiver(ChannelState* c, const conf::Conf& conf)
: state         (std::make_unique<AudioReceiverState>(conf))
, m_channelState(c)
{
}


/* -------------------------------------------------------------------------- */


AudioReceiver::AudioReceiver(const patch::Channel& p, ChannelState* c)
: state         (std::make_unique<AudioReceiverState>(p))
, m_channelState(c)
{
}


/* -------------------------------------------------------------------------- */


AudioReceiver::AudioReceiver(const AudioReceiver& o, ChannelState* c)
: state         (std::make_unique<AudioReceiverState>(*o.state))
, m_channelState(c)
{
}


/* -------------------------------------------------------------------------- */


void AudioReceiver::render(const AudioBuffer& in) const
{
	/* If armed and input monitor is on, copy input buffer to channel buffer: 
	this enables the input monitoring. The channel buffer will be overwritten 
	later on by pluginHost::processStack, so that you would record "clean" audio 
	(i.e. not plugin-processed). */

	bool armed        = m_channelState->armed.load();
	bool inputMonitor = state->inputMonitor.load();

	if (armed && inputMonitor)
		m_channelState->buffer.addData(in);  // add, don't overwrite
}























AudioReceiver_NEW::AudioReceiver_NEW(Channel_NEW& c)
: m_channel(c)
{
}


/* -------------------------------------------------------------------------- */


AudioReceiver_NEW::AudioReceiver_NEW(const patch::Channel& p, Channel_NEW& c)
: inputMonitor     (p.inputMonitor)
, overdubProtection(p.overdubProtection)
, m_channel        (c)
{
}


/* -------------------------------------------------------------------------- */


AudioReceiver_NEW::AudioReceiver_NEW(const AudioReceiver_NEW& o, Channel_NEW* c)
: inputMonitor     (o.inputMonitor)
, overdubProtection(o.overdubProtection)
, m_channel        (*c)
{
	assert(c != nullptr);
}


/* -------------------------------------------------------------------------- */


void AudioReceiver_NEW::render(const AudioBuffer& in) const
{
	/* If armed and input monitor is on, copy input buffer to channel buffer: 
	this enables the input monitoring. The channel buffer will be overwritten 
	later on by pluginHost::processStack, so that you would record "clean" audio 
	(i.e. not plugin-processed). */

	if (m_channel.armed && inputMonitor)
		m_channel.data->audioBuffer.addData(in);  // add, don't overwrite
}
}} // giada::m::









namespace giada::m::audioReceiver
{
Data::Data(const patch::Channel& p)
: inputMonitor     (p.inputMonitor)
, overdubProtection(p.overdubProtection)
{
}


/* -------------------------------------------------------------------------- */


void render(const channel::Data& ch, const AudioBuffer& in)
{
	/* If armed and input monitor is on, copy input buffer to channel buffer: 
	this enables the input monitoring. The channel buffer will be overwritten 
	later on by pluginHost::processStack, so that you would record "clean" audio 
	(i.e. not plugin-processed). */

	if (ch.armed && ch.audioReceiver->inputMonitor)
		ch.buffer->audioBuffer.addData(in);  // add, don't overwrite
}
}