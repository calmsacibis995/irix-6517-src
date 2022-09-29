/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.19 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <errno.h>
#include <libc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "fddivis.h"
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <cf.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif

FddiVis FV;

#ifdef CYPRESS_XGL
#include "Xutil.h"
#include "Xatom.h"
#include "MwmUtil.h"
extern Display *Xdpy;
#define ARROW		0
static	Cursor	HOURGLASS;
#define HGCURSOR	XC_box_spiral
/*
static Pixmap HGCURSOR = {
			0x7FFE,0x4002,0x200C,0x1A38,
			0x0FF0,0x07E0,0x03C0,0x0180,
			0x0180,0x0240,0x0520,0x0810,
			0x1108,0x23C4,0x47E2,0x7FFE
};
*/
#else
#define ARROW		0
#define HOURGLASS	1
static Cursor hour = {
		0x7FFE,0x4002,0x200C,0x1A38,
		0x0FF0,0x07E0,0x03C0,0x0180,
		0x0180,0x0240,0x0520,0x0810,
		0x1108,0x23C4,0x47E2,0x7FFE
};
#endif

static char *Message[] = {
/* 0 */  "Save fddivis config before quitting as:",
/* 1 */  "Record fddivis data in directory:",
/* 2 */  "Could not save data file!",
/* 3 */  "Replay fddivis data from directory:",
/* 4 */  "Could not read data file!",
/* 5 */  "Bad program or version in data file!",
/* 6 */  "??",  /* unused */
/* 7 */  "Could not find that host!",
/* 8 */  "Find which host?",
/* 9 */  "SMT Ping which host?",
/* 10 */ "Could not start smtping!",
/* 11 */ "Save fddivis image as:",
/* 12 */ "Must install std.sw.demos /usr/sbin/scrsave to save.",
/* 13 */ "Could not save to image file!",
/* 14 */ "Could not save config data!",
/* 15 */ "Service not supported yet!",
/* 16 */ "Directory access error!",
/* 17 */ "File access error!",
/* 18 */ "Restart ring map with IP hostname:",
/* 19 */ "Cannot access the SMT agent:",
/* 20 */ " ",
};

