/*
 * SymTab.c++
 *
 *	definition of SymTab class for regview
 *
 * Copyright 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
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

#ident "$Revision: 1.5 $"

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <dem.h>

#include <Xm/Text.h>
#include <Xm/ScrolledW.h>
#include <Xm/Form.h>

#include <Vk/VkWarningDialog.h>

#include "SymTab.h"

class Symbol {
public:
    char *name;			// Name of symbol
    void *value;		// Virtual address of sybol
    Symbol *next;		// Linkage
};

class Page {
public:
    void *vaddr;		// Virtual address of page
    Symbol *head, *tail;	// List of symbols on this page

    int symlen;			// Total number of bytes, including
				// newlines, needed to create a text
				// string to put in the text widget to
				// display all the symbols for this
				// page.

    Page *next;			// Linkage
};

XtResource SymTab::resources[] = {
    { "noRegMsg", "NoRegMsg", XmRString, sizeof (char *),
      XtOffset(SymTab *, noRegMsg), XmRString, "" },
    { "noAddrMsg", "NoAddrMsg", XmRString, sizeof (char *),
      XtOffset(SymTab *, noAddrMsg), XmRString,
      "Click on a page to see\nits symbols" },
    { "noSymMsg", "NoSymMsg", XmRString, sizeof (char *),
      XtOffset(SymTab *, noSymMsg), XmRString,
      "No symbols found on this page" },
    { "noObjMsg", "NoObjMsg", XmRString, sizeof (char *),
      XtOffset(SymTab *, noObjMsg), XmRString,
      "Can't find object for\nthis region" },
};

/*
 *  SymTab::SymTab(const char *name, Widget parent) : VkComponent(name)
 *
 *  Description:
 *      Constructor for SymTab class.  Create the widgets we need and
 *      get resources.
 *
 *  Parameters:
 *      name    ViewKit name of the SymTab component.
 *      parent  parent Widget
 */

SymTab::SymTab(const char *name, Widget parent) : VkComponent(name)
{
    text = XmCreateScrolledText(parent, "symTab", NULL, 0);
    XtManageChild(text);
    _baseWidget = XtParent(text);

    installDestroyHandler();
    getResources(resources, XtNumber(resources));

    table = NULL;
    pageSize = getpagesize();
}

/*
 * Destructor for SymTab class.
 */
SymTab::~SymTab()
{
}

/*
 *  void SymTab::freeSymbols(Symbol *sym)
 *
 *  Description:
 *      Free all the memory consumed by a list of symbols.
 *
 *  Parameters:
 *      sym  List of symbols to free.
 */

void SymTab::freeSymbols(Symbol *sym)
{
    Symbol *next;

    while (sym) {
	next = sym->next;
	free(sym->name);
	delete sym;
	sym = next;
    }
}

/*
 *  void SymTab::freeTable()
 *
 *  Description:
 *      Free the memory consumed by our symbol table.
 */

void SymTab::freeTable()
{
    Page *next;

    while (table) {
	next = table->next;
	freeSymbols(table->head);
	delete table;
	table = next;
    }
    last = NULL;
}

/*
 *  void SymTab::addSymbol(void *vaddr, char type, char *sym)
 *
 *  Description:
 *      Add a symbol to our symbol table.  This code assumes that the
 *      symbols are added in order by virtual address, which is what
 *      we tell nm to do (see setObject below).
 *      
 *      The arguments to this function correspond to a line of output
 *      from nm.
 *
 *  Parameters:
 *      vaddr  Virtual address
 *      type   Type of symbol
 *      sym    name of symbol
 */

