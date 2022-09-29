#ifndef __FDDIVIS__
#define __FDDIVIS__
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.14 $
 */

#include <tkApp.h>
#include <tkTimer.h>
#include <tkExec.h>
#include <tkPrompt.h>
#include <textFont.h>

#include "ui.h"
#include "font.h"
#include "subr.h"
#include "scope.h"
#include "resizeview.h"
#include "replaywindow.h"
#include "statuswindow.h"

#ifndef NULL
#define NULL		0
#endif

#define SCROLLSTEP	10.0	/* Step for 1 click of scroll bars	*/

#define X		0	/* Coordinates				*/
#define Y		1
#define Z		2

#define LOOKUP		1
#define INSERT		2

#define ALL		1
#define ACTIVE		2

#define ADD		1
#define IGNORE		2


#define SNOOPHOST	0x1	/* Host is the snooping host		*/
#define NISMASTER	0x2	/* Host is a NIS master			*/
#define NISSERVER	0x4	/* Host is a NIS server			*/
#define GATEWAY		0x8	/* Host is a gateway			*/
#define HOSTPICKED	0x10	/* Host is picked			*/
#define NETWORKPICKED	0x20	/* Network is picked			*/

#define DMFULLNAME	1	/* Display as host name and domain	*/
#define DMHOSTNAME	2	/* Display as name			*/
#define DMINETADDR	3	/* Display as Internet address		*/
#define DMENETADDR	4	/* Display as ethernet address		*/

#define PROMPTSIZE	128	/* size of names read from user		*/

#define INACTIVECOLOR	BLUE
#define PICKEDCOLOR	RED

#define SOLID		0
#define DASHED		1
#define DOTTED		2

#define QUITMSG		0
#define RECORDMSG	1
#define BADRECORDMSG	2
#define REPLAYMSG	3
#define BADREPLAYMSG	4
#define BADVERSIONMSG	5
// #define ??? MSG	6
#define NOHOSTMSG	7
#define FINDHOSTMSG	8
#define PINGHOSTMSG	9
#define CANTPINGMSG	10
#define SAVEIMAGEMSG	11
#define NOIMAGESAVEMSG	12
#define BADIMAGESAVEMSG	13
#define BADSAVEMSG	14
#define NOSUPPORTMSG	15
#define DIRACCERRMSG	16
#define FILEACCERRMSG	17
#define RESTARTMSG	18
#define NORESTARTMSG	19
#define NULLMSG		20

#define NOTIFY_DESTROY	700
#define FV_CONTINUE	701
#define FV_CANCEL	702
#define FV_QUIT		703
#define FV_SAVEANDQUIT	704
#define FV_RECORD	705
#define FV_SAVEIMAGE	706
#define FV_FINDHOST	707
#define FV_PINGHOST	708
#define FV_REPLAY	709
// #define FV_???	710
#define FV_DELETEHOST	711
#define FV_RECORD_OFF	712
#define FV_REPLAY_OFF	713
#define FV_RECANCEL	714
#define FV_RESTART	715
#define FV_BACKFINDHOST	716
#define FV_STATUS	717
#define FV_STATUS_OFF	718

#define ITOP(i)		((i) * 96.0)

class UserInterfaceWindow;
class MapWindow;
class RingWindow;
class Ring;
class Frame;
class tkNotifier;
class tkPrompt;

class Frame
{
protected:
	SMTD smtd;
	MAC_INFO *mi;
	IPHASE_INFO ifi;

	void dummysmtd(int);
	struct timeval poketms;
public:
	Frame(void);
	~Frame(void);

	int  frmcnt;			// total input frame count

	void kickpoke(void);
	void dopoke(void);
	void unreg_svc(void);
	void reg_svc(SMT_FS_INFO*);
	void do_demo(void);
	void init_frame(void);
	void xmit(int, LFDDI_ADDR*);
	int getsmtd(int);
	int  setsmtd(char*);
	void buildsif(void);
	void buildnif(void);
	int recvFrame(SMT_FB*, int);
};

class Ring
{
protected:
	RING	*srch;
public:
	Ring(void);
	~Ring(void);

	Symbol	*sidhash_lookup(SMT_STATIONID*);
	Symbol	*sidhash_add(RING*);
	void	sidhash_del(RING*);
	Symbol	*sahash_lookup(LFDDI_ADDR*);
	Symbol	*sahash_add(RING*);
	void	sahash_del(RING*);

	void	free_a_info(SINFO*);
	void	free_a_ring(RING*);
	void	flush_ring(void);
	void	list_insert(RING*, RING*);
	void	list_remove(RING*);

	int	update_ring(RING*, SMT_FB*);
	int	buildring(SMT_FB*);
	int	sortring(void);
	int	inrange(Coord, Coord, Coord);
	int	search(int, char*);
	RING*	findsel(Point&);
	void	init(SMT_FB*);

	void	hole_insert(RING*);
};

class MapView : public ResizeView
{
protected:
	Coord		oldVang;
	void            transform(Point &);
public:
	MapView(void);

	virtual void    paint(void);
	virtual void    beginSelect(Point &);
	virtual void    continueSelect(Point &);
	virtual void    endSelect(Point &);

	void		drawHand(int);

	// universe
	void            setUniverse(void);
};

class MapWindow : public tkWindow
{
public:
	MapView		*MAPView;
	virtual void    open(void);
	virtual void    resize(void);
	virtual void    clearWindow(void);
	virtual void    redrawWindow(void);
	virtual void    stowWindow(void);

	void		drawHand();
};

class RingView : public ResizeView
{
protected:
	void            resetViewport(void);
	void		dozum(RING*, Coord, Coord);
	long		popup;

