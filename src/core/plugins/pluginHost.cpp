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


#ifdef WITH_VST

#include <cassert>
#include "utils/log.h"
#include "utils/vector.h"
#include "core/model/model.h"
#include "core/channels/channel.h"
#include "core/const.h"
#include "core/plugins/plugin.h"
#include "core/plugins/pluginManager.h"
#include "core/plugins/pluginHost.h"


namespace giada::m::pluginHost
{
namespace
{
juce::MessageManager* messageManager_;
juce::AudioBuffer<float> audioBuffer_;
ID pluginId_;


/* -------------------------------------------------------------------------- */


void giadaToJuceTempBuf_(const AudioBuffer& outBuf)
{
	for (int i=0; i<outBuf.countFrames(); i++)
		for (int j=0; j<outBuf.countChannels(); j++)
			audioBuffer_.setSample(j, i, outBuf[i][j]);
}


/* juceToGiadaOutBuf_
Converts buffer from Juce to Giada. A note for the future: if we overwrite (=) 
(as we do now) it's SEND, if we add (+) it's INSERT. */

void juceToGiadaOutBuf_(AudioBuffer& outBuf)
{
	for (int i=0; i<outBuf.countFrames(); i++)
		for (int j=0; j<outBuf.countChannels(); j++)	
			outBuf[i][j] = audioBuffer_.getSample(j, i);
}


/* -------------------------------------------------------------------------- */


void processPlugins_(const std::vector<Plugin*>& plugins, juce::MidiBuffer& events)
{
	for (Plugin* p : plugins) {
		if (!p->valid || p->isSuspended() || p->isBypassed())
			continue;
		p->process(audioBuffer_, events);
		events.clear();
	}
}


ID clonePlugin_(ID pluginId)
{
	/*
	model::PluginsLock l(model::plugins);

	const Plugin&           original = model::get(model::plugins, pluginId);
	std::unique_ptr<Plugin> clone    = pluginManager::makePlugin(original);
	ID                      newId    = clone->id;

	model::plugins.push(std::move(clone));

	return newId;*/
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void close()
{
	messageManager_->deleteInstance();
	model::plugins.clear();
}


/* -------------------------------------------------------------------------- */


void init(int buffersize)
{
	messageManager_ = juce::MessageManager::getInstance();
	audioBuffer_.setSize(G_MAX_IO_CHANS, buffersize);
	pluginId_ = 0;
}


/* -------------------------------------------------------------------------- */


void processStack(AudioBuffer& outBuf, const std::vector<Plugin*>& plugins, 
	juce::MidiBuffer* events)
{
	assert(outBuf.countFrames() == audioBuffer_.getNumSamples());

	/* If events are null: Audio stack processing (master in, master out or
	sample channels. No need for MIDI events. 
	If events are not null: MIDI stack (MIDI channels). MIDI channels must not 
	process the current buffer: give them an empty and clean one. */
	
	if (events == nullptr) {
		giadaToJuceTempBuf_(outBuf);
		juce::MidiBuffer dummyEvents; // empty
		processPlugins_(plugins, dummyEvents);
	}
	else {
		audioBuffer_.clear();
		processPlugins_(plugins, *events);
	}
	juceToGiadaOutBuf_(outBuf);
}

/* -------------------------------------------------------------------------- */


void addPlugin(std::unique_ptr<Plugin> p, ID channelId)
{
	const Plugin& pluginRef = model::add<Plugin>(std::move(p));

    model::swap([channelId, &pluginRef](model::Layout& l)
    {
        /* TODO - unfortunately JUCE wants mutable plugin objects due to the
        presence of the non-const processBlock() method. Why not const_casting
        only in the Plugin class? */
		l.getChannel(channelId).plugins.push_back(const_cast<Plugin*>(&pluginRef));
    }, model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


void swapPlugin(const m::Plugin& p1, const m::Plugin& p2, ID channelId)
{
    model::swap([&](model::Layout& l)
    {
		std::vector<m::Plugin*>& pvec = l.getChannel(channelId).plugins;
		std::size_t index1 = u::vector::indexOf(pvec, &p1);
		std::size_t index2 = u::vector::indexOf(pvec, &p2);
		std::swap(pvec.at(index1), pvec.at(index2));
    }, model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


void freePlugin(const m::Plugin& plugin, ID channelId)
{
    model::swap([channelId, &plugin](model::Layout& l)
    {
		u::vector::remove(l.getChannel(channelId).plugins, &plugin);
    }, model::SwapType::HARD);

    model::remove(plugin);
}


void freePlugins(const std::vector<Plugin*>& plugins)
{
	for (const Plugin* p : plugins)
		model::remove(*p);
}


/* -------------------------------------------------------------------------- */


std::vector<ID> clonePlugins(std::vector<ID> pluginIds)
{
	std::vector<ID> out;
	for (ID id : pluginIds)
		out.push_back(clonePlugin_(id));
	return out;
}


/* -------------------------------------------------------------------------- */


void setPluginParameter(ID pluginId, ID channelId, int paramIndex, float value)
{
	model::ChannelDataLock lock(channelId);
    model::getPtr<Plugin>(pluginId)->setParameter(paramIndex, value);
}


/* -------------------------------------------------------------------------- */


void setPluginProgram(ID pluginId, ID channelId, int programIndex)
{
	model::ChannelDataLock lock(channelId);
    model::getPtr<Plugin>(pluginId)->setCurrentProgram(programIndex);
}


/* -------------------------------------------------------------------------- */


void toggleBypass(ID pluginId, ID channelId)
{
	model::ChannelDataLock lock(channelId);

	Plugin& plugin = *model::getPtr<Plugin>(pluginId);
	plugin.setBypass(!plugin.isBypassed());
}


/* -------------------------------------------------------------------------- */


void runDispatchLoop()
{
	messageManager_->runDispatchLoopUntil(10);
}
} // giada::m::pluginHost::


#endif // #ifdef WITH_VST
