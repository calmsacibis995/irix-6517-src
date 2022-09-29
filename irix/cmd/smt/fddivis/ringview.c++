/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	RingView class
 *
 *	$Revision: 1.12 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <osfcn.h>
#include <math.h>
#include <string.h>
#include <bstring.h>
#include <assert.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <malloc.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "fddivis.h"
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <smt_parse.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif

RingView::RingView()
{
	popup=defpup("Ring Window %t|Show Station Status|Show Ring Dump %l|Quit fddivis");

	tkrp = 0;
	stkrp = 0;
	TokenVang = MYPOSITION;
	subTokenVang = MYPOSITION;
}

void
RingView::setUniverse()
{
	register int rnum = FV.RingNum;

	/* Calculate workstation's scale factor */
	if (FV.FullScreenMode) {
		FV.sfact = 2.2 / (float)rnum;
		if (FV.sfact > 0.2)
			FV.sfact = 0.2;
		else if (FV.sfact < 0.1)
			FV.sfact = 0.1;
	} else {
		FV.sfact = 0.2;
	}

	/*
	 * 1. Get radius depends on the number of stations
	 *	because station's size is fixed.
	 * 2. get viewpoint depending radius and view angles.
	 */
	if (!FV.FullScreenMode && (rnum > SIZETHRESHOLD)) {
		Radius = (float)(rnum*RADIUS)/(float)SIZETHRESHOLD;
		EyeX = Radius * (float)(cos(RAD(FV.Vang)));
		EyeY = Radius * (float)(sin(RAD(FV.Vang)));
	} else {
		Radius = RADIUS;
		EyeX = 0.0;
		EyeY = 0.0;
	}
	perspective(50, 1.0, 400.0, 600.0);
	lookat(EyeX, EyeY, 500., EyeX, EyeY, 0., 0);

	/*
	 * Draw ring
	 */
	Delta = rnum <= 1 ? (Coord)3590 : (Coord)3600/(Coord)rnum;
	StartAng = FV.Vang-(Delta*(SIZETHRESHOLD/2))-(Delta-10);
	EndAng   = FV.Vang+(Delta*(SIZETHRESHOLD/2));

}

void
RingView::paint()
{
	int i;
	RING *rp;
	short pickid;
	Coord ang;

	if (!FV.ring)
		return;

	setUniverse();

	if (FV.picked != PICK_MODE) {
		ang = MYPOSITION;
		for (i = FV.RingNum, rp = FV.ring; i > 0; i--, rp = rp->next) {
			rp->ang = ang;
			rp->radian = RAD(ang);
			ang -= Delta;
			if (FV.Ring.inrange(rp->ang, StartAng, EndAng) != 0)
				drawlink(rp, Radius);
		}
	}

	pickid = 0;
	for (i = FV.RingNum, rp = FV.ring; i > 0; i--, rp = rp->next) {
		if (FV.Ring.inrange(rp->ang, StartAng, EndAng) == 0)
			continue;

		pickid++;
		if (FV.picked == pickid)
			FV.pickrp = rp;
		loadname(pickid);
		workstation(rp, Radius);
	}
	lastnode = pickid;

#ifdef DEBUGGATES
	FV.Gates = NSMTD;
#endif
#ifdef STASH
	if (FV.Gates > 1) {
		int j;
		int numGates = FV.Gates - 1;
		float savfact = FV.sfact;

		// keep size of gate constant.
		FV.sfact = 0.2;
		gatedelta = (Delta/numGates)*0.2/FV.sfact;
		if ((u_long)numGates & 1) {
			gateang = ((numGates-1)/2)*gatedelta;
		} else {
			gateang = (numGates/2)*gatedelta-(gatedelta/2.0);
		}
		gateang += rp->ang;
		for (j = 0, i = 0; j < numGates; j++, i++) {
			if (i == FV.DevNum) {
				j--;
				continue;
			}
			pickid++;
			loadname(pickid);
			drawgate(Radius,gateang-(gatedelta*j),FV.gatenames[i]);
		}
		FV.sfact = savfact;
	}
#else
	if (FV.Gates > 1) {
		int numGates = FV.Gates;
		float savfact = FV.sfact;

		// keep size of gate constant.
		FV.sfact = 0.2;
		gatedelta = 120.0;
		if (!FV.FullScreenMode && FV.RingNum >= SIZETHRESHOLD)
			gatedelta *= ((double)SIZETHRESHOLD/(double)FV.RingNum);
		gateang = rp->ang;
		for (i = 0; i < numGates; i++) {
			pickid++;
			loadname(pickid);
			drawgate(Radius, gateang-(gatedelta*(i-FV.DevNum)),
					FV.gatenames[i], i);
		}
		FV.sfact = savfact;
	}
#endif

	if (FV.picked != PICK_MODE)
		dozum(FV.magrp, EyeX, EyeY);
}

