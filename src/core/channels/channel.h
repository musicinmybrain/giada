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


#ifndef G_CHANNEL_H
#define G_CHANNEL_H


#include <optional>
#include "core/const.h"
#include "core/mixer.h"
#include "core/eventDispatcher.h"
#include "core/sequencer.h"
#include "core/channels/state.h"
#include "core/channels/samplePlayer.h"
#include "core/channels/audioReceiver.h"
#ifdef WITH_VST
#include "core/channels/midiReceiver.h"
#endif
#include "core/channels/midiLearner.h"
#include "core/channels/midiSender.h"
#include "core/channels/midiController.h"
#include "core/channels/midiLighter.h"
#include "core/channels/sampleActionRecorder.h"
#include "core/channels/midiActionRecorder.h"


namespace giada::m
{
namespace conf
{
struct Conf;
}
class Channel final
{
public:

    Channel(ChannelType t, ID id, ID columnId, Frame bufferSize, const conf::Conf& c);
    Channel(const Channel&);
    Channel(const patch::Channel& p, Frame bufferSize);
    Channel(Channel&&)                 = default;
    Channel& operator=(const Channel&) = delete;
    Channel& operator=(Channel&&)      = delete;
    ~Channel()                         = default;

    /* parse
    Parses live events. */

    void parse(const mixer::EventBuffer& e, bool audible) const;

    /* advance
    Processes static events (e.g. actions) in the current block. */

    void advance(Frame bufferSize) const;

    /* render
    Renders audio data to I/O buffers. */
     
    void render(AudioBuffer* out, AudioBuffer* in, bool audible) const;

    bool isInternal() const;
    bool isMuted() const;
    bool canInputRec() const;
    bool canActionRec() const;
    bool hasWave() const;
    ID getColumnId() const;
    ChannelType getType() const;
    
    ID id;

#ifdef WITH_VST
    std::vector<ID> pluginIds;
#endif

    /* state
    Pointer to mutable Channel state. */

    std::unique_ptr<ChannelState> state;

    MidiLearner midiLearner;
    MidiLighter midiLighter;

    std::optional<SamplePlayer>         samplePlayer;
    std::optional<AudioReceiver>        audioReceiver;
    std::optional<MidiController>       midiController;
#ifdef WITH_VST
    std::optional<MidiReceiver>         midiReceiver;
#endif
    std::optional<MidiSender>           midiSender;
    std::optional<SampleActionRecorder> sampleActionRecorder;
    std::optional<MidiActionRecorder>   midiActionRecorder;

private:

    void parse(const mixer::Event& e) const;

    void renderMasterOut(AudioBuffer& out) const;
    void renderMasterIn(AudioBuffer& in) const;
    void renderChannel(AudioBuffer& out, AudioBuffer& in, bool audible) const;

    AudioBuffer::Pan calcPanning() const;

    ChannelType m_type;
    ID m_columnId;
};










class Plugin;
class Channel_NEW final
{
public:

    struct State
    {
        WeakAtomic<Frame> tracker{0};
        bool              rewinding;
        Frame             offset;
    };

    struct Data // TODO Buffer
    {
        Data(Frame bufferSize);
        
        AudioBuffer      audioBuffer;
#ifdef WITH_VST
        juce::MidiBuffer midiBuffer;
#endif
    };

    Channel_NEW(ChannelType t, ID id, ID columnId, State& state, Data& data);
    Channel_NEW(const patch::Channel& p, State& state, Data& data, float samplerateRatio);
    Channel_NEW(const Channel_NEW& o);
    //Channel_NEW(Channel_NEW&& o)               = delete;
    //Channel_NEW& operator=(const Channel_NEW&) = delete;
    //Channel_NEW& operator=(Channel_NEW&&)      = delete;

    /* advance
    Advances internal state by processing static events (e.g. pre-recorded 
    actions or sequencer events) in the current block. */

    void advance(const sequencer::EventBuffer& e) const;

    /* react
    Reacts to live events coming from the EventDispatcher (human events) and
    updates itself accordingly. */

    void react(const eventDispatcher::EventBuffer& e, bool audible);

    /* render
    Renders audio data to I/O buffers. */
     
    void render(AudioBuffer* out, AudioBuffer* in, bool audible) const;

    bool isPlaying() const;
    bool isInternal() const;
    bool isMuted() const;
    bool canInputRec() const;
    bool canActionRec() const;
    bool hasWave() const;    

