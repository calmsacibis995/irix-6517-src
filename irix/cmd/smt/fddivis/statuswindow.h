#ifndef __STATUSWINDOW_H__
#define __STATUSWINDOW_H__
/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.4 $
 */
#include "replayview.h"

#define SUMM_COL_SEQ	0
#define SUMM_COL_TIME	2
#define SUMM_COL_LEN	4
#define SUMM_COL_SRC	6
#define SUMM_COL_SRCP	8
#define SUMM_COL_DST	10
#define SUMM_COL_DSTP	12
#define SUMM_COL_PROTO	14

#define NHEXCOLUMNS	16

#define NOTIFY_DESTROY	700
#define EVENT_CONTINUE	701

class titledTable;
class tkWindow;
class tkValueEvent;

typedef struct sortentry {
	RING *rp;
	int smac;
} STAB;

#define DELTA_FLAG	0x1
#define ZERO_FLAG	0x2

class StatusWindow : public tkWindow {
protected:
	titledTable*	statTable;

	tkLabel		*StatusText;

	tkRadioButton*	modeButton;
	tkButton*	holdButton;
	tkButton*	normalButton;
	tkButton*	deltaButton;
	tkButton*	zeroButton;
	tkButton*	updateButton;
	tkButton*	quitButton;

	tkButton*	saveButton;
	tkTextView	*saveEdit;
	tkTextView	*offsaveEdit;
	char		saveBuf[128];

	void		setATbl(RING*, int, int);
	int		cmd;

	int		dozero;
	int		rnum;
	int		lastnum;
	STAB		*sortary;
	void		saveTbl();
	void		sortATbl(int);
	char		ztds_buf[32];		// zero time & date stamp
	char		ts_buf[32];		// current timestamp
	char		tds_buf[32];		// current time & date stamp
	char		ptds_buf[32];		// previous udate
	void		getStatusText(char*);
	int		hold;
public:
	StatusWindow();
	~StatusWindow();

	virtual void	open();
	virtual void	close();
	virtual void	redrawWindow();
	virtual void	rcvEvent(tkEvent* e);

	int		delta;
	void		setTbl();
	void		updateTbl();

	tkTextViewEvent *saveEvent;
};
#endif
