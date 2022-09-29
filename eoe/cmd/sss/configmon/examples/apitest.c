#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/examples/RCS/apitest.c,v 1.3 1999/05/08 10:13:42 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>

#ifdef ALLOC_DEBUG
#include <klib/klib.h>
#endif
#include <sss/configmon_api.h>

extern int database_debug;

/*
 * config_history()
 */
int 
config_history(    
	char *dbname, 
	cm_value_t sys_id, 
	time_t start_time, 
	time_t end_time,
	FILE *ofp)
{
	int i, j, type = 0, next_type, item_type;
	cm_hndl_t *config;
	cm_hndl_t cme_history, cm_event, icp, change_itm, item;
	time_t change_time;

    if (!(config = cm_alloc_config(dbname, 0))) {
		return(1);
	}
	if (!(cme_history = cm_event_history(config, 
					sys_id, start_time, end_time, ALL_CM_EVENTS))) {
		cm_free_config(config);
		return(1);
	}
	if (!(cm_event = cm_item(cme_history, CM_FIRST))) {
		cm_free_list(config, cme_history);
		cm_free_config(config);
		return(1);
	}

	do {
		change_time = CM_UNSIGNED(cm_field(cm_event, CM_EVENT_TYPE, CE_TIME));
		if (CM_SIGNED(cm_field(cm_event, 
					CM_EVENT_TYPE, CE_TYPE)) == CONFIGMON_INIT) {
			fprintf(ofp, "\nCONFIGMON_INIT(");
			cm_print_time(ofp, change_time);
			fprintf(ofp, "):\n\n");
			continue;
		}
		else {
			fprintf(ofp, "\nCONFIG_CHANGE(");
			cm_print_time(ofp, change_time);
			fprintf(ofp, "):\n");
		}

		icp = cm_change_items(config, change_time);
		if (!(change_itm = cm_item(icp, CM_FIRST))) {
			cm_free_list(config, icp);
			continue;
		}
		do {
			next_type = 
				CM_SIGNED(cm_field(change_itm, CHANGE_ITEM_TYPE, CI_TYPE));

			if (type != next_type) {

				type = next_type;

				switch(type) {
					case SYSINFO_CURRENT:
						fprintf(ofp, "\nCURRENT SYSINFO:\n\n");
						break;

					case SYSINFO_OLD:
						fprintf(ofp, "\nOLD SYSINFO:\n\n");
						break;

					case HW_INSTALLED:
						fprintf(ofp, "\nHARDWARE INSTALLED:\n\n");
						break;

					case HW_DEINSTALLED:
						fprintf(ofp, "\nHARDWARE DEINSTALLED:\n\n");
						break;

					case SW_INSTALLED:
						fprintf(ofp, "\nSOFTWARE INSTALLED:\n\n");
						break;

					case SW_DEINSTALLED:
						fprintf(ofp, "\nSOFTWARE DEINSTALLED:\n\n");
						break;

					default:
						break;
				}
			}

			item = cm_get_change_item(config, change_itm);
			item_type = cm_change_item_type(change_itm);
			switch (item_type) {

				case SYSINFO_TYPE:
					cm_print_system_info(item, ofp);
					break;

				case HWCOMPONENT_TYPE:
					cm_print_hwcmp_info(item, ofp);
					break;

				case SWCOMPONENT_TYPE:
					cm_print_swcmp_info(item, ofp);
					break;

				default:
					fprintf(ofp, "UNKNOWN ITEM!\n");
					break;
			}
			cm_free_item(config, item, item_type);
			
		} while (change_itm = cm_item(icp, CM_NEXT));
		cm_free_list(config, icp);

	} while (cm_event = cm_item(cme_history, CM_NEXT));
	cm_free_list(config, cme_history);
	cm_free_config(config);
	return(0);
}

/*
 * show_swlist()
 */
