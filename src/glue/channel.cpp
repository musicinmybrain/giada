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


#include <functional>
#include <cmath>
#include <cassert>
#include <FL/Fl.H>
#include "gui/dialogs/mainWindow.h"
#include "gui/dialogs/sampleEditor.h"
#include "gui/dialogs/warnings.h"
#include "gui/elems/basics/input.h"
#include "gui/elems/basics/dial.h"
#include "gui/elems/sampleEditor/waveTools.h"
#include "gui/elems/sampleEditor/volumeTool.h"
#include "gui/elems/sampleEditor/boostTool.h"
#include "gui/elems/sampleEditor/panTool.h"
#include "gui/elems/sampleEditor/pitchTool.h"
#include "gui/elems/sampleEditor/rangeTool.h"
#include "gui/elems/sampleEditor/waveform.h"
#include "gui/elems/mainWindow/keyboard/keyboard.h"
#include "gui/elems/mainWindow/keyboard/channel.h"
#include "gui/elems/mainWindow/keyboard/sampleChannel.h"
#include "gui/elems/mainWindow/keyboard/channelButton.h"
#include "utils/gui.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "core/model/model.h"
#include "core/kernelAudio.h"
#include "core/mixerHandler.h"
#include "core/mixer.h"
#include "core/clock.h"
#include "core/plugins/pluginHost.h"
#include "core/conf.h"
#include "core/wave.h"
#include "core/recorder.h"
#include "core/recManager.h"
#include "core/plugins/plugin.h"
#include "core/waveManager.h"
#include "main.h"
#include "channel.h"


extern giada::v::gdMainWindow* G_MainWin;


