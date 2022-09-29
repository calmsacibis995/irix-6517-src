/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Action Options
 *
 *	$Revision: 1.3 $
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
class tuButton;
class PromptBox;
class NetLook;

#define SpectrumCommand "/usr/sbin/nv2spectrum"

class ActionOptions
{
public:
	ActionOptions(NetLook *);

	// Spectrum interoperability
	void addSpectrumCallBack(tuButton *);

protected:
	void createPromptBox(void);

	// Callbacks from UI
	void findAction(tuGadget *);
	void pingAction(tuGadget *);
	void traceAction(tuGadget *);
	void homeAction(tuGadget *);
	void hideAction(tuGadget *);
	void deleteAction(tuGadget *);

	void info(tuGadget *);
	void find(tuGadget *);
	void ping(tuGadget *);
	void trace(tuGadget *);
	void Delete(tuGadget *);
	void deleteAll(tuGadget *);
	void spectrum(tuGadget *);

private:
	NetLook *netlook;
	PromptBox *prompt;
};