#define USAGE "usage: fddivis [-c ConfFile] [-r RecordDir] [hostname]\n"
void
FddiVis::appInit(tkApp *tk, int argc, char *argv[], char *env[])
{
	char *home;
	int i;
	extern char* optarg;
	extern int optind, opterr;


	if ((home = getenv("HOME")) == NULL)
		return;

	if (home[strlen(home)-1] != '/')
		sprintf(ConfFile,"%s/%s", home, RCFILE);
	else
		sprintf(ConfFile,"%s%s", home, RCFILE);

	// scan early to get the control file name, and detect errors
	opterr = 0;
	while ((i = getopt(argc, argv, "c:r:")) != EOF) {
		switch (i) {
		case 'c':
			strncpy(ConfFile, optarg, sizeof(ConfFile)-1);
			break;
		case 'r':
			break;
		default:
			fputs(USAGE, stderr);
			exit(1);
		}
	}
	if (optind+1 < argc) {
		fputs(USAGE, stderr);
		exit(1);
	}

	readConf();
	// scan again to override the choice of control file
	optind = 1;
	while ((i = getopt(argc, argv, "c:r:")) != EOF) {
		switch (i) {
		case 'c':
			break;
		case 'r':
			strncpy(RecordDir, optarg, sizeof(RecordDir)-1);
			Drecord = RecordDir[0] != '\0';
			break;
		}
	}
	if (optind < argc)
		strncpy(Agent, argv[optind], sizeof(Agent)-1);

	if (getenv("DEBUG"))
		foreground();
	if (getenv("BYPASSSMTD"))
		 BypassSmtd = 1;
	if (getenv("SAVESMTD"))
		 SaveSmtd = 1;

	tkApp::appInit(tk,argc,argv,env);

	deflinestyle(DASHED,0xF0F0);
	deflinestyle(DOTTED,0xCCCC);
#ifdef CYPRESS_XGL
	XColor pc;
	pc.pixel = 1;
	pc.red = ~0;
	pc.green = 0;
	pc.blue = 0;
	pc.flags = 0xff;
	pc.pad = 0;
#else
	defcursor(HOURGLASS,hour);
#endif

// UI Window
	UIWindow = new UserInterfaceWindow;
	UIWindow->open();
	addWindow(UIWindow);
#ifdef CYPRESS_XGL
	HOURGLASS = UIWindow->defineFontCursor(HGCURSOR);
	XRecolorCursor(Xdpy, HOURGLASS, &pc, &pc);
#endif
	UIWindow->setCursor(HOURGLASS);

// Map Window
	MAPWindow = new MapWindow;
	MAPWindow->doublebuffer();
	MAPWindow->open();
	addWindow(MAPWindow);
#ifdef CYPRESS_XGL
	HOURGLASS = MAPWindow->defineFontCursor(HGCURSOR);
	XRecolorCursor(Xdpy, HOURGLASS, &pc, &pc);
#endif
	MAPWindow->setCursor(HOURGLASS);

// Ring Window
	RINGWindow = new RingWindow;
	RINGWindow->doublebuffer();
	RINGWindow->open();
	addWindow(RINGWindow);
#ifdef CYPRESS_XGL
	HOURGLASS = RINGWindow->defineFontCursor(HGCURSOR);
	XRecolorCursor(Xdpy, HOURGLASS, &pc, &pc);
#endif
	RINGWindow->setCursor(HOURGLASS);

	// init fonts
#ifdef CYPRESS_XGL
	font1 = tkGetFont("HlvB", 20.0);
	if (font1 == 0) {
		printf("can't get font1\n");
		exit(-1);
	}
	font2 = tkGetFont("HlvB", 14.0);
	if (font2 == 0) {
		printf("can't get font2\n");
		exit(-1);
	}
#else
	fminit();
	font0 = fmfindfont("HlvB");
	font1 = fmscalefont(font0,20.0);
	font2 = fmscalefont(font0,14.0);
	fmsetfont(font1);
#endif
	fontInit();
	::vn_init();


// Ring process
	Frame.init_frame();
	tkAddTranslator(smtsock, recvframe);

	eventManager = tkGetEventManager();

#ifdef CYPRESS_XGL
	notifyEvent = new tkValueEvent;
	notifyEvent->setClient(this);
#else
	notifyEvent = new tkValueEvent(NOTIFY_CLOSE,this);
#endif
	eventManager->expressInterest(notifyEvent);

#ifdef CYPRESS_XGL
	destroyEvent = new tkValueEvent;
	destroyEvent->setClient(this);
#else
	destroyEvent = new tkValueEvent(NOTIFY_DESTROY,this);
#endif
	eventManager->expressInterest(destroyEvent);

	timerEvent = new tkEvent(tkEVENT_TIMER_EXPIRED,this);
	eventManager->expressInterest(timerEvent);

	timer = new tkTimer;
	startTimer();

	UIWindow->setCursor(ARROW);
	MAPWindow->setCursor(ARROW);
	RINGWindow->setCursor(ARROW);

	if (Drecord)
		RINGWindow->RINGView->RecordOn();
}

void
Perror(int level,char* s)
{
	int err;
	char buf[256];

	err = errno;
	sprintf(buf,"%s: %s",PROGRAM,s);
	errno = err;
	perror(buf);
	if (level != 0) {
		fr_finish();
		exit(level);
	}
}

static time_t old_secs = 0;
static time_t sort_secs = 0;
static time_t svcreged_secs = 0;
static time_t status_secs = 0;

