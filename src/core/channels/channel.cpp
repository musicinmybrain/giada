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
#include "core/channels/state.h"
#include "core/mixerHandler.h"
#include "core/plugins/pluginManager.h"
#include "core/plugins/pluginHost.h"
#include "channel.h"


namespace giada::m 
{
Channel::Channel(ChannelType type, ID id, ID columnId, Frame bufferSize, const conf::Conf& conf)
: id         (id)
, state      (std::make_unique<ChannelState>(id, bufferSize))
, midiLighter(state.get())
, m_type     (type)
, m_columnId (columnId)
{
	switch (m_type) {

		case ChannelType::SAMPLE:
			samplePlayer.emplace(state.get());
			audioReceiver.emplace(state.get(), conf);
			sampleActionRecorder.emplace(state.get(), samplePlayer->state.get());	
			break;
		
		case ChannelType::PREVIEW:
			samplePlayer.emplace(state.get());
			break;
		
		case ChannelType::MIDI:
			midiController.emplace(state.get());
#ifdef WITH_VST
			midiReceiver.emplace(state.get());
#endif
			midiSender.emplace(state.get());
			midiActionRecorder.emplace(state.get());		
			break;	
		
		default: break;
	}
}


/* -------------------------------------------------------------------------- */


Channel::Channel(const Channel& o)
: id            (o.id)
#ifdef WITH_VST
, pluginIds     (o.pluginIds)
#endif
, state         (std::make_unique<ChannelState>(*o.state))
, midiLearner   (o.midiLearner)
, midiLighter   (o.midiLighter, state.get())
, m_type        (o.m_type)
, m_columnId    (o.m_columnId)
{
	switch (m_type) {

		case ChannelType::SAMPLE:
			samplePlayer.emplace(o.samplePlayer.value(), state.get());
			audioReceiver.emplace(o.audioReceiver.value(), state.get());
			sampleActionRecorder.emplace(o.sampleActionRecorder.value(), state.get(), samplePlayer->state.get());
			break;
		
		case ChannelType::PREVIEW:
			samplePlayer.emplace(o.samplePlayer.value(), state.get());
			break;
		
		case ChannelType::MIDI:
			midiController.emplace(o.midiController.value(), state.get());
#ifdef WITH_VST
			midiReceiver.emplace(o.midiReceiver.value(), state.get());
#endif
			midiSender.emplace(o.midiSender.value(), state.get());
			midiActionRecorder.emplace(o.midiActionRecorder.value(), state.get());
			break;

		default: break;
	}
}


/* -------------------------------------------------------------------------- */


Channel::Channel(const patch::Channel& p, Frame bufferSize)
: id            (p.id)
#ifdef WITH_VST
, pluginIds     (p.pluginIds)
#endif
, state         (std::make_unique<ChannelState>(p, bufferSize))
, midiLearner   (p)
, midiLighter   (p, state.get())
, m_type        (p.type)
, m_columnId    (p.columnId)
{
	switch (m_type) {

		case ChannelType::SAMPLE:
			samplePlayer.emplace(p, state.get());
			audioReceiver.emplace(p, state.get());
			sampleActionRecorder.emplace(state.get(), samplePlayer->state.get());
			break;
		
		case ChannelType::PREVIEW:
			samplePlayer.emplace(p, state.get());
			break;
		
		case ChannelType::MIDI:
			midiController.emplace(state.get());
#ifdef WITH_VST
			midiReceiver.emplace(p, state.get());
#endif
			midiSender.emplace(p, state.get());
			midiActionRecorder.emplace(state.get());	
			break;	
		
		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void Channel::parse(const mixer::EventBuffer& events, bool audible) const
{
	for (const mixer::Event& e : events) {

		if (e.action.channelId > 0 && e.action.channelId != id)
			continue;

		parse(e);
		midiLighter.parse(e, audible);

		if (midiController)       midiController->parse(e);
#ifdef WITH_VST
		if (midiReceiver)         midiReceiver->parse(e);
#endif
		if (midiSender)           midiSender->parse(e);
		if (samplePlayer)         samplePlayer->parse(e);
		if (midiActionRecorder)   midiActionRecorder->parse(e);
  		if (sampleActionRecorder && samplePlayer && samplePlayer->hasWave()) 
			sampleActionRecorder->parse(e);
	}
}


/* -------------------------------------------------------------------------- */


void Channel::advance(Frame bufferSize) const
{
	/* TODO - this is used only to advance samplePlayer for its quantizer. Use
	this to render actions in the future. */

	if (samplePlayer) samplePlayer->advance(bufferSize);
}


/* -------------------------------------------------------------------------- */


void Channel::render(AudioBuffer* out, AudioBuffer* in, bool audible) const
{
	if (id == mixer::MASTER_OUT_CHANNEL_ID)
		renderMasterOut(*out);
	else
	if (id == mixer::MASTER_IN_CHANNEL_ID)
		renderMasterIn(*in);
	else 
		renderChannel(*out, *in, audible);
}


/* -------------------------------------------------------------------------- */


void Channel::parse(const mixer::Event& e) const
{
	switch (e.type) {

		case mixer::EventType::CHANNEL_VOLUME:
			state->volume.store(e.action.event.getVelocityFloat()); break;

		case mixer::EventType::CHANNEL_PAN:
			state->pan.store(e.action.event.getVelocityFloat()); break;

		case mixer::EventType::CHANNEL_MUTE:
			state->mute.store(!state->mute.load()); break;

		case mixer::EventType::CHANNEL_TOGGLE_ARM:
			state->armed.store(!state->armed.load()); break;
			
		case mixer::EventType::CHANNEL_SOLO:
			state->solo.store(!state->solo.load()); 
			m::mh::updateSoloCount(); 
			break;

		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void Channel::renderMasterOut(AudioBuffer& out) const
{
	state->buffer.copyData(out);
#ifdef WITH_VST
	/*if (pluginIds.size() > 0)
		pluginHost::processStack(state->buffer, pluginIds, nullptr);*/
#endif
	out.copyData(state->buffer, state->volume.load());
}


/* -------------------------------------------------------------------------- */


void Channel::renderMasterIn(AudioBuffer& in) const
{
#ifdef WITH_VST
	/*if (pluginIds.size() > 0)
		pluginHost::processStack(in, pluginIds, nullptr);*/
#endif
}


/* -------------------------------------------------------------------------- */


void Channel::renderChannel(AudioBuffer& out, AudioBuffer& in, bool audible) const
{
	state->buffer.clear();

	if (samplePlayer)  samplePlayer->render(out);
	if (audioReceiver) audioReceiver->render(in);

	/* If MidiReceiver exists, let it process the plug-in stack, as it can 
	contain plug-ins that take MIDI events (i.e. synths). Otherwise process the
	plug-in stack internally with no MIDI events. */

#ifdef WITH_VST
	/*if (midiReceiver)
		midiReceiver->render(pluginIds); */
	/*else
	if (pluginIds.size() > 0)
		pluginHost::processStack(state->buffer, pluginIds, nullptr);*/
#endif

	if (audible)
	    out.addData(state->buffer, state->volume.load() * state->volume_i, calcPanning());
}


/* -------------------------------------------------------------------------- */


AudioBuffer::Pan Channel::calcPanning() const
{
	/* TODO - precompute the AudioBuffer::Pan when pan value changes instead of
	building it on the fly. */
	
	float pan = state->pan.load();

	/* Center pan (0.5f)? Pass-through. */

	if (pan == 0.5f) return { 1.0f, 1.0f };
	return { 1.0f - pan, pan };
}


/* -------------------------------------------------------------------------- */


ID Channel::getColumnId() const { return m_columnId; }
ChannelType Channel::getType() const { return m_type; }


/* -------------------------------------------------------------------------- */


bool Channel::isInternal() const
{
	return m_type == ChannelType::MASTER || m_type == ChannelType::PREVIEW;
}


bool Channel::isMuted() const
{
	/* Internals can't be muted. */
	return !isInternal() && state->mute.load() == true;
}


bool Channel::canInputRec() const
{
	if (m_type != ChannelType::SAMPLE)
		return false;

	bool armed       = state->armed.load();
	bool hasWave     = samplePlayer->hasWave();
	bool isProtected = audioReceiver->state->overdubProtection.load();
	bool canOverdub  = !hasWave || (hasWave && !isProtected);

	return armed && canOverdub;
}


bool Channel::canActionRec() const
{
	return hasWave() && !samplePlayer->state->isAnyLoopMode();
}


bool Channel::hasWave() const
{
	return m_type == ChannelType::SAMPLE && samplePlayer->hasWave();
}




























Channel_NEW::Data::Data(Frame bufferSize) 
: audioBuffer(bufferSize, G_MAX_IO_CHANS)
{
}

Channel_NEW::Channel_NEW(ChannelType type, ID id, ID columnId, 
	Channel_NEW::State& state, Channel_NEW::Data& data)
: state      (&state)
, data       (&data)
, id         (id)
, type       (type)
, columnId   (columnId)
, volume     (G_DEFAULT_VOL)
, volume_i   (G_DEFAULT_VOL)
, pan        (G_DEFAULT_PAN)
, mute       (false)
, solo       (false)
, armed      (false)
, key        (0)
, readActions(true)
, hasActions (false)
, height     (G_GUI_UNIT)
{
	switch (type) {

		case ChannelType::SAMPLE:
			samplePlayer.emplace();
			audioReceiver.emplace();
			sampleActionRecorder.emplace();
			break;
		
		case ChannelType::PREVIEW:
			samplePlayer.emplace();
			break;
		case ChannelType::MIDI:
			midiController.emplace();
#ifdef WITH_VST
			midiReceiver.emplace();
#endif
			midiSender.emplace();
			midiActionRecorder.emplace();		
			break;	
		
		default: break;
	}
}


/* -------------------------------------------------------------------------- */


Channel_NEW::Channel_NEW(const patch::Channel& p, Channel_NEW::State& state, 
	Channel_NEW::Data& data, float samplerateRatio)
: state         (&state)
, data          (&data)
, id            (p.id)
, type          (p.type)
, columnId      (p.columnId)
, volume        (p.volume)
, volume_i      (0.0f)
, pan           (p.pan)
, mute          (p.mute)
, solo          (p.solo)
, armed         (p.armed)
, key           (p.key)
, readActions   (p.readActions)
, hasActions    (p.hasActions)
, name          (p.name)
, height        (p.height)
#ifdef WITH_VST
, plugins       (pluginManager::hydratePlugins(p.pluginIds))
#endif
, midiLearner   (p)
{
	switch (type) {

		case ChannelType::SAMPLE:
			samplePlayer.emplace(p, samplerateRatio);
			audioReceiver.emplace(p);
			sampleActionRecorder.emplace();
			break;
		
		case ChannelType::PREVIEW:
			samplePlayer.emplace(p, samplerateRatio);
			break;

		case ChannelType::MIDI:
			midiController.emplace();
#ifdef WITH_VST
			midiReceiver.emplace();
#endif
			midiSender.emplace(p);
			midiActionRecorder.emplace();	
			break;	
		
		default: break;
	}
}


/* -------------------------------------------------------------------------- */


Channel_NEW::Channel_NEW(const Channel_NEW& o)
: state         (o.state)
, data          (o.data)
, id            (o.id)
, type          (o.type)
, columnId      (o.columnId)
, volume        (o.volume)
, volume_i      (0.0f)
, pan           (o.pan)
, mute          (o.mute)
, solo          (o.solo)
, armed         (o.armed)
, key           (o.key)
, readActions   (o.readActions)
, hasActions    (o.hasActions)
, name          (o.name)
, height        (o.height)
#ifdef WITH_VST
, plugins       (o.plugins)
#endif
, midiLearner   (o.midiLearner)
, midiLighter   (o.midiLighter)
{

	if (o.samplePlayer)         samplePlayer.emplace(o.samplePlayer.value());
	if (o.audioReceiver)        audioReceiver.emplace(o.audioReceiver.value());
	if (o.midiController)       midiController.emplace();
	if (o.midiReceiver)         midiReceiver.emplace();
	if (o.midiSender)           midiSender.emplace(o.midiSender.value());
	if (o.sampleActionRecorder) sampleActionRecorder.emplace();
	if (o.midiActionRecorder)   midiActionRecorder.emplace();
}


/* -------------------------------------------------------------------------- */


void Channel_NEW::react(const eventDispatcher::EventBuffer& events, bool audible)
{
	for (const eventDispatcher::Event& e : events) {

		if (e.channelId > 0 && e.channelId != id)
			continue;

        react(e);
        //midiLighter::react(*this, e, audible);

        //if (midiController)       midiController::react(*this, e);
	#ifdef WITH_VST
		//if (midiReceiver)         midiReceiver::react(*this, e);
	#endif
		//if (midiSender)           midiSender::react(*this, e);
		//if (samplePlayer)         samplePlayer::react(*this, e);
		//if (midiActionRecorder)   midiActionRecorder::react(*this, e);
		//if (sampleActionRecorder && samplePlayer && samplePlayer->hasWave())
		//	sampleActionRecorder::react(*this, e);
	}
}


/* -------------------------------------------------------------------------- */


void Channel_NEW::advance(const sequencer::EventBuffer& events) const
{
	for (const sequencer::Event& e : events) {
		//if (midiController)       midiController::advance(*this, e);
	#ifdef WITH_VST
		//if (midiReceiver)         midiReceiver::advance(*this, e);
	#endif
		//if (samplePlayer)         samplePlayer::advance(*this, e);
	}
}


/* -------------------------------------------------------------------------- */


void Channel_NEW::render(AudioBuffer* out, AudioBuffer* in, bool audible) const
{
	if (id == mixer::MASTER_OUT_CHANNEL_ID)
		renderMasterOut(*out);
	else
	if (id == mixer::MASTER_IN_CHANNEL_ID)
		renderMasterIn(*in);
	else 
		renderChannel(*out, *in, audible);
}


/* -------------------------------------------------------------------------- */


void Channel_NEW::react(const eventDispatcher::Event& e)
{
	switch (e.type) {

		case eventDispatcher::EventType::CHANNEL_VOLUME:
			volume = std::get<float>(e.data); break;

		case eventDispatcher::EventType::CHANNEL_PAN:
			pan = std::get<float>(e.data); break;

		case eventDispatcher::EventType::CHANNEL_MUTE:
			mute = !mute; break;

		case eventDispatcher::EventType::CHANNEL_TOGGLE_ARM:
			armed = !armed; break;
			
		case eventDispatcher::EventType::CHANNEL_SOLO:
			solo = !solo; 
			m::mh::updateSoloCount(); 
			break;

        case eventDispatcher::EventType::CHANNEL_REC_STATUS:
            recStatus = std::get<ChannelStatus>(e.data);
            break;
		
		case eventDispatcher::EventType::CHANNEL_SET_ACTIONS:
			hasActions = std::get<bool>(e.data);
			break;
		
		case eventDispatcher::EventType::CHANNEL_FUNCTION:
			std::get<std::function<void(Channel_NEW&)>>(e.data)(*this);
			break;

		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void Channel_NEW::renderMasterOut(AudioBuffer& out) const
{
	data->audioBuffer.copyData(out);
#ifdef WITH_VST
	if (plugins.size() > 0)
		pluginHost::processStack(data->audioBuffer, plugins, nullptr);
#endif
	out.copyData(data->audioBuffer, volume);
}


/* -------------------------------------------------------------------------- */


void Channel_NEW::renderMasterIn(AudioBuffer& in) const
{
#ifdef WITH_VST
	if (plugins.size() > 0)
		pluginHost::processStack(in, plugins, nullptr);
#endif
}


/* -------------------------------------------------------------------------- */


void Channel_NEW::renderChannel(AudioBuffer& out, AudioBuffer& in, bool audible) const
{
	data->audioBuffer.clear();

	//if (samplePlayer)  samplePlayer::render(*this, out);
	//if (audioReceiver) audioReceiver::render(*this, in);

	/* If MidiReceiver exists, let it process the plug-in stack, as it can 
	contain plug-ins that take MIDI events (i.e. synths). Otherwise process the
	plug-in stack internally with no MIDI events. */

#ifdef WITH_VST
	//if (midiReceiver)
	//	midiReceiver::render(*this, plugins);
	//else
	//if (plugins.size() > 0)
	//	pluginHost::processStack(data->audioBuffer, plugins, nullptr);
#endif

	if (audible)
	    out.addData(data->audioBuffer, volume * volume_i, calcPanning());
}


/* -------------------------------------------------------------------------- */


AudioBuffer::Pan Channel_NEW::calcPanning() const
{
	/* TODO - precompute the AudioBuffer::Pan when pan value changes instead of
	building it on the fly. */

	/* Center pan (0.5f)? Pass-through. */

	if (pan == 0.5f) return { 1.0f, 1.0f };
	return { 1.0f - pan, pan };
}


/* -------------------------------------------------------------------------- */


bool Channel_NEW::isInternal() const
{
	return type == ChannelType::MASTER || type == ChannelType::PREVIEW;
}


bool Channel_NEW::isMuted() const
{
	/* Internals can't be muted. */
	return !isInternal() && mute;
}


bool Channel_NEW::canInputRec() const
{
	if (type != ChannelType::SAMPLE)
		return false;

	bool hasWave     = samplePlayer->hasWave();
	bool isProtected = audioReceiver->overdubProtection;
	bool canOverdub  = !hasWave || (hasWave && !isProtected);

	return armed && canOverdub;
}


bool Channel_NEW::canActionRec() const
{
	return hasWave() && !samplePlayer->isAnyLoopMode();
}


bool Channel_NEW::hasWave() const
{
	return type == ChannelType::SAMPLE && samplePlayer->hasWave();
}

bool Channel_NEW::isPlaying() const
{
	return playStatus == ChannelStatus::PLAY || playStatus == ChannelStatus::ENDING;
}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void pumpChannelFunction(ID channelId, std::function<void(Channel_NEW&)> f)
{
	eventDispatcher::pumpEvent({ eventDispatcher::EventType::CHANNEL_FUNCTION, 
		0, channelId, f });
}
} // giada::m::



















namespace giada::m::channel
{
namespace
{
AudioBuffer::Pan calcPanning_(float pan)
{
	/* TODO - precompute the AudioBuffer::Pan when pan value changes instead of
	building it on the fly. */

	/* Center pan (0.5f)? Pass-through. */

	if (pan == 0.5f) return { 1.0f, 1.0f };
	return { 1.0f - pan, pan };
}


/* -------------------------------------------------------------------------- */


void react_(Data& d, const eventDispatcher::Event& e)
{
	switch (e.type) {

		case eventDispatcher::EventType::CHANNEL_VOLUME:
			d.volume = std::get<float>(e.data); break;

		case eventDispatcher::EventType::CHANNEL_PAN:
			d.pan = std::get<float>(e.data); break;

		case eventDispatcher::EventType::CHANNEL_MUTE:
			d.mute = !d.mute; break;

		case eventDispatcher::EventType::CHANNEL_TOGGLE_ARM:
			d.armed = !d.armed; break;
			
		case eventDispatcher::EventType::CHANNEL_SOLO:
			d.solo = !d.solo;
			m::mh::updateSoloCount(); 
			break;

        case eventDispatcher::EventType::CHANNEL_REC_STATUS:
            d.state->recStatus.store(std::get<ChannelStatus>(e.data));
            break;
		
		case eventDispatcher::EventType::CHANNEL_SET_ACTIONS:
			d.hasActions = std::get<bool>(e.data);
			break;
		
		case eventDispatcher::EventType::CHANNEL_FUNCTION:
			//TODO - std::get<std::function<void(Channel_NEW&)>>(e.data)(*this);
			break;

		default: break;
	}
}


/* -------------------------------------------------------------------------- */


void renderMasterOut_(const Data& d, AudioBuffer& out)
{
	d.buffer->audioBuffer.copyData(out);
#ifdef WITH_VST
	if (d.plugins.size() > 0)
		pluginHost::processStack(d.buffer->audioBuffer, d.plugins, nullptr);
#endif
	out.copyData(d.buffer->audioBuffer, d.volume);

}


/* -------------------------------------------------------------------------- */


void renderMasterIn_(const Data& d, AudioBuffer& in)
{
#ifdef WITH_VST
	if (d.plugins.size() > 0)
		pluginHost::processStack(in, d.plugins, nullptr);
#endif
}


/* -------------------------------------------------------------------------- */


void renderChannel_(const Data& d, AudioBuffer& out, AudioBuffer& in, bool audible)
{
	d.buffer->audioBuffer.clear();

	if (d.samplePlayer)  samplePlayer::render(d, out);
	if (d.audioReceiver) audioReceiver::render(d, in);

	/* If MidiReceiver exists, let it process the plug-in stack, as it can 
	contain plug-ins that take MIDI events (i.e. synths). Otherwise process the
	plug-in stack internally with no MIDI events. */

#ifdef WITH_VST
	if (d.midiReceiver)  
		midiReceiver::render(d, d.plugins);
	else 
	if (d.plugins.size() > 0)
		pluginHost::processStack(d.buffer->audioBuffer, d.plugins, nullptr);
#endif

	if (audible)
	    out.addData(d.buffer->audioBuffer, d.volume * d.volume_i, calcPanning_(d.pan));
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


Buffer::Buffer(Frame bufferSize)
: audioBuffer(bufferSize, G_MAX_IO_CHANS)
{
}


/* -------------------------------------------------------------------------- */


Data::Data(ChannelType type, ID id, ID columnId, State& state, Buffer& buffer)
: state      (&state)
, buffer     (&buffer)
, id         (id)
, type       (type)
, columnId   (columnId)
, volume     (G_DEFAULT_VOL)
, volume_i   (G_DEFAULT_VOL)
, pan        (G_DEFAULT_PAN)
, mute       (false)
, solo       (false)
, armed      (false)
, key        (0)
, readActions(true)
, hasActions (false)
, height     (G_GUI_UNIT)
{
	switch (type) {

		case ChannelType::SAMPLE:
			samplePlayer.emplace();
			audioReceiver.emplace();
			sampleActionRecorder.emplace();
			break;
		
		case ChannelType::PREVIEW:
			samplePlayer.emplace();
			break;

		case ChannelType::MIDI:
			midiController.emplace();
#ifdef WITH_VST
			midiReceiver.emplace();
#endif
			midiSender.emplace();
			midiActionRecorder.emplace();		
			break;	
		
		default: break;
	}
}


/* -------------------------------------------------------------------------- */


Data::Data(const patch::Channel& p, State& state, Buffer& buffer, float samplerateRatio)
: state         (&state)
, buffer        (&buffer)
, id            (p.id)
, type          (p.type)
, columnId      (p.columnId)
, volume        (p.volume)
, volume_i      (0.0f)
, pan           (p.pan)
, mute          (p.mute)
, solo          (p.solo)
, armed         (p.armed)
, key           (p.key)
, readActions   (p.readActions)
, hasActions    (p.hasActions)
, name          (p.name)
, height        (p.height)
#ifdef WITH_VST
, plugins       (pluginManager::hydratePlugins(p.pluginIds))
#endif
, midiLearner   (p)
{
	switch (type) {

		case ChannelType::SAMPLE:
			samplePlayer.emplace(p, samplerateRatio);
			audioReceiver.emplace(p);
			sampleActionRecorder.emplace();
			break;
		
		case ChannelType::PREVIEW:
			samplePlayer.emplace(p, samplerateRatio);
			break;

		case ChannelType::MIDI:
			midiController.emplace();
#ifdef WITH_VST
			midiReceiver.emplace();
#endif
			midiSender.emplace(p);
			midiActionRecorder.emplace();	
			break;	
		
		default: break;
	}
}



/* -------------------------------------------------------------------------- */


bool Data::isInternal() const
{
	return type == ChannelType::MASTER || type == ChannelType::PREVIEW;
}


bool Data::isMuted() const
{
	/* Internals can't be muted. */
	return !isInternal() && mute;
}


bool Data::canInputRec() const
{
	if (type != ChannelType::SAMPLE)
		return false;

	bool hasWave     = samplePlayer->hasWave();
	bool isProtected = audioReceiver->overdubProtection;
	bool canOverdub  = !hasWave || (hasWave && !isProtected);

	return armed && canOverdub;
}


bool Data::canActionRec() const
{
	return hasWave() && !samplePlayer->isAnyLoopMode();
}


bool Data::hasWave() const
{
	return type == ChannelType::SAMPLE && samplePlayer->hasWave();
}

bool Data::isPlaying() const
{
    ChannelStatus s = state->playStatus.load();
	return s == ChannelStatus::PLAY || s == ChannelStatus::ENDING;
}


/* -------------------------------------------------------------------------- */


void advance(const Data& d, const sequencer::EventBuffer& events)
{
	for (const sequencer::Event& e : events) {
		if (d.midiController) midiController::advance(d, e);
	#ifdef WITH_VST
		if (d.midiReceiver)   midiReceiver::advance(d, e);
	#endif
		if (d.samplePlayer)   samplePlayer::advance(d, e);
	}
}


/* -------------------------------------------------------------------------- */


void react(Data& d, const eventDispatcher::EventBuffer& events, bool audible)
{
	for (const eventDispatcher::Event& e : events) {

		if (e.channelId > 0 && e.channelId != d.id)
			continue;

        react_(d, e);
        midiLighter::react(d, e, audible);

		if (d.midiController)       midiController::react(d, e);
#ifdef WITH_VST
		if (d.midiReceiver)         midiReceiver::react(d, e);
#endif
		if (d.midiSender)           midiSender::react(d, e);
		if (d.samplePlayer)         samplePlayer::react(d, e);
		if (d.midiActionRecorder)   midiActionRecorder::react(d, e);
		if (d.sampleActionRecorder && d.samplePlayer && d.samplePlayer->hasWave()) 
			sampleActionRecorder::react(d, e);
	}
}


/* -------------------------------------------------------------------------- */


void render(const Data& d, AudioBuffer* out, AudioBuffer* in, bool audible)
{
	if (d.id == mixer::MASTER_OUT_CHANNEL_ID)
		renderMasterOut_(d, *out);
	else
	if (d.id == mixer::MASTER_IN_CHANNEL_ID)
		renderMasterIn_(d, *in);
	else 
		renderChannel_(d, *out, *in, audible);
}
}