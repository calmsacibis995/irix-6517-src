#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/examples/RCS/swconfig.c,v 1.2 1999/05/08 04:08:30 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>

#ifdef ALLOC_DEBUG
#include <klib/klib.h>
#endif
#include <sss/configmon_api.h>

/*
 * swconfig()
 */
int
swconfig(
	char *dbname, 
	k_uint_t sys_id, 
	time_t time, 
	FILE *ofp)
{
	cm_hndl_t config;
	cm_hndl_t swcmp;

	/* We can't accept a zero sys_id (localhost).
	 */
	if (sys_id == 0) {
		return(1);
	}

    if (!(config = cm_alloc_config(dbname, 0))) {
		return(1);
	}
	if (cm_get_sysconfig(config, sys_id, time)) {
		cm_free_config(config);
		return(1);
	}

	swcmp = cm_first_swcmp(config);
	while (swcmp) {
		cm_print_swcmp_info(swcmp, ofp);
		swcmp = cm_get_swcmp(swcmp, CM_NEXT);
	}

	cm_free_config(config);
	return(0);
}

main()
{
	cm_init();

	fprintf(stdout, "SOFTWARE CONFIG INFO:\n\n");
	swconfig("ssdb", 2952814609, 0, stdout);

#ifdef ALLOC_DEBUG
    /* XXX -- just to see if we forgot anyting...
     */
    alloc_debug = 1;
    free_temp_blocks();
#endif
	exit(0);
}