namespace giada::c::channel 
{
namespace
{
void printLoadError_(int res)
{
	if      (res == G_RES_ERR_WRONG_DATA)
		v::gdAlert("Multichannel samples not supported.");
	else if (res == G_RES_ERR_IO)
		v::gdAlert("Unable to read this sample.");
	else if (res == G_RES_ERR_PATH_TOO_LONG)
		v::gdAlert("File path too long.");
	else if (res == G_RES_ERR_NO_DATA)
		v::gdAlert("No file specified.");
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


// TODO - just pass const channel::Data&
SampleData::SampleData(const m::samplePlayer::Data& s, const m::audioReceiver::Data& a)
: waveId         (s.getWaveId())
, mode           (s.mode)
, isLoop         (s.isAnyLoopMode())
, pitch          (s.pitch)
, m_samplePlayer (&s)
, m_audioReceiver(&a)
{
}


Frame SampleData::getTracker() const           { return 0; /* TODO */ }
Frame SampleData::getBegin() const             { return m_samplePlayer->begin; }
Frame SampleData::getEnd() const               { return m_samplePlayer->end; }
bool  SampleData::getInputMonitor() const      { return m_audioReceiver->inputMonitor; }
bool  SampleData::getOverdubProtection() const { return m_audioReceiver->overdubProtection; }


/* -------------------------------------------------------------------------- */


MidiData::MidiData(const m::midiSender::Data& m)
: m_midiSender(&m)
{
}

bool MidiData::isOutputEnabled() const { return m_midiSender->enabled; }
int  MidiData::getFilter() const       { return m_midiSender->filter; }


/* -------------------------------------------------------------------------- */


Data::Data(const m::channel::Data& c)
: id         (c.id)
, columnId   (c.columnId)
#ifdef WITH_VST
, plugins    (c.plugins)
#endif
, type       (c.type)
, height     (c.height)
, name       (c.name)
, volume     (c.volume)
, pan        (c.pan)
, key        (c.key)
, hasActions (c.hasActions)
, m_channel  (c)
{
	if (c.type == ChannelType::SAMPLE)
		sample = std::make_optional<SampleData>(*c.samplePlayer, *c.audioReceiver);
	else
	if (c.type == ChannelType::MIDI)
		midi   = std::make_optional<MidiData>(*c.midiSender);
}


bool          Data::getSolo() const           { return m_channel.solo; }
bool          Data::getMute() const           { return m_channel.mute; }
ChannelStatus Data::getPlayStatus() const     { return m_channel.state->playStatus.load(); }
ChannelStatus Data::getRecStatus() const      { return m_channel.state->recStatus.load(); }
bool          Data::getReadActions() const    { return m_channel.readActions; }
bool          Data::isArmed() const           { return m_channel.armed; }
bool          Data::isRecordingInput() const  { return m::recManager::isRecordingInput(); }
bool          Data::isRecordingAction() const { return m::recManager::isRecordingAction(); }


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


Data getData(ID channelId)
{
    assert(false);
	//return Data(m::model::get().getChannel(channelId));
}


std::vector<Data> getChannels()
{
	std::vector<Data> out;
	for (const m::channel::Data& ch : m::model::get().channels)
		if (!ch.isInternal())
			out.push_back(Data(ch));
	return out;
}


/* -------------------------------------------------------------------------- */


int loadChannel(ID channelId, const std::string& fname)
{
	/* Save the patch and take the last browser's dir in order to re-use it the 
	next time. */

	m::conf::conf.samplePath = u::fs::dirname(fname);

	int res = m::mh::loadChannel(channelId, fname);
	if (res != G_RES_OK)
		printLoadError_(res);
	
	return res;
}


/* -------------------------------------------------------------------------- */


void addChannel(ID columnId, ChannelType type)
{
	m::mh::addChannel(type, columnId);
}


/* -------------------------------------------------------------------------- */


void addAndLoadChannel(ID columnId, const std::string& fpath)
{
	int res = m::mh::addAndLoadChannel(columnId, fpath);
	if (res != G_RES_OK)
		printLoadError_(res);
}


void addAndLoadChannels(ID columnId, const std::vector<std::string>& fpaths)
{
	if (fpaths.size() == 1)
		return addAndLoadChannel(columnId, fpaths[0]);

	bool errors = false;
	for (const std::string& f : fpaths)
		if (m::mh::addAndLoadChannel(columnId, f) != G_RES_OK)
			errors = true;

	if (errors)
		v::gdAlert("Some files weren't loaded sucessfully.");
}


/* -------------------------------------------------------------------------- */


void deleteChannel(ID channelId)
{
	if (!v::gdConfirmWin("Warning", "Delete channel: are you sure?"))
		return;
	u::gui::closeAllSubwindows();
	m::recorder::clearChannel(channelId);
	m::mh::deleteChannel(channelId);
}


/* -------------------------------------------------------------------------- */


void freeChannel(ID channelId)
{
	if (!v::gdConfirmWin("Warning", "Free channel: are you sure?"))
		return;
	u::gui::closeAllSubwindows();
	m::recorder::clearChannel(channelId);
	m::mh::freeChannel(channelId);
}


/* -------------------------------------------------------------------------- */


void setInputMonitor(ID channelId, bool value)
{
	m::model::get().getChannel(channelId).audioReceiver->inputMonitor = value;
    m::model::swap(m::model::SwapType::SOFT);
}


/* -------------------------------------------------------------------------- */


void setOverdubProtection(ID channelId, bool value)
{
    /*
    m::model::swap([channelId, value](m::model::Layout& l)
    {
		m::Channel_NEW& channel = l.getChannel(channelId);
		channel.audioReceiver->overdubProtection = value;
		if (value == true && channel.armed)
			channel.armed = false;
    }, m::model::SwapType::SOFT);*/
}


/* -------------------------------------------------------------------------- */


void cloneChannel(ID channelId)
{
	m::mh::cloneChannel(channelId);
}


/* -------------------------------------------------------------------------- */


void setSamplePlayerMode(ID channelId, SamplePlayerMode mode)
{
	m::model::get().getChannel(channelId).samplePlayer->mode = mode;
    m::model::swap(m::model::SwapType::HARD); // TODO - SOFT should be enough, fix geChannel refresh method
	u::gui::refreshActionEditor();
}


/* -------------------------------------------------------------------------- */


void setHeight(ID channelId, Pixel p)
{
	m::model::get().getChannel(channelId).height = p;
    m::model::swap(m::model::SwapType::SOFT);
}


/* -------------------------------------------------------------------------- */


void setName(ID channelId, const std::string& name)
{
	m::mh::renameChannel(channelId, name);
}
} // giada::c::channel::
