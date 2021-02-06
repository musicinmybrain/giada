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


#include <memory>
#include <algorithm>
#include <cassert>
#include "utils/log.h"
#include "core/model/model.h"
#include "core/action.h"
#include "core/idManager.h"
#include "core/recorder.h"


namespace giada {
namespace m {
namespace recorder
{
namespace
{
IdManager actionId_;


/* -------------------------------------------------------------------------- */


Action* findAction_(ActionMap& src, ID id)
{
	for (auto& [frame, actions] : src)
		for (Action& a : actions)
			if (a.id == id)
				return &a;
	assert(false);
	return nullptr;	
}


/* -------------------------------------------------------------------------- */

/* optimize
Removes frames without actions. */

void optimize_(ActionMap& map)
{
	for (auto it = map.cbegin(); it != map.cend();)
		it->second.size() == 0 ? it = map.erase(it) : ++it;
}


/* -------------------------------------------------------------------------- */


void removeIf_(std::function<bool(const Action&)> f)
{
    ActionMap& map = model::get().actions;

	for (auto& [frame, actions] : map)
		actions.erase(std::remove_if(actions.begin(), actions.end(), f), actions.end());
	optimize_(map);
	updateMapPointers(map);

    model::swap(model::SwapType::HARD);
}

/* -------------------------------------------------------------------------- */


bool exists_(ID channelId, Frame frame, const MidiEvent& event, const ActionMap& target)
{
	for (const auto& [_, actions] : target)
		for (const Action& a : actions) 
			if (a.channelId == channelId && a.frame == frame && a.event.getRaw() == event.getRaw())
				return true;
	return false;	
}


bool exists_(ID channelId, Frame frame, const MidiEvent& event)
{
	return exists_(channelId, frame, event, model::get().actions);
}
} // {anonymous}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


void init()
{
	actionId_ = IdManager();
	clearAll();
}


/* -------------------------------------------------------------------------- */


void clearAll()
{
    model::get().actions.clear();
    model::swap(model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


void clearChannel(ID channelId)
{
	removeIf_([=](const Action& a) { return a.channelId == channelId; });
}


/* -------------------------------------------------------------------------- */


void clearActions(ID channelId, int type)
{
	removeIf_([=](const Action& a)
	{ 
		return a.channelId == channelId && a.event.getStatus() == type;
	});
}


/* -------------------------------------------------------------------------- */


void deleteAction(ID id)
{
	removeIf_([=](const Action& a) { return a.id == id; });
}


void deleteAction(ID currId, ID nextId)
{
	removeIf_([=](const Action& a) { return a.id == currId || a.id == nextId; });
}


/* -------------------------------------------------------------------------- */


void updateKeyFrames(std::function<Frame(Frame old)> f)
{
	recorder::ActionMap temp;

	/* Copy all existing actions in local map by cloning them, with just a
	difference: they have a new frame value. */

	for (const auto& [oldFrame, actions] : model::get().actions) {
		Frame newFrame = f(oldFrame);
		for (const Action& a : actions) {
			Action copy = a;
			copy.frame = newFrame;
			temp[newFrame].push_back(copy);
		}
G_DEBUG(oldFrame << " -> " << newFrame);
	}

	updateMapPointers(temp);

	model::get().actions = temp;
    model::swap(model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


void updateEvent(ID id, MidiEvent e)
{
	findAction_(model::get().actions, id)->event = e;
    model::swap(model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


void updateSiblings(ID id, ID prevId, ID nextId)
{
	Action* pcurr = findAction_(model::get().actions, id);
	Action* pprev = findAction_(model::get().actions, prevId);
	Action* pnext = findAction_(model::get().actions, nextId);

	pcurr->prev   = pprev;
	pcurr->prevId = pprev->id;
	pcurr->next   = pnext;
	pcurr->nextId = pnext->id;

	if (pprev != nullptr) {
		pprev->next   = pcurr;
		pprev->nextId = pcurr->id;
	}
	if (pnext != nullptr) {
		pnext->prev   = pcurr;
		pnext->prevId = pcurr->id;
	}

	model::swap(model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


bool hasActions(ID channelId, int type)
{
	model::ActionsLock lock(model::actions);
	
	for (const auto& [frame, actions] : model::get().actions)
		for (const Action& a : actions)
			if (a.channelId == channelId && (type == 0 || type == a.event.getStatus()))
				return true;
	return false;
}


/* -------------------------------------------------------------------------- */


Action makeAction(ID id, ID channelId, Frame frame, MidiEvent e)
{
	Action out {actionId_.generate(id), channelId, frame, e, -1, -1};
	actionId_.set(id);
	return out;
}


Action makeAction(const patch::Action& a)
{
	actionId_.set(a.id);
	return Action {a.id, a.channelId, a.frame, a.event, -1, -1, a.prevId,
		a.nextId};
}


/* -------------------------------------------------------------------------- */


Action rec(ID channelId, Frame frame, MidiEvent event)
{
	/* Skip duplicates. */

	if (exists_(channelId, frame, event))
		return {};

	Action a = makeAction(0, channelId, frame, event);
	
	/* If key frame doesn't exist yet, the [] operator in std::map is smart 
	enough to insert a new item first. No plug-in data for now. */

	model::get().actions[frame].push_back(a);
	updateMapPointers(model::get().actions);
	
	model::swap(model::SwapType::HARD);

	return a;
}


/* -------------------------------------------------------------------------- */


void rec(std::vector<Action>& actions)
{
	if (actions.size() == 0)
		return;

	for (const Action& a : actions)
		if (!exists_(a.channelId, a.frame, a.event, model::get().actions))
			model::get().actions[a.frame].push_back(a);
	updateMapPointers(model::get().actions);

	model::swap(model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


void rec(ID channelId, Frame f1, Frame f2, MidiEvent e1, MidiEvent e2)
{
	ActionMap& map = model::get().actions;

	map[f1].push_back(makeAction(0, channelId, f1, e1));
	map[f2].push_back(makeAction(0, channelId, f2, e2));

	Action* a1 = findAction_(map, map[f1].back().id);
	Action* a2 = findAction_(map, map[f2].back().id);
	a1->nextId = a2->id;
	a2->prevId = a1->id;

	updateMapPointers(map);
	
	model::swap(model::SwapType::HARD);
}


/* -------------------------------------------------------------------------- */


const std::vector<Action>* getActionsOnFrame(Frame frame)
{
	if (model::get().actions.count(frame) == 0)
		return nullptr;
	return &model::get().actions.at(frame);
}


/* -------------------------------------------------------------------------- */


Action getClosestAction(ID channelId, Frame f, int type)
{
	Action out = {};
	forEachAction([&](const Action& a)
	{
		if (a.event.getStatus() != type || a.channelId != channelId)
			return;
		if (!out.isValid() || (a.frame <= f && a.frame > out.frame))
			out = a;
	});
	return out;
}


/* -------------------------------------------------------------------------- */


std::vector<Action> getActionsOnChannel(ID channelId)
{
	std::vector<Action> out;
	forEachAction([&](const Action& a)
	{
		if (a.channelId == channelId)
			out.push_back(a);
	});
	return out;
}


/* -------------------------------------------------------------------------- */


void updateMapPointers(ActionMap& src)
{
	for (auto& kv : src) {
		for (Action& action : kv.second) {
			if (action.nextId != 0)
				action.next = findAction_(src, action.nextId);
			if (action.prevId != 0)
				action.prev = findAction_(src, action.prevId);
		}
	}
}


/* -------------------------------------------------------------------------- */


void forEachAction(std::function<void(const Action&)> f)
{
	for (auto& [_, actions] : model::get().actions)
		for (const Action& action : actions)
			f(action);
}


/* -------------------------------------------------------------------------- */


ID getNewActionId()
{
	return actionId_.generate();
}
}}} // giada::m::recorder::