int
show_swlist(
	char *dbname, 
	cm_value_t sys_id, 
	time_t start_time,
	time_t end_time,
	FILE *ofp)
{
	int i, flag;
	cm_hndl_t config;
	cm_hndl_t dbcmd;
	cm_hndl_t listp;
	cm_hndl_t item;

	/*
	database_debug = 2;
	*/

    if (!(config = cm_alloc_config(dbname, 0))) {
		return(1);
	}
	dbcmd = cm_alloc_dbcmd(config, SWCOMPONENT_TYPE);

	/* Add the query conditions
	 */
    cm_add_condition(dbcmd, SW_SYS_ID, OP_EQUAL, (cm_value_t)sys_id);
	cm_add_condition(dbcmd, SW_NAME, OP_ORDER_BY, (cm_value_t)0);
	if (start_time) {
		cm_add_condition(dbcmd, SW_INSTALL_TIME, OP_GREATER_OR_EQUAL,
			(cm_value_t)start_time);
	}
	if (end_time) {
		cm_add_condition(dbcmd, SW_INSTALL_TIME, OP_LESS_OR_EQUAL,
			(cm_value_t)end_time);
	}
	if (start_time || end_time) {
		flag = ALL_FLG;
	}
	else {
		flag = ACTIVE_FLG;
	}

	if (!(listp = cm_select_list(dbcmd, ALL_FLG))) {
		cm_free_dbcmd(dbcmd);
		cm_free_config(config);
		return(1);
	}
	if (item = cm_item(listp, CM_FIRST)) {
		do {
			cm_print_swcmp_info(item, ofp);
		} while (item = cm_item(listp, CM_NEXT));
	}
	cm_free_list(config, listp);
	cm_free_dbcmd(dbcmd);
	cm_free_config(config);
	return(0);
}

/*
 * show_hwlist()
 */
int
show_hwlist(
	char *dbname, 
	cm_value_t sys_id, 
	time_t start_time,
	time_t end_time,
	FILE *ofp)
{
	int i, flag;
	cm_hndl_t config;
	cm_hndl_t dbcmd;
	cm_hndl_t listp;
	cm_hndl_t item;
	
	/*
	database_debug = 1;
	*/

    if (!(config = cm_alloc_config(dbname, 0))) {
		return(1);
	}
	dbcmd = cm_alloc_dbcmd(config, HWCOMPONENT_TYPE);

	/* Add the query conditions
	 */
    cm_add_condition(dbcmd, HW_SYS_ID, OP_EQUAL, (cm_value_t)sys_id);
	if (start_time) {
		cm_add_condition(dbcmd, HW_INSTALL_TIME, OP_GREATER_OR_EQUAL,
			(cm_value_t)start_time);
	}
	if (end_time) {
		cm_add_condition(dbcmd, HW_INSTALL_TIME, OP_LESS_OR_EQUAL,
			(cm_value_t)end_time);
	}
	if (start_time || end_time) {
		flag = ALL_FLG;
	}
	else {
		flag = ACTIVE_FLG;
	}

	if (!(listp = cm_select_list(dbcmd, ALL_FLG))) {
		cm_free_dbcmd(dbcmd);
		cm_free_config(config);
		return(1);
	}
	if (item = cm_item(listp, CM_FIRST)) {
		do {
			cm_print_hwcmp_info(item, ofp);
		} while (item = cm_item(listp, CM_NEXT));
	}
	cm_free_list(config, listp);
	cm_free_dbcmd(dbcmd);
	cm_free_config(config);
	return(0);

}

/*
 * show_list()
 */
int
show_list(
	char *dbname, 
	cm_value_t sys_id, 
	time_t start_time,
	time_t end_time,
	FILE *ofp)
{
	int i, flag;
	cm_hndl_t config;
	cm_hndl_t dbcmd;
	cm_hndl_t listp;
	cm_hndl_t item;

	/*
	database_debug = 1;
	*/

    if (!(config = cm_alloc_config(dbname, 0))) {
		return(1);
	}
	dbcmd = cm_alloc_dbcmd(config, HWCOMPONENT_TYPE);

	/* Add the query conditions
	 */
    cm_add_condition(dbcmd, HW_SYS_ID, OP_EQUAL, (cm_value_t)sys_id);
	if (start_time) {
		cm_add_condition(dbcmd, HW_INSTALL_TIME, OP_GREATER_OR_EQUAL,
			(cm_value_t)start_time);
	}
	if (end_time) {
		cm_add_condition(dbcmd, HW_INSTALL_TIME, OP_LESS_OR_EQUAL,
			(cm_value_t)end_time);
	}
	if (start_time || end_time) {
		flag = ALL_FLG;
	}
	else {
		flag = ACTIVE_FLG;
	}

	if (!(listp = cm_select_list(dbcmd, ALL_FLG))) {
		cm_free_dbcmd(dbcmd);
		cm_free_config(config);
		return(1);
	}
	if (item = cm_item(listp, CM_FIRST)) {
		do {
			cm_print_hwcmp_info(item, ofp);
		} while (item = cm_item(listp, CM_NEXT));
	}
	cm_free_list(config, listp);
	cm_free_dbcmd(dbcmd);
	cm_free_config(config);
	return(0);

}

