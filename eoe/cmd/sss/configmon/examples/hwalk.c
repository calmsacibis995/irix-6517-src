#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/examples/RCS/hwalk.c,v 1.3 1999/05/08 06:39:49 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>

#ifdef ALLOC_DEBUG
#include <klib/klib.h>
#endif
#include <sss/configmon_api.h>

#define STRTOULL(s) (strtoull(s, (char **)NULL, 16))

/*
 * walk_hwconfig()
 */
int
walk_hwconfig(
	char *dbname, 
	k_uint_t sys_id, 
	time_t time, 
	FILE *ofp)
{
	char cmd;
	cm_hndl_t config;
	cm_hndl_t hwcmp, next_hwcmp;
	cm_hndl_t swcmp;
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

	hwcmp = cm_first_hwcmp(config);
	cm_print_hwcmp_info(hwcmp, ofp);
	while (1) {

		fprintf(ofp, "(0x%x) >> ", hwcmp);
		while (!fgets(inbuf, 128, stdin)) {
			fprintf(ofp, "(0x%x) >> ", hwcmp);
		}
		cmd = inbuf[0];

		switch (cmd) {

			case 's':
			case 'S':
				next_hwcmp = cm_get_hwcmp(hwcmp, CM_FIRST);
				break;

			case 'e':
			case 'E':
				next_hwcmp = cm_get_hwcmp(hwcmp, CM_LAST);
				break;

			case 'n':
			case 'N':
				next_hwcmp = cm_get_hwcmp(hwcmp, CM_NEXT);
				break;

			case 'p':
			case 'P':
				next_hwcmp = cm_get_hwcmp(hwcmp, CM_PREV);
				break;

			case 'f':
			case 'F':
				next_hwcmp = cm_get_hwcmp(hwcmp, CM_NEXT_PEER);
				break;

			case 'b':
			case 'B':
				next_hwcmp = cm_get_hwcmp(hwcmp, CM_PREV_PEER);
				break;

			case 'u':
			case 'U':
				next_hwcmp = cm_get_hwcmp(hwcmp, CM_LEVEL_UP);
				break;

			case 'd':
			case 'D':
				next_hwcmp = cm_get_hwcmp(hwcmp, CM_LEVEL_DOWN);
				break;

			case 'q':
			case 'Q':
				goto done;
		}
		if (next_hwcmp) {
			hwcmp = next_hwcmp;
			next_hwcmp = (cm_hndl_t)NULL;
		}
		cm_print_hwcmp_info(hwcmp, ofp);
	}
done:
	cm_free_config(config);
	return(0);
}

main(int argc, char **argv)
{
	unsigned long long sys_id;

	if (argc <= 1) {
		exit();
	}
	sys_id = STRTOULL(argv[1]);

	cm_init();

	fprintf(stdout, "CONFIG_HISTORY:\n\n");
	walk_hwconfig("ssdb", sys_id, 0, stdout);

#ifdef ALLOC_DEBUG
    /* XXX -- just to see if we forgot anyting...
     */
    alloc_debug = 1;
    free_temp_blocks();
#endif
	exit(0);
}

