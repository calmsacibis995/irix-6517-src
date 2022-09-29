#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/examples/RCS/select_test.c,v 1.2 1999/05/08 04:08:30 tjm Exp $"

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
	int i, flag = CM_TEMP;
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
	dbcmd = cm_alloc_dbcmd(config, SWCOMPONENT_TYPE);

	/* Add the query conditions
	 */
	/*
	cm_add_condition(dbcmd, SW_INSTALL_TIME, OP_EQUAL, (cm_value_t)917942400);
	cm_add_condition(dbcmd, SW_REC_KEY, OP_EQUAL, (cm_value_t)46);
	cm_add_condition(dbcmd, SW_NAME, OP_GREATER_OR_EQUAL, (cm_value_t)"dev");
	cm_add_condition(dbcmd, SW_NAME, OP_LESS_OR_EQUAL, (cm_value_t)"eoe");
	cm_add_condition(dbcmd, SW_SYS_ID, OP_EQUAL, (cm_value_t)0xb0006011);
	cm_add_condition(dbcmd, SW_INSTALL_TIME, OP_GREATER_THAN, 
		(cm_value_t)917942400);
	cm_add_condition(dbcmd, SW_NAME, OP_GREATER_OR_EQUAL, (cm_value_t)"print");
	*/
	cm_add_condition(dbcmd, SW_SYS_ID, OP_EQUAL, 
		(cm_value_t)2952814609);
	cm_add_condition(dbcmd, SW_VERSION, OP_LESS_OR_EQUAL, 
		(cm_value_t) 1275268810);
	cm_add_condition(dbcmd, SW_VERSION, OP_GREATER_OR_EQUAL, 
		(cm_value_t) 1274627333);
	cm_add_condition(dbcmd, SW_NAME, OP_ORDER_BY, (cm_value_t)0);

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
	int i, flag = CM_TEMP;
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
	/*
	cm_add_condition(dbcmd, HW_INSTALL_TIME, OP_EQUAL, (cm_value_t)919470864);
	cm_add_condition(dbcmd, HW_REC_KEY, OP_EQUAL, (cm_value_t)8);
	cm_add_condition(dbcmd, HW_NAME, OP_GREATER_OR_EQUAL, (cm_value_t)"IP27");
	cm_add_condition(dbcmd, HW_SYS_ID, OP_EQUAL, (cm_value_t)0xb0006011);
	cm_add_condition(dbcmd, HW_INSTALL_TIME, OP_GREATER_OR_EQUAL, 
		(cm_value_t)919470064);
	*/
	cm_add_condition(dbcmd, HW_SYS_ID, OP_EQUAL, (cm_value_t)2952814609);
	cm_add_condition(dbcmd, HW_LOCATION, OP_ORDER_BY, (cm_value_t)0);

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

extern unsigned long long klib_debug;

main()
{
	cm_init();

	fprintf(stdout, "SWLIST:\n\n");
	show_swlist("ssdb", 2952814609, 0, 0, stdout); 
	fprintf(stdout, "HWLIST:\n\n");
	show_hwlist("ssdb", 2952814609, 0, 0, stdout); 

#ifdef ALLOC_DEBUG
    /* XXX -- just to see if we forgot anyting...
     */
    alloc_debug = 1;
    free_temp_blocks();
#endif
	exit(0);
}

