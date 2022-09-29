/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.5 $"

#ifdef __STDC__
	#pragma weak get_num_modules = _get_num_modules
	#pragma weak get_module_info = _get_module_info
#endif
#include "synonyms.h"
#include <sys/systeminfo.h>
#include <sys/syssgi.h>


int
get_num_modules(void)
{
	int rc;

	rc = (int)syssgi(SGI_NUM_MODULES);

	return rc;
	
}

int
get_module_info(int mod_index, module_info_t *mod_info,size_t info_size)
{
	int rc;

	rc = (int)syssgi(SGI_MODULE_INFO,mod_index,mod_info,info_size);

	return rc;
}
