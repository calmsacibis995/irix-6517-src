/*
 * SymTab.h
 *
 *	declaration of SymTab class for regview
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

#ident "$Revision: 1.2 $"

#include <sys/procfs.h>

#include <Vk/VkComponent.h>

class Page;
class Symbol;

/*
 * This component is a scrolled text widget that you initialize with
 * an object and then you feed the starting virtual addresses of pages
 * to it and it displays the symbols that it knows about for that
 * page.
 */
class SymTab : public VkComponent {
public:
    SymTab(const char *name, Widget parent);
    ~SymTab();
    void setObject(const char *objName, prmap_sgi_t *map, long offset);
    void setPage(void *vaddr);
private:
    static XtResource resources[];
    char *noRegMsg, *noAddrMsg, *noSymMsg, *noObjMsg;
    Page *table, *last;
    Widget text;
    int pageSize;

    void freeSymbols(Symbol *sym);
    void freeTable();
    void addSymbol(void *vaddr, char type, char *sym);
    Page *findPage(void *vaddr);
};