void SymTab::addSymbol(void *vaddr, char, char *sym)
{
    Symbol *s = new Symbol;
    s->name = strdup(sym);
    s->value = vaddr;
    s->next = NULL;
    
    if (last && (char *)last->vaddr <= (char *)vaddr
	&& (char *)vaddr < (char *)last->vaddr + pageSize) {
	last->tail->next = s;
	last->tail = s;
	last->symlen += strlen(s->name) + strlen("00000000") + 2;
    } else {
	/*
	 * This code fills gaps in our symbol table with fake symbols
	 * that look like "lastSymbol + 623", where lastSymbol is the
	 * last symbol we found and 623 is the byte offset of the
	 * beginning of the empty page from the last symbol.  This
	 * keeps pages for multi-page symbols filled with meaningful
	 * information.
	 */
	if (last) {
	    void *lastVaddr;
	    char *lastSym;
	    int lastSymLen;
	    lastVaddr = last->tail->value;
	    lastSym = last->tail->name;
	    lastSymLen = strlen(lastSym);

	    char *page = (char *)vaddr - ((unsigned int)vaddr % pageSize);
	    for (char *pg = (char *)lastVaddr
		 - ((unsigned int)lastVaddr) % pageSize + pageSize;
		    pg < page; pg += pageSize) {
		Symbol *symbol = new Symbol;
		symbol->value = (void *)pg;
		symbol->next = NULL;
		symbol->name = (char *)malloc(lastSymLen + 10);
		(void)sprintf(symbol->name, "%s + %d", lastSym, 
			      pg - (char *)lastVaddr); 
		
		Page *p = new Page;
		p->vaddr = pg;
		p->head = p->tail = symbol;
		p->next = NULL;
		p->symlen = strlen(symbol->name) + strlen("00000000") + 1;
		last->next = p;
		last = p;
	    }
	}

	Page *p = new Page;
	p->vaddr = (void *)((char *)vaddr - ((unsigned int)vaddr % pageSize));
	p->head = p->tail = s;
	p->next = NULL;
	p->symlen = strlen(s->name) + strlen("00000000") + 2;
	if (last) {
	    last->next = p;
	} else {
	    table = p;
	}
	last = p;
    }
}

/*
 *  void SymTab::setObject(const char *objName)
 *
 *  Description:
 *      Initialize our symbol table to store all the symbols that nm
 *      can find in objName.
 *
 *  Parameters:
 *      objName  Name of object to run nm on to get symbol table.
 */

void SymTab::setObject(const char *objName, prmap_sgi_t *map, long offset)
{
    /*
     * Free the old table
     */
    freeTable();

    /*
     * Just put up a message if there is no object.
     */
    if (!objName[0]) {
	XmTextSetString(text, noObjMsg);
	return;
    }

    if (access("/usr/bin/nm", F_OK | X_OK) == -1) {
	theWarningDialog->post("noNmMsg");
	return;
    }

    /*
     * Run nm to get the symbols from the object.
     */
    char cmd[PATH_MAX + 50];
    (void)sprintf(cmd, "/usr/bin/nm -x -Bn %s 2>/dev/null", objName);
    FILE *p = popen(cmd, "r");
    if (!p) {
	XmTextSetString(text, strerror(errno));
	return;
    }
    char stdioBuf[BUFSIZ];
    (void)setbuffer(p, stdioBuf, sizeof stdioBuf);

    /*
     * Parse nm's output
     */
    char line[500], type, sym[500];
    void *vaddr;
    while (fgets(line, sizeof line, p) != NULL) {
	if (sscanf(line, "%x %c %s", &vaddr, &type, sym) == 3) {
	    if (vaddr && type != 'U') {
		vaddr = (void *)((char *)vaddr + offset);
		if ((char *)map->pr_vaddr <= (char *)vaddr
		    && (char *)vaddr < (char *)map->pr_vaddr + map->pr_size) {
		    char buf[1000];
		    addSymbol(vaddr, type, demangle(sym, buf) == 0 ? buf : sym);
		}
	    }
	}
    }

    pclose(p);

    /*
     * Tell user to click to see symbols
     */
    XmTextSetString(text, noAddrMsg);
}

/*
 *  Page *SymTab::findPage(void *vaddr)
 *
 *  Description:
 *      Find a page in our symbol table.
 *
 *  Parameters:
 *      vaddr  Virtual address of the page we're looking for.
 *
 *  Returns:
 *	The page if we have it, NULL if we don't.
 */

Page *SymTab::findPage(void *vaddr)
{
    Page *p = table;
	
    while (p) {
	if (p->vaddr == vaddr) {
	    return p;
	}
	p = p->next;
    }

    return NULL;
}

/*
 *  void SymTab::setPage(void *vaddr)
 *
 *  Description:
 *      Display all the symbols for the page which starts at "vaddr".
 *
 *  Parameters:
 *      vaddr  Virtual address of page to display.
 */

void SymTab::setPage(void *vaddr)
{
    Page *p = findPage(vaddr);
    if (!p) {
	XmTextSetString(text, noSymMsg);
	return;
    }

    char *str = new char[p->symlen], *pc = str;
    XmTextSetString(text, "");

    Symbol *sym = p->head;
    while (sym) {
	(void)sprintf(pc, "%08x %s", sym->value, sym->name);
	sym = sym->next;
	if (sym) {
	    strcat(pc, "\n");
	}
	pc += strlen(pc);
    }
    XmTextSetString(text, str);
    delete [] str;
}
