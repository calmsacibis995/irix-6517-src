/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Menu Gadget
 *
 *	$Revision: 1.1 $
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
class tuMenuBar;
class NetLook;
class FileOptions;
class GadgetOptions;
class ActionOptions;
class ToolOptions;
class HelpOptions;

class MenuGadget {
public:
	MenuGadget(NetLook *nl, tuGadget *, const char *instanceName);

private:
	const char *name;
	tuMenuBar *menu;

	FileOptions *fileHandler;
	GadgetOptions *gadgetHandler;
	ActionOptions *actionHandler;
	ToolOptions *toolHandler;
	HelpOptions *helpHandler;
};
