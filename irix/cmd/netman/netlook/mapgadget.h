/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Map Gadget
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
class GLMapGadget;

class MapGadget {
public:
	MapGadget(NetLook *nl, tuGadget *, const char *instanceName);

protected:
	// Callbacks from UI
	void closeIt(tuGadget *);
	void help(tuGadget *);

private:
	const char *name;
	tuGadget *ui;
	GLMapGadget *glmg;
	NetLook *netlook;
};
