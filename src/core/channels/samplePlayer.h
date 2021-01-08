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


#ifndef G_CHANNEL_SAMPLE_PLAYER_H
#define G_CHANNEL_SAMPLE_PLAYER_H


#include "core/types.h"
#include "core/const.h"
#include "core/mixer.h" // TODO - forward declare
#include "core/audioBuffer.h" // TODO - forward declare
#include "core/channels/waveReader.h"
#include "core/channels/sampleController.h"
#include "core/channels/sampleAdvancer.h"
#include "core/channels/sampleReactor.h"


namespace giada::m
{
class  Wave;
struct SamplePlayerState;
class SamplePlayer
{
public:

    SamplePlayer(ChannelState*);
    SamplePlayer(const patch::Channel& p, ChannelState*);
    SamplePlayer(const SamplePlayer&, ChannelState* c=nullptr);

    void parse(const mixer::Event& e) const;
    void advance(Frame bufferSize) const;
    void render(AudioBuffer& out) const;

    bool hasWave() const;
    bool hasLogicalWave() const;
    bool hasEditedWave() const;
    ID getWaveId() const;
    Frame getWaveSize() const;

    /* loadWave
    Loads Wave 'w' into this channel and sets it up (name, markers, ...). */

    void loadWave(const Wave* w);
    
    /* setWave
    Just sets the pointer to a Wave object. Used during de-serialization. The
    ratio is used to adjust begin/end points in case of patch vs. conf sample
    rate mismatch. */

    void setWave(const Wave& w, float samplerateRatio);

    /* setInvalidWave
    Same as setWave(nullptr) plus the invalid ID (i.e. 0). */
    
    void setInvalidWave(); 

    /* kickIn
    Starts the player right away at frame 'f'. Used when launching a loop after
    being live recorded. */
    
    void kickIn(Frame f);


    /* state
    Pointer to mutable SamplePlayerState state. */

    std::unique_ptr<SamplePlayerState> state;

private:

    bool shouldLoop() const;

    ID m_waveId;

    /* m_waveReader
    Used to read data from Wave and fill incoming buffer. */

    WaveReader m_waveReader;

    /* m_sampleController
    Managers events for this Sample Player. */

    SampleController m_sampleController;

    /* m_channelState
    Pointer to Channel state. Needed to alter the playStatus status when the
    sample is over. */

    ChannelState* m_channelState;
};



























class Channel_NEW;
class SamplePlayer_NEW
{
public:

    SamplePlayer_NEW(Channel_NEW&);
    SamplePlayer_NEW(const patch::Channel& p, Channel_NEW&, float samplerateRatio);
    SamplePlayer_NEW(const SamplePlayer_NEW& o, Channel_NEW* c=nullptr);

    void react(const eventDispatcher::Event& e);
    void advance(const sequencer::Event& e) const;
    void render(AudioBuffer& out) const;

    bool hasWave() const;
    bool hasLogicalWave() const;
    bool hasEditedWave() const;
    bool isAnyLoopMode() const;
    ID getWaveId() const;
    Frame getWaveSize() const;
    Frame getTracker() const;
    const Wave* getWave() const;

    /* loadWave
    Loads Wave 'w' into this channel and sets it up (name, markers, ...). */

    void loadWave(const Wave* w);
    
    /* setWave
    Just sets the pointer to a Wave object. Used during de-serialization. The
    ratio is used to adjust begin/end points in case of patch vs. conf sample
    rate mismatch. If nullptr, set the wave to invalid. */

    void setWave(const Wave* w, float samplerateRatio);

    /* kickIn
    Starts the player right away at frame 'f'. Used when launching a loop after
    being live recorded. */
    
    void kickIn(Frame f);

    float            pitch;
    SamplePlayerMode mode;
    Frame            shift;
    Frame            begin;
    Frame            end;
	bool             velocityAsVol; // Velocity drives volume
    bool             quantizing;
    Quantizer        quantizer;

private:

    bool shouldLoop() const;

    ID             m_waveId;
    WaveReader_NEW m_waveReader;
    SampleAdvancer m_sampleAdvancer;
    SampleReactor  m_sampleReactor;
    Channel_NEW*   m_channel;
};
} // giada::m::















namespace giada::m::channel { struct Data; }
namespace giada::m::samplePlayer
{
struct Data
{
    Data() = default;
    Data(const patch::Channel& p, float samplerateRatio);
    Data(const Data& o) = default;

    bool hasWave() const;
    bool hasLogicalWave() const;
    bool hasEditedWave() const;
    bool isAnyLoopMode() const;
    ID getWaveId() const;
    Frame getWaveSize() const;
    const Wave* getWave() const;

    float            pitch;
    SamplePlayerMode mode;
    Frame            shift;
    Frame            begin;
    Frame            end;
	bool             velocityAsVol; // Velocity drives volume
    bool             quantizing; // TODO move to sampleReactor
    WaveReader_NEW   waveReader;
};


void react  (channel::Data& ch, const eventDispatcher::Event& e);
void advance(const channel::Data& ch, const sequencer::Event& e);
void render (const channel::Data& ch, AudioBuffer& out);

/* loadWave
Loads Wave 'w' into channel ch and sets it up (name, markers, ...). */

void loadWave(channel::Data& ch, const Wave* w);

/* setWave
Just sets the pointer to a Wave object. Used during de-serialization. The
ratio is used to adjust begin/end points in case of patch vs. conf sample
rate mismatch. If nullptr, set the wave to invalid. */

void setWave(channel::Data& ch, const Wave* w, float samplerateRatio);

/* kickIn
Starts the player right away at frame 'f'. Used when launching a loop after
being live recorded. */

void kickIn(channel::Data& ch, Frame f);
}

#endif
