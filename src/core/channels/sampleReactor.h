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


#ifndef G_CHANNEL_SAMPLE_REACTOR_H
#define G_CHANNEL_SAMPLE_REACTOR_H


#include "core/eventDispatcher.h"
#include "core/quantizer.h"


namespace giada::m
{
class Channel_NEW;
class SamplePlayer_NEW;
class SampleReactor
{
public:

    SampleReactor(Channel_NEW&, SamplePlayer_NEW&);

    void react(const eventDispatcher::Event& e);

private:

    void press(int velocity);
    void release();
    void kill();
    void onStopBySeq();
    void toggleReadActions();

    ChannelStatus pressWhileOff(int velocity, bool isLoop);
    ChannelStatus pressWhilePlay(SamplePlayerMode mode, bool isLoop);
    void rewind(Frame localFrame=0);

    Channel_NEW&      m_channel;
    SamplePlayer_NEW& m_samplePlayer;
    Quantizer         m_quantizer;
};
} // giada::m::







namespace giada::m::channel { struct Data; }
namespace giada::m::sampleReactor
{
struct Data
{
    Quantizer quantizer;
};

void react(channel::Data& c, const eventDispatcher::Event& e);
}

#endif