/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * gd_beatsInput
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2017 Giovanni A. Zuliani | Monocasual
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


#include "../../utils/gui.h"
#include "../../core/mixer.h"
#include "../../core/clock.h"
#include "../../core/conf.h"
#include "../../core/const.h"
#include "../../glue/main.h"
#include "../elems/ge_mixed.h"
#include "gd_beatsInput.h"
#include "gd_mainWindow.h"


extern Mixer				 G_Mixer;
extern gdMainWindow *mainWin;


using namespace giada;


gdBeatsInput::gdBeatsInput()
	: gWindow(164, 60, "Beats")
{
	if (conf::beatsX)
		resize(conf::beatsX, conf::beatsY, w(), h());

	set_modal();

	beats     = new gInput(8,  8,  35, 20);
	bars      = new gInput(47, 8,  35, 20);
	ok 		    = new gClick(86, 8,  70, 20, "Ok");
	resizeRec = new gCheck(8,  40, 12, 12, "resize recorded actions");
	end();

	char buf_bars[3]; sprintf(buf_bars, "%d", clock::getBars());
	char buf_beats[3]; sprintf(buf_beats, "%d", clock::getBeats());
	beats->maximum_size(2);
	beats->value(buf_beats);
	beats->type(FL_INT_INPUT);
	bars->maximum_size(2);
	bars->value(buf_bars);
	bars->type(FL_INT_INPUT);
	ok->shortcut(FL_Enter);
	ok->callback(cb_update_batt, (void*)this);
	resizeRec->value(conf::resizeRecordings);

	gu_setFavicon(this);
	setId(WID_BEATS);
	show();
}


/* ------------------------------------------------------------------ */


gdBeatsInput::~gdBeatsInput()
{
	conf::beatsX = x();
	conf::beatsY = y();
	conf::resizeRecordings =	resizeRec->value();
}


/* ------------------------------------------------------------------ */


void gdBeatsInput::cb_update_batt(Fl_Widget *w, void *p) { ((gdBeatsInput*)p)->__cb_update_batt(); }


/* ------------------------------------------------------------------ */


void gdBeatsInput::__cb_update_batt()
{
	if (!strcmp(beats->value(), "") || !strcmp(bars->value(), ""))
		return;
	glue_setBeats(atoi(beats->value()), atoi(bars->value()), resizeRec->value());
	do_callback();
}
