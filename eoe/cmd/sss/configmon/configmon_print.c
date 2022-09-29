#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/RCS/configmon_print.c,v 1.4 1999/05/27 23:13:43 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include <sss/configmon.h>

/*
 * cm_print_hwcmp_info()
 */
void 
cm_print_hwcmp_info(cm_hndl_t hwcp, FILE *ofp)
{
	k_uint_t value;

	fprintf(ofp, "%5d  ", 
		CM_SIGNED(cm_field(hwcp, HWCOMPONENT_TYPE, HW_SEQ)));

	if (value = cm_field(hwcp, HWCOMPONENT_TYPE, HW_NAME)) {
		fprintf(ofp, "%14s  ", CM_STRING(value));
	}
	else {
		fprintf(ofp, "            NA  ");
	}

	if (value = cm_field(hwcp, HWCOMPONENT_TYPE, HW_LOCATION)) {
		fprintf(ofp, "%8s  ", CM_STRING(value));
	}
	else {
		fprintf(ofp, "      NA  ");
	}

	if (value = cm_field(hwcp, HWCOMPONENT_TYPE, HW_SERIAL_NUMBER)) {
		fprintf(ofp, "%10s  ", CM_STRING(value));
	}
	else {
		fprintf(ofp, "        NA  ");
	}

	if (value = cm_field(hwcp, HWCOMPONENT_TYPE, HW_PART_NUMBER)) {
		fprintf(ofp, "%12s  ", CM_STRING(value));
	}
	else {
		fprintf(ofp, "          NA  ");
	}

	if (value = cm_field(hwcp, HWCOMPONENT_TYPE, HW_REVISION)) {
		fprintf(ofp, "%8s\n", CM_STRING(value));
	}
	else {
		fprintf(ofp, "      NA\n");
	}
}

/*
 * cm_print_hwconfig()
 */
void
cm_print_hwconfig(cm_hndl_t config, FILE *ofp)
{
	cm_hndl_t hwcp;

	if (hwcp = cm_first_hwcmp(config)) {
		fprintf(ofp, "  SEQ            NAME  LOCATION  SERIAL_NUM   "
			"PART_NUMBER  REVISION\n");
		fprintf(ofp, "=============================================="
			"=====================\n");
		do {
#ifdef NOT
			if (cm_field(hwcp, HWCOMPONENT_TYPE, HW_CATEGORY) <= HWC_BOARD) {
				cm_print_hwcmp_info(hwcp, ofp);
			}
#else
			cm_print_hwcmp_info(hwcp, ofp);
#endif
		} while (hwcp = cm_get_hwcmp(hwcp, CM_NEXT));
		fprintf(ofp, "=============================================="
			"=====================\n");
	}
	else {
		fprintf(ofp, "No hardware config data in the database!\n");
	}
	return;
}

/*
 * cm_print_swcmp_info()
 */
void
cm_print_swcmp_info(cm_hndl_t swcp, FILE *ofp)
{
	time_t time;
	char *tbuf;

	tbuf = (char *)kl_alloc_block(100, CM_TEMP);
	time = CM_UNSIGNED(cm_field(swcp, SWCOMPONENT_TYPE, SW_INSTALL_TIME));
	cftime(tbuf, "%m/%d/%Y", &time);
	fprintf(ofp, "%5d  %12llu  %-16s  %10u  %10s\n",
		CM_UNSIGNED(cm_field(swcp, SWCOMPONENT_TYPE, SW_REC_KEY)),
		CM_UNSIGNED_LONG(cm_field(swcp, SWCOMPONENT_TYPE, SW_SYS_ID)),
		CM_STRING(cm_field(swcp, SWCOMPONENT_TYPE, SW_NAME)),
		CM_UNSIGNED(cm_field(swcp, SWCOMPONENT_TYPE, SW_VERSION)),
		tbuf);
	kl_free_block(tbuf);
}

/*
 * cm_print_swconfig()
 */
void
cm_print_swconfig(cm_hndl_t config, FILE *ofp)
{
	cm_hndl_t swcp;

	if (swcp = cm_first_swcmp(config)) {
		fprintf(ofp, "  KEY        SYS_ID  NAME                 VERSION   "
			"INSTALLED\n");
		fprintf(ofp, "==================================================="
			"==========\n");
		do {
			cm_print_swcmp_info(swcp, ofp);
		} while (swcp = cm_get_swcmp(swcp, CM_NEXT));
		fprintf(ofp, "===================================================="
			"=========\n");
	}
	else {
		fprintf(ofp, "No software config data in the database!\n");
	}
	return;
}

/*
 * cm_print_system_info()
 */
void
cm_print_system_info(cm_hndl_t sysinfo, FILE *ofp)
{
	time_t time;
	char *tbuf, systype[6];

	if (!sysinfo) {
		return;
	}

	fprintf(ofp, "REC_KEY       SYS_ID SYS_TYPE SERIAL_NUMBER      "
		"IP_ADDRESS HOSTNAME\n");
	fprintf(ofp, "===================================================="
		"===========================\n");

	tbuf = (char *)kl_alloc_block(100, CM_TEMP);
	time = CM_UNSIGNED(cm_field(sysinfo, SYSINFO_TYPE, SYS_TIME));
	cftime(tbuf, "%m/%d/%Y", &time);

	sprintf(systype, "IP%d", 
		CM_SIGNED(cm_field(sysinfo, SYSINFO_TYPE, SYS_SYS_TYPE)));

	fprintf(ofp, "%7d %12llu %8s %13s %15s %-15s\n",
		CM_SIGNED(cm_field(sysinfo, SYSINFO_TYPE, SYS_REC_KEY)),
		CM_UNSIGNED_LONG(cm_field(sysinfo,  SYSINFO_TYPE, SYS_SYS_ID)),
		systype,
		CM_STRING(cm_field(sysinfo, SYSINFO_TYPE, SYS_SERIAL_NUMBER)),
		CM_STRING(cm_field(sysinfo, SYSINFO_TYPE, SYS_IP_ADDRESS)),
		CM_STRING(cm_field(sysinfo, SYSINFO_TYPE, SYS_HOSTNAME)));

	fprintf(ofp, "\n  ACTIVE=%hd, LOCAL=%hd, TIME=%u (%s)\n",
		CM_SHORT(cm_field(sysinfo, SYSINFO_TYPE, SYS_ACTIVE)),
		CM_SHORT(cm_field(sysinfo, SYSINFO_TYPE, SYS_LOCAL)), 
		time,
		tbuf);
	kl_free_block(tbuf);

	fprintf(ofp, "===================================================="
		"===========================\n");

	return;
}

/* 
 * cm_print_sysinfo()
 */
void
cm_print_sysinfo(cm_hndl_t config, FILE *ofp)
{
	cm_hndl_t sysinfo = (cm_hndl_t)((configmon_t*)config)->sysinfo;

	if (sysinfo) {
		cm_print_system_info(sysinfo, ofp);
	}
	else {
		fprintf(ofp, "No system info data in the database!\n");
	}
}

/*
 * cm_print_time()
 */
void
cm_print_time(FILE *ofp, time_t time)
{
    char *tbuf;

    tbuf = (char *)kl_alloc_block(100, K_TEMP);
    cftime(tbuf, "%m/%d/%Y:%H:%M:%S", &time);
    fprintf(ofp, "%s", tbuf);
    kl_free_block(tbuf);
}

