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
#include <vector>
#include <algorithm>
#include "utils/fs.h"
#include "utils/string.h"
#include "utils/log.h"
#include "utils/vector.h"
#include "glue/main.h"
#include "glue/channel.h"
#include "core/model/model.h"
#include "core/channels/channelManager.h"
#include "core/kernelMidi.h"
#include "core/mixer.h"
#include "core/const.h"
#include "core/init.h"
#include "core/plugins/pluginHost.h"
#include "core/plugins/pluginManager.h"
#include "core/plugins/plugin.h"
#include "core/waveFx.h"
#include "core/conf.h"
#include "core/patch.h"
#include "core/recorder.h"
#include "core/recorderHandler.h"
#include "core/recManager.h"
#include "core/clock.h"
#include "core/kernelAudio.h"
#include "core/midiMapConf.h"
#include "core/wave.h"
#include "core/waveManager.h"
#include "core/mixerHandler.h"


namespace giada::m::mh
{
namespace
{
channel::Data& addChannel_(ChannelType type, ID columnId)
{
	channelManager::ChannelInfo info = channelManager::createInfo();

    model::swap([=](model::Layout& l)
    {
		l.channels.push_back(channelManager::create(type, columnId, info));
    }, model::SwapType::HARD);

   return model::get().channels.back();
}


/* -------------------------------------------------------------------------- */


waveManager::Result createWave_(const std::string& fname)
{
	return waveManager::createFromFile(fname, /*ID=*/0, conf::conf.samplerate, 
		conf::conf.rsmpQuality); 
}


/* -------------------------------------------------------------------------- */


bool anyChannel_(std::function<bool(const channel::Data&)> f)
{
	return std::any_of(model::get().channels.begin(), model::get().channels.end(), f);
}


/* -------------------------------------------------------------------------- */


template <typename F>
std::vector<ID> getChannelsIf_(F f)
{
	model::ChannelsLock l(model::channels);

	std::vector<ID> ids;
	for (const Channel* c : model::channels)
		if (f(c)) ids.push_back(c->id);
	
	return ids;	
}


std::vector<ID> getRecordableChannels_()
{
	return getChannelsIf_([] (const Channel* c) { return c->canInputRec() && !c->hasWave(); });
}


std::vector<ID> getOverdubbableChannels_()
{
	return getChannelsIf_([] (const Channel* c) { return c->canInputRec() && c->hasWave(); });
}


/* -------------------------------------------------------------------------- */

/* pushWave_
Pushes a new wave into Channel 'ch' and into the corresponding Wave list.
Use this when modifying a local model, before swapping it. */

void pushWave_(Channel& ch, std::unique_ptr<Wave>&& w)
{
	assert(ch.getType() == ChannelType::SAMPLE);

	model::waves.push(std::move(w));

	model::WavesLock l(model::waves);
	ch.samplePlayer->loadWave(model::waves.back());
}


/* -------------------------------------------------------------------------- */


void setupChannelPostRecording_(Channel& c)
{
	/* Start sample channels in loop mode right away. */
	if (c.samplePlayer->state->isAnyLoopMode())
		c.samplePlayer->kickIn(clock::getCurrentFrame());
	/* Disable 'arm' button if overdub protection is on. */
	if (c.audioReceiver->state->overdubProtection.load() == true)
		c.state->armed.store(false);
}


/* -------------------------------------------------------------------------- */


/* recordChannel_
Records the current Mixer audio input data into an empty channel. */

void recordChannel_(ID channelId)
{
    assert(false);
#if 0
	/* Create a new Wave with audio coming from Mixer's virtual input. */

	std::string filename = "TAKE-" + std::to_string(patch::patch.lastTakeId++) + ".wav";

	std::unique_ptr<Wave> wave = waveManager::createEmpty(clock::getFramesInLoop(), 
		G_MAX_IO_CHANS, conf::conf.samplerate, filename);

	wave->copyData(mixer::getRecBuffer());

	/* Update Channel with the new Wave. The function pushWave_ will take
	care of pushing it into the Wave stack first. */

	model::onSwap(model::channels, channelId, [&](Channel& c)
	{
		pushWave_(c, std::move(wave));
		setupChannelPostRecording_(c);
	});
#endif
}


/* -------------------------------------------------------------------------- */


/* overdubChannel_
Records the current Mixer audio input data into a channel with an existing
Wave, overdub mode. */

void overdubChannel_(ID channelId)
{
	ID waveId;
	model::onGet(model::channels, channelId, [&](Channel& c)
	{
		waveId = c.samplePlayer->getWaveId();
	});

	model::onGet(m::model::waves, waveId, [&](Wave& w)
	{
		w.addData(mixer::getRecBuffer());
		w.setLogical(true);
	});

	model::onGet(model::channels, channelId, [&](Channel& c)
	{
		setupChannelPostRecording_(c);
	});
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void init()
{
	mixer::init(clock::getFramesInLoop(), kernelAudio::getRealBufSize());

	channelManager::ChannelInfo infoOut = channelManager::createInfo(mixer::MASTER_OUT_CHANNEL_ID);
	channelManager::ChannelInfo infoIn  = channelManager::createInfo(mixer::MASTER_IN_CHANNEL_ID);
	channelManager::ChannelInfo infoPre = channelManager::createInfo(mixer::PREVIEW_CHANNEL_ID);

    model::swap([&](model::Layout& l) 
	{ 
		l.channels.clear();
		l.channels.push_back(channelManager::create(ChannelType::MASTER,  /*columnId=*/0, infoOut));
		l.channels.push_back(channelManager::create(ChannelType::MASTER,  /*columnId=*/0, infoIn));
		l.channels.push_back(channelManager::create(ChannelType::PREVIEW, /*columnId=*/0, infoPre));

	}, model::SwapType::NONE);
}


/* -------------------------------------------------------------------------- */


void close()
{
	mixer::disable();
}


/* -------------------------------------------------------------------------- */


void addChannel(ChannelType type, ID columnId)
{
	addChannel_(type, columnId);
}


/* -------------------------------------------------------------------------- */


int loadChannel(ID channelId, const std::string& fname)
{
	waveManager::Result res = createWave_(fname); 

	if (res.status != G_RES_OK) 
		return res.status;

	const Wave& wave = model::add<Wave>(std::move(res.wave));
	const Wave* old  = model::get().getChannel(channelId).samplePlayer->getWave();

    model::swap([channelId, &wave](model::Layout& l)
    {
        samplePlayer::loadWave(l.getChannel(channelId), &wave);
    }, model::SwapType::HARD);

	/* Remove old wave, if any. It is safe to do it now: the audio thread is
	already processing the new layout. */

	if (old != nullptr)
        model::remove<Wave>(*old);

	return res.status;
}


/* -------------------------------------------------------------------------- */


int addAndLoadChannel(ID columnId, const std::string& fname)
{
	waveManager::Result res = createWave_(fname);
	if (res.status == G_RES_OK)
		addAndLoadChannel(columnId, std::move(res.wave));
	return res.status;
}


void addAndLoadChannel(ID columnId, std::unique_ptr<Wave>&& w)
{
    /*
    const Wave&          wave    = model::add<Wave>(std::move(w));
    const channel::Data& channel = addChannel_(ChannelType::SAMPLE, columnId);

    model::swap([channelId = channel.id, &wave](model::Layout& l)
    {
       samplePlayer::loadWave(l.getChannel(channelId), &wave);
    }, model::SwapType::HARD);*/
}


/* -------------------------------------------------------------------------- */


void cloneChannel(ID channelId)
{
	model::ChannelsLock cl(model::channels);
	model::WavesLock    wl(model::waves);

	const Channel&           oldChannel = model::get(model::channels, channelId);
	std::unique_ptr<Channel> newChannel = channelManager::create(oldChannel);

	/* Clone plugins, actions and wave first in their own lists. */
	
#ifdef WITH_VST
	newChannel->pluginIds = pluginHost::clonePlugins(oldChannel.pluginIds);
#endif
	recorderHandler::cloneActions(channelId, newChannel->id);
	
	if (newChannel->samplePlayer && newChannel->samplePlayer->hasWave()) 
	{
		Wave& wave = model::get(model::waves, newChannel->samplePlayer->getWaveId());
		pushWave_(*newChannel, waveManager::createFromWave(wave, 0, wave.getSize()));
	}

	/* Then push the new channel in the channels list. */

	model::channels.push(std::move(newChannel));
}


/* -------------------------------------------------------------------------- */


void freeChannel(ID channelId)
{
#if 0
	const Channel_NEW* ch = model::getPtr<channel::Data>(channelId);

	assert(ch != nullptr);
	assert(ch->samplePlayer);

	const Wave* wave = ch->samplePlayer->getWave();

    model::swap([channelId](model::Layout& l)
    {
        assert(false);
    	//samplePlayer::loadWave(l.getChannel(channelId), nullptr);
    }, model::SwapType::HARD);

	if (wave != nullptr)
		model::remove<Wave>(*wave);
#endif
}


/* -------------------------------------------------------------------------- */


void freeAllChannels()
{
    model::swap([](model::Layout& l)
    {
        assert(false);
		//for (Channel_NEW& ch : l.channels)
    	//	if (ch.samplePlayer) samplePlayer::loadWave(ch, nullptr);
    }, model::SwapType::HARD);

	model::clear<Wave>();
}


/* -------------------------------------------------------------------------- */


void deleteChannel(ID channelId)
{
#if 0
	const Channel_NEW* ch = model::getPtr<Channel_NEW>(channelId);

	assert(ch != nullptr);

	const Wave*                wave    = ch->samplePlayer ? ch->samplePlayer->getWave() : nullptr;
#ifdef WITH_VST
	const std::vector<Plugin*> plugins = ch->plugins;
#endif

    model::swap([channelIndex = model::getIndex<Channel_NEW>(channelId)](model::Layout& l)
    {
    	// TODO - l.channels.erase(l.channels.begin() + channelIndex);
    }, model::SwapType::HARD);

	if (wave != nullptr)
		model::remove<Wave>(*wave);
#ifdef WITH_VST
	pluginHost::freePlugins(plugins);
#endif
#endif
}


/* -------------------------------------------------------------------------- */


void renameChannel(ID channelId, const std::string& name)
{
    model::swap([channelId, &name](model::Layout& l)
    {
    	l.getChannel(channelId).name = name;
    }, model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


void updateSoloCount()
{
	bool hasSolos = anyChannel_([](const channel::Data& ch)
	{
		return !ch.isInternal() && ch.solo;
	});

    model::swap([hasSolos](model::Layout& l)
    {
    	l.mixer.hasSolos = hasSolos;
    }, model::SwapType::NONE);
}


/* -------------------------------------------------------------------------- */


void setInToOut(bool v)
{
	model::swap([v](model::Layout& l) {	l.mixer.inToOut = v; }, model::SwapType::NONE);
}


/* -------------------------------------------------------------------------- */


float getInVol()
{
    return model::get().getChannel(mixer::MASTER_IN_CHANNEL_ID).volume;
}


float getOutVol()
{
    return model::get().getChannel(mixer::MASTER_OUT_CHANNEL_ID).volume;
}


bool getInToOut()
{
    return model::get().mixer.inToOut;
}


/* -------------------------------------------------------------------------- */

/* Push a new Wave into each recordable channel. Warning: this algorithm will 
require some changes when we will allow overdubbing (the previous existing Wave
has to be overwritten somehow). */

void finalizeInputRec()
{
	for (ID id : getRecordableChannels_())
		recordChannel_(id);
	for (ID id : getOverdubbableChannels_())
		overdubChannel_(id);

	mixer::clearRecBuffer();
}


/* -------------------------------------------------------------------------- */


bool hasInputRecordableChannels()
{
	return anyChannel_([](const channel::Data& ch) { return ch.canInputRec(); });
}


bool hasActionRecordableChannels()
{
	return anyChannel_([](const channel::Data& ch) { return ch.canActionRec(); });
}


bool hasLogicalSamples()
{
	return anyChannel_([](const channel::Data& ch)
	{ 
		return ch.samplePlayer && ch.samplePlayer->hasLogicalWave(); }
	);
}


bool hasEditedSamples()
{
	return anyChannel_([](const channel::Data& ch)
	{ 
		return ch.samplePlayer && ch.samplePlayer->hasEditedWave();
	});
}


bool hasActions()
{
	return anyChannel_([](const channel::Data& ch) { return ch.hasActions; });
}


bool hasAudioData()
{
	return anyChannel_([](const channel::Data& ch)
	{ 
		return ch.samplePlayer && ch.samplePlayer->hasWave();
	});
}
} // giada::m::mh::
