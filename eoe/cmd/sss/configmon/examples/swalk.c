#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/examples/RCS/swalk.c,v 1.2 1999/05/08 04:08:30 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>

#ifdef ALLOC_DEBUG
#include <klib/klib.h>
#endif
#include <sss/configmon_api.h>

/*
 * walk_swconfig()
 */
int
walk_swconfig(
	char *dbname, 
	k_uint_t sys_id, 
	time_t time, 
	FILE *ofp)
{
	char cmd;
	cm_hndl_t config;
	cm_hndl_t swcmp, next_swcmp;
	cm_hndl_t sysinfo;
	char inbuf[128];

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
	cm_print_swcmp_info(swcmp, ofp);
	while (1) {

		fprintf(ofp, "(0x%x) >> ", swcmp);
		while (!fgets(inbuf, 128, stdin)) {
			fprintf(ofp, "(0x%x) >> ", swcmp);
		}
		cmd = inbuf[0];

		switch (cmd) {

			case 's':
			case 'S':
				next_swcmp = cm_get_swcmp(swcmp, CM_FIRST);
				break;

			case 'e':
			case 'E':
				next_swcmp = cm_get_swcmp(swcmp, CM_LAST);
				break;

			case 'n':
			case 'N':
				next_swcmp = cm_get_swcmp(swcmp, CM_NEXT);
				break;

			case 'p':
			case 'P':
				next_swcmp = cm_get_swcmp(swcmp, CM_PREV);
				break;

			case 'q':
			case 'Q':
				goto done;
		}
		if (next_swcmp) {
			swcmp = next_swcmp;
			next_swcmp = (cm_hndl_t)NULL;
		}
		cm_print_swcmp_info(swcmp, ofp);
	}
done:
	cm_free_config(config);
	return(0);
}

main()
{
	cm_init();

	fprintf(stdout, "CONFIG_HISTORY:\n\n");
	walk_swconfig("ssdb", 2952814609, 0, stdout);

#ifdef ALLOC_DEBUG
    /* XXX -- just to see if we forgot anyting...
     */
    alloc_debug = 1;
    free_temp_blocks();
#endif
	exit(0);
}

