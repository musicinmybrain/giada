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
#include "utils/fs.h"
#include "core/model/model.h"
#include "core/channels/channel.h"
#include "core/channels/samplePlayer.h"
#include "core/const.h"
#include "core/conf.h"
#include "core/kernelAudio.h"
#include "core/patch.h"
#include "core/mixer.h"
#include "core/idManager.h"
#include "core/wave.h"
#include "core/waveManager.h"
#include "core/plugins/pluginHost.h"
#include "core/plugins/pluginManager.h"
#include "core/plugins/plugin.h"
#include "core/action.h"
#include "core/recorderHandler.h"
#include "channelManager.h"


namespace giada::m::channelManager
{
namespace
{
IdManager channelId_;
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void init()
{
	channelId_ = IdManager();
}


/* -------------------------------------------------------------------------- */


std::unique_ptr<Channel> create(ChannelType type, ID columnId, const conf::Conf& conf)
{
	std::unique_ptr<Channel> ch = std::make_unique<Channel>(type, 
		channelId_.generate(), columnId, kernelAudio::getRealBufSize(), conf);
	
	return ch;
}


channel::Data create(ChannelType type, ID columnId, ChannelInfo extra)
{
	return channel::Data(type, extra.id, columnId, extra.state, extra.buffer);
}


ChannelInfo createInfo(ID channelId)
{
	return {
	    channelId_.generate(channelId), // if cId == 0 generate a new one, else reuse it
		model::add<channel::State> (std::make_unique<channel::State>()),
		model::add<channel::Buffer>(std::make_unique<channel::Buffer>(kernelAudio::getRealBufSize()))
	};
}


/* -------------------------------------------------------------------------- */


std::unique_ptr<Channel> create(const Channel& o)
{
	std::unique_ptr<Channel> ch = std::make_unique<Channel>(o);
	ID id = channelId_.get();
	ch->id        = id;
	ch->state->id = id;
	return ch;
}


/* -------------------------------------------------------------------------- */


Channel_NEW deserializeChannel(const patch::Channel& pch, ChannelInfo info, float samplerateRatio)
{
	//return Channel_NEW(pch, info.state, info.buffer, samplerateRatio);
}


/* -------------------------------------------------------------------------- */


const patch::Channel serializeChannel(const Channel_NEW& c)
{
	patch::Channel pc;

#ifdef WITH_VST
	for (const Plugin* p : c.plugins)
		pc.pluginIds.push_back(p->id);
#endif

	pc.id                = c.id;
	pc.type              = c.type;
    pc.columnId          = c.columnId;
    pc.height            = c.height;
    pc.name              = c.name;
    pc.key               = c.key;
    pc.mute              = c.mute;
    pc.solo              = c.solo;
    pc.volume            = c.volume;
    pc.pan               = c.pan;
    pc.hasActions        = c.hasActions;
    pc.readActions       = c.readActions;
    pc.armed             = c.armed;
    pc.midiIn            = c.midiLearner.enabled;
    pc.midiInFilter      = c.midiLearner.filter;
    pc.midiInKeyPress    = c.midiLearner.keyPress.getValue();
    pc.midiInKeyRel      = c.midiLearner.keyRelease.getValue();
    pc.midiInKill        = c.midiLearner.kill.getValue();
    pc.midiInArm         = c.midiLearner.arm.getValue();
    pc.midiInVolume      = c.midiLearner.volume.getValue();
    pc.midiInMute        = c.midiLearner.mute.getValue();
    pc.midiInSolo        = c.midiLearner.solo.getValue();
	pc.midiInReadActions = c.midiLearner.readActions.getValue();
	pc.midiInPitch       = c.midiLearner.pitch.getValue();
    pc.midiOutL          = c.midiLighter.enabled;
    pc.midiOutLplaying   = c.midiLighter.playing.getValue();
    pc.midiOutLmute      = c.midiLighter.mute.getValue();
    pc.midiOutLsolo      = c.midiLighter.solo.getValue();

	if (c.type == ChannelType::SAMPLE) {
		pc.waveId            = c.samplePlayer->getWaveId();
		pc.mode              = c.samplePlayer->mode;
		pc.begin             = c.samplePlayer->begin;
		pc.end               = c.samplePlayer->end;
		pc.pitch             = c.samplePlayer->pitch;
		pc.shift             = c.samplePlayer->shift;
		pc.midiInVeloAsVol   = c.samplePlayer->velocityAsVol;
		pc.inputMonitor      = c.audioReceiver->inputMonitor;
		pc.overdubProtection = c.audioReceiver->overdubProtection;

	}
	else
	if (c.type == ChannelType::MIDI) {
		pc.midiOut     = c.midiSender->enabled;
		pc.midiOutChan = c.midiSender->filter;
	}

	return pc;
}
} // giada::m::channelManager::
