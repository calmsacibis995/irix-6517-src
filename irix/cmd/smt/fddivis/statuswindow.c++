/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.9 $
 */

#include <tkTypes.h>
#include <stdParts.h>
#include <tkParentView.h>
#include <tkScrollBar.h>
#include <tkExec.h>
#include <BorderPntView.h>
#include <tkNotifier.h>
#include <tkPrompt.h>
#include <tkEvent.h>
#include <events.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <values.h>
#include "fddivis.h"
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <bstring.h>
#include <smt_parse.h>
#include <smt_subr.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif

const int AllocSize = 20; /* rows */
const int tblWidth = 3; /* cols */
static float tblWidths[DUMPCNT][tblWidth] = {
	{ 2.5, 3.0, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
	{ 2.5, 1.3, 3.0},
};
static char* Titles[DUMPCNT][tblWidth] = {
	{ "Station ID", "Neighbor station", "MAC address"},
	{ "Station ID", "TREQ(msec)", "MAC address"},
	{ "Station ID", "TNEG(msec)", "MAC address"},
	{ "Station ID", "TMAX(msec)", "MAC address"},
	{ "Station ID", "TVX(msec)", "MAC address"},
	{ "Station ID", "TMIN(msec)", "MAC address"},
	{ "Station ID", "SBA(80nsec)", "MAC address"},
	{ "Station ID", "Frame count", "MAC address"},
	{ "Station ID", "Error count", "MAC address"},
	{ "Station ID", "Lost count", "MAC address"},
	{ "Station ID", "LER", "MAC address"},
	{ "Station ID", "LEM count", "MAC address"},
	{ "Station ID", "Receive count", "MAC address"},
	{ "Station ID", "Transmit Ct", "MAC address"},
	{ "Station ID", "Not Copied Ct", "MAC address"},
	{ "Station ID", "EB Error Ct", "MAC address"},
};
static char* cmdnames[DUMPCNT] = {
	"MAC neighbor",
	"Treq",
	"Tneg",
	"Tmax",
	"Tvx",
	"Tmin",
	"Sync Bandwidth Allocation",
	"Frame Count",
	"Error Count",
	"Lost Count",
	"LER",
	"LEM Count",
	"Receive Count",
	"Transmit Count",
	"Not Copied Count",
	"EB Error Count",
};
static int deltaTbl[DUMPCNT] = {
	0, // MAC neighbor
	0, // Treq
	0, // Tneg
	0, // Tmax
	0, // Tvx
	0, // Tmin
	0, // Sync Bandwidth Allocation
	1, // Frame Count
	1, // Error Count
	1, // Lost Count
	0, // LER
	1, // LEM Count
	1, // Receive Count
	1, // Transmit Count
	1, // Not Copied Count
	1, // EB Error Count
};

/*
 * Containing window for analyzer application
 */
StatusWindow::StatusWindow()
{
	cmd = FV.statcmd;

	statTable = new titledTable(AllocSize, tblWidth);
	statTable->setColumns(tblWidths[cmd], Titles[cmd]);
	statTable->setFont("CourierBold11");
	statTable->setRowSelection(FALSE);
	statTable->setOneSelection(TRUE);
	statTable->leftJustify(0);
	statTable->leftJustify(1);
	statTable->leftJustify(2);
	addView(*statTable);

	// Status cmd name
	tkPen pen(BLUE,-1,-1,-1);
	Box2 box(0.0,0.0,100.0,100.0);
	StatusText = new tkLabel(box,pen,cmdnames[cmd]);
	StatusText->setjust();

	// Delta/Zero Button
	if (deltaTbl[cmd]) {
		updateButton = 0;

		modeButton = new tkRadioButton;
		modeButton->setClient(this);
		addAView(*modeButton);

		zeroButton = makePickButton("Reset",1,MENU_BTN_WD,MENU_BTN_HT);
		modeButton->addButton(*zeroButton);

		deltaButton = makePickButton("Delta",2,MENU_BTN_WD,MENU_BTN_HT);
		modeButton->addButton(*deltaButton);

		normalButton=makePickButton("Total",3,MENU_BTN_WD,MENU_BTN_HT);
		modeButton->addButton(*normalButton);

		holdButton=makePickButton("Freeze",4,MENU_BTN_WD,MENU_BTN_HT);
		modeButton->addButton(*holdButton);

		modeButton->setCurrentButton(normalButton);
	} else {
		modeButton = 0;

		// Update Button
		updateButton = makeMenuButton("Update",MENU_BTN_WD,MENU_BTN_HT);
		updateButton->setClient(this);
		addAView(*updateButton);
	}

	// Quit Button
	quitButton = makeMenuButton("Quit", MENU_BTN_WD, MENU_BTN_HT);
	quitButton->setClient(this);
	addAView(*quitButton);

	// Save Button
	saveButton = makeMenuButton("Save", MENU_BTN_WD, MENU_BTN_HT);
	saveButton->setClient(this);
	addAView(*saveButton);
	saveEdit = 0;
	offsaveEdit = 0;

	setTitle(cmdnames[cmd]);
#define RPX1 (float)(AEDGE)
#define RPX2 (float)((SCRXMAX-SCRYMAX-AEDGE)*2.7)
#define RPY2 (float)(SCRYMAX-((BNR+AEDGE)*2)-(RPX2-RPX1)/2)
#define RPY1 (float)(RPY2*1.0/3.0)
	prefposition(RPX1, cmd?RPX2:RPX2*1.3, RPY1, RPY2);
	setStartUpNoClose(TRUE);
	setStartUpNoQuit(TRUE);
	setHandleClose(TRUE);

	rnum = 0;
	hold = 0;
	delta = 0;
	sortary = 0;
}


StatusWindow::~StatusWindow()
{
	delete (titledTable*) statTable;
	if (modeButton) {
		delete (tkButton*)holdButton;
		delete (tkButton*)normalButton;
		delete (tkButton*)deltaButton;
		delete (tkButton*)zeroButton;
		delete (tkButton*)modeButton;
	}
	if (updateButton)
		delete (tkButton*)updateButton;
	if (saveEdit)
		delete (tkTextView*)saveEdit;
	if (offsaveEdit)
		delete (tkTextView*)offsaveEdit;
	delete (tkButton*)saveButton;
	delete (tkButton*)quitButton;
	delete (tkLabel*)StatusText;
}


void
StatusWindow::open()
{
	statTable->setClient(this);
	tkWindow::open();
	setIconTitle(cmdnames[cmd]);
	::winconstraints();

	FV.NeedOpSIF++;
	saveEvent = new tkTextViewEvent(tkEVENT_ACTION,this,
						getParentWindow()->getgid());
	tkGetEventManager()->expressInterest(saveEvent);
}

void
StatusWindow::close()
{
	if (FV.NeedOpSIF > 0)
		FV.NeedOpSIF--;
	if (sortary) {
		free(sortary);
		sortary = 0;
		rnum = 0;
	}
	tkGetEventManager()->loseInterest(saveEvent);
	tkWindow::close();
}

void
StatusWindow::redrawWindow()
{
	const float menuSpace = 2.5 * MENU_BTN_HT;

#ifdef CYPRESS_XGL
	if (gid == tkInvalidGID)
		return;
	tkCONTEXT savecx = tkWindow::pushContext(gid);
#else
	if (gid < 0)
		return;
	long savedgid = winget();
	winset(gid);
#endif

	reshapeviewport();
	updateScreenArea();

	ortho2(-0.5, screenArea.xlen+0.5, -0.5, screenArea.ylen+0.5);
	Box2 bb = getBounds();
	withStdColor(BG_ICOLOR) {
		sboxf(0.0, 0.0, bb.xlen, bb.ylen);
	}

	float ylen = bb.ylen - menuSpace;
	Box2 statBounds(0.0, bb.ylen - ylen, bb.xlen, ylen);
	statTable->setBounds(statBounds);
	statTable->resize();

	float y = (menuSpace-2.0 * MENU_BTN_HT) / 3.0;
	// directory text port
	Box2 txBounds(bb.xorg+20.0, 3.0*y+MENU_BTN_HT, bb.xlen, MENU_BTN_HT);
	if (saveEdit) {
		txBounds.xlen *= 0.2;
		StatusText->setstring("Save to file name: ");
		Box2 ttx(txBounds.xorg+txBounds.xlen, 2.0*y+MENU_BTN_HT,
			 bb.xlen*0.5, MENU_BTN_HT);
		saveEdit->setBounds(ttx);
	}
	StatusText->changeArea(txBounds);
	StatusText->draw();

	// Menu buttons
	Box2 menuBounds(bb.xlen-MENU_BTN_WD-3.0, y, MENU_BTN_WD, MENU_BTN_HT);
	quitButton->setBounds(menuBounds);
	menuBounds.xorg -= (MENU_BTN_WD+3.0);
	saveButton->setBounds(menuBounds);
	if (modeButton) {
		menuBounds.xorg -= (MENU_BTN_WD+3.0);
		holdButton->setBounds(menuBounds);
		menuBounds.xorg -= (MENU_BTN_WD+3.0);
		normalButton->setBounds(menuBounds);
		menuBounds.xorg -= (MENU_BTN_WD+3.0);
		deltaButton->setBounds(menuBounds);
		menuBounds.xorg -= (MENU_BTN_WD+3.0);
		zeroButton->setBounds(menuBounds);
	}
	if (updateButton) {
		menuBounds.xorg -= (MENU_BTN_WD+3.0);
		updateButton->setBounds(menuBounds);
	}

	tkParentView::paint();

	color(15);
	statBounds.outline();

#ifdef CYPRESS_XGL
	tkWindow::popContext(savecx);
#else
	if (savedgid >= 0)
		winset(savedgid);
#endif
}


void
StatusWindow::rcvEvent(tkEvent *e)
{
	time_t now;
	int v;

	switch(e->name()) {
	case tkEVENT_VALUECHANGE:
		if (e->sender() == modeButton) {
			hold = 0;
			((tkValueEvent*)e)->getValue(&v);
			switch (v) {
			case 1:
				dozero = 1;
				now = time(0);
				(void)strftime(ztds_buf,sizeof(ztds_buf),
					       "%b %d %T", localtime(&now));
				delta  = ZERO_FLAG;
				break;
			case 2:
				delta = DELTA_FLAG;
				break;
			case 3:
				delta = 0;
				dozero = 0;
				break;
			case 4:
				hold = 1;
				break;
			}
			if (saveEdit) {
				saveEdit->loseKeyboardFocus();
				removeAView(*saveEdit);
				offsaveEdit = saveEdit;
				saveEdit = 0;
			}
			setTbl();
			break;
		}
		if (e->sender() == updateButton) {
			setTbl();
			break;
		}
		if (e->sender() == saveButton) {
			saveEdit = offsaveEdit;
			offsaveEdit = 0;
			if (!saveEdit) {
				Box2 box(0.0,0.0,100.0,100.0);
				tkPen *tvPen = new tkPen(TVIEW_TEXT_ICOLOR,
					TVIEW_BG_ICOLOR, TVIEW_EDGE_ICOLOR,
					TVIEW_BORDER_LW);
				saveEdit = makeFancyTextView(saveBuf,
						sizeof(saveBuf),box,tvPen);
			}
			saveEdit->setClient(this);
			addAView(*saveEdit);
			redrawWindow();
		}
		if (e->sender() == quitButton) {
			tkEvent quit(FV_STATUS_OFF);
			FV.statcmd = cmd;
			FV.rcvEvent(&quit);
		}
		break;


	case tkEVENT_ACTION:
		if (e->sender() == saveEdit) {
			if (((tkTextViewEvent*)e)->getterminator()) {
				strcpy(saveBuf, saveEdit->gettext());
				saveEdit->loseKeyboardFocus();
				saveTbl();
				removeAView(*saveEdit);
				offsaveEdit = saveEdit;
				saveEdit = 0;
				setTbl();
				break;
			}
			saveEdit->setText(saveBuf);
		}
		break;

	default:
		tkWindow::rcvEvent(e);
	}
}

#define PRF(ptr,val) \
	if (!r->ptr) { \
		sprintf(saa,"%s", "unknown"); \
		break; } \
	if (delta&DELTA_FLAG) \
		sprintf(saa,"%-3.3f", \
			FROMBCLK(r->ptr->val-r->zdump[cmd].d_f)); \
	else if (delta&ZERO_FLAG) \
		sprintf(saa,"%-3.3f", \
			FROMBCLK(r->ptr->val-r->zdump[cmd].z_f)); \
	else \
		sprintf(saa,"%-3.3f", FROMBCLK(r->ptr->val)); \
	r->zdump[cmd].d_f = r->ptr->val; \
	break

#define PRU(ptr,val) \
	if (!r->ptr) { \
		sprintf(saa,"%s", "unknown"); \
		break; } \
	if (delta&DELTA_FLAG) \
		sprintf(saa,"%-10u", r->ptr->val - r->zdump[cmd].d_u); \
	else if (delta&ZERO_FLAG) \
		sprintf(saa,"%-10u", r->ptr->val - r->zdump[cmd].z_u); \
	else \
		sprintf(saa,"%-10u", r->ptr->val); \
	r->zdump[cmd].d_u = r->ptr->val; \
	break

void
StatusWindow::setATbl(RING *rp, int seq, int smac)
{
	SINFO *r;
	char saa[128];

	// Set Item 0
	if (smac && cmd == DUMP_RING) {
		statTable->setEntry(seq, 0, " ");
	} else {
		if (FV.FddiBitMode != 0)
			strncpy(saa, sidtoa(&rp->sid), sizeof(saa));
		else
			(void)fddi_sidtoa(&rp->sid, saa);
		statTable->setEntry(seq, 0, &saa[0]);
	}

	// Set Item 1
	r = (smac == 0) ? &rp->sm : &rp->dm;
	switch (cmd) {
		case DUMP_RING:
			if (!FV.SymAdrMode||fddi_ntohost(&r->una,saa) != 0) {
				if (FV.FddiBitMode != 0)
					strcpy(saa,midtoa(&r->una));
				else
					fddi_ntoa(&r->una, saa);
			}
			break;
		case DUMP_TREQ: PRF(mac_stat, treq);
		case DUMP_TNEG: PRF(mac_stat, tneg);
		case DUMP_TMAX: PRF(mac_stat, tmax);
		case DUMP_TVX:  PRF(mac_stat, tvx);
		case DUMP_TMIN: PRF(mac_stat, tmin);
		case DUMP_SBA:  PRU(mac_stat, sba);
		case DUMP_FRAME:PRU(mac_stat, frame_ct);
		case DUMP_ERR:  PRU(mac_stat, error_ct);
		case DUMP_LOST: PRU(mac_stat, lost_ct);
		case DUMP_LER:  PRU(ler,      ler_estimate);
		case DUMP_LEM:  PRU(ler,      lem_ct);
		case DUMP_RECV: PRU(mfc,      recv_ct);
		case DUMP_XMIT: PRU(mfc,      xmit_ct);
		case DUMP_NCC:  PRU(mfncc,    notcopied_ct);
		case DUMP_EBS:  PRU(ebs,      eberr_ct);
	}
	statTable->setEntry(seq, 1, &saa[0]);

	// Set Item 2
	if (!FV.SymAdrMode||fddi_ntohost(&r->sa,saa) != 0) {
		if (FV.FddiBitMode != 0)
			strcpy(saa,midtoa(&r->sa));
		else
			fddi_ntoa(&r->sa, saa);
	}
	statTable->setEntry(seq, 2, &saa[0]);
}

#define SS(ptr, fld) \
	if (r->ptr) { if (!r1->ptr) doit = 1; \
		else if (r->ptr->fld > r1->ptr->fld) doit = 1; } \
	break

#define DZF(ptr,val) \
	if (r->ptr) \
		r->zdump[cmd].z_f = r->ptr->val; \
	break
#define DZU(ptr,val) \
	if (r->ptr) \
		r->zdump[cmd].z_u = r->ptr->val; \
	break

#define SSU(ptr, fld) \
	if (r->ptr) { \
		if (!r1->ptr) \
			doit = 1; \
		else if (delta & DELTA_FLAG) { \
			if ((r->ptr->fld - r->zdump[cmd].d_u) > \
			    (r1->ptr->fld- r1->zdump[cmd].d_u)) \
				doit = 1; \
		} else if (delta & ZERO_FLAG) { \
			if ((r->ptr->fld - r->zdump[cmd].z_u) > \
			    (r1->ptr->fld- r1->zdump[cmd].z_u)) \
				doit = 1; \
		} else if (r->ptr->fld > r1->ptr->fld) { \
			doit = 1; \
		} \
	} \
	break

#define SSF(ptr, fld) \
	if (r->ptr) { \
		if (!r1->ptr) \
			doit = 1; \
		else if (delta & DELTA_FLAG) { \
			if ((r->ptr->fld - r->zdump[cmd].d_f) > \
			    (r1->ptr->fld- r1->zdump[cmd].d_f)) \
				doit = 1; \
		} else if (delta & ZERO_FLAG) { \
			if ((r->ptr->fld - r->zdump[cmd].z_f) > \
			    (r1->ptr->fld- r1->zdump[cmd].z_f)) \
				doit = 1; \
		} else if (r->ptr->fld > r1->ptr->fld) { \
			doit = 1; \
		} \
	} \
	break

void
StatusWindow::sortATbl(int tot)
{
	STAB tmp;
	SINFO *r, *r1;
	int i, j, doit;

	if (dozero && deltaTbl[cmd]) {
		dozero = 0;
		for (i = 0; i < tot; i++) {
			r = (sortary[i].smac == 0) ?
				&sortary[i].rp->sm : &sortary[i].rp->dm;
			switch (cmd) {
			case DUMP_TREQ: DZF(mac_stat, treq);
			case DUMP_TNEG: DZF(mac_stat, tneg);
			case DUMP_TMAX: DZF(mac_stat, tmax);
			case DUMP_TVX:  DZF(mac_stat, tvx);
			case DUMP_TMIN: DZF(mac_stat, tmin);
			case DUMP_SBA:  DZU(mac_stat, sba);
			case DUMP_FRAME:DZU(mac_stat, frame_ct);
			case DUMP_ERR:  DZU(mac_stat, error_ct);
			case DUMP_LOST: DZU(mac_stat, lost_ct);
			case DUMP_LER:  DZU(ler, ler_estimate);
			case DUMP_LEM:  DZU(ler, lem_ct);
			case DUMP_RECV: DZU(mfc, recv_ct);
			case DUMP_XMIT: DZU(mfc, xmit_ct);
			case DUMP_NCC:  DZU(mfncc, notcopied_ct);
			case DUMP_EBS:  DZU(ebs, eberr_ct);
			}
		}
	}

	if (tot <= 1)
		return;
	tot--;
	for (i = 0; i < tot; i++) {
		for (j = 0; j < tot-i; j++) {
			doit = 0;
			r = (sortary[j].smac == 0) ?
				&sortary[j].rp->sm : &sortary[j].rp->dm;
			r1 =(sortary[j+1].smac == 0)?
				&sortary[j+1].rp->sm:&sortary[j+1].rp->dm;
			switch (cmd) {
			case DUMP_TREQ: SSF(mac_stat, treq);
			case DUMP_TNEG: SSF(mac_stat, tneg);
			case DUMP_TMAX: SSF(mac_stat, tmax);
			case DUMP_TVX:  SSF(mac_stat, tvx);
			case DUMP_TMIN: SSF(mac_stat, tmin);
			case DUMP_SBA:  SSU(mac_stat, sba);
			case DUMP_FRAME:SSU(mac_stat, frame_ct);
			case DUMP_ERR:  SSU(mac_stat, error_ct);
			case DUMP_LOST: SSU(mac_stat, lost_ct);
			case DUMP_LER:  SSU(ler, ler_estimate);
			case DUMP_LEM:  SSU(ler, lem_ct);
			case DUMP_RECV: SSU(mfc, recv_ct);
			case DUMP_XMIT: SSU(mfc, xmit_ct);
			case DUMP_NCC:  SSU(mfncc, notcopied_ct);
			case DUMP_EBS:  SSU(ebs, eberr_ct);
			}
			if (doit) {
				tmp = sortary[j];
				sortary[j] = sortary[j+1];
				sortary[j+1] = tmp;
			}
		}
	}
}

void
StatusWindow::setTbl()
{
	RING *rp;
	char buf[128];
	int i, tot, tmprnum;

	tot = FV.RingNum;
	if (cmd == DUMP_RING) {
		for (rp = FV.ring, i = 0; i < tot; i++, rp = rp->next) {
			setATbl(rp, i, 0);
			if (rp->smac) {
				i++;
				tot++;
				setATbl(rp, i, 1);
			}
		}
		lastnum = tot;
	} else {
		tmprnum = tot * 2;
		if (sortary == NULL) {
			sortary = (STAB*)Malloc(tmprnum*sizeof(*sortary));
			rnum = tmprnum;
		} else if (rnum < tmprnum) {
			STAB *tst = (STAB*)Malloc(tmprnum*sizeof(STAB));
			bcopy((char*)sortary,(char*)tst,rnum*sizeof(STAB));
			free(sortary);
			sortary = tst;
			rnum = tmprnum;
		}

		// Build sort table.
		for (rp = FV.ring, i = 0; i < tot; i++, rp = rp->next) {
			sortary[i].rp = rp;
			sortary[i].smac = 0;
			if (rp->smac) {
				i++;
				tot++;
				sortary[i].rp = rp;
				sortary[i].smac = 1;
			}
		}
		sortATbl(tot);

		if (lastnum != tot) {
			if (lastnum > tot)
				statTable->clear();
			lastnum = tot;
		}
		for (i = 0; i < tot; i++)
			setATbl(sortary[i].rp, i, sortary[i].smac);
	}

	getStatusText(buf);
	StatusText->setstring(buf);
	redrawWindow();
}

void
StatusWindow::getStatusText(char *buf)
{
	time_t now;

	now = time(0);
	(void)strcpy(ptds_buf, tds_buf);
	(void)strftime(ts_buf,sizeof(ts_buf),"%T",localtime(&now));
	(void)strftime(tds_buf,sizeof(tds_buf),"%b %d %T",localtime(&now));

	if (delta & ZERO_FLAG) {
		sprintf(buf,"%s from %s to %s",
			cmdnames[cmd], ztds_buf, ts_buf);
	} else if (delta & DELTA_FLAG) {
		sprintf(buf, "%s from %s to %s",
			cmdnames[cmd], ptds_buf, ts_buf);
	} else {
		sprintf(buf, "%s at %s",
			cmdnames[cmd], tds_buf);
	}
}

/*
 * Update value only.
 */
void
StatusWindow::updateTbl()
{
	char buf[128];
	register int i;

	if (updateButton || hold)
		return;

	if (FV.recal) {
		setTbl();
		return;
	}

#ifdef CYPRESS_XGL
	if (gid == tkInvalidGID)
		return;
	tkCONTEXT savecx = tkWindow::pushContext(gid);
#else
	if (gid < 0)
		return;
	long savedgid = winget();
	winset(gid);
#endif

	for (i = 0; i < lastnum; i++) {
		SINFO *r;
		char saa[128];

		if (sortary[i].smac)
			r = &sortary[i].rp->dm;
		else
			r = &sortary[i].rp->sm;
		switch (cmd) {
			case DUMP_RING:
				break;
			case DUMP_TREQ: PRF(mac_stat, treq);
			case DUMP_TNEG: PRF(mac_stat, tneg);
			case DUMP_TMAX: PRF(mac_stat, tmax);
			case DUMP_TVX:  PRF(mac_stat, tvx);
			case DUMP_TMIN: PRF(mac_stat, tmin);
			case DUMP_SBA:  PRU(mac_stat, sba);
			case DUMP_FRAME:PRU(mac_stat, frame_ct);
			case DUMP_ERR:  PRU(mac_stat, error_ct);
			case DUMP_LOST: PRU(mac_stat, lost_ct);
			case DUMP_LER:  PRU(ler,      ler_estimate);
			case DUMP_LEM:  PRU(ler,      lem_ct);
			case DUMP_RECV: PRU(mfc,      recv_ct);
			case DUMP_XMIT: PRU(mfc,      xmit_ct);
			case DUMP_NCC:  PRU(mfncc,    notcopied_ct);
			case DUMP_EBS:  PRU(ebs,      eberr_ct);
		}
		statTable->setEntry(i, 1, &saa[0]);
		statTable->repaint(i, 1);
	}

	getStatusText(buf);

	// If the "where do you want to save it?" question is not
	// being asked, then upate the title of the screen
	if (!saveEdit) {
		// clear text area first
		Box2 bb = getBounds();
		float y = MENU_BTN_HT / 3.0;
		withStdColor(BG_ICOLOR) {
			sboxf(bb.xorg+20.0, y+MENU_BTN_HT,
				bb.xorg+bb.xlen*0.9, y+MENU_BTN_HT*2.0);
		}
		StatusText->setstring(buf);
		StatusText->draw();
	}

#ifdef CYPRESS_XGL
	tkWindow::popContext(savecx);
#else
	if (savedgid >= 0)
		winset(savedgid);
#endif
}

/*
 * Save into a file.
 */
void
StatusWindow::saveTbl()
{
	FILE *f;
	char buf[128];
	int sts = 0;
	register int i;

	if ((f = fopen(saveBuf, "w")) == NULL) {
		sts = 1;
		sprintf(buf, "open %s failed", saveBuf);
		FV.post(FILEACCERRMSG, buf);
		return;
	}

	if (0 > fprintf(f, "%s at %s\n\n", cmdnames[cmd], tds_buf)) {
		sts = 1;
		fclose(f);
		sprintf(buf, "write %s failed", saveBuf);
	}

	for (i = 0; i < lastnum && !sts; i++) {
		char *e0, *e1, *e2;

		statTable->getEntry(i, 0, &e0);
		statTable->getEntry(i, 1, &e1);
		statTable->getEntry(i, 2, &e2);
		sprintf(buf, "%s\t%s\t%s\n", e0, e1, e2);
		if (fwrite(buf,1,strlen(buf), f) != strlen(buf)) {
			sts = 1;
			sprintf(buf, "write %s failed", saveBuf);
		}
		fflush(f);
	}

	fflush(f);
	fclose(f);
	if (sts)
		FV.post(FILEACCERRMSG, buf);
}
