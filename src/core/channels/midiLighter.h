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


#ifndef G_CHANNEL_MIDI_LIGHTER_H
#define G_CHANNEL_MIDI_LIGHTER_H


#include <memory>


namespace giada::m
{
namespace mixer
{
struct Event;
}
struct MidiLighterState;

/* MidiLighter
Learns and emits MIDI lightning messages to physical hardware on events. */

class MidiLighter
{
public:

    MidiLighter(ChannelState*);
    MidiLighter(const patch::Channel&, ChannelState*);
    MidiLighter(const MidiLighter&, ChannelState* c=nullptr);

    void parse(const mixer::Event& e, bool audible) const;

    /* state
    Pointer to mutable MidiLighterState state. */

    std::unique_ptr<MidiLighterState> state;

private:

    void sendMute(uint32_t l_mute) const;
    void sendSolo(uint32_t l_solo) const;
    void sendStatus(uint32_t l_playing, bool audible) const;

    ChannelState* m_channelState;
};








class Channel_NEW;
class MidiLighter_NEW
{
public:

    MidiLighter_NEW(const Channel_NEW&);
    MidiLighter_NEW(const MidiLighter_NEW&, const Channel_NEW* c=nullptr);

    void react(const eventDispatcher::Event& e, bool audible) const;

    /* enabled
    Tells whether MIDI ligthing is enabled or not. */
    
	bool enabled;

    /* MIDI learning fields for MIDI ligthing. */

	MidiLearnParam playing;
	MidiLearnParam mute;
	MidiLearnParam solo;

private:

    void sendMute(uint32_t l_mute) const;
    void sendSolo(uint32_t l_solo) const;
    void sendStatus(uint32_t l_playing, bool audible) const;

    const Channel_NEW* m_channel;
};
} // giada::m::




namespace giada::m::channel { struct Data; }
namespace giada::m::midiLighter
{
struct Data
{
    Data() = default;
    Data(const patch::Channel& p);
    Data(const Data& o) = default;

    /* enabled
    Tells whether MIDI ligthing is enabled or not. */
    
	bool enabled;

    /* MIDI learning fields for MIDI ligthing. */

	MidiLearnParam playing;
	MidiLearnParam mute;
	MidiLearnParam solo;
};


void react(channel::Data& ch, const eventDispatcher::Event& e, bool audible);
}


#endif