void
FddiVis::rcvEvent(tkEvent *e)
{
	time_t new_secs;
	register tkObject *sender = e->sender();

	switch(e->name()) {

	case tkEVENT_TIMER_EXPIRED:
		startTimer();
		if (ShowToken)
			RINGWindow->drawToken();

		// do nothing more than move the token more
		// often than once a second
		new_secs = time(0);
		if (new_secs == old_secs)
			return;

		if (!Dreplay) {
			int doupdate = 0;

			// do not poke the ring if we are behind
			if (new_secs > old_secs+2)
				Frame.dopoke();

			// Guarantee at least 1 sort every 10 sec.
			if (new_secs - sort_secs >= 10)
				needsort = 1;
			if (needsort) {
				sort_secs = new_secs;
				if (Ring.sortring())
					doupdate = 1;
				needsort = 0;
			}

			if (RINGWindow->flashWindow())
				doupdate = 1;
			if (doupdate) {
				RINGWindow->redrawWindow();
				MAPWindow->redrawWindow();
			}
			if (Drecord)
				RINGWindow->RINGView->Record();

			// update the status windows every two seconds
			if (NeedOpSIF
			    && new_secs - status_secs >= 2) {
				register int i;
				for (i = 0; i < DUMPCNT; i++) {
					if (STATUSWindow[i])
						STATUSWindow[i]->updateTbl();
				}
				recal = 0;
				status_secs = new_secs;
			}

			// re-register with smtd as often as it unregisters us
			// or if we have not seen any packets for a while.
			if (svcreged
			    && (new_secs-svcreged_secs >= REGTIMO-1
				|| (frmcnt == Frame.frmcnt
				    && new_secs-svcreged_secs >= T_NOTIFY))) {
				Frame.unreg_svc();
				c_preg.timo = (int)time(0) + REGTIMO;
				Frame.reg_svc(&c_preg);
				svcreged_secs = new_secs;
			}
		}
		old_secs = new_secs;
		break;

	case NOTIFY_CLOSE:
		if (sender == notify) {
			notify->close();
			(void)notifyClose((tkValueEvent*)e);
			tkEvent *destroy = new tkEvent(NOTIFY_DESTROY,
						       this,notify);
			eventManager->postEvent(destroy,FALSE);
		} else if (sender == prompt) {
			if (notifyClose((tkValueEvent*)e)) {
				prompt->close();
				tkEvent *destroy = new tkEvent(NOTIFY_DESTROY,
							       this,prompt);
				eventManager->postEvent(destroy,FALSE);
			}
		}
		break;

	case NOTIFY_DESTROY:
		if (sender == notify) {
			delete (tkNotifier*)notify;
			notify = NULL;
		} else if (sender == prompt) {
			delete (tkPrompt*)prompt;
			prompt = NULL;
		}
		break;

	case FV_REPLAY:
		if (REPLAYWindow != 0)
			break;
		REPLAYWindow = new ReplayWindow();
		REPLAYWindow->open();
		addWindow(REPLAYWindow);
		REPLAYWindow->redrawWindow();
		Dreplay = 1;
		break;

	case FV_REPLAY_OFF:
		if (REPLAYWindow == 0)
			break;
		REPLAYWindow->close();
		removeWindow(REPLAYWindow);
		REPLAYWindow = 0;
		RINGWindow->RINGView->ReplayOff();
		Dreplay = 0;
		break;

	case FV_STATUS:
		if (statcmd < 0 || STATUSWindow[statcmd] != 0)
			break;
		STATUSWindow[statcmd] = new StatusWindow();
		STATUSWindow[statcmd]->open();
		addWindow(STATUSWindow[statcmd]);
		STATUSWindow[statcmd]->setTbl();
		statcmd = -1;
		break;

	case FV_STATUS_OFF:
		if (statcmd < 0 || STATUSWindow[statcmd] == 0)
			break;
		STATUSWindow[statcmd]->close();
		removeWindow(STATUSWindow[statcmd]);
		STATUSWindow[statcmd] = 0;
		statcmd = -1;
		break;

	default:
		tkApp::rcvEvent(e);
		break;
	}
}

void
FddiVis::startTimer()
{
	timer->startTimer(ShowToken ? 0.05 : 1.0);
}

#define DEF_FullScreenMode  0
#define DEF_ActiveMode	POLL_ACTIVE
#define DEF_SymAdrMode	1
#define DEF_FddiBitMode	0
#define DEF_Freeze	0
#define DEF_ShowToken	0
#define DEF_FrameSelect	(SEL_NIF|SEL_CSIF|SEL_OPSIF|SEL_SRF)
#define DEF_FindBuf	""
#define DEF_MagAddr	1
#define DEF_Dint	0
#define DEF_Qint	RINGTIMO
#define DEF_AgeFactor	600
#define DEF_DemoStaType	0
#define DEF_DemoAttType	0
#define DEF_DemoConnType 0
#define DEF_DemoUnas	0
#define DEF_DemoDnas	0
#define DEF_DemoOrphs	0
#define DEF_RecordDir	""
#define DEF_ReplayDir	""
#define DEF_RecordInterval 0
#define DEF_Drecord	0
#define DEF_curPanel	0
#define DEF_doerrlog	0
#define DEF_Agent	"localhost"
#define DEF_DevNum	0

#define NCONF2(nm,pat) {if (nm != DEF_##nm) fprintf(fp, pat "\n", nm);}
#define NCONF(nm) NCONF2(nm, #nm" = %d")
#define SCONF(nm) {if (strcmp(nm,DEF_##nm)) fprintf(fp, #nm" = %s\n", nm);}