void
RingView::beginSelect(Point& p)
{
	p = p;
	FV.pickrp = 0;
	FV.sifrp = 0;
	initnames();
	pick(pickbuf, sizeof(pickbuf)/sizeof(pickbuf[0]));
	FV.picked = PICK_MODE;
	paint();
	FV.picked = PICK_NONE;
}

void
RingView::continueSelect(Point& p)
{
	p = p;
#ifdef STASH
	printf("contS: x=%f, y=%f\n", (float)p.getX(), (float)p.getY());
#endif
}

void
RingView::endSelect(Point& p)
{
	int numpicked = (int)endpick(pickbuf);

	if (numpicked <= 0)
		return;
	FV.picked = (int)pickbuf[1];
	FV.RINGWindow->redrawWindow();

#ifdef STASH
	if ((FV.picked>lastnode) && (FV.picked<=(lastnode+FV.Gates)))
		opengate(FV.picked-lastnode, Radius, gateang, gatedelta);
#else
	int selgate = FV.picked - lastnode - 1;
	if ((selgate >= 0) && (selgate < FV.Gates) && (selgate != FV.DevNum))
		opengate(selgate, Radius, gateang, gatedelta);
#endif

	FV.sifrp = FV.pickrp;
	if (FV.sifrp) {
		FV.Frame.xmit(SMT_FC_CONF_SIF, &FV.sifrp->sm.sa);
		FV.Frame.xmit(SMT_FC_OP_SIF,   &FV.sifrp->sm.sa);
	}
	p = p;
	FV.picked = PICK_NONE;
}

void
RingView::moveLocate(Point& p)
{
	if (FV.MagAddr) {
		RING *q = FV.Ring.findsel(p);

		if (q && q != FV.magrp) {
			FV.magrp = q;
			FV.RINGWindow->redrawWindow();
			FV.MAPWindow->redrawWindow();
		}
		return;
	}

	if (FV.magrp) {
		FV.magrp = 0;
		FV.RINGWindow->redrawWindow();
		FV.MAPWindow->redrawWindow();
	}
}

void
RingView::doubleClick(Point& p)
{
	int numpicked = (int)endpick(pickbuf);

	p = p;
	FV.sifrp = 0;
	if (numpicked <= 0)
		return;

	FV.picked = (int)pickbuf[1];
	FV.RINGWindow->redrawWindow();
	if (FV.pickrp)
		visit(&FV.pickrp->sm.sa);
}

void
RingView::dozum(RING *rp, Coord xd, Coord yd)
{
	register float h, l;

	if (rp == 0)
		return;

#define SSS 5.0
#define LLL 15.0
	if ((FV.FullScreenMode != 0) || (FV.RingNum <= SIZETHRESHOLD)) {
		l = -LLL/2.0;
		h = LLL/2.0;
	} else if (xd >= 0.0) {
		if (yd >= 0.0) {	/* quad 1 */
			l = xd + SSS*(float)(cos(RAD(450)));
			h = yd + SSS*(float)(sin(RAD(450))) + LLL;
		} else {		/* quad 4 */
			l = xd + SSS*(float)(cos(RAD(3150)));
			h = yd + SSS*(float)(sin(RAD(3150)));
		}
	} else {
		if (yd >= 0.0) {	/* quad 2 */
			l = xd + SSS*(float)(cos(RAD(1350))) - LLL;
			h = yd + SSS*(float)(sin(RAD(1350))) + LLL;
		} else {		/* quad 3 */
			l = xd + SSS*(float)(cos(RAD(2250))) - LLL;
			h = yd + SSS*(float)(sin(RAD(2250)));
		}
	}

#ifdef DEBUGxxx
	{
		register float b, r;
		float ccl[4][3];

		b = h - LLL;
		r = l + LLL;
		ccl[0][0] = l; ccl[0][1] = h; ccl[0][2] = 10.0;
		ccl[1][0] = l; ccl[1][1] = b; ccl[1][2] = 10.0;
		ccl[2][0] = r; ccl[2][1] = b; ccl[2][2] = 10.0;
		ccl[3][0] = r; ccl[3][1] = h; ccl[3][2] = 10.0;
		c3i(redcol);
		bgnclosedline(); DOV3FS(ccl); endclosedline();
	}
#endif
	paintsif(rp, l, h);
}