	short		pickbuf[50];
	Coord		TokenVang;
	Coord		subTokenVang;
	int		tkDirection;

	void		Replay(RECPLAY*, int);
	int		RecordOpr;
	int		RecordRingNum;
	int		RecordTicks;

	short		lastnode;
public:
	RingView(void);

	virtual void    paint(void);
	virtual void    beginSelect(Point &);
	virtual void    continueSelect(Point &);
	virtual void    endSelect(Point &);
	virtual void	moveLocate(Point& p);
	virtual void    doubleClick(Point &);
	virtual void    menuSelect(Point &);

	void		drawtoken(void);
	void		resettoken(void);
	void		Record();
	void		RecordOn();
	void		RecordOff();
	void		ReplayOn(int);
	void		ReplaySeq(int);
	void		ReplayOff();
	RECPLAY		*ReplayDelete(int, int);

	// universe
	void            setUniverse(void);
	float		Radius;
	float		EyeX;
	float		EyeY;
	Coord		Delta;
	Coord		StartAng;
	Coord		EndAng;

	// record/replay
	RING		*OnlineRp;
	int		OnlineRnum;
	int		NumRecords;
	RECPLAY		*RecordTbl;

	// Gateway
	Coord		gateang;
	Coord		gatedelta;

	// draw Token
	RING		*tkrp;
	RING		*stkrp;
};

class RingWindow : public tkWindow
{
protected:
	int		zbuf;
public:
	RingView	*RINGView;

	virtual void    open(void);
	virtual void    close(void);
	virtual void    resize(void);
	virtual void    clearWindow(void);
	virtual void    redrawWindow(void);
	virtual void    stowWindow(void);
	virtual void    rcvEvent(tkEvent* e);

	int		flashWindow(void);
	void		drawToken(void);
};

class FddiVis : public tkApp
{
protected:
	tkValueEvent *notifyEvent;
	tkEvent *destroyEvent;
	tkEvent *timerEvent;
	tkEventManager *eventManager;
	tkTimer *timer;
	tkNotifier *notify;
	tkPrompt *prompt;

	char ConfFile[MAXNAMELEN];
	void readConf();
	int writeConf(char *fn);

	int writeImagefile(char *fn);
	int notifyClose(tkValueEvent *e);
	void ping(char *string);

public:
	// Operation
	Coord Vang;
	int RingNum;

	// Display
	int FullScreenMode;
	int ActiveMode;
	int SymAdrMode;
	int FddiBitMode;
	int Freeze;
	int ShowToken;
	int FrameSelect;
	char FindBuf[PROMPTSIZE];
	int MagAddr;

	int Dint;
	int Qint;
	int AgeFactor;

	char PingBuf[PROMPTSIZE];

	// Demo
	int DemoStaType;
	int DemoAttType;
	int DemoConnType;
	int DemoUnas;
	int DemoDnas;
	int DemoOrphs;

	int BypassSmtd;
	int SaveSmtd;

	// Files
	char RecordDir[PROMPTSIZE];
	char ReplayDir[PROMPTSIZE];
	char CurReplayDir[PROMPTSIZE];
	int RecordInterval;
	int Drecord;
	int Dreplay;

	int curPanel;

	// Buttons
	tkButton *cb;

	// Timer Value
	long tmv;

	// Windows
	UserInterfaceWindow	*UIWindow;	/* User interface window */
	MapWindow		*MAPWindow;	/* Map window */
	RingWindow		*RINGWindow;	/* FDDI ring window */
	ReplayWindow		*REPLAYWindow;	/* Replay prompt window */

	// Status display
	StatusWindow	*STATUSWindow[DUMPCNT];
	int		statcmd;
	int		NeedOpSIF;	// need Op SIF for a status window
	int		doerrlog;
	int		recal;

	// ring
	Ring		Ring;
	RING		*ring;
	SMT_FB		*rfp;
	SMT_FB		*xfp;
	int		smtsock;
	int		svcreged;	// service registered with smtd
	RING		*magrp;
	RING		*pickrp;
	RING		*sifrp;
	int		RingOpr;
	int		sig_blink;
	Scope		*sidtbl;
	Scope		*satbl;
	SMT_FS_INFO	c_preg;
	int		frmcnt;		// shadow frame count
	float		sfact;
	int		picked;
	u_long		cursort;
	int		needsort;

	// Frame
	Frame	Frame;
	char	Agent[PROMPTSIZE];
	char	trunkname[10];
	int	Gates;
	int	DevNum;
	char	gatenames[NSMTD][10];

	// Fonts
#ifdef CYPRESS_XGL
	textFont	*font1;
	textFont	*font2;
#else
	fmfonthandle font0;
	fmfonthandle font1;
	fmfonthandle font2;
#endif

	virtual void appInit(tkApp *,int,char *[],char *[]);
	virtual void rcvEvent(tkEvent *e);
	void startTimer();
	void post(int msg, char *errString = NULL);
	void ask(int msg,int event,char *text);
	void ask2way(int msg,int event,char *text);
	void ask3way(int msg,int event, int event2, char *text);
	void saveImage();
	void quit();
	void askSearch();
	void FindStation(int, char *);
	void ReadRgb(char *);
	void chgBtnState(int sts);
	void Restart(char *ahost);
};

extern FddiVis FV;

void Perror(int,char *);


#ifndef _LANGUAGE_C_PLUS_PLUS_2_0
overload l_sin, l_cos;
#endif
float l_sin(int);
float l_sin(float);
float l_cos(int);
float l_cos(float);

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
extern int startupnoclose(Bool);
extern int startupnoquit(Bool);
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif

#endif