    State* state;
    Data*  data;

    ID            id;
    ChannelType   type;
    ID            columnId;
    float         volume;
    float         volume_i;    // Internal volume used for velocity-drives-volume mode on Sample Channels
    float         pan;
    bool          mute;
    bool          solo;
    bool          armed;
    int           key;
    bool          readActions;
    bool          hasActions;
    std::string   name;
    Pixel         height;
    ChannelStatus playStatus = ChannelStatus::OFF;
    ChannelStatus recStatus  = ChannelStatus::OFF;
#ifdef WITH_VST
    std::vector<Plugin*> plugins;
#endif

    midiLearner::Data midiLearner;
    midiLighter::Data midiLighter;

    std::optional<samplePlayer::Data>         samplePlayer;
    std::optional<audioReceiver::Data>        audioReceiver;
    std::optional<midiController::Data>       midiController;
#ifdef WITH_VST
    std::optional<midiReceiver::Data>         midiReceiver;
#endif
    std::optional<midiSender::Data>           midiSender;
    std::optional<sampleActionRecorder::Data> sampleActionRecorder;
    std::optional<midiActionRecorder::Data>   midiActionRecorder;
    
private:

    void react(const eventDispatcher::Event& e);

    void renderMasterOut(AudioBuffer& out) const;
    void renderMasterIn(AudioBuffer& in) const;
    void renderChannel(AudioBuffer& out, AudioBuffer& in, bool audible) const;

    AudioBuffer::Pan calcPanning() const;
};


void pumpChannelFunction(ID channelId, std::function<void(Channel_NEW&)> f);
} // giada::m::














namespace giada::m::channel
{
struct State
{
    WeakAtomic<Frame>         tracker    = 0;
    WeakAtomic<ChannelStatus> playStatus = ChannelStatus::OFF;
    WeakAtomic<ChannelStatus> recStatus  = ChannelStatus::OFF;
    bool                      rewinding;
    Frame                     offset;
};

struct Buffer
{
    Buffer(Frame bufferSize);
    
    AudioBuffer      audioBuffer; // TODO - rename audio
#ifdef WITH_VST
    juce::MidiBuffer midiBuffer; // TODO - rename midi
#endif
};

struct Data
{
    Data(ChannelType t, ID id, ID columnId, State& state, Buffer& buffer);
    Data(const patch::Channel& p, State& state, Buffer& buffer, float samplerateRatio);
    Data(const Data& o)          = default;
    Data(Data&& o)               = default;
    Data& operator=(const Data&) = default;
    Data& operator=(Data&&)      = default;

    bool isPlaying() const;
    bool isInternal() const;
    bool isMuted() const;
    bool canInputRec() const;
    bool canActionRec() const;
    bool hasWave() const;    
    
    State*               state;
    Buffer*              buffer;
    ID                   id;
    ChannelType          type;
    ID                   columnId;
    float                volume;
    float                volume_i;    // Internal volume used for velocity-drives-volume mode on Sample Channels
    float                pan;
    bool                 mute;
    bool                 solo;
    bool                 armed;
    int                  key;
    bool                 readActions;
    bool                 hasActions;
    std::string          name;
    Pixel                height;
#ifdef WITH_VST
    std::vector<Plugin*> plugins;
#endif

    midiLearner::Data midiLearner;
    midiLighter::Data midiLighter;

    std::optional<samplePlayer::Data>         samplePlayer;
    std::optional<audioReceiver::Data>        audioReceiver;
    std::optional<midiController::Data>       midiController;
#ifdef WITH_VST
    std::optional<midiReceiver::Data>         midiReceiver;
#endif
    std::optional<midiSender::Data>           midiSender;
    std::optional<sampleActionRecorder::Data> sampleActionRecorder;
    std::optional<midiActionRecorder::Data>   midiActionRecorder;
};

/* advance
Advances internal state by processing static events (e.g. pre-recorded 
actions or sequencer events) in the current block. */

void advance(const Data& d, const sequencer::EventBuffer& e);

/* react
Reacts to live events coming from the EventDispatcher (human events) and
updates itself accordingly. */

void react(Data& d, const eventDispatcher::EventBuffer& e, bool audible);

/* render
Renders audio data to I/O buffers. */
    
void render(const Data& d, AudioBuffer* out, AudioBuffer* in, bool audible);
}

#endif
