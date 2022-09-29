/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *	SGI module loader idbg routines.
 */

#ident	"$Revision: 1.8 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/systm.h>

#include <elf.h>
#include <aouthdr.h>
#include <filehdr.h>
#include <scnhdr.h>
#include <sym.h>
#include <symconst.h>
#include <reloc.h>

#include <sys/mload.h>
#include <sys/mloadpvt.h>

/* 
 * idbg routines
 *
 * interfaces:
 *
 *	mlist - print list of loaded/registered modules
 *	msyms - print list of symbols for a loaded module
 *	mname - given address, find symbol and offset
 *	maddr - given name, find address
 *	mlkup - print all symbols which contain the given string
 *	mlkaddr - print 10 symbols and addresses in +/- the given address
 */


/* interfaces */
void idbg_mlist(int);
void idbg_msyms(int);
void idbg_mlkup(char *);
void idbg_mlkaddr(__psint_t);
void mloadidbginit(void);

/* external references */
extern struct dbstbl dbstab[];
extern char nametab[];
extern int symmax;
extern void idbg_maddr(__psint_t *, char *);
extern void idbg_mname(__psunsigned_t *, char *);
extern int idbg_melfname(ml_info_t *, __psunsigned_t *, char *);
extern int idbg_melfaddr(ml_info_t *, __psint_t *, char *);

static void
melflkaddr(ml_info_t *m, __psint_t addr)
{
	ml_sym_t *sym, *symtab;
	int closeidx, start, i, max;

	sym = symtab = m->ml_symtab;

	/* if the symbol isn't in this table, continue */
	if ((!m->ml_nsyms) || (addr < sym->m_addr) || 
				(addr > symtab[m->ml_nsyms-1].m_addr)) {
			return;
	}

	/* find symbol closest to addr */
	for (i = 0, closeidx = -1; i < (m->ml_nsyms - 1); i++, sym++) {
		if (addr >= sym->m_addr && addr < (sym+1)->m_addr) {
			closeidx = i;
			break;
		}
	}

	/* of course we should have found a closeidx ... */
	if (closeidx < 0)
		return;

	/* print 4 symbols before closeidx, closeidx, and 5 after */
	start = closeidx >= 4 ? closeidx - 4 : 0;
	for (i = start; i <= closeidx; i++) {
		qprintf ("0x%x\t%s\n", symtab[i].m_addr,
				m->ml_stringtab + symtab[i].m_off);
	}

	max = (closeidx + 5) > m->ml_nsyms ? m->ml_nsyms : closeidx + 5;
	for (i = closeidx+1; i < max; i++) {
		qprintf ("0x%x\t%s\n", symtab[i].m_addr,
				m->ml_stringtab + symtab[i].m_off);
	}
	return;
}

/*
 * kp mlist - print loaded module list
 */
