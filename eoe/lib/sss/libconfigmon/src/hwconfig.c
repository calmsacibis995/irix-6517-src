#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/src/RCS/hwconfig.c,v 1.1 1999/05/08 03:19:00 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>

#include <klib.h>
#include <configmon.h>

/*
 * select_hwcomponents()
 */
int
select_hwcomponents(configmon_t *cmp)
{
	int count;
	database_t *dbp = cmp->database;
	db_cmd_t *dbcmd;

	/* We can only allow for hwcomponents to be selected for a 
	 * single sys_id.
	 */
	if (SYS_ID(cmp) == 0) {
		return(0);
	}
	dbcmd = alloc_dbcmd(dbp);
	setup_dbquery(dbcmd, HWCOMPONENT_TBL, SYS_ID(cmp), 0, 0, 0);
	count = select_records(dbcmd);
	free_dbcmd(dbcmd);
	return(count);
}

/*
 * get_next_hwcomponent()
 */
hw_component_t *
get_next_hwcomponent(configmon_t *cmp)
{
	db_cmd_t *dbcmd;
	database_t *dbp = cmp->database;
	hw_component_t *cp;

	if (cp = (hw_component_t *)next_record(dbp, HWCOMPONENT_TBL)) {
		if (cp->c_data_key) {
			dbcmd = alloc_dbcmd(dbp);
			setup_dbquery(dbcmd, cp->c_data_table, 
						cp->c_sys_id, 0, cp->c_data_key, 0);
			if (!(cp->c_data = find_record(dbcmd))) {
				/* XXX - ERROR? */
			}
			free_dbcmd(dbcmd);
		}
	}
	return(cp);
}

/*
 * get_hwcomponents()
 */
int
get_hwcomponents(configmon_t *cmp)
{
	int i, aflag = ALLOCFLG(cmp->flags);
	database_t *dbp = cmp->database;
	sysconfig_t *scp = cmp->db_sysconfig;
	void *ptr;
	hw_component_t *cp, *last;

	/* Select all the hwcomponents for sys_id
	 */
	if (!select_hwcomponents(cmp)) {
		return(1);
	}

	/* Setup the root of the hwconfig tree...
	 */
	last = (hw_component_t *)kl_alloc_block(sizeof(hw_component_t), aflag);
	last->c_next = last->c_prev = last;
	last->c_sys_id = SYS_ID(cmp);
	last->c_type = scp->sys_type;

	/* Link the root into the sysconfig struct
	 */
	scp->hwcmp_root = last;

	while (cp = get_next_hwcomponent(cmp)) {
		hwcmp_insert_next(last, cp);
		last = cp;
	}
	kl_update_hwconfig(scp->hwconfig);
	return(0);
}

/*
 * add_hwcomponent()
 */
int
add_hwcomponent(configmon_t *cmp, hw_component_t *cp)
{
	int iflag, count;
	hw_component_t *root, *hwcp, *parent;

	root = cmp->db_sysconfig->hwcmp_root;

	/* Get the parent hw_component and make sure the it's the
	 * right one (basicly that there is a pointer to a list of
	 * sub-components.
	 */
	parent = find_hwcmp(root, cp->c_parent_key);
	if (!parent->c_cmplist) {
		return(1);
	}
	if (hwcp = hw_find_location(parent->c_cmplist, cp)) {
		replace_hwcomponent(hwcp, cp);
	}
	else if (hwcp = hw_find_insert_point(parent->c_cmplist, cp, &iflag)) {
		insert_hwcomponent(hwcp, cp, iflag);
	}
	return(0);
}

/*
 * find_hwcmp()
 */
hw_component_t *
find_hwcmp(hw_component_t *root, int key)
{
    hw_component_t *hwcp;

    if (!(hwcp = root)) {
        return((hw_component_t *)NULL);
    }

    do {
        if (hwcp->c_rec_key == key) {
            break;
        }
    } while (hwcp = next_hwcmp(hwcp));
    return(hwcp);
}

/*
 * find_sub_hwcmp()
 */
hw_component_t *
find_sub_hwcmp(hw_component_t *root, int parent_key, char *location)
{
    hw_component_t *hwcp, *cp;

    hwcp = find_hwcmp(root, parent_key);
    if (cp = hwcp->c_cmplist) {
        do {
            if (cp->c_location && strstr(cp->c_location, location)) {
                return(cp);
            }
            cp = cp->c_next;
        } while (cp != hwcp->c_cmplist);
    }
    return((hw_component_t *)NULL);
}

/*
 * remove_hwcomponent()
 */
int
remove_hwcomponent(hw_component_t *root, int key)
{
    hw_component_t *hwcp;

    if (!(hwcp = find_hwcmp(root, key))) {
        return(1);
    }
    unlink_hwcomponent(hwcp);
    free_hwcomponents(hwcp);
    return(0);
}


