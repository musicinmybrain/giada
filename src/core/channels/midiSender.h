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


#ifndef G_CHANNEL_MIDI_SENDER_H
#define G_CHANNEL_MIDI_SENDER_H


namespace giada {
namespace m
{
namespace mixer
{
struct Event;
}
struct ChannelState;
struct MidiSenderState;
class MidiSender
{
public:

    MidiSender(ChannelState*);
    MidiSender(const patch::Channel&, ChannelState*);
    MidiSender(const MidiSender&, ChannelState* c=nullptr);

    void parse(const mixer::Event& e) const;

    /* state
    Pointer to mutable MidiSenderState state. */

    std::unique_ptr<MidiSenderState> state;

private:

    void send(MidiEvent e) const;

    ChannelState* m_channelState;
};









class Channel_NEW;
class MidiSender_NEW
{
public:

    MidiSender_NEW(Channel_NEW&);
    MidiSender_NEW(const patch::Channel&, Channel_NEW&);
    MidiSender_NEW(const MidiSender_NEW& o, Channel_NEW* c=nullptr);

    void react(const eventDispatcher::Event& e) const;

    /* enabled
    Tells whether MIDI output is enabled or not. */
    
	bool enabled;

    /* filter
    Which MIDI channel data should be sent to. */
    
    int filter;

private:

    void send(MidiEvent e) const;

    Channel_NEW& m_channel;
};
}} // giada::m::




namespace giada::m::channel { struct Data; }
namespace giada::m::midiSender
{
struct Data
{
    Data() = default;
    Data(const patch::Channel& p);
    Data(const Data& o) = default;

    /* enabled
    Tells whether MIDI output is enabled or not. */
    
	bool enabled;

    /* filter
    Which MIDI channel data should be sent to. */
    
    int filter;
};

void react(const channel::Data& ch, const eventDispatcher::Event& e);
}

#endif
