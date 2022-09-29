#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/src/RCS/configmon_lib.c,v 1.5 1999/05/28 00:29:51 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/systeminfo.h>

#include <klib.h>
#include <configmon.h>

/*
 * alloc_configmon()
 */
configmon_t *
alloc_configmon(char *dbname, int bounds, int flags, int dsoflg)
{
	configmon_t *cmp;

	cmp = kl_alloc_block(sizeof(configmon_t), ALLOCFLG(flags));
	cmp->flags = flags;
	cmp->curtime = time(0);

	if (!(cmp->database = open_database(dbname, bounds, flags, dsoflg))) {
		kl_free_block(cmp);
		return((configmon_t *)NULL);
	}
	return(cmp);
}

/*
 * get_system_info()
 *
 *  If sys_id is set, get the system_info_s record for that system, 
 *  otherwise get the system_info_s record for localhost.
 */
int
get_system_info(configmon_t *cmp, k_uint_t sys_id)
{
	int count;
	db_cmd_t *dbcmd;
	database_t *dbp = cmp->database;
	system_info_t *sip;

	/* If there aren't any matches, just return...
	 */
	dbcmd = alloc_dbcmd(dbp);
	if (sys_id) {
		setup_dbquery(dbcmd, SYSTEM_TBL, sys_id, 0, 0, 0);
	}
	else {
		setup_dbquery(dbcmd, SYSTEM_TBL, 0, LOCALHOST, 0, 0);
	}
	count = select_records(dbcmd);
	free_dbcmd(dbcmd);
	if (count != 1) {
		return(1);
	}
	sip = (system_info_t*)next_record(dbp, SYSTEM_TBL);
	cmp->sysinfo = sip;
	return(0);
}

/*
 * free_configmon()
 */
void
free_configmon(configmon_t *cmp)
{
	if (cmp->sysinfo) {
		free_system_info(cmp->sysinfo);
	}
	if (cmp->database) {
		close_database(cmp->database);
	}
	if (cmp->sysconfig) {
		free_sysconfig(cmp->sysconfig);
	}
	if (cmp->db_sysconfig) {
		free_sysconfig(cmp->db_sysconfig);
	}
	kl_free_block(cmp);
}

/*
 * setup_configmon()
 */
int
setup_configmon(configmon_t *cmp, k_uint_t sys_id)
{
    /* Check the state of the database. It's possible that the database
     * tables have not been created yet. If that's the case, we need to
     * return with an error (the tables will need to be setup by the
     * ConfigMon task).
     */
    if (!(cmp->database->state & TABLES_EXIST)) {
        return(1);
    }
    else {
        /* If the database exists, then the system_info record we are
         * looking for (be it for localhost or a sys_id) HAS to be there.
         * If it isn't, it means there is something wrong with the database,
         * or we're looking for a sys_id that isn't there (it's bogus).
         */
        if (get_system_info(cmp, sys_id)) {
            return(1);
        }
    }
    return(0);
}

/*
 * db_sysconfig()
 */
