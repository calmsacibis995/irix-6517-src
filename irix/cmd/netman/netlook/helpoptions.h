/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Help Options
 *
 *	$Revision: 1.2 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

class tuGadget;
class NetLook;

class HelpOptions
{
public:
	HelpOptions(NetLook *);

	void close(void);

	// Callbacks from UI
	void snoop(tuGadget *);
	void map(tuGadget *);
	void netnode(tuGadget *);
	void traffic(tuGadget *);
	void hide(tuGadget *);

protected:
	// Callbacks from UI
	void general(tuGadget *);
	void main(tuGadget *);
	void newfile(tuGadget *);
	void datafile(tuGadget *);
	void uifile(tuGadget *);
	void info(tuGadget *);
	void find(tuGadget *);
	void ping(tuGadget *);
	void trace(tuGadget *);
	void home(tuGadget *);
	void hideAction(tuGadget *);
	void del(tuGadget *);
	void tools(tuGadget *);

private:
	NetLook *netlook;
};
