#include <sys/types.h>
#include <sys/idbg.h>			/* For setsym symbols */
#include <sys/SN/arch.h>
#include <sys/SN/gda.h>			/* For setsym symbols */
#include "symbol.h"
#include "ip27prom.h"
#include "libc.h"
#include "libasm.h"
#include "libkl.h"

#if 0
#define SYMMAX	1800

#else
#define SYMMAX	1
#endif
#define NAMESIZ SYMMAX*12

struct dbstbl dbstab[SYMMAX] = {0};

char nametab[NAMESIZ] = {0};
int symmax = SYMMAX;
int namesize = NAMESIZ;
char   *symbol_name(int indx, char *name_tab, struct dbstbl *sym_tab);
void   *symbol_addr(int indx, struct dbstbl *sym_tab);
size_t	symbol_size(int indx, struct dbstbl *sym_tab);

/*
 * Search for name, return mapped prom address
 * return 0 if can't find name
 * assumed table is sorted in ascending prom addr
 */


#if 0
int symtab_lookup_name(syminfo_t *syminfo, 
		       struct dbstbl *sym_tab,
		       char *name_tab, int symmax)
{
    struct dbstbl *dbptr;
    char *targetname, *curname;
    int i;

    for (i = 0; i < symmax; i++) {
	dbptr = &sym_tab[i];

	if (dbptr->addr == 0)
	    return -1;

	targetname = syminfo->sym_name;
	curname = (char *) &name_tab[dbptr->noffst];

	while (LBYTE(targetname) == LBYTE(curname)) {
	    if (LBYTE(targetname) == 0) {
		syminfo->sym_size = symbol_size(i, sym_tab);
		syminfo->sym_addr = symbol_addr(i, sym_tab); 
		syminfo->sym_name = symbol_name(i, name_tab, sym_tab); 
		return i;
	    }
	    targetname++;
	    curname++;
	}
    }
    return -1;
}
#else
int symtab_lookup_name(syminfo_t *syminfo, 
		       struct dbstbl *sym_tab,
		       char *name_tab, int symmax)
{
 return -1;
}

#endif
int symbol_lookup_name(syminfo_t *syminfo)
{
    int ndx;

    if ((get_BSR() & BSR_GDASYMS) && (GDA!=NULL) && (GDA->g_dbstab != NULL)) {
	if ((ndx = symtab_lookup_name(syminfo, GDA->g_dbstab, 
				      GDA->g_nametab, GDA->g_symmax)) != -1) {
	    return ndx;
	}
    }
    
    return symtab_lookup_name(syminfo, dbstab, nametab, symmax);
}

/*
 * Search for prom address in name table created by setsym,
 * and return symbol number.  Return -1 if can't find in table.
 */

#if 0
int symtab_lookup_addr(syminfo_t *syminfo, 
		       struct dbstbl *sym_tab,
		       char *name_tab, int symmax)
{
    int found;
    int hi;
    int mid;
    int lo;

    hi = symmax - 1;
    mid = (symmax - 1) / 2;
    lo = 0;

    while (mid != lo) {
	if ((__psint_t) syminfo->sym_addr < sym_tab[mid].addr) {
	    hi = mid;
	} else {
	    lo = mid;
	}
	mid = (hi + lo) / 2;
    }

    if ((__psint_t) syminfo->sym_addr < sym_tab[hi].addr)
	found = lo;
    else
	found = hi;

    if (((__psint_t) syminfo->sym_addr - sym_tab[found].addr) > 0x2000)
	return -1;

    syminfo->sym_addr = symbol_addr(found, sym_tab);
    syminfo->sym_name = symbol_name(found, name_tab, sym_tab);
    syminfo->sym_size = symbol_size(found, sym_tab);

    return (found);
}
#else
int symtab_lookup_addr(syminfo_t *syminfo, 
		       struct dbstbl *sym_tab,
		       char *name_tab, int symmax)
{
	return -1;
}

#endif

int symbol_lookup_addr(syminfo_t *syminfo)
{
    int ndx;

    if ((get_BSR() & BSR_GDASYMS) && (GDA!=NULL) && (GDA->g_dbstab != NULL)) {
	if ((ndx = symtab_lookup_addr(syminfo, GDA->g_dbstab, 
				      GDA->g_nametab, GDA->g_symmax)) != -1) 
	    return ndx;
    }
    return symtab_lookup_addr(syminfo, dbstab, nametab, symmax);
}


/*
 * symbol_name
 * symbol_addr
 * symbol_size
 */

char *symbol_name(int indx, char *name_tab, struct dbstbl *sym_tab)
{
    return (char *) &name_tab[sym_tab[indx].noffst];
}

void *symbol_addr(int indx, struct dbstbl *sym_tab)
{
    return (void *) sym_tab[indx].addr;
}

size_t symbol_size(int indx, struct dbstbl *sym_tab)
{
    return ((indx == symmax - 1) ? 0 :
	    sym_tab[indx + 1].addr - sym_tab[indx].addr);
}

static char *lookup_name(void *addr, void *offst)
{
    int indx;
    __psunsigned_t *off = offst;
    syminfo_t syminfo;
    syminfo.sym_addr = addr;
    if (symbol_lookup_addr(&syminfo) == -1)
	return NULL;

    *off = (__psunsigned_t) addr - (__psunsigned_t)syminfo.sym_addr;

    if (*off > 0x2000)
	return (NULL);

    return syminfo.sym_name;
}

/*
 * symbol_name_off - Translate a PROM address to a "symbol name + offset"
 * string and output this string. If there is no valid translation, just
 * print the hex address.
 */

void symbol_name_off(void *addr, char *buf)
{
    char *nptr;
    __psunsigned_t offst;
    int in_prom;

    in_prom = ((__psunsigned_t) addr >= 0xffffffffbfc00000ULL &&
	       (__psunsigned_t) addr <	0xffffffffbfd00000ULL);

    if (in_prom)
	addr = (void *) (K2BASE | (__psunsigned_t) addr & 0x1fffffffULL);

    if (nptr = lookup_name(addr, &offst)) {
	if (offst)
	    sprintf(buf, "%s+0x%lx", nptr, offst);
	else
	    sprintf(buf, "%s", nptr);
    } else
	sprintf(buf, "0x%lx", addr);
}

/*
 * symbol_dis
 *
 *   Returns the number of bytes of instruction decoded.
 *
 *   Uses routines from symmon's disMips.c.
 *   prom_fetch_kname and getinst are included to satisfy disMips.c's needs.
 */

char *prom_fetch_kname(inst_t *inst_p, char *buf, int jmp)
{
    char *nptr;
    __psunsigned_t offst;

    if (nptr = lookup_name(inst_p, &offst)) {
	if (offst == 0 && jmp)
	    sprintf(buf, "%s", nptr);
	else
	    sprintf(buf, "%s+0x%04lx", nptr, offst);
    } else
	sprintf(buf, "0x%lx", inst_p);

    return buf;
}

inst_t getinst(inst_t *pc)
{
    return *pc;
}

int symbol_dis(inst_t *addr)
{
    extern int _PrintInstruction(inst_t *, int, int);

    return _PrintInstruction((inst_t *) addr, 0, 0);
}
