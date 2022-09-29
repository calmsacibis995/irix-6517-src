/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Dialog Boxes
 *
 *	$Revision: 1.5 $
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

#include <tuDialogBox.h>

enum tuDialogType { tuEmpty, tuInformation, tuProgress,
		    tuQuestion, tuWarning, tuAction };

enum tuDialogHitCode { tuNone, tuOK, tuYes, tuNo, tuCancel, tuHelp };


class DialogBox : public tuDialogBox {
public:
	DialogBox(const char* instanceName, tuColorMap* cmap,
		  tuResourceDB* db, char* progName, char* progClass,
		  Window transientFor = NULL);
	DialogBox(const char* instanceName, tuTopLevel* othertoplevel,
		  tuBool transientForOtherTopLevel = False);
	~DialogBox(void);

	virtual void map(tuBool propagate = True);
	virtual void unmap(tuBool propagate = True);

	virtual void setText(const char *c);
	virtual void setText(const char *c[]);

	virtual void setName(const char *c);

	void setCallBack(tuCallBack *c);

	enum tuDialogHitCode getHitCode(void);

	enum tuDialogType getDialogType(void);
	void setDialogType(enum tuDialogType t);

	void information(const char *format, ...);
	void progress(const char *format, ...);
	void question(const char *format, ...);
	void warning(const char *format, ...);
	void warning(int err, const char *format, ...);
	void error(const char *format, ...);
	void error(int err, const char *format, ...);

	void setMultiMessage(tuBool m);
	inline tuBool getMultiMessage(void);

	void setLineLength(unsigned int len);
	inline unsigned int getLineLength(void);

	void setWrapLength(unsigned int len);
	inline unsigned int getWrapLength(void);

protected:
	void initialize(void);
	void accelerator(tuGadget *);
	void callBack(tuGadget *);
	void makeMessage(char *);

private:
	enum tuDialogType dialogType;
	enum tuDialogHitCode hitCode;
	tuGadget *defaultGadget;
	tuBool mapping;
	tuBool multiMessage;
	const char *breakChars;
	char *text;
	tuCallBack *callback;
	unsigned int textLength;
	unsigned int lineLength;
	unsigned int wrapLength;
};

// Inline functions
tuBool
DialogBox::getMultiMessage(void)
{
    return multiMessage;
}

unsigned int
DialogBox::getLineLength(void)
{
    return lineLength;
}

unsigned int
DialogBox::getWrapLength(void)
{
    return wrapLength;
}
