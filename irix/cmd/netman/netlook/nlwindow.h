/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Generic Window
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

class NetLook;

class nlWindow : public tuTopLevel
{
public:
	nlWindow(NetLook *nl, const char *instanceName, tuColorMap *cmap,
		 tuResourceDB *db, char *progName, char *progClass,
		 Window transientFor = NULL);
	nlWindow(NetLook *nl, const char *instanceName,
		 tuTopLevel *othertoplevel,
		 tuBool transientForOtherTopLevel = False);

	inline void *getChild(void);

	void setNormalCursor(void);
	void setDragCursor(void);
	void setSweepCursor(void);

protected:
	void initialize(NetLook *nl, const char *instanceName);

private:
	void *child;
};

// Inline functions
void *
nlWindow::getChild(void)
{
    return child;
}