int 
db_sysconfig(configmon_t *cmp, time_t time)
{
	if (!(cmp->db_sysconfig = alloc_sysconfig(cmp->flags))) {
		return(1);
	}

	/* Check to make sure that the time we are going for is a valid
	 * one (greater than the time the configmon tables were initialized
	 * and less than or equal to the current time).
	 */
	if (time) {
		int i, count;
		database_t *dbp;
		db_cmd_t *dbcmd;
		cm_event_t *cmep;

		if (time > cmp->curtime) {
			/* This time is in the future...
			 */
			return(1);
		}
		dbp = cmp->database;
		dbcmd = alloc_dbcmd(dbp);
		sprintf(dbp->sqlbuf, "SELECT * FROM %s WHERE sys_id=\"%llx\" AND "
			"type=1", TABLE_NAME(dbp, CM_EVENT_TBL), SYS_ID(cmp));
		setup_dbcmd(dbcmd, SSDB_REQTYPE_SELECT, CM_EVENT_TBL, dbp->sqlbuf);
		count = select_records(dbcmd);
		free_dbcmd(dbcmd);
		if (count == 0) {
			return(2);
		}
		if (count > 1) {
			/* This SHOULD never happen. If it does, the database is 
			 * most likely screwed up...just bail.
			 */
			for (i = 0; i < count; i++) {
				cmep = next_record(dbp, CM_EVENT_TBL);
				free_record_memory(dbp, CM_EVENT_TBL, cmep);
			}
			return(3);
		}
		cmep = next_record(dbp, CM_EVENT_TBL);
		if (time < cmep->time) {
			free_record_memory(dbp, CM_EVENT_TBL, cmep);
			return(4);
		}
		free_record_memory(dbp, CM_EVENT_TBL, cmep);
	}

	/* Check to see if your system record is out-of-date (e.g., too
	 * recent for the time we are requesting...
	 */
	if (time && (time < cmp->sysinfo->time)) {
		get_db_sysinfo(cmp, SYS_ID(cmp), time);
	}

	cmp->db_sysconfig->sys_id = SYS_ID(cmp);
	cmp->db_sysconfig->sys_type = SYS_TYPE(cmp);
	cmp->db_sysconfig->hwconfig->h_sys_id = SYS_ID(cmp);
	cmp->db_sysconfig->hwconfig->h_sys_type = SYS_TYPE(cmp);
	cmp->db_sysconfig->swconfig->s_sys_id = SYS_ID(cmp);
	cmp->db_sysconfig->swconfig->s_sys_type = SYS_TYPE(cmp);

	get_db_hwconfig(cmp, time);
	get_db_swconfig(cmp, time);
	return(0);
}

/* 
 * get_db_sysinfo()
 */
int
get_db_sysinfo(configmon_t *cmp, k_uint_t sys_id, time_t time)
{
	int i, j, flag = ALLOCFLG(cmp->flags);
	int cme_count, cep_count, sysinfo_count;
	db_cmd_t *dbcmd;
	cm_event_t *cmep, **cmepp;
	change_item_t *cep;
	database_t *dbp = cmp->database;
	system_info_t *sip = cmp->sysinfo;
	system_info_t *sysinfo;

	if (!sys_id) {
		if (!cmp->sysinfo) {
			return(1);
		}
		sys_id = SYS_ID(cmp);
	}

	/* Select all of the cm_event_s records and load them into the
	 * record data array.
	 */
	dbcmd = alloc_dbcmd(cmp->database);
	setup_dbquery(dbcmd, CM_EVENT_TBL, sys_id, 0, 0, 0);
	cme_count = select_records(dbcmd);
	clean_dbcmd(dbcmd);
	cmepp = (cm_event_t**)kl_alloc_block(cme_count * sizeof(void *), flag);

	i = 0;
	while (cmep = (cm_event_t *)next_record(cmp->database, CM_EVENT_TBL)) {
		cmepp[i++] = cmep;
	}

	/* Now, starting with the most recent change and, walking backward, 
	 * check to see if there was a system_info change.
	 */
	for (i = cme_count - 1; i >= 0; i--) {

		cmep = cmepp[i];

		if (cmep->time < time) {
			/* We've gone back further than we need to...free the block
			 * and continue (so that all other too-early blocks will be
			 * freed up).
			 */
			kl_free_block(cmep);
			continue;
		}

		if (cmep->type & SYSINFO_CHANGE) {
			setup_dbquery(dbcmd, CHANGE_ITEM_TBL, sys_id, TIME, 0, cmep->time);
			cep_count = select_records(dbcmd);
			clean_dbcmd(dbcmd);
			sysinfo = (system_info_t*)NULL;
			for (j = 0; j < cep_count; j++) {
				if (cep = next_record(cmp->database, CHANGE_ITEM_TBL)) {
					if (cep->type == SYSINFO_OLD) {
						setup_dbquery(dbcmd, SYSARCHIVE_TBL, sys_id, 
							0, cep->rec_key, 0);
						sysinfo_count = select_records(dbcmd);
						clean_dbcmd(dbcmd);
						sysinfo = next_record(cmp->database, SYSARCHIVE_TBL);
						free_system_info(cmp->sysinfo);
						cmp->sysinfo = sysinfo;
					}
					kl_free_block(cep);
				}
			}
		}
		kl_free_block(cmep);
	}
	kl_free_block(cmepp);
	free_dbcmd(dbcmd);
	return(0);
}