#define POPUP_STAT	1
#define POPUP_DUMP	2
#define POPUP_QUIT	3

void
RingView::menuSelect(Point &p)
{
	p = p;
	switch (dopup(popup)) {
		case POPUP_STAT: dostat(FV.trunkname); break;
		case POPUP_DUMP: dumpring(FV.RingNum,FV.ring,DUMP_RING); break;
		case POPUP_QUIT: FV.quit(); break;
	}
}

static float parrow[3][3];
static float sarrow[3][3];
void
RingView::resettoken()
{
	tkrp = 0;
	stkrp = 0;
	bzero((char*)&parrow, sizeof(parrow));
	bzero((char*)&sarrow, sizeof(sarrow));
}

static int prevDirection;
static float zeroVang;
void
RingView::drawtoken()
{
#define THETA	400.0
#define LTH	(900.0+THETA/2.0)
#define RTH	(900.0-THETA/2.0)
#define TKPROP	(FV.sfact >= 0.2 ? 60.0 : 300.0*FV.sfact)
#define TKW	0.6
#define TKANG	((3600.0*TKW)/(2.0*s_rd*(double)M_PI))
#define DOARROW(x) bgnpolygon(); v3f(x[0]); v3f(x[1]); v3f(x[2]); endpolygon()

	register float x, y;
	register double radang;
	register int wrapped = FV.RingOpr&WRAPPED;
	Coord s_rd = (Coord)Radius-RINGGAP;

	if (FV.RingNum <= 1)
		return;
	if (!tkrp || !stkrp) {
		if (!FV.ring)
			return;
		tkrp = FV.ring; TokenVang = tkrp->ang;
		stkrp = FV.ring; subTokenVang = tkrp->ang;
		prevDirection = 2;
	}
	setUniverse();

	// erase old shadow
	c3i(blackcol); clear();

	tkDirection = prevDirection;
	switch (prevDirection) {
	case 2:
		// adjust token angles
		if ((tkrp->ang-TokenVang)/Delta < 0.0) {
			TokenVang = tkrp->ang;
			subTokenVang = stkrp->ang;
		}

		TokenVang -= TKPROP;
		subTokenVang += TKPROP;
		if ((tkrp->ang-TokenVang) >= Delta) {
			tkrp = tkrp->next;
			stkrp = stkrp->prev;
#ifdef SKIP_SASTK
			while(tkrp->flt > LT_TWIST || tkrp->blt > LT_TWIST)
				tkrp = tkrp->next;
			while(stkrp->flt > LT_TWIST || stkrp->blt > LT_TWIST)
				stkrp = stkrp->prev;
#endif
			TokenVang = tkrp->ang;
			subTokenVang = stkrp->ang;
			if (tkrp->flt == LT_WRAP) {
				tkDirection = 1;
				zeroVang = tkrp->ang;
			}
		}

		radang = RAD(TokenVang);
		parrow[0][0] = x = (float)cos(radang)*Radius;
		parrow[0][1] = y = (float)sin(radang)*Radius;
		radang = RAD((TokenVang+RTH));
		parrow[1][0] = x + (float)cos(radang)*TKW;
		parrow[1][1] = y + (float)sin(radang)*TKW;
		radang = RAD((TokenVang+LTH));
		parrow[2][0] = x + (float)cos(radang)*TKW;
		parrow[2][1] = y + (float)sin(radang)*TKW;
		c3i(GREENOVLY); DOARROW(parrow);

		if (!wrapped) {
			radang = RAD(subTokenVang);
			sarrow[0][0] = x = (float)cos(radang)*s_rd;
			sarrow[0][1] = y = (float)sin(radang)*s_rd;
			radang = RAD((subTokenVang-RTH));
			sarrow[1][0] = x + (float)cos(radang)*TKW;
			sarrow[1][1] = y + (float)sin(radang)*TKW;
			radang = RAD((subTokenVang-LTH));
			sarrow[2][0] = x + (float)cos(radang)*TKW;
			sarrow[2][1] = y + (float)sin(radang)*TKW;
			c3i(GREYOVLY); DOARROW(sarrow);
		}
		break;

	case -2:
		if ((TokenVang - tkrp->ang)/Delta < 0.0) {
			TokenVang = tkrp->ang;
		}
		TokenVang += TKPROP;
		if ((TokenVang - tkrp->ang) >= Delta) {
			tkrp = tkrp->prev;
			TokenVang = tkrp->ang;
			if (tkrp->blt == LT_WRAP) {
				tkDirection = -1;
				zeroVang = tkrp->ang - 1800.0;
			}
		}
		radang = RAD(TokenVang);
		parrow[0][0] = x = (float)cos(radang)*s_rd;
		parrow[0][1] = y = (float)sin(radang)*s_rd;
		radang = RAD((TokenVang-RTH));
		parrow[1][0] = x + (float)cos(radang)*TKW;
		parrow[1][1] = y + (float)sin(radang)*TKW;
		radang = RAD((TokenVang-LTH));
		parrow[2][0] = x + (float)cos(radang)*TKW;
		parrow[2][1] = y + (float)sin(radang)*TKW;
		c3i(GREENOVLY); DOARROW(parrow);
		break;

	case 1:
		if ((tkrp->ang - zeroVang)/1800.0 < 0.0) {
			zeroVang = tkrp->ang;
		}
		zeroVang -= TKPROP*3.0;
		if ((tkrp->ang - zeroVang) >= 1800.0) {
			TokenVang = tkrp->ang;
			tkDirection = -2;
		}

		radang = RAD(TokenVang);
		x = (float)cos(radang)*(Radius-HALFGAP);
		y = (float)sin(radang)*(Radius-HALFGAP);
		radang = RAD(zeroVang);
		x += (float)cos(radang)*(HALFGAP);
		y += (float)sin(radang)*(HALFGAP);

		parrow[0][0] = x;
		parrow[0][1] = y;
		radang = RAD((zeroVang+RTH));
		parrow[1][0] = x + (float)cos(radang)*TKW;
		parrow[1][1] = y + (float)sin(radang)*TKW;
		radang = RAD((zeroVang+LTH));
		parrow[2][0] = x + (float)cos(radang)*TKW;
		parrow[2][1] = y + (float)sin(radang)*TKW;
		c3i(GREENOVLY); DOARROW(parrow);
		break;

	case -1:
		if ((tkrp->ang-1800.0-zeroVang)/1800.0 < 0.0) {
			zeroVang = tkrp->ang-1800.0;
		}
		zeroVang -= TKPROP*3.0;
		if ((tkrp->ang-1800.0-zeroVang) >= 1800.0) {
			TokenVang = tkrp->ang;
			tkDirection = 2;
		}

		radang = RAD(TokenVang);
		x = (float)cos(radang)*(Radius-HALFGAP);
		y = (float)sin(radang)*(Radius-HALFGAP);
		radang = RAD(zeroVang);
		x += (float)cos(radang)*(HALFGAP);
		y += (float)sin(radang)*(HALFGAP);

		parrow[0][0] = x;
		parrow[0][1] = y;
		radang = RAD((zeroVang+RTH));
		parrow[1][0] = x + (float)cos(radang)*TKW;
		parrow[1][1] = y + (float)sin(radang)*TKW;
		radang = RAD((zeroVang+LTH));
		parrow[2][0] = x + (float)cos(radang)*TKW;
		parrow[2][1] = y + (float)sin(radang)*TKW;
		c3i(GREENOVLY); DOARROW(parrow);
		break;
	}

	prevDirection = tkDirection;
}

