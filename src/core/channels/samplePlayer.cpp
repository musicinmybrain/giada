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


#include <algorithm>
#include <cassert>
#include "core/channels/channel.h"
#include "core/channels/state.h"
#include "core/wave.h"
#include "core/clock.h"
#include "core/waveManager.h"
#include "samplePlayer.h"


namespace giada::m 
{
SamplePlayer::SamplePlayer(ChannelState* c)
: state             (std::make_unique<SamplePlayerState>())
, m_waveId          (0)
, m_sampleController(c, state.get())
, m_channelState    (c)
{
}


/* -------------------------------------------------------------------------- */


SamplePlayer::SamplePlayer(const SamplePlayer& o, ChannelState* c)
: state             (std::make_unique<SamplePlayerState>(*o.state))
, m_waveId          (o.m_waveId)
, m_waveReader      (o.m_waveReader)
, m_sampleController(o.m_sampleController, c, state.get())
, m_channelState    (c)
{
}


/* -------------------------------------------------------------------------- */


SamplePlayer::SamplePlayer(const patch::Channel& p, ChannelState* c)
: state             (std::make_unique<SamplePlayerState>(p))
, m_waveId          (p.waveId)
, m_sampleController(c, state.get())
, m_channelState    (c)
{
}


/* -------------------------------------------------------------------------- */


void SamplePlayer::parse(const mixer::Event& e) const
{
    if (e.type == mixer::EventType::CHANNEL_PITCH)
        state->pitch.store(e.action.event.getVelocityFloat());

    if (hasWave())
        m_sampleController.parse(e);
}


/* -------------------------------------------------------------------------- */


void SamplePlayer::advance(Frame bufferSize) const
{
    m_sampleController.advance(bufferSize);
}


/* -------------------------------------------------------------------------- */


void SamplePlayer::render(AudioBuffer& /*out*/) const
{
    assert(m_channelState != nullptr);

    if (m_waveReader.wave == nullptr || !m_channelState->isPlaying())
        return;

    Frame begin   = state->begin.load();
    Frame end     = state->end.load();
    Frame tracker = state->tracker.load();
    float pitch   = state->pitch.load();
    Frame used    = 0;

    /* Audio data is temporarily stored to the working audio buffer. */

    AudioBuffer& buffer = m_channelState->buffer;

    /* Adjust tracker in case someone has changed the begin/end points in the
    meantime. */
    
    if (tracker < begin || tracker >= end)
        tracker = begin;

    /* If rewinding, fill the tail first, then reset the tracker to the begin
    point. The rest is performed as usual. */

    if (state->rewinding) {
		if (tracker < end)
            m_waveReader.fill(buffer, tracker, 0, pitch);
        state->rewinding = false;
		tracker = begin;
    }

    used     = m_waveReader.fill(buffer, tracker, state->offset, pitch);
    tracker += used;

G_DEBUG ("block=[" << tracker - used << ", " << tracker << ")" << 
         ", used=" << used << ", range=[" << begin << ", " << end << ")" <<
         ", tracker=" << tracker << 
         ", offset=" << state->offset << ", globalFrame=" << clock::getCurrentFrame());

    if (tracker >= end) {
G_DEBUG ("last frame tracker=" << tracker);
        tracker = begin;
        m_sampleController.onLastFrame();
        if (shouldLoop()) {
            Frame offset = std::min(static_cast<Frame>(used / pitch), buffer.countFrames() - 1);
            tracker += m_waveReader.fill(buffer, tracker, offset, pitch);
        }
    }

    state->offset = 0;
    state->tracker.store(tracker);
}


/* -------------------------------------------------------------------------- */


void SamplePlayer::loadWave(const Wave* w)
{
    m_waveReader.wave = w;

    state->tracker.store(0);
    state->shift.store(0);
    state->begin.store(0);

    if (w != nullptr) {
        m_waveId = w->id;
        m_channelState->playStatus.store(ChannelStatus::OFF);
        m_channelState->name = w->getBasename(/*ext=*/false);
        state->end.store(w->getSize() - 1);
    }
    else {
        m_waveId = 0;
        m_channelState->playStatus.store(ChannelStatus::EMPTY);
        m_channelState->name = "";
        state->end.store(0);
    }
}


/* -------------------------------------------------------------------------- */


void SamplePlayer::setWave(const Wave& w, float samplerateRatio)
{
    m_waveReader.wave = &w;
    m_waveId = w.id;

    if (samplerateRatio != 1.0f) {
        Frame begin = state->begin.load();
        Frame end   = state->end.load();
        Frame shift = state->shift.load();
        state->begin.store(begin * samplerateRatio);
        state->end.store(end * samplerateRatio);
        state->shift.store(shift * samplerateRatio);
    }
}


void SamplePlayer::setInvalidWave()
{
    m_waveReader.wave = nullptr;
    m_waveId = 0;
}


/* -------------------------------------------------------------------------- */


void SamplePlayer::kickIn(Frame f)
{
    assert(hasWave());
    
	state->tracker.store(f);
	m_channelState->playStatus.store(ChannelStatus::PLAY);    
}


/* -------------------------------------------------------------------------- */


bool SamplePlayer::shouldLoop() const
{
    ChannelStatus    playStatus = m_channelState->playStatus.load();
    SamplePlayerMode mode       = state->mode.load();
    
    return (mode == SamplePlayerMode::LOOP_BASIC  || 
            mode == SamplePlayerMode::LOOP_REPEAT || 
            mode == SamplePlayerMode::SINGLE_ENDLESS) && playStatus == ChannelStatus::PLAY;
}


/* -------------------------------------------------------------------------- */


bool SamplePlayer::hasWave() const        { return m_waveReader.wave != nullptr; }
bool SamplePlayer::hasLogicalWave() const { return hasWave() && m_waveReader.wave->isLogical(); }
bool SamplePlayer::hasEditedWave() const  { return hasWave() && m_waveReader.wave->isEdited(); }


/* -------------------------------------------------------------------------- */


ID SamplePlayer::getWaveId() const
{
    return m_waveId;
}


/* -------------------------------------------------------------------------- */


Frame SamplePlayer::getWaveSize() const
{
    return hasWave() ? m_waveReader.wave->getSize() : 0;
}








































SamplePlayer_NEW::SamplePlayer_NEW(Channel_NEW& c)
: pitch             (G_DEFAULT_PITCH)
, mode              (SamplePlayerMode::SINGLE_BASIC)
, velocityAsVol     (false)
, m_waveId          (0)
//, m_sampleController(c, *this)
, m_sampleAdvancer  (c, *this)
, m_sampleReactor   (c, *this)
, m_channel         (&c)
{
}


/* -------------------------------------------------------------------------- */


SamplePlayer_NEW::SamplePlayer_NEW(const patch::Channel& p, Channel_NEW& c, float samplerateRatio)
: pitch             (p.pitch)
, mode              (p.mode)
, shift             (p.shift)
, begin             (p.begin)
, end               (p.end)
, velocityAsVol     (p.midiInVeloAsVol)
, quantizing        (false)
, m_waveId          (p.waveId)
, m_sampleAdvancer  (c, *this)
, m_sampleReactor   (c, *this)
, m_channel         (&c)
{
    setWave(waveManager::hydrateWave(p.waveId), samplerateRatio);
}


/* -------------------------------------------------------------------------- */


SamplePlayer_NEW::SamplePlayer_NEW(const SamplePlayer_NEW& o, Channel_NEW* c)
: pitch             (o.pitch)
, mode              (o.mode)
, shift             (o.shift)
, begin             (o.begin)
, end               (o.end)
, velocityAsVol     (o.velocityAsVol)
, quantizing        (false)
, m_waveId          (o.m_waveId)
, m_waveReader      (o.m_waveReader)
, m_sampleAdvancer  (*c, *this)
, m_sampleReactor   (*c, *this)
, m_channel         (c)
{
    assert(c != nullptr);
}


/* -------------------------------------------------------------------------- */


void SamplePlayer_NEW::react(const eventDispatcher::Event& e)
{
    if (e.type == eventDispatcher::EventType::CHANNEL_PITCH)
        pitch = std::get<Action>(e.data).event.getVelocityFloat();
    if (hasWave())
        m_sampleReactor.react(e);
}


/* -------------------------------------------------------------------------- */


void SamplePlayer_NEW::advance(const sequencer::Event& e) const
{
    m_sampleAdvancer.advance(e);
}


/* -------------------------------------------------------------------------- */


void SamplePlayer_NEW::render(AudioBuffer& /*out*/) const
{
    if (m_waveReader.wave == nullptr || !m_channel->isPlaying())
        return;

    Frame tracker = m_channel->state->tracker.load();
    Frame used    = 0;

    /* Audio data is temporarily stored to the working audio buffer. */

    AudioBuffer& buffer = m_channel->data->audioBuffer;

    /* Adjust tracker in case someone has changed the begin/end points in the
    meantime. */
    
    if (tracker < begin || tracker >= end)
        tracker = begin;

    /* If rewinding, fill the tail first, then reset the tracker to the begin
    point. The rest is performed as usual. */

    if (m_channel->state->rewinding) {
		if (tracker < end)
            m_waveReader.fill(buffer, tracker, 0, pitch);
        m_channel->state->rewinding = false;
		tracker = begin;
    }

    used     = m_waveReader.fill(buffer, tracker, m_channel->state->offset, pitch);
    tracker += used;

G_DEBUG ("block=[" << tracker - used << ", " << tracker << ")" << 
         ", used=" << used << ", range=[" << begin << ", " << end << ")" <<
         ", tracker=" << tracker << 
         ", offset=" << m_channel->state->offset << ", globalFrame=" << clock::getCurrentFrame());

    if (tracker >= end) {
G_DEBUG ("last frame tracker=" << tracker);
        tracker = begin;
        m_sampleAdvancer.onLastFrame(); // TODO - better moving this to samplerAdvancer::advance
        if (shouldLoop()) {
            Frame offset = std::min(static_cast<Frame>(used / pitch), buffer.countFrames() - 1);
            tracker += m_waveReader.fill(buffer, tracker, offset, pitch);
        }
    }

    m_channel->state->offset = 0;
    m_channel->state->tracker.store(tracker);
}


/* -------------------------------------------------------------------------- */


void SamplePlayer_NEW::loadWave(const Wave* w)
{
    //m_waveReader.wave = w;

    m_channel->state->tracker.store(0);
    shift = 0;
    begin = 0;

    if (w != nullptr) {
        m_waveId = w->id;
        m_channel->playStatus = ChannelStatus::OFF;
        m_channel->name = w->getBasename(/*ext=*/false);
        end = w->getSize() - 1;
    }
    else {
        m_waveId = 0;
        m_channel->playStatus = ChannelStatus::EMPTY;
        m_channel->name = "";
        end = 0;
    }
}


/* -------------------------------------------------------------------------- */


void SamplePlayer_NEW::setWave(const Wave* w, float samplerateRatio)
{
    if (w == nullptr) {
        m_waveReader.wave = nullptr;
        m_waveId = 0;
        return;
    }

    //m_waveReader.wave = w;
    m_waveId = w->id;

    if (samplerateRatio != 1.0f) {
        begin *= samplerateRatio;
        end   *= samplerateRatio;
        shift *= samplerateRatio;
    }
}


/* -------------------------------------------------------------------------- */


void SamplePlayer_NEW::kickIn(Frame f)
{
    assert(hasWave());
    
	m_channel->state->tracker.store(f);
    m_channel->playStatus = ChannelStatus::PLAY;
}


/* -------------------------------------------------------------------------- */


bool SamplePlayer_NEW::shouldLoop() const
{
    ChannelStatus playStatus = m_channel->playStatus;

    return (mode == SamplePlayerMode::LOOP_BASIC  || 
            mode == SamplePlayerMode::LOOP_REPEAT || 
            mode == SamplePlayerMode::SINGLE_ENDLESS) && playStatus == ChannelStatus::PLAY;
}


/* -------------------------------------------------------------------------- */


bool SamplePlayer_NEW::hasWave() const        { return m_waveReader.wave != nullptr; }
bool SamplePlayer_NEW::hasLogicalWave() const { return hasWave() && m_waveReader.wave->isLogical(); }
bool SamplePlayer_NEW::hasEditedWave() const  { return hasWave() && m_waveReader.wave->isEdited(); }


/* -------------------------------------------------------------------------- */


bool SamplePlayer_NEW::isAnyLoopMode() const
{
	return mode == SamplePlayerMode::LOOP_BASIC  || 
	       mode == SamplePlayerMode::LOOP_ONCE   || 
	       mode == SamplePlayerMode::LOOP_REPEAT || 
	       mode == SamplePlayerMode::LOOP_ONCE_BAR;
}


/* -------------------------------------------------------------------------- */


const Wave* SamplePlayer_NEW::getWave() const
{
    return m_waveReader.wave;
}


ID SamplePlayer_NEW::getWaveId() const
{
    return m_waveId;
}


/* -------------------------------------------------------------------------- */


Frame SamplePlayer_NEW::getTracker() const
{
    return m_channel->state->tracker.load();
}


/* -------------------------------------------------------------------------- */


Frame SamplePlayer_NEW::getWaveSize() const
{
    return hasWave() ? m_waveReader.wave->getSize() : 0;
}
} // giada::m::