/*
 * get_db_hwconfig()
 */
int
get_db_hwconfig(configmon_t *cmp, time_t time)
{
	int i, flag = K_TEMP;
	int count, rec_count;
	db_cmd_t *dbcmd;
	hw_component_t *hwcmp, *last, *cp, *root;
	cm_event_t *cmep, **cmepp;
	change_item_t *cep;

	if (get_hwcomponents(cmp)) {
		return(1);
	}

	/* If all we want is the current hwconfig (from the database), then
	 * we can return...
	 */
	if ((cmp->db_sysconfig->date = time) == 0) {
		return(0);
	}

	/* Select all of the cm_event_s records and load them into the
	 * record data array.
	 */
	dbcmd = alloc_dbcmd(cmp->database);
	setup_dbquery(dbcmd, CM_EVENT_TBL, SYS_ID(cmp), 0, 0, 0);
	count = select_records(dbcmd);
	clean_dbcmd(dbcmd);
	cmepp = (cm_event_t**)kl_alloc_block(count * sizeof(void *), flag);

	i = 0;
	while (cmep = (cm_event_t *)next_record(cmp->database, CM_EVENT_TBL)) {
		cmepp[i++] = cmep;
	}

	/* Now, starting with the most recent change and walking backward, 
	 * adjust the hwconfig tree (removing those records that were added
	 * and adding back those records that had been removed).
	 */
	for (i = count - 1; i >= 0; i--) {

		cmep = cmepp[i];

		if (time && (cmep->time < time)) {
			/* We've gone back further than we need to...free the block
			 * and continue (so that all other too-early blocks will be
			 * freed up).
			 */
			kl_free_block(cmep);
			continue;
		}

		/* Back out any records that were added...
		 */
		setup_dbquery(dbcmd, CHANGE_ITEM_TBL, SYS_ID(cmp), TIME, 0, cmep->time);
		rec_count = select_records(dbcmd);
		clean_dbcmd(dbcmd);
		while (cep = next_record(cmp->database, CHANGE_ITEM_TBL)) {
			if (cep->type == HW_INSTALLED) {
				remove_hwcomponent(cmp->db_sysconfig->hwcmp_root, 
					cep->rec_key);
			}
			kl_free_block(cep);
		}

		/* Add back in any records that were removed from the tree
		 */
		setup_dbquery(dbcmd, HWARCHIVE_TBL, SYS_ID(cmp), 
				DEINSTALL_TIME, 0, cmep->time);
		rec_count = select_records(dbcmd);
		clean_dbcmd(dbcmd);

		/* We don't need this block anymore, so lets free it up...
		 */
		kl_free_block(cmep);

		last = root = (hw_component_t *)NULL;
		while(cp = next_record(cmp->database, HWARCHIVE_TBL)) {

			cp->c_next = cp->c_prev = cp;
			cp->c_deinstall_time = 0;

			/* Link in the private data structure (if there is one)
			 */
			if (cp->c_data_key) {
				setup_dbquery(dbcmd, cp->c_data_table, cp->c_sys_id, 
						0, cp->c_data_key, 0);
				if (!(cp->c_data = find_record(dbcmd))) {
					/* XXX - ERROR? */
				}
				clean_dbcmd(dbcmd);
			}
			if (!root) {
				last = root = cp;
			}
			else if (cp->c_level <= root->c_level) {
				add_hwcomponent(cmp, root);
				last = root = (hw_component_t *)NULL;
			}
			else {
				if (cp->c_level > last->c_level) {
					if (cp->c_parent_key != last->c_rec_key) {
						add_hwcomponent(cmp, root);
						last = root = (hw_component_t *)NULL;
					}
					hwcmp_add_child(last, cp);
				}
				else if (cp->c_level == last->c_level) {
					hwcmp_add_peer(last, cp);
				}
				else {
again:
					last = last->c_parent;
					if (cp->c_level < last->c_level) {
						goto again;
					}

					/* Make sure both records have the same parent
					 */
					if (cp->c_parent_key != last->c_parent_key) {
						add_hwcomponent(cmp, root);
						last = root = (hw_component_t *)NULL;
					}
					hwcmp_add_peer(last, cp);
				}
				last = cp;
			}
		}
		if (root) {
			add_hwcomponent(cmp, root);
		}
	}
	kl_free_block(cmepp);
	free_dbcmd(dbcmd);
	return(0);
}

