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
#include "core/wave.h"
#include "core/clock.h"
#include "core/waveManager.h"
#include "samplePlayer.h"


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


/* -------------------------------------------------------------------------- */


void setWave_(samplePlayer::Data& sp, Wave* w, float samplerateRatio)
{
    if (w == nullptr) {
        sp.waveReader.wave = nullptr;
        return;
    }

    sp.waveReader.wave = w;

    if (samplerateRatio != 1.0f) {
        sp.begin *= samplerateRatio;
        sp.end   *= samplerateRatio;
        sp.shift *= samplerateRatio;
    }
}
} // {anonymous}


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
    setWave_(*this, waveManager::hydrateWave(p.waveId), samplerateRatio);
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
        ch.samplePlayer->pitch = std::get<float>(e.data);
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
    const WaveReader& waveReader = ch.samplePlayer->waveReader;

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
    setWave_(ch.samplePlayer.value(), w, samplerateRatio);
}


/* -------------------------------------------------------------------------- */


void kickIn(channel::Data& ch, Frame f)
{
	ch.state->tracker.store(f);
    ch.state->playStatus.store(ChannelStatus::PLAY);
}
} // giada::m::samplePlayer::