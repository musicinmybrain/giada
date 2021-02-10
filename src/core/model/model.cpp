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
#include "core/model/model.h"
#ifdef G_DEBUG_MODE
#include "core/channels/channelManager.h"
#endif


namespace giada::m::model
{
namespace 
{
struct State
{
	Clock::State clock;
	Mixer::State mixer;
	std::vector<std::unique_ptr<channel::State>> channels;
};


struct Data
{
	std::vector<std::unique_ptr<channel::Buffer>> channels;
	std::vector<std::unique_ptr<Wave>>            waves;
#ifdef WITH_VST
	std::vector<std::unique_ptr<Plugin>>          plugins;
#endif	
};


/* -------------------------------------------------------------------------- */


template <typename T>
auto getIter_(const std::vector<std::unique_ptr<T>>& source, ID id)
{
	return u::vector::findIf(source, [id](const std::unique_ptr<T>& p) { return p->id == id; });
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


std::function<void(SwapType)> onSwap_ = nullptr;

Swapper<Layout> layout;
State           state;
Data            data;


/* -------------------------------------------------------------------------- */


ChannelDataLock::ChannelDataLock(channel::Data& ch)
: channelId(ch.id)
, wave     (ch.samplePlayer->getWave())
, plugins  (ch.plugins)
{
	samplePlayer::setWave(ch, nullptr, 0);
	ch.plugins = {};
	swap(SwapType::NONE);
}


ChannelDataLock::ChannelDataLock(ID channelId) 
: ChannelDataLock(model::get().getChannel(channelId)) 
{
}


ChannelDataLock::~ChannelDataLock()
{
    channel::Data& ch = model::get().getChannel(channelId);
	samplePlayer::setWave(ch, wave, 1.0f);
    ch.plugins = plugins;
	swap(SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


channel::Data& Layout::getChannel(ID id)
{
	return const_cast<channel::Data&>(const_cast<const Layout*>(this)->getChannel(id));
}


const channel::Data& Layout::getChannel(ID id) const
{
	auto it = std::find_if(channels.begin(), channels.end(), [id](const channel::Data& c)
	{
		return c.id == id;
	});
	assert(it != channels.end());
	return *it;
}


/* -------------------------------------------------------------------------- */


void init()
{
	get().clock.state = &state.clock;
	get().mixer.state = &state.mixer;
	swap(SwapType::NONE);
}


/* -------------------------------------------------------------------------- */


Layout& get()
{
	return layout.get();
}


Lock get_RT()
{
	return Lock(layout);
}


void swap(SwapType t)
{
    layout.swap();
    if (onSwap_) onSwap_(t);
}


void onSwap(std::function<void(SwapType)> f)
{
	onSwap_ = f;
}


/* -------------------------------------------------------------------------- */


template <typename T> 
std::vector<std::unique_ptr<T>>& getAll()
{
	if constexpr (std::is_same_v<T, Plugin>) return data.plugins;
    if constexpr (std::is_same_v<T, Wave>)   return data.waves;

	assert(false);
}

template std::vector<std::unique_ptr<Plugin>>& getAll<Plugin>();
template std::vector<std::unique_ptr<Wave>>&   getAll<Wave>();


/* -------------------------------------------------------------------------- */


template <typename T>
T* find(ID id)
{
	std::vector<std::unique_ptr<T>>* source = nullptr;

	if constexpr (std::is_same_v<T, Plugin>)        source = &data.plugins;
    if constexpr (std::is_same_v<T, Wave>)          source = &data.waves;

    assert(source != nullptr);

	auto it = getIter_(*source, id);
	return it == source->end() ? nullptr : it->get();
}

template Plugin*        find<Plugin>       (ID id);
template Wave*          find<Wave>         (ID id);
template channel::Data* find<channel::Data>(ID id);


/* -------------------------------------------------------------------------- */


template <typename T> 
T& add(std::unique_ptr<T> obj)
{
	std::vector<std::unique_ptr<T>>* dest = nullptr;

	if constexpr (std::is_same_v<T, Plugin>)          dest = &data.plugins;
    if constexpr (std::is_same_v<T, Wave>)            dest = &data.waves;
    if constexpr (std::is_same_v<T, channel::Buffer>) dest = &data.channels;
    if constexpr (std::is_same_v<T, channel::State>)  dest = &state.channels;

    assert(dest != nullptr);

	dest->push_back(std::move(obj));
	return *dest->back();
}

template Plugin&           add<Plugin>         (std::unique_ptr<Plugin> p);
template Wave&             add<Wave>           (std::unique_ptr<Wave> p);
template channel::Buffer&  add<channel::Buffer>(std::unique_ptr<channel::Buffer> p);
template channel::State&   add<channel::State> (std::unique_ptr<channel::State> p);


/* -------------------------------------------------------------------------- */


template <typename T> 
void remove(const T& ref)
{
	std::vector<std::unique_ptr<T>>* dest = nullptr;

	if constexpr (std::is_same_v<T, Plugin>) dest = &data.plugins;
    if constexpr (std::is_same_v<T, Wave>)   dest = &data.waves;

    assert(dest != nullptr);

	u::vector::removeIf(*dest, [&ref] (const auto& other) { return other.get() == &ref; });
}

template void remove<Plugin>(const Plugin& t);
template void remove<Wave>  (const Wave& t);


/* -------------------------------------------------------------------------- */


template <typename T> 
void clear()
{
	std::vector<std::unique_ptr<T>>* dest = nullptr;

    if constexpr (std::is_same_v<T, Wave>)   dest = &data.waves;
    if constexpr (std::is_same_v<T, Plugin>) dest = &data.plugins;

    assert(dest != nullptr);

	dest->clear();	
}

template void clear<Wave>();
template void clear<Plugin>();


/* -------------------------------------------------------------------------- */


#ifdef G_DEBUG_MODE

void debug()
{
	puts("======== SYSTEM STATUS ========");
	
	puts("model::layout");
	
	int i = 0;
	for (const channel::Data& c : get().channels) {
		printf("\t%d) - ID=%d name='%s' type=%d columnID=%d state=%p\n",
		    i++, c.id, c.name.c_str(), (int) c.type, c.columnId, (void*) &c.state);
#ifdef WITH_VST
		if (c.plugins.size() > 0) {
			puts("\t\tplugins:");
			for (const auto& p : c.plugins)
				printf("\t\t\tID=%d\n", p->id);
		}
#endif
	}

	puts("model::state.channels");
	
	i = 0;
	for (const auto& c : state.channels) {
		printf("\t%d) - %p\n", i++, (void*) c.get());
	}

	puts("model::data.waves");

	i = 0;
	for (const auto& w : data.waves)
		printf("\t%d) %p - ID=%d name='%s'\n", i++, (void*) w.get(), w->id, w->getPath().c_str());

#ifdef WITH_VST

	puts("model::data.plugins");

	i = 0;
	for (const auto& p : data.plugins)
		printf("\t%d) %p - ID=%d\n", i++, (void*) p.get(), p->id);

#endif
}

#endif // G_DEBUG_MODE
} // giada::m::model::