/* 
 * get_db_swconfig()
 */
int
get_db_swconfig(configmon_t *cmp, time_t time)
{
	int i, flag = ALLOCFLG(cmp->flags);
	int count, rec_count;
	db_cmd_t *dbcmd;
	sw_component_t *swcp, *last, *cp, *root;
	cm_event_t *cmep, **cmepp;
	change_item_t *cep;
	database_t *dbp = cmp->database;
	sysconfig_t *scp = cmp->db_sysconfig;

	if (get_swcomponents(cmp)) {
		return(1);
	}

	/* If all we want is the current swconfig (from the database), then
	 * we can return...
	 */
	if ((cmp->db_sysconfig->date = time) == 0) {
		return(0);
	}

	/* Select all of the cm_event_s records and load them into the
	 * record data array.
	 */
	dbcmd = alloc_dbcmd(cmp->database);
	setup_dbquery(dbcmd, CM_EVENT_TBL, SYS_ID(cmp), 0, 0, 0);
	count = select_records(dbcmd);
	clean_dbcmd(dbcmd);
	cmepp = (cm_event_t**)kl_alloc_block(count * sizeof(void *), flag);

	i = 0;
	while (cmep = (cm_event_t *)next_record(cmp->database, CM_EVENT_TBL)) {
		cmepp[i++] = cmep;
	}

	/* Now, starting with the most recent change and walking backward, 
	 * adjust the swconfig tree (removing those records that were added
	 * and adding back those records that had been removed).
	 */
	for (i = count - 1; i >= 0; i--) {

		cmep = cmepp[i];

		if (cmep->time < time) {
			/* We've gone back further than we need to...free the block
			 * and continue (so that all other too-early blocks will be
			 * freed up).
			 */
			kl_free_block(cmep);
			continue;
		}

		/* Back out any records that were added...
		 */
		setup_dbquery(dbcmd, CHANGE_ITEM_TBL, SYS_ID(cmp), TIME, 0, cmep->time);
		rec_count = select_records(dbcmd);
		clean_dbcmd(dbcmd);
		while (cep = next_record(dbp, CHANGE_ITEM_TBL)) {
			if (cep->type == SW_INSTALLED) {
				remove_swcmp(&scp->swcmp_root, cep->rec_key);
			}
			kl_free_block(cep);
		}

		/* Add back in any records that were removed from the tree
		 */
		setup_dbquery(dbcmd, SWARCHIVE_TBL, SYS_ID(cmp), 
										DEINSTALL_TIME, 0, cmep->time);
		rec_count = select_records(dbcmd);
		clean_dbcmd(dbcmd);
		while(swcp = next_record(dbp, SWARCHIVE_TBL)) {
			swcp->sw_deinstall_time = 0;
			add_swcomponent((sw_component_t **)&scp->swcmp_root, swcp);
		}
		kl_free_block(cmep);
	}
	kl_free_block(cmepp);
	free_dbcmd(dbcmd);
	return(0);
}

/*
 * free_system_info()
 */
void
free_system_info(system_info_t *sysinfo)
{
    if (sysinfo->serial_number) {
        kl_free_block(sysinfo->serial_number);
    }
    if (sysinfo->hostname) {
        kl_free_block(sysinfo->hostname);
    }
    if (sysinfo->ip_address) {
        kl_free_block(sysinfo->ip_address);
    }
    kl_free_block(sysinfo);
}

