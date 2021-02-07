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


#ifndef G_PLUGIN_HOST_H
#define G_PLUGIN_HOST_H


#include <functional>
#include "deps/juce-config.h"
#include "core/types.h"


namespace giada::m 
{
class Plugin;
class AudioBuffer;
}
namespace giada::m::pluginHost
{
void init(int buffersize);
void close();

/* addPlugin
Adds a new plugin to channel 'channelId'. */

void addPlugin(std::unique_ptr<Plugin> p, ID channelId);

/* processStack
Applies the fx list to the buffer. */

void processStack(AudioBuffer& outBuf, const std::vector<Plugin*>& plugins, 
	juce::MidiBuffer* events=nullptr);

/* swapPlugin 
Swaps plug-in 1 with plug-in 2 in Channel 'channelId'. */

void swapPlugin(const m::Plugin& p1, const m::Plugin& p2, ID channelId);

/* freePlugin.
Unloads plugin from channel 'channelId'. */

void freePlugin(const m::Plugin& plugin, ID channelId);

/* freePlugins
Unloads multiple plugins. Useful when freeing or deleting a channel. */

void freePlugins(const std::vector<Plugin*>& plugins);

/* clonePlugins
Clones all the plug-ins in the 'plugins' vector. */

std::vector<Plugin*> clonePlugins(const std::vector<Plugin*>& plugins);

void setPluginParameter(ID pluginId, ID channelId, int paramIndex, float value);

void setPluginProgram(ID pluginId, ID channelId, int programIndex);

void toggleBypass(ID pluginId, ID channelId);

/* runDispatchLoop
Wakes up plugins' GUI manager for N milliseconds. */

void runDispatchLoop();
} // giada::m::pluginHost::


#endif

#endif // #ifdef WITH_VST