/*ARGSUSED*/
void
idbg_mlist(int x)
{
	extern ml_info_t *mlinfolist;
	ml_info_t *m;
	int i;

	m = mlinfolist;
	if (!m) {
	    qprintf("No modules loaded.\n");
	    return;
	}
	while (m) {
	    qprintf("%s: id %d", m->ml_desc->m_prefix, m->ml_desc->m_id);
	    if (m->ml_text != 0 && m->ml_end != 0) {
		    if (m->ml_text == m->ml_base) {
			    qprintf(", text 0x%x to 0x%x, refcnt %d\n",
				    m->ml_text, m->ml_end,
				    m->ml_desc->m_refcnt);
		    } else {
			    qprintf(", text 0x%x to 0x%x (align 0x%x), refcnt %d\n",
				    m->ml_text, m->ml_end,
				    m->ml_text - m->ml_base,
				    m->ml_desc->m_refcnt);
		    }
	    } else {
		    qprintf(", refcnt %d\n", m->ml_desc->m_refcnt);
	    }
	    qprintf("\tml_info_t:  0x%x\n", m);
	    qprintf("\tflags:  ");
	    if (m->ml_desc->m_flags & M_REGISTERED)
		qprintf ("REGISTERED ");
	    if (m->ml_desc->m_flags & M_LOADED)
		qprintf ("LOADED ");
	    if (m->ml_desc->m_flags & M_TRANSITION)
		qprintf ("TRANSITION ");
	    if (m->ml_desc->m_flags & M_UNLOADING)
		qprintf ("UNLOADING ");
	    if (m->ml_desc->m_flags & M_NOAUTOUNLOAD)
		qprintf ("NOAUTOUNLOAD ");

	    if (m->ml_desc->m_flags & M_EXPIRED) {
		    qprintf("\ttflags: EXPIRED \n");
	    } else
		    qprintf ("\n");
	    if (m->ml_desc->m_delay != M_NOUNLD) {
		if (m->ml_desc->m_delay == M_UNLDDEFAULT)
		   qprintf ("\tunload delay %d minutes\n", module_unld_delay);
		else
		   qprintf ("\tunload delay %d minutes\n",m->ml_desc->m_delay);
	    } else
	        qprintf ("\tno auto-unload\n");
	    if (m->ml_desc->m_type == M_DRIVER) {
		cfg_driver_t *drv = m->ml_desc->m_data;
		qprintf("\tdriver info:  MAJOR ");
		for (i=0; i<drv->d.d_nmajors; i++)
		    qprintf ("%d  ", drv->d.d_majors[i]);
		qprintf("\n\t\t");
		if (drv->d_typ == (MDRV_CHAR|MDRV_BLOCK))
		    qprintf ("bdevsw=0x%x cdevsw=0x%x ", drv->d_bdevsw, drv->d_cdevsw);
		else if (drv->d_typ == MDRV_CHAR)
		    qprintf ("cdevsw=0x%x  ", drv->d_cdevsw);
		else if (drv->d_typ == MDRV_BLOCK)
		    qprintf ("bdevsw=0x%x  ", drv->d_bdevsw);
		else if (drv->d_typ == MDRV_STREAM)
		    qprintf ("cdevsw=0x%x  ", drv->d_cdevsw);
		qprintf ("open 0x%x, unload 0x%x\n", 
		    drv->d_ropen, drv->d_unload);
	    } else if (m->ml_desc->m_type == M_FILESYS) {
		qprintf ("Illegal module type: filesys\n");
	    } else if (m->ml_desc->m_type == M_STREAMS) {
		cfg_streams_t *str = m->ml_desc->m_data;
		qprintf("\tstreams module info:  ");
		qprintf("fmodsw entry %d\n", str->s_fmod);
		qprintf("\topen 0x%x, unload 0x%x, close 0x%x, info 0x%x\n", 
		    str->s_ropen, str->s_unload, str->s_rclose, str->s_info);
	    } else if (m->ml_desc->m_type == M_OTHER) {
		qprintf ("Illegal module type: other\n");
	    }
	    m = m->ml_next;
	}
}

/*
 * kp msyms - print loaded module symbols
 */
void
idbg_msyms(int id)
{
	ml_info_t *m;
	int i;

	m = mlinfolist;
	while (m) {
	    if (m->ml_desc->m_id == id)
		break;
	    m = m->ml_next;
	}
	if (!m) {
	    qprintf("Module %d not found.\n", id);
	    return;
	}
	qprintf ("Symbols for module %d (prefix %s)\n", id,
	    m->ml_desc->m_prefix);
	for (i = 0; i < m->ml_nsyms; ++i) {
	    if (m->flags & ML_ELF) {
		ml_sym_t *symtab = m->ml_symtab;
		qprintf ("%s 0x%x\n", m->ml_stringtab + symtab[i].m_off,
							symtab[i].m_addr);
	    }
	}
}

/*
 * kp mlkup - fetch symbol addr from a loaded module symbol table
 */
 
void
idbg_mlkup(char *name)
{
	ml_sym_t *elfsym;
	ml_info_t *m;
	int i;

	m = mlinfolist;
	while (m) {
	    if (m->flags & ML_ELF) {
		elfsym = (ml_sym_t *)m->ml_symtab;
		for (i = 0; i < m->ml_nsyms; i++, elfsym++ )
			if (strstr (&m->ml_stringtab[elfsym->m_off], name)) 
				qprintf("0x%x\t%s\n", elfsym->m_addr,
				       &m->ml_stringtab[elfsym->m_off]);
	    }
	    m = m->ml_next;
	}
	return;
}

/*
 * kp mlkaddr - list 10 symbols in the range of addr 
 */
void
idbg_mlkaddr(__psint_t addr)
{
	extern ml_info_t *mlinfolist;
	ml_info_t *m;

	m = mlinfolist;
	while (m) {
		if (m->flags & ML_ELF)
			melflkaddr(m, addr);
		m = m->ml_next;
	}
	return;
}

