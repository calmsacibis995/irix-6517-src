/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.3 $
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

class ReplayWindow : public tkWindow {
protected:
	titledTable*	summaryTable;

	tkTextView	*ReplayDirText;

	tkButton*	undeleteButton;
	tkButton*	undeleteAllButton;
	tkButton*	deleteButton;
	tkButton*	deleteAllButton;
	tkButton*	quitButton;

	void		doDel(int, int);
	void		setATbl(RECPLAY*, int);
	void		setTbl(int);
	int		curSeq;

public:
	ReplayWindow();
	~ReplayWindow();

	virtual void	open();
	virtual void	close();
	virtual void	redrawWindow();
	virtual void	rcvEvent(tkEvent* e);

	tkTextViewEvent *ReplayEvent;
	tkEvent		*selectedEvent;
};
