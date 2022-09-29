#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/src/RCS/swconfig.c,v 1.1 1999/05/08 03:19:00 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#include <klib.h>
#include <configmon.h>

/*
 * get_swcomponents()
 */
int
get_swcomponents(configmon_t *cmp) 
{
	sw_component_t *swcp;
	database_t *dbp = cmp->database;
	sysconfig_t *scp = cmp->db_sysconfig;

	/* Select all the swcomponents for sys_id
	 */
	if (!select_swcomponents(cmp)) {
		return(1);
	}

	while (swcp = get_next_swcomponent(cmp)) {
		add_swcomponent((sw_component_t **)&scp->swcmp_root, swcp);
	}
	/* XXX 
	update_swconfig();
	*/
	return(0);
}

/*
 * select_swcomponents()
 */
int
select_swcomponents(configmon_t *cmp)
{
	int count;
	database_t *dbp = cmp->database;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
	setup_dbquery(dbcmd, SWCOMPONENT_TBL, SYS_ID(cmp), 0, 0, 0);
	count = select_records(dbcmd);
	free_dbcmd(dbcmd);
	return(count);
}

/*
 * get_next_swcomponent()
 */
sw_component_t *
get_next_swcomponent(configmon_t *cmp)
{
	sw_component_t *swcp;
	database_t *dbp = cmp->database;

	swcp = (sw_component_t *)next_record(dbp, SWCOMPONENT_TBL);
	return(swcp);
}

/*
 * find_swcmp()
 */
sw_component_t *
find_swcmp(sw_component_t *root, int key)
{
    sw_component_t *swcp;

    if (!(swcp = first_swcomponent(root))) {
        return((sw_component_t *)NULL);
    }

    do {
        if (swcp->sw_key == key) {
            break;
        }
    } while (swcp = next_swcomponent(swcp));
    return(swcp);
}

/*
 * remove_swcmp()
 */
int
remove_swcmp(sw_component_t **root, int key)
{
    sw_component_t *swcp;

    if (!(swcp = find_swcmp(*root, key))) {
        return(1);
    }
    unlink_swcomponent(root, swcp);
    free_swcomponent(swcp);
    return(0);
}