/*
 * show_item_history()
 */
int
show_item_history(
	char *dbname, 
	cm_value_t sys_id, 
	time_t start_time, 
	time_t end_time, 
	FILE *ofp)
{
	int i, flag = CM_TEMP;
	cm_hndl_t config;
	cm_hndl_t dbcmd;
	cm_hndl_t listp;
	cm_hndl_t item_h;
	cm_hndl_t item;

    if (!(config = cm_alloc_config(dbname, 0))) {
		return(1);
	}
	dbcmd = cm_alloc_dbcmd(config, SYSINFO_TYPE);

	/* Add the query conditions
	 */
	cm_add_condition(dbcmd, SYS_SYS_ID, OP_EQUAL, (cm_value_t)sys_id);
/*
	cm_add_condition(dbcmd, SYS_HOSTNAME, OP_EQUAL, (cm_value_t)"strlab03");
*/

	if (!(listp = cm_item_history(dbcmd, 0, 0))) {
		cm_free_dbcmd(dbcmd);
		cm_free_config(config);
		return(1);
	}
	if (item = cm_item(listp, CM_FIRST)) {
		do {
			cm_print_system_info(item, ofp);
		} while (item = cm_item(listp, CM_NEXT));
	}
	cm_free_list(config, listp);
	cm_free_dbcmd(dbcmd);

	/* Setup a dbcmd
	 */
	dbcmd = cm_alloc_dbcmd(config, HWCOMPONENT_TYPE);
	cm_add_condition(dbcmd, HW_SYS_ID, OP_EQUAL, (cm_value_t)sys_id);
	cm_add_condition(dbcmd, HW_SERIAL_NUMBER, OP_EQUAL, (cm_value_t)"DDR890");

	if (!(listp = cm_item_history(dbcmd, 0, 0))) {
		cm_free_dbcmd(dbcmd);
		cm_free_config(config);
		return(1);
	}
	if (item = cm_item(listp, CM_FIRST)) {
		do {
			cm_print_hwcmp_info(item, ofp);
		} while (item = cm_item(listp, CM_NEXT));
	}
	cm_free_list(config, listp);
	cm_free_dbcmd(dbcmd);
	cm_free_config(config);
	return(0);
}

extern unsigned long long klib_debug;

main()
{
    /* Set up signal handler for SEGV and BUSERR signals so that the
	 * application doesn't dump core.
	 */
	if (kl_sig_setup()) {
		fprintf(stderr, "kl_sig_setup() failed!\n");
	}

	cm_init();

	fprintf(stdout, "CONFIG_HISTORY:\n\n");
	config_history("ssdb", 2952814609, 0, 0, stdout);

	fprintf(stdout, "\n\n");
	fprintf(stdout, "ITEM_HISTORY:\n\n");
	show_item_history("ssdb", 2952814609, 0, 0, stdout); 

	fprintf(stdout, "\n\n");
	fprintf(stdout, "LIST:\n\n");
	show_list("ssdb", 2952814609, 0, 0, stdout); 

	fprintf(stdout, "\n\n");
	fprintf(stdout, "HWLIST:\n\n");
	show_hwlist("ssdb", 2952814609, 0, 0, stdout); 

	fprintf(stdout, "\n\n");
	fprintf(stdout, "SWLIST:\n\n");
	show_swlist("ssdb", 2952814609, 0, 0, stdout); 

#ifdef ALLOC_DEBUG
    /* XXX -- just to see if we forgot anyting...
     */
    alloc_debug = 1;
    free_temp_blocks();
#endif

	cm_free();

	exit(0);
}