static void
mflags(int flags)
{
	if (flags & M_NOINIT) 
		qprintf("M_NOINIT ");
	if (flags & M_INITED)
		qprintf("M_INITED ");
	if (flags & M_SYMDEBUG)
		qprintf("M_SYMDEBUG ");
	if (flags & M_REGISTERED)
		qprintf("M_REGISTERED ");
	if (flags & M_LOADED)
		qprintf("M_LOADED ");
	if (flags & M_TRANSITION)
		qprintf("M_TRANSITION ");
	if (flags & M_UNLOADING)
		qprintf("M_UNLOADING ");
	if (flags & M_NOAUTOUNLOAD)
		qprintf("M_NOAUTOUNLOAD ");
}

static void
mtype(int type)
{
	switch(type) {
		case M_DRIVER:
			qprintf("M_DRIVER ");
			break;
		case M_STREAMS:
			qprintf("M_STREAMS ");
			break;
		case M_IDBG:
			qprintf("M_IDBG ");
			break;
		case M_LIB:
			qprintf("M_LIB ");
			break;
		case M_FILESYS:
			qprintf("M_FILESYS ");
			break;
		case M_ENET:
			qprintf("M_ENET ");
			break;
		case M_SYMTAB:
			qprintf("M_SYMTAB ");
			break;
		case M_OTHER:
			qprintf("M_OTHER ");
			break;
	};
}

/*
 * kp mlinfo - print ml_info_t 
 */

void
idbg_mlinfo(ml_info_t *m)
{
	cfg_desc_t *desc = m->ml_desc;

	qprintf("\tfname %s fnamelen %d prefix %s id %d\n", desc->m_fname, 
		desc->m_fnamelen, desc->m_prefix, desc->m_id);
	qprintf("\tm_flags ");
	mflags (desc->m_flags);	
	qprintf("m_type ");
	mtype(desc->m_type);
	qprintf("\n\tflags 0x%x next 0x%x obj 0x%x\n",
		m->flags, m->ml_next, m->ml_obj);
	qprintf("\tsymtab 0x%x strtab 0x%x nsyms %d strtabsz %d\n",
		m->ml_symtab, m->ml_stringtab, m->ml_nsyms, m->ml_strtabsz);
	if (m->ml_text == m->ml_base)
		qprintf("\tdesc 0x%x text 0x%x end 0x%x \n",
			m->ml_desc, m->ml_text, m->ml_end);
	else
		qprintf("\tdesc 0x%x text 0x%x end 0x%x (align 0x%x)\n",
			m->ml_desc, m->ml_text, m->ml_end,
			m->ml_text - m->ml_base);
	qprintf("\tcfg_vers %d m_data 0x%x\n",
		desc->m_cfg_version, desc->m_data);
	if (m->ml_desc->m_delay != M_NOUNLD) {
		if (m->ml_desc->m_delay == M_UNLDDEFAULT)
			qprintf ("\tunld delay %d mins ", module_unld_delay);
		else
			qprintf ("\tunld delay %d mins ",m->ml_desc->m_delay);
	} else
		qprintf ("\tno auto-unload ");
	qprintf("tflags 0x%x\n", desc->m_tflags);
	qprintf("\trefcnt %d m_info 0x%x m_next 0x%x\n",
		desc->m_refcnt, desc->m_info, desc->m_next);
	qprintf("\ttimeoutid 0x%x tsema &0x%x\n\n",
		desc->m_timeoutid, &desc->m_transsema); 	
}

void
mloadidbginit(void)
{
	idbg_addfunc ("maddr", idbg_maddr);
	idbg_addfunc ("mlist", idbg_mlist);
	idbg_addfunc ("mlkaddr", idbg_mlkaddr);
	idbg_addfunc ("mlkup", idbg_mlkup);
	idbg_addfunc ("mname", idbg_mname);
	idbg_addfunc ("msyms", idbg_msyms);
	idbg_addfunc ("mlinfo", idbg_mlinfo);
}

int
mloadidbgunload(void)
{
	idbg_delfunc (idbg_maddr);
	idbg_delfunc (idbg_mlist);
	idbg_delfunc (idbg_mlkaddr);
	idbg_delfunc (idbg_mlkup);
	idbg_delfunc (idbg_mname);
	idbg_delfunc (idbg_msyms);
	idbg_delfunc (idbg_mlinfo);

	return 0;
}