int
FddiVis::writeConf(char *fn)
{
	FILE *fp;

	if ((fp = fopen(fn, "w")) == NULL)
		return(-1);

	NCONF(FullScreenMode);
	NCONF(ActiveMode);
	NCONF(SymAdrMode);
	NCONF(FddiBitMode);
	NCONF(Freeze);
	NCONF(ShowToken);
	NCONF(FrameSelect);
	SCONF(FindBuf);
	NCONF(MagAddr);

	NCONF2(Dint, "SnapInterval = %d");
	NCONF2(Qint, "QueryInterval = %d");
	NCONF2(AgeFactor, "AgingInterval = %d");

	NCONF(DemoStaType);
	NCONF(DemoAttType);
	NCONF(DemoConnType);
	NCONF(DemoUnas);
	NCONF(DemoDnas);
	NCONF(DemoOrphs);

	SCONF(RecordDir);
	SCONF(ReplayDir);
	NCONF(RecordInterval);
	NCONF(Drecord);

	NCONF(curPanel);

	NCONF(doerrlog);

	SCONF(Agent);
	NCONF(DevNum);

	fclose(fp);
	return(0);
}

void
FddiVis::readConf()
{
#define NAME_SIZE	64
	char line[NAME_SIZE+3+PROMPTSIZE+20];
	char *name, *equal, *val;
	char **lp;
	FILE *fp;
	int i;
#define CK(nm) (!strncmp(name,nm,equal-name))

	// set some values that are not saved in the file.
	BypassSmtd = 0;
	SaveSmtd = 0;
	RingNum = 0;
	Vang = MYPOSITION;

	RingOpr = RINGOPR;

	// initialize before reading the file
	FullScreenMode = DEF_FullScreenMode;
	ActiveMode = DEF_ActiveMode;
	SymAdrMode = DEF_SymAdrMode;
	FddiBitMode = DEF_FddiBitMode;
	Freeze = DEF_Freeze;
	ShowToken = DEF_ShowToken;
	FrameSelect = DEF_FrameSelect;
	strcpy(FindBuf, DEF_FindBuf);
	MagAddr = DEF_MagAddr;

	Qint = DEF_Qint;
	Dint = DEF_Dint;
	AgeFactor = DEF_AgeFactor;

	DemoStaType = DEF_DemoStaType;
	DemoAttType = DEF_DemoAttType;
	DemoConnType = DEF_DemoConnType;
	DemoUnas = DEF_DemoUnas;
	DemoDnas = DEF_DemoDnas;
	DemoOrphs = DEF_DemoOrphs;

	strcpy(RecordDir,DEF_RecordDir);
	strcpy(ReplayDir,DEF_ReplayDir);
	RecordInterval = DEF_RecordInterval;
	Drecord = DEF_Drecord;

	curPanel = DEF_curPanel;
	doerrlog = DEF_doerrlog;
	strcpy(Agent, DEF_Agent);
	DevNum = DEF_DevNum;

	if ((fp = fopen(ConfFile, "r")) == NULL)
		return;

	while (lp = getline(line, sizeof(line), fp)) {
		skipblank(lp);
		name = *lp;
		if (*name == '\0' || *name == '#' || *name == '\n')
			continue;
		i = strlen(name);
		if (name[i-1] == '\n')
			name[i-1] = '\0';

		equal = strpbrk(name," \t=");
		val = equal;
		if (!equal) {
			printf("\"%s\" ignored\n", name);
			continue;
		}
		while (*val != '=')
			val++;
		if (*val++ != '=') {
			printf("\"%s\" ignored\n", name);
			continue;
		}
		while (*val == ' ' || *val == '\t')
			val++;

		if (CK("SymAdrMode")) {
			SymAdrMode = (int)atol(val);
		} else if (CK("FddiBitMode")) {
			FddiBitMode = (int)atol(val);
		} else if (CK("FullScreenMode")) {
			FullScreenMode = (int)atol(val);
		} else if (CK("ActiveMode")) {
			ActiveMode = (int)atol(val);
		} else if (CK("Freeze")) {
			Freeze = (int)atol(val);
		} else if (CK("FindBuf")) {
			strcpy(FindBuf, val);
		} else if (CK("DemoUnas")) {
			DemoUnas = (int)atol(val);
		} else if (CK("DemoDnas")) {
			DemoDnas = (int)atol(val);
		} else if (CK("DemoOrphs")) {
			DemoOrphs = (int)atol(val);
		} else if (CK("DemoStaType")) {
			DemoStaType = (int)atol(val);
		} else if (CK("DemoAttType")) {
			DemoAttType = (int)atol(val);
		} else if (CK("DemoConnType")) {
			DemoConnType = (int)atol(val);
		} else if (CK("RecordDir")) {
			strcpy(RecordDir, val);
		} else if (CK("ReplayDir")) {
			strcpy(ReplayDir, val);
		} else if (CK("RecordInterval")) {
			RecordInterval = (int)atol(val);
		} else if (CK("curPanel")) {
			curPanel = (int)atol(val);
		} else if (CK("FrameSelect")) {
			FrameSelect = (int)atol(val);
		} else if (CK("QueryInterval")) {
			Qint = (int)atol(val);
		} else if (CK("SnapInterval")) {
			Dint = (int)atol(val);
		} else if (CK("AgingInterval")) {
			AgeFactor = (int)atol(val);
		} else if (CK("MagAddr")) {
			MagAddr = (int)atol(val);
		} else if (CK("Drecord")) {
			Drecord = (int)atol(val);
		} else if (CK("Agent")) {
			strcpy(Agent, val);
		} else if (CK("DevNum")) {
			DevNum = (int)atol(val);
		} else if (CK("ShowToken")) {
			ShowToken = (int)atol(val);
		} else if (CK("doerrlog")) {
			doerrlog = (int)atol(val);
		} else
			printf("\"%s\" ignored\n", *lp);
	}
	return;
}

