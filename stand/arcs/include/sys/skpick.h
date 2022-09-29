/* skpick.h: Allows users of the kernel standalone library to pick-n-choose
 *	     which device drivers and file systems they get linked with by
 *	     stubbing out the externally used routines.  This is only needed
 *	     for modules that export more than their install routine.
 *
 *	     To use, #define the drivers and filesystems you wish to exclude
 *	     in a C file, and then include this file.  It will make
 *	     a stubs to keep the requested drivers from getting linked in.
 *
 *	     NOTE: This is usually done automatically by gconf when using
 *		   the node! syntax.
 */
#ifndef __SYS_SKPICK__
#define __SYS_SKPICK__

#ident "$Revision: 1.16 $"

#include <arcs/errno.h>

#define XLEAF(name)	int name() {return(0);}
#define XLEAFER(name)	int name() {return(ENXIO);}

/* drivers */

#ifdef NO_mgras
XLEAF(pon_graphics)
#endif

#ifdef NO_scsi		/* implies NO_dksc and NO_tpsc */
int scsieditintdone;
unsigned char scsi_ha_id[2];
XLEAF(scsiunit_init)
#endif

#if defined(NO_tpsc) && !defined(NO_scsi)
XLEAF(tpsc_strat)
XLEAF(tpsctapetype)
XLEAF(tpsc_tapeid)
XLEAF(_tpscopen)
XLEAF(_tpscclose)
#endif

#if defined(NO_dksc) && !defined(NO_scsi)
XLEAF(dksc_strat)
#endif

#ifdef NO_sgikbd
XLEAFER(config_keyboard)		/* called from duart */
XLEAF(kb_translate)
XLEAF(bell)
#endif

#ifdef NO_ms
XLEAFER(ms_config)			/* all called from duart */
XLEAF(ms_install)
XLEAF(ms_input)
XLEAF(_mspoll)
XLEAF(_init_mouse)
#endif

#ifdef NO_pcms
XLEAF(_mspoll)
XLEAF(_init_mouse)
#endif

/* file systems */

#if defined(NO_dvh) && !defined(NO_scsi)
XLEAF(vh_checksum)
XLEAF(is_vh)
#endif

#endif	/*__SYS_SKPICK__ */
