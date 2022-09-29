/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Prompt Box
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

#include <tuTopLevel.h>

class tuCallBack;
class tuRowColumn;
class tuTextField;
class tuDeck;

enum PromptHitCode { tuNone, tuOK, tuApply, tuReset, tuCancel, tuHelp };

class PromptBox : public tuTopLevel {
public:
	PromptBox(const char* instanceName, tuColorMap* cmap,
		  tuResourceDB* db, char* progName, char* progClass,
		  Window transientFor = NULL);
	PromptBox(const char* instanceName, tuTopLevel* othertoplevel,
		  tuBool transientForOtherTopLevel = False);
	~PromptBox(void);

	virtual void map(tuBool propagate = True);

	virtual void setText(const char* s);
	virtual void setText(const char* s[]);

	inline tuTextField *getTextField(void);

	void setCallBack(tuCallBack* c);

	void setHelp(tuBool b);
	inline tuBool getHelp(void);
	void setApply(tuBool b);
	inline tuBool getApply(void);
	void setReset(tuBool b);
	inline tuBool getReset(void);

	enum PromptHitCode getHitCode(void);

	void mapWithCancelUnderMouse(void);

protected:
	void initialize(void);
	void setDeck(void);
	void ok(tuGadget *);
	void apply(tuGadget *);
	void reset(tuGadget *);
	void cancel(tuGadget *);
	void help(tuGadget *);

private:
	tuRowColumn* textContainer;
	tuTextField* textField;
	tuDeck* deck;
	tuCallBack* callback;
	tuBool showHelp;
	tuBool showApply;
	tuBool showReset;
	enum PromptHitCode hitCode;
};

// Inline functions
tuTextField *
PromptBox::getTextField(void)
{
    return textField;
}

tuBool
PromptBox::getHelp(void)
{
    return showHelp;
}

tuBool
PromptBox::getApply(void)
{
    return showApply;
}

tuBool
PromptBox::getReset(void)
{
    return showReset;
}