void
dopost(char *msg)
{
	FV.post(NOHOSTMSG, msg);
}

void
FddiVisQuit()
{
	FV.quit();
}

void
FddiVis::post(int msg, char* errString)
{
	if (notify != NULL)
		delete (tkNotifier*)notify;

	notify = new tkNotifier();
	notify->setButton(0,"Continue",FV_CONTINUE);
	notify->setButtonCount(1);
	notify->setWindowSize(400.0,150.0);
	notify->setShutButton(0);
	if (errString == NULL)
		notify->initWindow(1,&Message[msg]);
	else {
		char *msgs[2];
		msgs[0] = Message[msg];
		msgs[1] = errString;
		notify->initWindow(2,msgs);
	}
	addWindow(notify);
	inputHog(notify);
}

void
FddiVis::saveImage()
{
	struct stat buf;

	if (stat(IMAGESAVE,&buf) == -1) {
		post(NOIMAGESAVEMSG);
		return;
	}

	ask(SAVEIMAGEMSG,FV_SAVEIMAGE,FV_RGBFILE);
}

void
FddiVis::ask2way(int msg,int event, char *text)
{
	if (prompt != NULL)
		delete (tkPrompt*)prompt;

	prompt = new tkPrompt();
	prompt->setButtonCount(2);
	prompt->setButton(0,"quit",FV_CANCEL);
	prompt->setAcceptButton(1,"Next",event);
	prompt->setShutButton(0);
	float w = ITOP(3.5);
	float h = ITOP(1.0 + 7.0/8.0);
	prompt->setWindowSize(w,h);
	tkPen* tvPen = new tkPen(TVIEW_TEXT_ICOLOR,TVIEW_BG_ICOLOR,
							TVIEW_EDGE_ICOLOR,1);
	Box2 tvBox(UNBOUND_TILE,ITOP(.75),w - 2.0*UNBOUND_TILE,ITOP(3.0/8.0));
	prompt->initDataField(0, 0, tvBox, tvPen);
	if (text != NULL)
		prompt->setEditText(text);
	prompt->initWindow(1,&Message[msg]);
	prompt->closeOnpost(FALSE);
	addWindow(prompt);
	inputHog(prompt);
}

void
FddiVis::askSearch()
{
	if (prompt != NULL)
		delete (tkPrompt*)prompt;

	prompt = new tkPrompt();
	prompt->setButtonCount(3);
	prompt->setButton(0,"Cancel",FV_RECANCEL);
	prompt->setButton(1,"Next",FV_FINDHOST);
	prompt->setAcceptButton(2,"Prev",FV_BACKFINDHOST);
	prompt->setShutButton(0);
	float w = ITOP(3.5);
	float h = ITOP(1.0 + 7.0/8.0);
	prompt->setWindowSize(w,h);
	tkPen* tvPen = new tkPen(TVIEW_TEXT_ICOLOR,TVIEW_BG_ICOLOR,
							TVIEW_EDGE_ICOLOR,1);
	Box2 tvBox(UNBOUND_TILE,ITOP(.75),w - 2.0*UNBOUND_TILE,ITOP(3.0/8.0));
	prompt->initDataField(0, 0, tvBox, tvPen);
	prompt->setEditText(FindBuf);
	prompt->initWindow(1,&Message[FINDHOSTMSG]);
	prompt->closeOnpost(FALSE);
	addWindow(prompt);
	inputHog(prompt);
}

