/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/syssgi.h>
#include <sys/mload.h>

int mload_debug = 0;
#define	DPRINTF if (mload_debug) printf

static mod_info_t *getmlist();
static int mload_errno;

/*
 * The following examples use the SGI_MCONFIG, CF_LIST syssgi call to 
 * get a list of registered and/or loaded modules and extract information.
 * The CF_LIST SGI_MCONFIG syscall uses the mod_info_t structure to
 * pass information back. This structure is defined in sys/mload.h.
 *
 * The system calls used in this example are the same calls that are
 * used by the "ml list" command to get the same type of information.
 *
 */

/*
 * mload_getid - given a module's prefix, return the module's id
 *
 * return -1 on failure
 *
 */

int 
mload_getid (char *prefix)
{
	mod_info_t *mod, *modspc;
	int size, i;

	if ((modspc = getmlist(&size)) == (mod_info_t *) -1)	
		return -1;

	for (mod = modspc, i=0; i < size/sizeof(mod_info_t); ++i, ++mod) {
		if (strcmp(mod->m_prefix, prefix) == 0)
			return mod->m_id; 
	}

	return -1;
}

/*
 * mload_getprefix - given a module's id, return the module's prefix
 *
 * return -1 on failure
 *
 */

char *
mload_getprefix (int id)
{
	mod_info_t *mod, *modspc;
	int size, i;

	if ((modspc = getmlist(&size)) == (mod_info_t *) -1)	
		return (char *)-1;

	for (mod = modspc, i=0; i < size/sizeof(mod_info_t); ++i, ++mod) {
		if (mod->m_id == id)
			return mod->m_prefix; 
	}

	return (char *)-1;
}

static mod_info_t *
getmlist (int *size)
{
	mod_info_t *modspc;

	/*
	 * An SGI_MCONFIG/CF_LIST call with 0 as the third argument,
	 * returns the amount of memory that will need to be allocated
	 * for the mlist information.
	 */

	if (!(*size = (int)syssgi (SGI_MCONFIG, CF_LIST, 0))) {
		DPRINTF ("No modules loaded or registered.\n");
		mload_errno = MERR_NOTLOADED;
		return (mod_info_t *) -1;
	}

	if (!(modspc = (mod_info_t *)malloc(*size))) {
		DPRINTF ("Failed to malloc %d\n", *size);
		mload_errno = ENOMEM;
		return (mod_info_t *) -1;
	}

	/* Get the mlist information */

	if (syssgi (SGI_MCONFIG, CF_LIST, modspc, *size) != *size) {
		DPRINTF ("Error copying module list.\n");
		mload_errno = ENOMEM;
		return (mod_info_t *) -1;
	}

	if (modspc->m_cfg_version != M_CFG_CURRENT_VERSION) { 
		DPRINTF ("Version mismatch with kernel.\n");
		mload_errno = MERR_BADCFGVERSION;
		return (mod_info_t *) -1;
	}

	return modspc;
}