void
RingView::RecordOn()
{
	DIR *dirp;
	char buf[sizeof(FV.RecordDir)+20];

	if (FV.RecordDir[0] == '\0')
		strcpy(FV.RecordDir, ".");
	else
		(void)mkdir(FV.RecordDir, 0777);

	dirp = opendir(FV.RecordDir);
	if (!dirp) {
		sprintf(buf, "Open \"%s\" failed.", FV.RecordDir);
		FV.post(DIRACCERRMSG, buf);
		RecordOff();
		return;
	}
	closedir(dirp);

	FV.Drecord = 1;
	FV.chgBtnState(FV.Drecord);

	RecordOpr = FV.RingOpr;
	RecordRingNum = FV.RingNum;
	RecordTicks = 0;
	Record();
}

void
RingView::RecordOff()
{
	FV.Drecord = 0;
	FV.chgBtnState(FV.Drecord);
}

void
RingView::Record()
{
	int i;
	FILE *f;
	RING *rp;
	time_t now;
	char tfname[MAXPATHLEN+1];

	// record only when it is time, when not replaying, and
	// if something interesting has happened
	RecordTicks++;
	if (RecordTicks < FV.RecordInterval)
		return;
	RecordTicks = 0;
	if (FV.Dreplay)
		return;
	if (RecordOpr == FV.RingOpr
	    && RecordRingNum == FV.RingNum)
		return;
	RecordOpr = FV.RingOpr;
	RecordRingNum = FV.RingNum;


	(void)mkdir(FV.RecordDir, 0777);
	(void)strncpy(tfname, FV.RecordDir, sizeof(tfname)-1);
	tfname[sizeof(tfname)-1] = '\0';
	i = strlen(tfname);
	now = time(0);
	(void)strftime(&tfname[i],sizeof(tfname)-i-1,"/%a-%y.%m.%d-%T",
		       localtime(&now));
	i = strlen(tfname);
	// add hostname to file name if not localhost
	if (strcmp(FV.Agent, "localhost")
	    && i < sizeof(tfname)-3) {
		strcat(tfname, "-");
		strncat(tfname, FV.Agent, sizeof(tfname)-1);
	}
	if ((f = fopen(tfname, "w")) == NULL) {
		sprintf(tfname, "Record file open failed");
		goto badrecret;
	}

	i = SMTD_VERS;
	if (fwrite((char*)&i,1,sizeof(i), f) != sizeof(i))
		goto recret;
	i = FV.RingNum;
	if (fwrite((char*)&i,1,sizeof(i), f) != sizeof(i))
		goto recret;
	i = FV.RingOpr;
	if (fwrite((char*)&i,1,sizeof(i), f) != sizeof(i))
		goto recret;

	for (i = FV.RingNum, rp = FV.ring; i > 0; i--, rp = rp->prev) {
		if (fwrite((char*)rp,1,sizeof(*rp), f) != sizeof(*rp))
			break;
	}
	fflush(f);
	fclose(f);
	return;
recret:
	fflush(f);
	fclose(f);
	sprintf(tfname, "Record file write failed");

badrecret:
	FV.Drecord = 0;
	FV.chgBtnState(FV.Drecord);
	FV.post(FILEACCERRMSG, tfname);
}