void
FddiVis::ask3way(int msg,int event, int event2, char *text)
{
	if (prompt != NULL)
		delete (tkPrompt*)prompt;

	prompt = new tkPrompt();
	prompt->setButtonCount(3);
	prompt->setButton(0,"Cancel",FV_RECANCEL);
	prompt->setAcceptButton(1,"Start",event);
	prompt->setButton(2,"Stop",event2);
	prompt->setShutButton(0);
	float w = ITOP(3.5);
	float h = ITOP(1.0 + 7.0/8.0);
	prompt->setWindowSize(w,h);
	tkPen* tvPen = new tkPen(TVIEW_TEXT_ICOLOR,TVIEW_BG_ICOLOR,
							TVIEW_EDGE_ICOLOR,1);
	Box2 tvBox(UNBOUND_TILE,ITOP(.75),w - 2.0*UNBOUND_TILE,ITOP(3.0/8.0));
	prompt->initDataField(0, 0, tvBox, tvPen);
	if (text != NULL)
		prompt->setEditText(text);
	prompt->initWindow(1,&Message[msg]);
	prompt->closeOnpost(FALSE);
	addWindow(prompt);
	inputHog(prompt);
}

void
FddiVis::ask(int msg,int event,char *text)
{
	if (prompt != NULL)
		delete (tkPrompt*)prompt;

	prompt = new tkPrompt();
	prompt->setButtonCount(2);
	prompt->setButton(0,"Cancel",FV_CANCEL);
	prompt->setAcceptButton(1,"Accept",event);
	prompt->setShutButton(0);
	float w = ITOP(3.5);
	float h = ITOP(1.0 + 7.0/8.0);
	prompt->setWindowSize(w,h);
	tkPen* tvPen = new tkPen(TVIEW_TEXT_ICOLOR,TVIEW_BG_ICOLOR,
							TVIEW_EDGE_ICOLOR,1);
	Box2 tvBox(UNBOUND_TILE,ITOP(.75),w - 2.0*UNBOUND_TILE,ITOP(3.0/8.0));
	prompt->initDataField(0, 0, tvBox, tvPen);
	if (text != NULL)
		prompt->setEditText(text);
	prompt->initWindow(1,&Message[msg]);
	addWindow(prompt);
	inputHog(prompt);
}

void
FddiVis::quit()
{
	if (prompt != NULL)
		delete (tkPrompt*)prompt;

	prompt = new tkPrompt();
	prompt->setButtonCount(3);
	prompt->setButton(0,"Cancel",FV_CANCEL);
	prompt->setButton(1,"Quit",FV_QUIT);
	prompt->setAcceptButton(2,"Save & Quit",FV_SAVEANDQUIT, 110.0);
	prompt->setShutButton(0);
	float w = ITOP(3.5);
	float h = ITOP(1.0 + 7.0/8.0);
	prompt->setWindowSize(w,h);
	tkPen* tvPen = new tkPen(TVIEW_TEXT_ICOLOR,TVIEW_BG_ICOLOR,
							TVIEW_EDGE_ICOLOR,1);
	Box2 tvBox(UNBOUND_TILE,ITOP(.75),w - 2.0*UNBOUND_TILE,ITOP(3.0/8.0));
	prompt->initDataField(0, 0, tvBox, tvPen);
	prompt->setEditText(ConfFile);
	prompt->initWindow(1,&Message[QUITMSG]);
	addWindow(prompt);
	inputHog(prompt);
}

void
FddiVis::chgBtnState(int sts)
{
	if (cb == 0)
		return;
	cb->setCurrentState(sts);
	cb->setCurrentVisualState(tkBUTTON_QUIET);
}

