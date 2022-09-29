/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Tool Options
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
class tuTopLevel;
class PromptBox;

class ToolOptions {
public:
	ToolOptions(tuTopLevel * = 0);
	~ToolOptions(void);

	// Must pass a top level either here or through the constructor
	// before the first callback.
	inline void setTopLevel(tuTopLevel *);

	void setInterface(const char *);
	void setFilter(const char *);

protected:
	void analyzer(tuGadget *);
	void browser(tuGadget *);
	void netgraph(tuGadget *);
	void netlook(tuGadget *);
	void nettop(tuGadget *);
	void run(tuGadget *);
	void doPrompt(const char *);

private:
	tuTopLevel *toplevel;
	PromptBox *prompt;
	char *interface;
	char *filter;
};

// Inline functions
void
ToolOptions::setTopLevel(tuTopLevel *t)
{
    toplevel = t;
}