void
RingView::Replay(RECPLAY *recp, int summary)
{
	int i, j;
	int tmp_vers, tmp_rnum, tmp_stat;
	FILE *f;
	RING *rp;
	char tfname[PROMPTSIZE+1+sizeof(recp->name)+1];

	tkrp = 0;
	stkrp = 0;
	sprintf(tfname, "%s/%s", FV.ReplayDir, recp->name);
	if ((f = fopen(tfname, "r")) == NULL) {
		recp->RecStat &= STALE;
		FV.post(FILEACCERRMSG, tfname);
		return;
	}

	if (recp->RecRp == 0) {
		SINFO *ip;

		// version
		i = fread(&tmp_vers, 1, sizeof(tmp_vers), f);
		if (i != sizeof(tmp_vers) || (tmp_vers != SMTD_VERS))
			goto rply_ret2;

		// number of nodes
		i = fread(&tmp_rnum, 1, sizeof(tmp_rnum), f);
		if (i != sizeof(tmp_rnum))
			goto rply_ret2;

		// ring status
		i = fread(&tmp_stat, 1, sizeof(tmp_stat), f);
		if (i != sizeof(tmp_stat))
			goto rply_ret2;

		if (summary) {
			struct stat st;

			if (stat(tfname, &st) < 0)
				goto rply_ret2;
			recp->ts = st.st_ctime;
			recp->RecNum = tmp_rnum;
			recp->RecStat = tmp_stat;
			goto rply_ret1;
		}

		if (tmp_stat != recp->RecStat)
			goto rply_ret2;

		i = 0;
		rp = (RING *)Malloc(sizeof(RING));
		while (fread((char*)rp,1,sizeof(*rp),f) == sizeof(*rp)) {
			for (j = 0, ip = &rp->sm; j < 2; j++, ip++) {
				ip->nbrs = 0;
				ip->pds = 0;
				ip->mac_stat = 0;
				ip->ler = 0;
				ip->mfc = 0;
				ip->mfncc = 0;
				ip->mpv = 0;
				ip->ebs = 0;
			}

			if (i == 0) {
				recp->RecRp = rp;
				rp->prev = rp;
				rp->next = rp;
			} else {
				FV.Ring.list_insert(rp, recp->RecRp);
			}
			i++;
			rp = (RING *)Malloc(sizeof(RING));
		}
		free(rp);

		if ((i != tmp_rnum) || (i != recp->RecNum))
			goto rply_ret2;

		fclose(f);
	}

	if (!(recp->RecStat&STALE)) {
		FV.RingOpr = recp->RecStat;
		FV.RingNum = recp->RecNum;
		FV.ring = recp->RecRp;
		FV.RINGWindow->redrawWindow();
		FV.MAPWindow->redrawWindow();
	}
	return;

rply_ret2:
	recp->RecStat &= STALE;
	FV.post(FILEACCERRMSG, tfname);
rply_ret1:
	fclose(f);
}

