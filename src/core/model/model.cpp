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
	Clock_NEW::State clock;
	Mixer_NEW::State mixer;
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
auto getIter_NEW(const std::vector<std::unique_ptr<T>>& source, ID id)
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


ChannelDataLock::ChannelDataLock(const channel::Data& ch)
: m_channel(ch)
, m_wave   (ch.samplePlayer->getWave())
, m_plugins(ch.plugins)
{
	swap([this](Layout& l)
	{
	    assert(false);
		//samplePlayer::setWave(l.getChannel(m_channel.id), nullptr, 0);
		l.getChannel(m_channel.id).plugins = {};
	}, SwapType::NONE);	
}


ChannelDataLock::ChannelDataLock(ID channelId) : ChannelDataLock(*getPtr<channel::Data>(channelId)) {}


ChannelDataLock::~ChannelDataLock()
{
	swap([this](Layout& l)
	{
        assert(false);
            //samplePlayer::setWave(l.getChannel(m_channel.id), m_wave, 1.0f);
            l.getChannel(m_channel.id).plugins = m_plugins;
        }, SwapType::HARD);
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
    layout.swap([](Layout& l)
    {
		l.clock.state = &state.clock;
		l.mixer.state = &state.mixer;
    });
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


void swap(std::function<void(Layout&)> f, SwapType t)
{
	layout.swap(f);
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
T* getPtr(ID id)
{
	std::vector<std::unique_ptr<T>>* source = nullptr;

	if constexpr (std::is_same_v<T, Plugin>)      source = &data.plugins;
    if constexpr (std::is_same_v<T, Wave>)        source = &data.waves;

    assert(source != nullptr);

	auto it = getIter_NEW(*source, id);
	return it == source->end() ? nullptr : it->get();
}

template Plugin*        getPtr<Plugin>       (ID id);
template Wave*          getPtr<Wave>         (ID id);
template channel::Data* getPtr<channel::Data>(ID id);


/* -------------------------------------------------------------------------- */


template <typename T>
std::size_t getIndex(ID id)
{
	static_assert(std::is_same_v<T, channel::Data>); // Only channel::Data for now

	// Why decltype: https://stackoverflow.com/a/25185070/3296421
	auto it = u::vector::findIf(get().channels, [id](const channel::Data& c) { return c.id == id; });
	return std::distance<decltype(it)>(get().channels.begin(), it);
}

template std::size_t getIndex<channel::Data>(ID id);


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

    if constexpr (std::is_same_v<T, Wave>) dest = &data.waves;

    assert(dest != nullptr);

	dest->clear();	
}

template void clear<Wave>();





































RCUList<Clock>    clock(std::make_unique<Clock>());
RCUList<Mixer>    mixer(std::make_unique<Mixer>());
RCUList<Kernel>   kernel(std::make_unique<Kernel>());
RCUList<Recorder> recorder(std::make_unique<Recorder>());
RCUList<MidiIn>   midiIn(std::make_unique<MidiIn>());
RCUList<Actions>  actions(std::make_unique<Actions>());
RCUList<Channel> channels;
RCUList<Wave>     waves;
#ifdef WITH_VST
RCUList<Plugin>   plugins;
#endif


Actions::Actions(const Actions& o) : map(o.map)
{
	/* Needs to update all pointers of prev and next actions with addresses 
	coming from the new 'actions' map.  */

	recorder::updateMapPointers(map);
}


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











#if 0	
	ChannelsLock chl(channels);
	ClockLock    cl(clock);
	WavesLock    wl(waves);
	ActionsLock  al(actions);
#ifdef WITH_VST
	PluginsLock  pl(plugins);
#endif

	puts("======== SYSTEM STATUS ========");
	
	puts("model::channels");

	int i = 0;
	for (const Channel* c : channels) {
		printf("\t%d) %p - ID=%d name='%s' type=%d columnID=%d\n",
		        i++, (void*) c, c->state->id, c->state->name.c_str(), (int) c->getType(), c->getColumnId());
/*
		if (c->hasData())
			printf("\t\twave: ID=%d\n", static_cast<const SampleChannel*>(c)->waveId);
*/
#ifdef WITH_VST
		if (c->pluginIds.size() > 0) {
			puts("\t\tplugins:");
			for (ID id : c->pluginIds)
				printf("\t\t\tID=%d\n", id);
		}
#endif
	}

	puts("model::waves");

	i = 0;
	for (const Wave* w : waves) 
		printf("\t%d) %p - ID=%d name='%s'\n", i++, (void*)w, w->id, w->getPath().c_str());
		
#ifdef WITH_VST
	puts("model::plugins");

	i = 0;
	for (const Plugin* p : plugins) {
		if (p->valid)
			printf("\t%d) %p - ID=%d name='%s'\n", i++, (void*)p, p->id, p->getName().c_str());
		else
			printf("\t%d) %p - ID=%d INVALID\n", i++, (void*)p, p->id); 
	}
#endif

	puts("model::clock");

	printf("\tclock.status   = %d\n", static_cast<int>(clock.get()->status));
	printf("\tclock.bars     = %d\n", clock.get()->bars);
	printf("\tclock.beats    = %d\n", clock.get()->beats);
	printf("\tclock.bpm      = %f\n", clock.get()->bpm);
	printf("\tclock.quantize = %d\n", clock.get()->quantize);

	puts("model::actions");

	for (auto& kv : actions.get()->map) {
		printf("\tframe: %d\n", kv.first);
		for (const Action& a : kv.second)
			printf("\t\t(%p) - ID=%d, frame=%d, channel=%d, value=0x%X, prevId=%d, prev=%p, nextId=%d, next=%p\n", 
				(void*) &a, a.id, a.frame, a.channelId, a.event.getRaw(), a.prevId, (void*) a.prev, a.nextId, (void*) a.next);	
	}
	
	puts("===============================");
#endif
}

#endif // G_DEBUG_MODE
} // giada::m::model::
