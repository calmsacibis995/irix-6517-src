/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snoop Gadget
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

#include <tuArray.h>

class tuGadget;
class NetLook;
class tuTextField;
class tuRowColumn;
class Node;

class SnoopGadgetArray : public tuArray
{
public:
	SnoopGadgetArray(void) : tuArray(sizeof(void *)) { }
	~SnoopGadgetArray(void) { }

	inline void *&operator[](const int);
	inline void add(void * const);
	inline int find(void *);
	inline void remove(void * const);
	inline void remove(int index, int count = 1);
};

class SnoopGadget
{
public:
	SnoopGadget(NetLook *nl, tuGadget *parent, const char *instanceName);

	// Add to list
	unsigned int add(const char *name, tuBool activate);
	unsigned int get(char **entry[]);

	// Set filter text
	void setFilterText(const char *);

	// Deactivate a node
	void deactivate(Node *);
	void deactivate(const char *);

protected:
	unsigned int check(tuGadget *);

	// Callbacks from UI
	void vcheck(tuGadget *);
	void edit(tuGadget *);
	void change(tuGadget *);
	void doFilterText(tuGadget *);
	void archiver(tuGadget *);
	void closeIt(tuGadget *);
	void help(tuGadget *);

private:
	const char *name;
	tuGadget *ui;
	NetLook *netlook;

	SnoopGadgetArray checkbox;
	SnoopGadgetArray node;

	tuRowColumn *container;
	tuTextField *filterText;
	tuGadget *entryField;
};

// Inline functions
void *&
SnoopGadgetArray::operator[](const int i)
{
    assert(i >= 0 && i < length);
    return *((void **)base + i);
}

void
SnoopGadgetArray::add(void * const t)
{
    append(1);
    (*this)[length - 1] = t;
}

int
SnoopGadgetArray::find(void *t)
{
    return findptr((void *) &t);
}

void
SnoopGadgetArray::remove(void * const t)
{
    tuArray::remove(find(t));
}

void
SnoopGadgetArray::remove(int index, int count)
{
    tuArray::remove(index, count);
}