RECPLAY *
RingView::ReplayDelete(int seq, int del)
{
	int i;
	RECPLAY *recp = RecordTbl;

	for (i = 0; i < NumRecords; i++, recp = recp->next) {
		if (i == seq) {
			if (del)
				recp->RecStat |= DELETED;
			else
				recp->RecStat &= ~DELETED;
			return(recp);
		}
	}
	return(0);
}

void
RingView::ReplaySeq(int seq)
{
	int i;
	RECPLAY *recp = RecordTbl;

	for (i = 0; i < NumRecords; i++, recp = recp->next) {
		if (i == seq) {
			if (!(recp->RecStat&STALE))
				Replay(recp, 0);
			return;
		}
	}
}

void
RingView::ReplayOn(int complain)
{
	DIR *dirp;
	struct direct *dp;
	RECPLAY *recp, *tp;

	if (FV.CurReplayDir[0] == '\0')
		return;
	if ((dirp = opendir(FV.CurReplayDir)) == NULL) {
		char buf[sizeof(FV.CurReplayDir)+20];
		if (complain) {
			sprintf(buf, "Open %s failed.", FV.CurReplayDir);
			FV.post(DIRACCERRMSG, buf);
		}
		return;
	}

	ReplayOff();

	tp = 0;
	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;

		recp = (RECPLAY *)Malloc(sizeof(RECPLAY));
		bzero(recp, sizeof(*recp));
		strncpy(recp->name, dp->d_name, sizeof(recp->name)-1);
		if (RecordTbl == 0)
			RecordTbl = recp;
		else
			tp->next = recp;
		tp = recp;
		Replay(recp, 1);
		NumRecords++;
	}
	closedir(dirp);

	if (NumRecords <= 0)
		return;

	/* sort by time */
	register int i, j;
	RECPLAY **ptbl, *tmp;

	tmp = RecordTbl;
	ptbl = (RECPLAY**)Malloc(sizeof(tmp)*NumRecords);
	for (i = 0; i < NumRecords; i++, tmp = tmp->next) {
		ptbl[i] = tmp;
	}
	for (i = 0; i < NumRecords; i++) {
		for (j = i; j < NumRecords; j++) {
			if (ptbl[i]->ts > ptbl[j]->ts) {
				tmp = ptbl[j];
				ptbl[j] = ptbl[i];
				ptbl[i] = tmp;
			}
		}
	}
	for (i = 0; i < NumRecords; i++) {
		if (i == NumRecords-1)
			ptbl[i]->next = 0;
		else
			ptbl[i]->next = ptbl[i+1];
	}
	RecordTbl = ptbl[0];

	OnlineRp = FV.ring;
	OnlineRnum = FV.RingNum;
}

void
RingView::ReplayOff()
{
	int i, j;
	RING *rp, *trp;
	RECPLAY *recp;

	if ((RecordTbl == 0) || (NumRecords == 0))
		return;

	for (i = 0, recp = RecordTbl; i<NumRecords; i++, recp = recp->next) {
		if (recp->RecStat & DELETED) {
			char buf[sizeof(FV.CurReplayDir)+1
				 +sizeof(recp->name)+1];
			sprintf(buf, "%s/%s", FV.CurReplayDir, recp->name);
			(void)unlink(buf);
		}
		if (recp->RecRp == 0)
			continue;
		for (rp = recp->RecRp, j = recp->RecNum; j > 0; j--) {
			trp = rp;
			rp = rp->next;
			FV.Ring.free_a_info(&trp->sm);
			FV.Ring.free_a_info(&trp->dm);
			free(trp);
		}
	}

	free(RecordTbl);
	RecordTbl = 0;
	NumRecords = 0;

	FV.ring = OnlineRp;
	FV.RingNum = OnlineRnum;
	OnlineRp = 0;
	OnlineRnum = 0;
	FV.magrp = 0;
	FV.pickrp = 0;
	FV.RINGWindow->RINGView->resettoken();
	FV.RINGWindow->redrawWindow();
	FV.MAPWindow->redrawWindow();
}