namespace giada::m::samplePlayer
{
namespace
{
bool shouldLoop_(const channel::Data& ch)
{
    ChannelStatus    playStatus = ch.state->playStatus.load();
    SamplePlayerMode mode       = ch.samplePlayer->mode;
    
    return (mode == SamplePlayerMode::LOOP_BASIC  || 
            mode == SamplePlayerMode::LOOP_REPEAT || 
            mode == SamplePlayerMode::SINGLE_ENDLESS) && playStatus == ChannelStatus::PLAY;
}
}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


Data::Data()
: pitch        (G_DEFAULT_PITCH)
, mode         (SamplePlayerMode::SINGLE_BASIC)
, velocityAsVol(false)
{
}


/* -------------------------------------------------------------------------- */


Data::Data(const patch::Channel& p, float samplerateRatio)
: pitch        (p.pitch)
, mode         (p.mode)
, shift        (p.shift)
, begin        (p.begin)
, end          (p.end)
, velocityAsVol(p.midiInVeloAsVol)
{
    //setWave(waveManager::hydrateWave(p.waveId), samplerateRatio);
}


/* -------------------------------------------------------------------------- */


bool Data::hasWave() const        { return waveReader.wave != nullptr; }
bool Data::hasLogicalWave() const { return hasWave() && waveReader.wave->isLogical(); }
bool Data::hasEditedWave() const  { return hasWave() && waveReader.wave->isEdited(); }


/* -------------------------------------------------------------------------- */


bool Data::isAnyLoopMode() const
{
	return mode == SamplePlayerMode::LOOP_BASIC  || 
	       mode == SamplePlayerMode::LOOP_ONCE   || 
	       mode == SamplePlayerMode::LOOP_REPEAT || 
	       mode == SamplePlayerMode::LOOP_ONCE_BAR;
}


/* -------------------------------------------------------------------------- */


Wave* Data::getWave() const
{
    return waveReader.wave;
}


ID Data::getWaveId() const
{
    if (hasWave())
        return waveReader.wave->id;
    return 0;
}


/* -------------------------------------------------------------------------- */


Frame Data::getWaveSize() const
{
    return hasWave() ? waveReader.wave->getSize() : 0;
}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void react(channel::Data& ch, const eventDispatcher::Event& e)
{
    if (e.type == eventDispatcher::EventType::CHANNEL_PITCH)
        ch.samplePlayer->pitch = std::get<Action>(e.data).event.getVelocityFloat();
    if (ch.samplePlayer->hasWave())
        sampleReactor::react(ch, e);
}


/* -------------------------------------------------------------------------- */


void advance(const channel::Data& ch, const sequencer::Event& e)
{
    sampleAdvancer::advance(ch, e);
}


/* -------------------------------------------------------------------------- */


void render(const channel::Data& ch, AudioBuffer& out)
{
    const WaveReader_NEW& waveReader = ch.samplePlayer->waveReader;

    if (waveReader.wave == nullptr || !ch.isPlaying())
        return;

    Frame tracker = ch.state->tracker.load();
    Frame used    = 0;

    /* Audio data is temporarily stored to the working audio buffer. */

    AudioBuffer& buffer = ch.buffer->audioBuffer;

    /* Adjust tracker in case someone has changed the begin/end points in the
    meantime. */
    
    tracker = std::clamp(tracker, ch.samplePlayer->begin, ch.samplePlayer->end);

    /* If rewinding, fill the tail first, then reset the tracker to the begin
    point. The rest is performed as usual. */

    if (ch.state->rewinding) {
		if (tracker < ch.samplePlayer->end)
            waveReader.fill(buffer, tracker, 0, ch.samplePlayer->pitch);
        ch.state->rewinding = false;
		tracker = ch.samplePlayer->begin;
    }

    used     = waveReader.fill(buffer, tracker, ch.state->offset, ch.samplePlayer->pitch);
    tracker += used;

G_DEBUG ("block=[" << tracker - used << ", " << tracker << ")" << 
         ", used=" << used << ", range=[" << ch.samplePlayer->begin << ", " << ch.samplePlayer->end << ")" <<
         ", tracker=" << tracker << 
         ", offset=" << ch.state->offset << ", globalFrame=" << clock::getCurrentFrame());

    if (tracker >= ch.samplePlayer->end) {
G_DEBUG ("last frame tracker=" << tracker);
        tracker = ch.samplePlayer->begin;
        sampleAdvancer::onLastFrame(ch); // TODO - better moving this to samplerAdvancer::advance
        if (shouldLoop_(ch)) {
            Frame offset = std::min(static_cast<Frame>(used / ch.samplePlayer->pitch), buffer.countFrames() - 1);
            tracker += waveReader.fill(buffer, tracker, offset, ch.samplePlayer->pitch);
        }
    }

    ch.state->offset = 0;
    ch.state->tracker.store(tracker);
}


/* -------------------------------------------------------------------------- */


void loadWave(channel::Data& ch, Wave* w)
{
    ch.samplePlayer->waveReader.wave = w;

    ch.state->tracker.store(0);
    ch.samplePlayer->shift = 0;
    ch.samplePlayer->begin = 0;

    if (w != nullptr) {
        ch.state->playStatus.store(ChannelStatus::OFF);
        ch.name = w->getBasename(/*ext=*/false);
        ch.samplePlayer->end = w->getSize() - 1;
    }
    else {
        ch.state->playStatus.store(ChannelStatus::EMPTY);
        ch.name = "";
        ch.samplePlayer->end = 0;
    }
}


/* -------------------------------------------------------------------------- */


void setWave(channel::Data& ch, Wave* w, float samplerateRatio)
{
    if (w == nullptr) {
        ch.samplePlayer->waveReader.wave = nullptr;
        return;
    }

    ch.samplePlayer->waveReader.wave = w;

    if (samplerateRatio != 1.0f) {
        ch.samplePlayer->begin *= samplerateRatio;
        ch.samplePlayer->end   *= samplerateRatio;
        ch.samplePlayer->shift *= samplerateRatio;
    }
}


/* -------------------------------------------------------------------------- */


void kickIn(channel::Data& ch, Frame f)
{
    assert(ch.samplePlayer->hasWave());
    
	ch.state->tracker.store(f);
    ch.state->playStatus.store(ChannelStatus::PLAY);
}
}