int
FddiVis::notifyClose(tkValueEvent *e)
{
	tkValue value;
	int fv;
	int retval = 1;
	e->getValue(value);
	value.getValue(&fv);

	UIWindow->setCursor(HOURGLASS);
	MAPWindow->setCursor(HOURGLASS);
	RINGWindow->setCursor(HOURGLASS);

	switch(fv) {
		case FV_SAVEANDQUIT:
			if (writeConf(prompt->getData()) != 0) {
				post(BADSAVEMSG);
				break;
			}
			/* fall through */

		case FV_QUIT:
			UIWindow->setCursor(ARROW);
			MAPWindow->setCursor(ARROW);
			RINGWindow->setCursor(ARROW);
			RINGWindow->close();
			MAPWindow->close();
			UIWindow->close();
			fr_finish();
			exit(0);

		case FV_SAVEIMAGE:
			if (writeImagefile(prompt->getData()) != 0) {
				post(BADIMAGESAVEMSG);
				break;
			}
			break;

		case FV_RECORD_OFF:
			RINGWindow->RINGView->RecordOff();
			break;

		case FV_RECORD:
			strcpy(FV.RecordDir, prompt->getData());
			RINGWindow->RINGView->RecordOn();
			retval = Drecord;
			break;

		case FV_RECANCEL:
			chgBtnState(Drecord);
			break;

		case FV_BACKFINDHOST:
			FindStation(1, prompt->getData());
			retval = 0;
			break;

		case FV_FINDHOST:
			FindStation(0, prompt->getData());
			retval = 0;
			break;

		case FV_PINGHOST:
			ping(prompt->getData());
			break;

		case FV_RESTART:
			Restart(prompt->getData());
			break;
	}

	UIWindow->setCursor(ARROW);
	MAPWindow->setCursor(ARROW);
	RINGWindow->setCursor(ARROW);
	return(retval);
}

int
FddiVis::writeImagefile(char *fn)
{
	int rc;
	char buf[256];

	UIWindow->redrawWindow();
	RINGWindow->redrawWindow();
	MAPWindow->redrawWindow();
	sprintf(buf, "%s %s %d %d %d %d",
			IMAGESAVE, fn, 0, SCRXMAX-1, 0, SCRYMAX-1);
	rc = system(buf);

	return(rc);
}

/*
 *
 */
void
FddiVis::ReadRgb(char *fn)
{
	char *qqq = fn;	// satisfy compiler.
//	stat_readrgb(fn);
}

void
FddiVis::FindStation(int CCWise, char *buf)
{
	if (*buf == '\0')
		return;

	strcpy(FindBuf, buf);
	if (Ring.search(CCWise, FindBuf) <= 0) {
		char err[PROMPTSIZE+20];
		sprintf(err, "%s is not found", buf);
		post(NOHOSTMSG, err);
		return;
	}
	RINGWindow->redrawWindow();
	MAPWindow->drawHand();
	return;
}

void
FddiVis::ping(char *station)
{
	pid_t id;
	char title[64];

	if (strcmp(Agent, "localhost")) {
		post(NULLMSG, "SMT ping is only for localhost");
		return;
	}

	if (*station == '\0')
		return;
	strcpy(PingBuf, station);

	if (id = fork()) {
		if (id < 0)
			printf("SMT ping fork failed\n");
		return;
	}

	/* child */
	sprintf(title, "SMT ping to %s", station);
	execlp("/bin/wsh",
	     "wsh",
	     "-d",
	     "-s", "80,24",
	     "-n", "smtping",
	     "-t", title,
	     "-c", "/usr/etc/smtping", station,
	     (char*)0);
	perror("exec smtping");
	abort();
}

void
FddiVis::Restart(char *ahost)
{
	if (FV.svcreged != 0)
		Frame.unreg_svc();

	map_close();
	tkRemoveTranslator(smtsock);
	Ring.flush_ring();

	// take blank to mean localhost
	if (*ahost == '\0')
		ahost = "localhost";

	if (strcmp(Agent, ahost)) {
		if (Frame.setsmtd(ahost)) {
			post(NORESTARTMSG, ahost);
			return;
		}
		strcpy(Agent, ahost);
	} else if (Frame.setsmtd(Agent)) {
		post(NORESTARTMSG, Agent);
		return;
	}

	// make sure to get first NIF of my own.
	int tmp = FrameSelect;
	FrameSelect = SEL_NIF;
	Frame.buildnif();
	FrameSelect = tmp;

	tkAddTranslator(smtsock, recvframe);
	Frame.kickpoke();
	RINGWindow->redrawWindow();
	MAPWindow->redrawWindow();
}
