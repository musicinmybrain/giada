/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2020 Giovanni A. Zuliani | Monocasual
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


#ifndef G_CHANNEL_SAMPLE_ADVANCER_H
#define G_CHANNEL_SAMPLE_ADVANCER_H


#include "core/sequencer.h"


namespace giada::m
{
class Channel_NEW;
class SamplePlayer_NEW;
class SampleAdvancer
{
public:

    SampleAdvancer(Channel_NEW&, SamplePlayer_NEW&);

    void onLastFrame() const;
    void advance(const sequencer::Event& e) const;

private:

    void onFirstBeat(Frame localFrame) const;
    void onBar(Frame localFrame) const;
    void parseActions(const std::vector<Action>& as, Frame localFrame) const;
    void rewind(Frame localFrame) const;
    void stop(Frame localFrame) const;
    
    const Channel_NEW&      m_channel;
    const SamplePlayer_NEW& m_samplePlayer;
};
} // giada::m::





namespace giada::m::channel { struct Data; }
namespace giada::m::sampleAdvancer
{
void onLastFrame(const channel::Data& ch);
void advance(const channel::Data& ch, const sequencer::Event& e);
}


#endif
