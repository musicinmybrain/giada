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


#ifndef G_CHANNEL_AUDIO_RECEIVER_H
#define G_CHANNEL_AUDIO_RECEIVER_H


#include <memory>


namespace giada {
namespace m
{
namespace conf
{
struct Conf;
}
namespace patch
{
struct Channel; 
}
class  AudioBuffer;
struct ChannelState;
struct AudioReceiverState;

/* AudioReceiver 
Operates on input audio streams for audio recording and input monitor. */

class AudioReceiver
{
public:

    AudioReceiver(ChannelState*, const conf::Conf&);
    AudioReceiver(const patch::Channel&, ChannelState*);
    AudioReceiver(const AudioReceiver&, ChannelState* c=nullptr);

    void render(const AudioBuffer& in) const;

    /* state
    Pointer to mutable AudioReceiverState state. */

    std::unique_ptr<AudioReceiverState> state;

private:

    ChannelState* m_channelState;
};












class Channel_NEW;
class AudioReceiver_NEW
{
public:

    AudioReceiver_NEW(Channel_NEW&);
    AudioReceiver_NEW(const patch::Channel&, Channel_NEW&);
    AudioReceiver_NEW(const AudioReceiver_NEW& o, Channel_NEW* c=nullptr);

    void render(const AudioBuffer& in) const;

    bool inputMonitor;
    bool overdubProtection;

private:

    Channel_NEW& m_channel;
};
}} // giada::m::









namespace giada::m::channel { struct Data; }
namespace giada::m::audioReceiver
{
struct Data
{
    Data() = default;
    Data(const patch::Channel& p);
    Data(const Data& o) = default;

    bool inputMonitor;
    bool overdubProtection;
};

void render(const channel::Data& ch, const AudioBuffer& in);
}

#endif
