#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/examples/RCS/hwconfig.c,v 1.2 1999/05/08 04:08:30 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>

#ifdef ALLOC_DEBUG
#include <klib/klib.h>
#endif
#include <sss/configmon_api.h>

/*
 * print_module_info()
 */
void
print_module_info(hw_component_t *m, FILE *ofp)
{
    if (m->c_serial_number) {
        fprintf(ofp, "%5d        MODULEID        %2d  %12s\n",
            m->c_level, atoi(m->c_name), m->c_serial_number);
    }
    else {
        fprintf(ofp, "%5d        MODULEID        %2d  NA\n",
            m->c_level, atoi(m->c_name));
    }
}

/*
 * hwconfig()
 */
int
hwconfig(
	char *dbname, 
	k_uint_t sys_id, 
	time_t time, 
	FILE *ofp)
{
	int sys_type;
	cm_hndl_t config;
	cm_hndl_t hwcmp;
	cm_hndl_t sysp;
	char *desc;

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

	if (!(sysp = cm_sysinfo(config))) {
		cm_free_config(config);
		return(1);
	}
	sys_type = (int)cm_field(sysp, SYSINFO_TYPE, SYS_SYS_TYPE);

	hwcmp = cm_first_hwcmp(config);
	while (hwcmp) {

		if (cm_field(hwcmp, HWCOMPONENT_TYPE, HW_LEVEL) == 0) {
			hwcmp = cm_get_hwcmp(hwcmp, CM_NEXT);
			continue;
		}

		if (sys_type == 27) {
			if (cm_field(hwcmp, HWCOMPONENT_TYPE, HW_CATEGORY) == HWC_MODULE) {
				print_module_info(hwcmp, ofp);
				hwcmp = cm_get_hwcmp(hwcmp, CM_NEXT);
				continue;
			}
		}
		
#ifdef NOT
		if (cm_field(hwcmp, HWCOMPONENT_TYPE, HW_CATEGORY) <= HWC_BOARD) {
			cm_print_hwcmp_info(hwcmp, ofp);
		}
#else
		if (desc = hwcp_description(hwcmp, 0, K_TEMP)) {
			fprintf(ofp, "%s\n", desc);
			kl_free_block(desc);
		}
		else {
			cm_print_hwcmp_info(hwcmp, ofp);
		}
#endif
		hwcmp = cm_get_hwcmp(hwcmp, CM_NEXT);
	}

	cm_free_config(config);
	return(0);
}

main()
{
	cm_init();

	fprintf(stdout, "HARDWARE CONFIG:\n\n");
	hwconfig("ssdb", 2952814609, 0, stdout);

#ifdef ALLOC_DEBUG
    /* XXX -- just to see if we forgot anyting...
     */
    alloc_debug = 1;
    free_temp_blocks();
#endif
	exit(0);
}

