#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/RCS/configmon.c,v 1.10 1999/10/21 21:16:19 leedom Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <libexc.h>
#include <klib/klib.h>
#include <sss/configmon.h>

#include "configmon_print.h"

extern jmp_buf klib_jbuf;

#define COMPARE_SW	1
#define COMPARE_HW	2

/* Global variables
 */
char namelist[256];
char kernname[256];
char corefile[256];
char *program;
char dbname[26] = "ssdb";
int sys_type = 0;
int bounds = -1;                        /* bounds value with -n option       */
int bounds_flag = 0;                    /* bounds flag option                */
int sysinfo_flag = 0;
int hardware_flag = 0;
int software_flag = 0;
int update_flag = 0;
int sgm_flag = 0;
int kill_flag = 0;		/* drop all configmon tables */
int silent_flag = 0;    /* drop all configmon tables SILENTLY */
int errflg = 0;
FILE *ofp = stdout;
FILE *errorfp = stderr;

/* A note on exit status values. Individual bits signify different
 * change conditions. It's possible for more than one bit to be set
 * (e.g., both hardware and software changes can be detected). It's
 * necessary to test for each individual bit (if non-zero and not
 * an error).
 *
 * 0x00 = no errors, no changes in config (from SSDB version)
 * 0x01 = no errors, CONFIGMON_INIT
 * 0x02 = no errors, SYSINFO_CHANGE
 * 0x04 = no errors, HARDWARE_CHANGE
 * 0x08 = no errors, SOFTWARE_CHANGE
 * 0x10 = no errors, SYSTEM_CHANGE
 * 0xff = error condition (CONFIGMON_ERROR defined below)
 */
#define CONFIGMON_ERROR		0xff

int klp_hndl;				/* handle of the current klib_s struct */

/*
 * usage()
 */
void
usage()
{
	fprintf(stderr, "Usage: %s: [-n bounds] [-g] [-u] [-i] [-h] [-s]\n", 
		program);
}

/*
 * bad_option()
 */
void
bad_option(char *p, char c)
{
	fprintf(stderr, "%s: invalid command line option -- %c\n", p, c);
}

/*
 * initialize_configmon()
 * 
 *   This function assumes the sys_id we are trying to setup is 
 *   for the localhost (which is why sys_id is not passed in) 
 *   and if the database is not setup or the system record is out 
 *   of date, it will update the database. 
 */
static int
initialize_configmon(configmon_t *cmp)
{
	int new_system_rec = 0;
	int bounds = cmp->database->bounds;
	system_info_t *sysinfo;

	/* Check the state of the database. It's possible that the database
	 * tables have not been created yet. If that is the case, we need to 
	 * create them. 
	 */
	if (!(cmp->database->state & TABLES_EXIST)) {

		setup_tables(cmp->database, K_IP, bounds);
		if (!cmp->database->state & TABLES_EXIST) {
			return(1);
		}

		/* Get the system info record for localhost if there is one
		 * (it's possible that one was created at the start of system
		 * bring-up before the configmon tables were initialized).
		 */
		if (get_system_info(cmp, 0)) {
			if (!(cmp->sysinfo = get_sysinfo(bounds, cmp->flags))) {
				return(1);
			}
			new_system_rec = 1;
		}
		cmp->event_type |= CONFIGMON_INIT;
	}
	else {
		/* XXX -- at this point, we should make a copy of the current
		 * tables, just in case we have a problem (system crash?) while
		 * we are in the middle of the update operation. That way, we
		 * will have something to go back to and try over.
		 */
	}

	/* At this point, the system_info record we are looking for localhost 
	 * HAS to be there (if it isn't, it means there is something REALLY
	 * wrong with the database and we should should just return an error).
	 * If a new system rec was allocated above, we already know that
	 * the one in the database is up-to-date. Otherwise, we have to
	 * compare the one that's in the database to the one from the live
	 * system.
	 */
	if (!new_system_rec) {

		/* If the database is just now being initialized, then we have
		 * already gotten the system record from the database. If not,
		 * then we need to get it now.
		 */
		if (!(cmp->event_type & CONFIGMON_INIT)) {

			/* Get the system info record from the database
			 */
			if (get_system_info(cmp, 0)) {
				return(1);
			}
		}

		/* We have to make sure the record in the database is up-to-date.
		 */
		if (sysinfo = check_sysinfo(cmp->sysinfo, bounds, cmp->flags)) {
			if (sysinfo->sys_id != SYS_ID(cmp)) {

				/* This is a NEW system!!!
				 */
				free_system_info(sysinfo);
				cmp->event_type = SYSTEM_CHANGE;
				return(1);
			}
			else {
				/* Delete the old system_info record from the system_table
				 * and write it to the sysarchive_table.
				 */
				delete_records(cmp->database, SYSTEM_TBL, SYS_ID(cmp),
					cmp->sysinfo->rec_key);
				insert_record(cmp->database, SYSARCHIVE_TBL, cmp->sysinfo);
				insert_change_item(cmp->database, SYS_ID(cmp), 
					cmp->curtime, SYSINFO_OLD, SYSARCHIVE_TBL, 
					cmp->sysinfo->rec_key);
				cmp->event_type |= SYSINFO_CHANGE;
				free_system_info(cmp->sysinfo);
				cmp->sysinfo = sysinfo;
				new_system_rec = 1;
			}
		}
	}

	/* If we allocated a new system_info_s struct, we need to insert it
	 * into the database (and archive the old one if one already existed).
	 */
	if (new_system_rec) {
		cmp->sysinfo->rec_key = next_key(cmp->database, SYSTEM_TBL);
		insert_record(cmp->database, SYSTEM_TBL, cmp->sysinfo);

		/* We only insert a change record when we are replacing an 
		 * existing system info record...
		 */
		if (cmp->event_type & SYSINFO_CHANGE) {
			insert_change_item(cmp->database, SYS_ID(cmp), 
					cmp->curtime, SYSINFO_CURRENT, SYSTEM_TBL, 
					cmp->sysinfo->rec_key);
		}
	}
	return(0);
}

/*
 * lv_sysconfig()
 */
int
lv_sysconfig(configmon_t *cmp)
{
	int flags, first_time = 1;

	if (!(cmp->sysconfig = alloc_sysconfig(cmp->flags|STRINGTAB_FLG))) {
		return(1);
	}
	cmp->sysconfig->sys_id = SYS_ID(cmp);
	cmp->sysconfig->sys_type = SYS_TYPE(cmp);
	switch (SYS_TYPE(cmp)) {
		case 30:
		case 32:
			flags = (INVENT_SCSI_ALL|INVENT_CPUCHIP|INVENT_MEM_MAIN);
			break;

		default:
			flags = (INVENT_SCSI_ALL|INVENT_NETWORK);
			break;
	}

try_again:

	if (kl_get_hwconfig(cmp->sysconfig->hwconfig, flags)) {
		if (KL_ERROR == KLE_AGAIN) {
			if (first_time) {
				first_time = 0;
				goto try_again;
			}
		}
		free_sysconfig(cmp->sysconfig);
		cmp->sysconfig = (sysconfig_t *)NULL;
		return(1);
	}

	/* Get the software components (products) from the live system
	 */
	if (get_swconfig(cmp->sysconfig->swconfig)) {
		free_sysconfig(cmp->sysconfig);
		cmp->sysconfig = (sysconfig_t *)NULL;
		return(1);
	}
	update_sysconfig(cmp->sysconfig);
	return(0);
}

/*
 * compare_sysconfig_data()
 */
static int
compare_sysconfig_data(configmon_t *cmp, int compflg)
{
	hw_component_t *dbcp, *lvcp;

	if (cmp->sysconfig->sys_id != cmp->db_sysconfig->sys_id) {
		return(1);
	}

	/* Compare the hwconfig data. We need to step over the root of 
	 * each tree since it is not an actual hwcomponent.
	 */
	if (compflg & COMPARE_HW) {
		dbcp = cmp->db_sysconfig->hwcmp_root->c_cmplist;
		lvcp = cmp->sysconfig->hwcmp_root->c_cmplist;

		if (hw_compare_lists(cmp, dbcp, lvcp)) {
			cmp->state |= HW_DIFFERENCES;
		}
	}

	/* Compare the swconfig data 
	 */
	if (compflg & COMPARE_SW) {
		if (sw_compare_trees(cmp)) {
			cmp->state |= SW_DIFFERENCES;
		}
	}

	if (cmp->state & (HW_DIFFERENCES|SW_DIFFERENCES)) {
		return(1);
	}
	return(0);
}

/*
 * add_to_hwarchive()
 */
static void
add_to_hwarchive(configmon_t *cmp, hw_component_t *hwcp)
{
	if (!hwcp) {
		return;
	}

	/* Make sure the hwcomponent we are about to archive points only to itself.
	 * If not, we could try and archive all of the the hwcomponents that are 
	 * still in the active tree.
	 */
	hwcp->c_next = hwcp->c_prev = hwcp;

	if (cmp->hwcmp_archive) {
		insert_hwcomponent(cmp->hwcmp_archive, hwcp, 0);
	}
	else {
		cmp->hwcmp_archive = hwcp;
	}
}

/*
 * archive_hwcomponents()
 */
static void
archive_hwcomponents(configmon_t *cmp)
{
	hw_component_t *next, *cp;

	if (!(next = cmp->hwcmp_archive)) {
		return;
	}
	do {
		next->c_deinstall_time = cmp->curtime;
		insert_record(cmp->database, HWARCHIVE_TBL, next);
		insert_change_item(cmp->database, SYS_ID(cmp), cmp->curtime, 
				HW_DEINSTALLED, HWARCHIVE_TBL, next->c_rec_key);

		if (cp = cm_get_hwcmp(next, CM_NEXT)) {
			do {
				if ((cp == next->c_next) || (cp->c_level <= next->c_level)) {
					break;
				}
				cp->c_deinstall_time = cmp->curtime;
				insert_record(cmp->database, HWARCHIVE_TBL, cp);
				insert_change_item(cmp->database, SYS_ID(cmp), cmp->curtime, 
					HW_DEINSTALLED, HWARCHIVE_TBL, cp->c_rec_key);
			} while (cp = cm_get_hwcmp(cp, CM_NEXT));
		}
		next = next->c_next;
	} while (next != cmp->hwcmp_archive);

	/* Free up the hw_component_s structs that were just archived.
	 */
	free_next_hwcmp(cmp->hwcmp_archive);
	cmp->hwcmp_archive = (hw_component_t *)NULL;
	cmp->event_type |= HARDWARE_DEINSTALLED;
}

/*
 * put_hwcomponents()
 */
static int
put_hwcomponents(configmon_t *cmp)
{
	int seq = 0, deleted, newhwcmp = 0;
	hw_component_t *cp;
	database_t *dbp = cmp->database;

	if (!dbp) {
		return(1);
	}

	if (!(cp = cmp->db_sysconfig->hwcmp_root)) {
		if (!(cp = cmp->sysconfig->hwcmp_root)) {
			return(1);
		}
	}

	deleted = delete_records(dbp, HWCOMPONENT_TBL, SYS_ID(cmp), 0);

	do {
		cp->c_seq = seq++;
		/* See if the current record has a record key assigned. This
		 * key has to be unique across both hwcomponent_table and 
		 * hwarchive_table. Note that the root (level 0) of the 
		 * hwconfig tree does not need a key as it is not put into 
		 * the database.
		 */
		if (cp->c_level && !cp->c_rec_key) {
			/* This is a new component. We have to make sure the data
			 * record (if any) gets setup properly and inserted into
			 * the database.
			 */
			newhwcmp = 1;
			cp->c_rec_key = next_key(dbp, HWCOMPONENT_TBL);
			cp->c_sys_id = SYS_ID(cmp);
	
			if (cp->c_parent) {
				cp->c_parent_key = cp->c_parent->c_rec_key;
			}

			cp->c_install_time = cmp->curtime;
		 
			switch(cp->c_category) {
				case HW_BOARD:
					if (cp->c_type == BD_NODE) {
						NODE_DATA(cp)->sys_id = SYS_ID(cmp); 
						NODE_DATA(cp)->comp_rec_key = cp->c_rec_key; 
						if (!NODE_DATA(cp)->rec_key) {
							NODE_DATA(cp)->rec_key = next_key(dbp, NODE_TBL);
						}
						cp->c_data_table = NODE_TBL;
						cp->c_data_key = NODE_DATA(cp)->rec_key;
					}
					break;

				case HW_CPU:	
					CPU_DATA(cp)->sys_id = SYS_ID(cmp);
					CPU_DATA(cp)->comp_rec_key = cp->c_rec_key;
					if (!CPU_DATA(cp)->cpu_rec_key) {
						CPU_DATA(cp)->cpu_rec_key = next_key(dbp, CPU_TBL);
					}
					cp->c_data_table = CPU_TBL;
					cp->c_data_key = CPU_DATA(cp)->cpu_rec_key;
					break;

				case HW_MEMBANK:
					MEMBANK_DATA(cp)->sys_id = SYS_ID(cmp);
					MEMBANK_DATA(cp)->comp_rec_key = cp->c_rec_key;
					if (!MEMBANK_DATA(cp)->mb_rec_key) {
						MEMBANK_DATA(cp)->mb_rec_key = 
							next_key(dbp, MEMBANK_TBL);
					}
					cp->c_data_table = MEMBANK_TBL;
					cp->c_data_key = MEMBANK_DATA(cp)->mb_rec_key;
					break;

				case HW_IOA:
					IOA_DATA(cp)->sys_id = SYS_ID(cmp);
					IOA_DATA(cp)->comp_rec_key = cp->c_rec_key;
					if (!IOA_DATA(cp)->ioa_rec_key) {
						IOA_DATA(cp)->ioa_rec_key = next_key(dbp, IOA_TBL);
					}
					cp->c_data_table = IOA_TBL;
					cp->c_data_key = IOA_DATA(cp)->ioa_rec_key;
					break;
			}

			/* Write out the data to the appropriate data table
			 */
			if (cp->c_data) {
				insert_record(dbp, cp->c_data_table, cp->c_data);
			}
		}

		/* Now, dump the component to the database. Make sure we don't 
		 * put the level-zero hwcomponent (root). It's not really a 
		 * hwcomponent.
		 */
		if (cp->c_level) {
			insert_record(dbp, HWCOMPONENT_TBL, cp);
			if (newhwcmp) {
				if (deleted) {
					/* Insert a change record for the new record
					 */
					insert_change_item(dbp, SYS_ID(cmp), cmp->curtime, 
						HW_INSTALLED, HWCOMPONENT_TBL, cp->c_rec_key);
					cmp->event_type |= HARDWARE_INSTALLED;
				}
				newhwcmp = 0;
			}
		}
		cp = next_hwcmp(cp);
	} while (cp);
	return(0);
}

/*
 * hw_find_match()
 */
static int
hw_find_match(hw_component_t *list, hw_component_t *hwcp) 
{
	hw_component_t *hcp;

	hcp = list;
	do {
		if (!compare_hwcomponents(hcp, hwcp)) {
			hcp->c_state = MATCH_FOUND;
			hwcp->c_state = MATCH_FOUND;
			return(1);
		}
		hcp = hcp->c_next;
	} while (hcp != list);
	return(0);
}

/*
 * hw_find_changes()
 *
 * Walk through list1 looking for matches in list2. Return the
 * number of itesm on list1 that did not have a match. Note that
 * there may be items on list2 that do not have a match either, 
 * but they will not be reflected in the return value (they will
 * have a zero value in their c_state field).
 */
static int
hw_find_changes(hw_component_t *list1, hw_component_t *list2)
{
	int change_count = 0;

	hw_component_t *hcp;

	hcp = list1;
	do {
		if (!(hcp->c_state & MATCH_FOUND) && !hw_find_match(list2, hcp)) {
			change_count++;
		}
		hcp = hcp->c_next;
	} while (hcp != list1);
	return(change_count);
}

/*
 * clone_hwcomponents()
 */
static hw_component_t *
clone_hwcomponents(configmon_t *cmp, hw_component_t *hwcp)
{
	int level, count = 0;
	hw_component_t *root, *next, *new, *cur;
	hwconfig_t *hcp = cmp->sysconfig->hwconfig;
	hw_component_t *dup_hwcomponent();

	root = dup_hwcomponent(hcp, hwcp);
	root->c_parent = hwcp->c_parent;
	level = root->c_level;
	cur = root;

	if (next = cm_get_hwcmp(hwcp, CM_NEXT)) {
		do {
			/* If we have gone beyond our "root" then end the search
			 */
			if (next->c_level <= hwcp->c_level) {
				break;
			}
			new = dup_hwcomponent(hcp, next);
			if (new->c_level > level) {
				ht_insert_child((htnode_t *)cur, (htnode_t *)new, HT_BEFORE);
				new->c_parent = cur;
				level = new->c_level;
			}
			else if (new->c_level == level) {
				ht_insert_pier((htnode_t *)cur, (htnode_t *)new, HT_AFTER);
				new->c_parent = cur->c_parent;
			}
			else {
again:
				if (cur = cur->c_parent) {
					if (cur->c_level == new->c_level) {
						ht_insert_pier((htnode_t *)cur, 
							(htnode_t *)new, HT_AFTER);
						level = new->c_level;
						new->c_parent = cur->c_parent;
					}
					goto again;
				}
			}
			cur = new;
			count++;
		} while (next = cm_get_hwcmp(next, CM_NEXT));
	}
	return(root);
}

/*
 * hw_set_state()
 */
static void
hw_set_state(hw_component_t *hwcp, int state)
{
	hw_component_t *cp;

	hwcp->c_state = state;
	if (cp = cm_get_hwcmp(hwcp, CM_NEXT)) {
		do {
			if (cp->c_level <= hwcp->c_level) {
				break;
			}
			cp->c_state = state;
		} while (cp = cm_get_hwcmp(cp, CM_NEXT));
	}
}

/*
 * hw_compare_lists()
 */
static int
hw_compare_lists(configmon_t *cmp, hw_component_t *dbcp, hw_component_t *lvcp)
{
	int iflag, db_changes, lf_changes, change_cnt = 0;
	hw_component_t *next, *clone, *cp;

	db_changes = hw_find_changes(dbcp, lvcp);
	lf_changes = hw_find_changes(lvcp, dbcp);

	/* Cycle through the live tree and see if there are any
	 * hwcomponent records that need to be cloned and placed
	 * in the database tree.
	 */
	if (lf_changes) {
		next = lvcp;
		do {
			if (!next->c_state) {
				clone = clone_hwcomponents(cmp, next);
				hw_set_state(clone, MATCH_FOUND);
				if (cp = hw_find_location(dbcp, clone)) {
					replace_hwcomponent(cp, clone);
					add_to_hwarchive(cmp, cp);
				}
				else if (cp = hw_find_insert_point(dbcp, clone, &iflag)) {
					insert_hwcomponent(cp, clone, iflag);
				}
				if (cp == dbcp) {
					dbcp = clone;
				}
				change_cnt++;
			}
			next = next->c_next;
		} while (next != lvcp);
	}

	/* Cycle through the database tree and see if there are any 
	 * hwcomponents that need to be placed on the archive list.
	 */
	if (db_changes) {
		next = dbcp;
		do {
			if (!next->c_state) {
				cp = next->c_next;
				unlink_hwcomponent(next);
				add_to_hwarchive(cmp, next);
				change_cnt++;
				if (next == dbcp) {
					dbcp = cp;
				}
				next = cp;
			}
			else {
				next = next->c_next;
			}
		} while (next != dbcp);
	}

	next = dbcp;
	do {
		if (cp = hw_find_location(lvcp, next)) {
			if (next->c_cmplist && cp->c_cmplist) {
				change_cnt += 
					hw_compare_lists(cmp, next->c_cmplist, cp->c_cmplist);
			}
		}
		next = next->c_next;
	} while (next != dbcp);
	return(change_cnt);
}

/* 
 * put_swcomponents()
 */
static int
put_swcomponents(configmon_t *cmp)
{
	int deleted, newswcmp = 0;
	sw_component_t *swcp;
	database_t *dbp = cmp->database;

	if (!(swcp = cmp->db_sysconfig->swcmp_root)) {
		if (!(swcp = cmp->sysconfig->swcmp_root)) {
			return(1);
		}
	}

	deleted = delete_records(dbp, SWCOMPONENT_TBL, SYS_ID(cmp), 0);

	if (swcp = first_swcomponent(swcp)) {
		do {
			if (!swcp->sw_key) {
				/* This is a new component 
				 */
				newswcmp = 1;
				swcp->sw_key = next_key(dbp, SWCOMPONENT_TBL);
				swcp->sw_sys_id = SYS_ID(cmp);
			}

			/* Now, dump the component to the database. 
			 */
			insert_record(dbp, SWCOMPONENT_TBL, swcp);
			if (newswcmp) {
				if (deleted) {
					/* Insert a change record for the new record
					 */
					insert_change_item(dbp, SYS_ID(cmp), cmp->curtime, 
						SW_INSTALLED, SWCOMPONENT_TBL, swcp->sw_key);
					cmp->event_type |= SOFTWARE_INSTALLED;
				}
				newswcmp = 0;
			}
		} while (swcp = next_swcomponent(swcp));
		return(0);
	}
	return(1);
}

/*
 * add_to_swarchive()
 */
static void
add_to_swarchive(configmon_t *cmp, sw_component_t *swcp)
{
	enqueue((element_t **)&cmp->swcmp_archive, (element_t *)swcp);
}

/*
 * archive_swcomponents()
 */
static void
archive_swcomponents(configmon_t *cmp)
{
	sw_component_t *swcp, *next;

	if (!(swcp = cmp->swcmp_archive)) {
		return;
	}
	do {
		swcp->sw_deinstall_time = cmp->curtime;
		insert_record(cmp->database, SWARCHIVE_TBL, swcp);
		insert_change_item(cmp->database, SYS_ID(cmp), cmp->curtime, 
				SW_DEINSTALLED, SWARCHIVE_TBL, swcp->sw_key);
		next = (sw_component_t *)((element_t *)swcp)->next;
		free_swcomponent(swcp);
		swcp = next;
	} while (swcp != cmp->swcmp_archive);

	/* Free up the hw_component_s structs that were just archived.
	 */
	cmp->swcmp_archive = (sw_component_t *)NULL;
	cmp->event_type |= SOFTWARE_DEINSTALLED;
}

/* 
 * compare_swcomponents()
 */
static int
compare_swcomponents(sw_component_t *swcp1, sw_component_t *swcp2)
{
	if (swcp1->sw_version != swcp2->sw_version) {
		return(1);
	}
	if (!string_match(swcp1->sw_description, swcp2->sw_description)) {
		return(1);
	}
	if (swcp1->sw_install_time != swcp2->sw_install_time) {
		return(1);
	}
	return(0);
}

/*
 * clone_swcomponent()
 */
static sw_component_t *
clone_swcomponent(sysconfig_t *scp, sw_component_t *swcp)
{
	int aflag = ALLOCFLG(scp->flags);
	sw_component_t *new_swcp;

	new_swcp = kl_dup_block((caddr_t *)swcp, aflag);
	new_swcp->sw_name = (char *)kl_str_to_block(swcp->sw_name, aflag);
	new_swcp->sw_description = 
		(char *)kl_str_to_block(swcp->sw_description, aflag);
	new_swcp->sw_left = (sw_component_t *)NULL;
	new_swcp->sw_right = (sw_component_t *)NULL;
	new_swcp->sw_parent = (sw_component_t *)NULL;
	return(new_swcp);
}

/*
 * sw_compare_trees()
 */
static int
sw_compare_trees(configmon_t *cmp)
{
	int count = 0;
	sw_component_t *dbswcp, *swcp, *parent, *clone;
	sw_component_t *dbswroot = cmp->db_sysconfig->swcmp_root;
	sw_component_t *swroot = cmp->sysconfig->swcmp_root;

	if (!swroot || !(swcp = first_swcomponent(swroot))) {
		/* XXX -- should return a -1 value to distinguish an error
		 * from a changed/added record.
		 */
		return(1);
	}
	do {
		if (swcp->sw_state & MATCH_FOUND) {
			continue;
		}
		if (dbswcp = find_swcomponent(dbswroot, swcp->sw_name)) {
			if (compare_swcomponents(swcp, dbswcp)) {

				/* Replace the sw_component that was found in the
				 * database tree. Place the old db record in the
				 * archive table and enter a change record.
				 */
				clone = clone_swcomponent(cmp->sysconfig, swcp);
				if (dbswcp == dbswroot) {
					dbswroot = clone;
				}
				else {
					parent = dbswcp->sw_parent;
					if (parent->sw_left == dbswcp) {
						parent->sw_left = clone;
					}
					else {
						parent->sw_right = clone;
					}
				}

				if (clone->sw_left = dbswcp->sw_left) {
					clone->sw_left->sw_parent = clone;
				}
				if (clone->sw_right = dbswcp->sw_right) {
					clone->sw_right->sw_parent = clone;
				}
				clone->sw_parent = dbswcp->sw_parent;
				clone->sw_height = dbswcp->sw_height;
				dbswcp->sw_left = dbswcp->sw_right = (sw_component_t *)NULL;
				add_to_swarchive(cmp, dbswcp);
				clone->sw_state |= MATCH_FOUND;
				swcp->sw_state |= MATCH_FOUND;
				count++;
			}
			else {
				dbswcp->sw_state |= MATCH_FOUND;
				swcp->sw_state |= MATCH_FOUND;
			}
		}
		else {
			/* Add the sw_component to the database tree
			 */
			clone = clone_swcomponent(cmp->sysconfig, swcp);
			add_swcomponent(&dbswroot, clone);
			clone->sw_state |= MATCH_FOUND;
			swcp->sw_state |= MATCH_FOUND;
			count++;
		}
	} while (swcp = next_swcomponent(swcp));

	/* If the root node of our swcomponent tree was exchanged for a more
	 * current record, then we have to make sure the pointer in the 
	 * db_sysconfig record is correct.
	 */
	if (cmp->db_sysconfig->swcmp_root != dbswroot) {
		cmp->db_sysconfig->swcmp_root = dbswroot;
	}

	/* Now, cycle through the database tree and see if there are any
	 * sw_components that were in the database tree but were not in
	 * the live tree.
	 */
	dbswcp = first_swcomponent(cmp->db_sysconfig->swcmp_root);
	do {

next:
		if (!(dbswcp->sw_state & MATCH_FOUND)) {
			swcp = next_swcomponent(dbswcp);
			unlink_swcomponent(&cmp->db_sysconfig->swcmp_root, dbswcp);
			dbswcp->sw_left = dbswcp->sw_right = (sw_component_t *)NULL;
			add_to_swarchive(cmp, dbswcp);
			count++;
			if (dbswcp = swcp) {
				goto next;
			}
			else {
				break;
			}
		}
		
	} while (dbswcp = next_swcomponent(dbswcp));
	return(count);
}

/*
 * configmon()
 */
configmon_t *
configmon(char *dbname, int bounds)
{
	int compare_flag = 0;
	int flag = K_TEMP;
	configmon_t *cmp;

	if (!(cmp = alloc_configmon(dbname, bounds, flag, 0))) {
		return((configmon_t *)NULL);
	}
	if (initialize_configmon(cmp)) {
		free_configmon(cmp);
		return((configmon_t *)NULL);
	}
	lock_tables(cmp->database);

	/* Get the current system configuration information.
	 */
	if (lv_sysconfig(cmp)) {
		unlock_tables(cmp->database);
		free_configmon(cmp);	
		return((configmon_t *)NULL);
	}

	/* Now get the system configuration data from the database. If
	 * the database does not contain any sysconfig data, put the
	 * information from the live system. Otherwise, see if there 
	 * have been any changes.
	 */
	db_sysconfig(cmp, 0);
	if (cmp->db_sysconfig->hwcmp_root) {
		compare_flag |= COMPARE_HW;
	}
	else {
		put_hwcomponents(cmp);
	}
	if (cmp->db_sysconfig->swcmp_root) {
		compare_flag |= COMPARE_SW;
	}
	else {
		put_swcomponents(cmp);
	}

	if (compare_flag) {

		/* Compare live and database views of system config data
		 */
		if (compare_sysconfig_data(cmp, compare_flag)) {

			/* There were differences in the system config data. 
			 * the db_sysconfig struct should now contain all new
			 * component records. All data for any changed tables
			 * needs to be written back out to the database (after
			 * all records for sys_id have been deleted).
			 */
			if (cmp->state & HW_DIFFERENCES) {
				put_hwcomponents(cmp);
				archive_hwcomponents(cmp);
			}
			if (cmp->state & SW_DIFFERENCES) {
				put_swcomponents(cmp);
				archive_swcomponents(cmp);
			}
		}
	}

	if (cmp->event_type) {
		insert_cm_event(cmp->database, SYS_ID(cmp), 
			cmp->curtime, cmp->event_type, cmp->flags);
		if (send_cm_event(cmp->curtime, cmp->event_type)) {

			/* XXX -- ERROR sending event to eventmond?
			 */
		}
	}
	unlock_tables(cmp->database);
	return(cmp);
}

/*
 * main()
 */
main(int argc, char **argv)
{
	int c, ret, exit_status;
	configmon_t *cmp = (configmon_t *)NULL;

	/* getopt() variables.
	 */
	extern char *optarg;
	extern int optind;
	extern int opterr;

	/* Set opterr to zero to suppress getopt() error messages
	 */
	opterr = 0;

	/* Initialize a few default variables.
	 */
	sprintf(corefile, "/dev/mem");

	/* Get the name of the program...
	 */
	program = argv[0];

	/* Grab all of the command line arguments.
	 */
	while ((c = getopt(argc, argv, "gikn:chsuS")) != -1) {
		
		switch (c) {

			case 'g':
				sgm_flag++;
				break;

			case 'i':
				sysinfo_flag++;
				break;

			case 'n':
				/* unix/vmcore file extentison
				 */
				if (((char*)optarg)[0] == '-') {
					fprintf(stderr, "%s: Bounds was not specified\n", program);
					errflg++;
				}
				else {
					bounds = atoi(optarg);
					bounds_flag++;
				}
				break;

			case 'h':
				hardware_flag++;
				break;

			case 'k':
				kill_flag++;
				break;

			case 's':
				software_flag++;
				break;

			case 'u':
				update_flag++;
				break;

			case 'S':
				silent_flag++;
				break;

			default:
				bad_option(program, c);
				errflg++;
				break;
		}
		if (errflg) {
			usage();
			exit(CONFIGMON_ERROR);
		}
	}

	if (!bounds_flag) {
		/* Get the name of the namelist and corefile...
		 */
		if (optind < argc) {
			if ((argc - optind) > 1) {
				strcpy(namelist, argv[optind]);
				strcpy(corefile, argv[optind+1]);
			}
			else {
				usage();
				exit(CONFIGMON_ERROR);
			}
		}
		else {
			/* get the name of the auto boot kernel from NVRAM
			 */
			if (sgikopt("OSLoadFilename", namelist, 256)) {
				sprintf(namelist, "/unix");
			}
			else {
				/* Make sure that the namelist starts with root
				 */
				if (namelist[0] != '/') {
					kernname[0] = '/';
					kernname[1] = 0;
					strcat(kernname, namelist);
					strcpy(namelist, kernname);
				}
			}
		}
	}
	else if (bounds >= 0) {
		sprintf(namelist, "unix.%d", bounds);
		sprintf(corefile, "vmcore.%d.comp", bounds);
	}
	else {
		fprintf(stderr, "Bounds value for -n option needs to be >= 0!\n");
		errflg++;
	}

	/* get the kernname from NVRAM to see if this is a bootp kernel
	 */
	if (!sgikopt("kernname", kernname, 256)) {
		if (!strncmp(kernname, "bootp", 5)) {
			fprintf(stderr, "%s: cannot run against a bootp kernel\n", program);
			exit(1);
		}
	}

	if (kill_flag) {
		char ch;

		if (!silent_flag) {
			fprintf(errorfp, "\nAre you SURE you want to delete all "
							 "configmon tables? (Y/N) [N]: ");
			ch = getchar();
			if ((ch == 'Y') || (ch == 'y')) {
				fprintf(stderr, "Dropping all configmon tables");
				if (bounds >= 0) {
					fprintf(stderr, ", bounds=%d\n", bounds);
				}
				else {
					fprintf(stderr, "\n");
				}
				db_cleanup(dbname, bounds, 0);
			}
		}
		else {
			db_cleanup(dbname, bounds, 0);
		}
		if (!update_flag) {
			exit(0);
		}
	}

	if (setjmp(klib_jbuf)) {
		exit(1);
	}

#ifdef CM_COREDUMP
	/* If there is some fatal error (SEGV, SEGBUS, SEGABRT) then
	 * allow the signal handler to generate a core dump...
	 */
	generate_core = 1;
#endif

	/* Set up the error handler right away, just in case there is 
	 * a KLIB error during startup.
	 */
	kl_setup_error(stderr, program, NULL);

	/* Set up signal handler for SEGV and BUSERR signals so that 
	 * ConfigMon doesn't dump core.
	 */
	kl_sig_setup();

	if (update_flag) {

		/* Make sure the efective UID is root's
		 */
		if (geteuid() != 0) {
			fprintf(errorfp, "\nOnly ROOT can check or update "
				"system configuration data\n\n");
			exit(CONFIGMON_ERROR);
		}

		/* Initialize the klib memory allocator. Note that if 
		 * kl_init_mempool() is not called, malloc() and free() 
		 * will be used when allocating or freeing blocks of 
		 * memory. Or, another set of functions can be registered 
		 * instead.
		 */
		kl_init_mempool(0, 0, 0);

		/* Initialize the klib_s struct that will provide a single 
		 * point of contact for various bits of KLIB data.
		 */
		if ((klp_hndl = init_klib()) == -1) {
			fprintf(stderr, "Could not initialize klib_s struct!\n");
			exit(CONFIGMON_ERROR);
		}

		/* Set any KLIB global behavior flags (e.g., K_IGNORE_FLAG). 
		 */
		set_klib_flags(0);

		/* Setup info relating to the corefile...
		 */
		if (kl_setup_meminfo(namelist, corefile, O_RDWR)) {
			fprintf(stderr, "Could not setup meminfo!\n");
			exit(CONFIGMON_ERROR);
		}

		if (kl_setup_symtab(namelist)) {
			fprintf(errorfp, "\nCould not initialize libsym!\n");
			exit(CONFIGMON_ERROR);
		}

		if (libkern_init()) {
			fprintf(errorfp, "\nCould not initialize libkern!\n");
			exit(CONFIGMON_ERROR);
		}

		cmp = configmon(dbname, bounds);
	}
	else {
		cm_init();
	}

	/* Make sure that we've allocated a configmon_s struct via the 
	 * call to cm_alloc_config(). If no tables exist, then we cannot
	 * go any further...
	 */
	if (!cmp) {
		if (!(cmp = (configmon_t *)cm_alloc_config(dbname, 0))) {
			exit(CONFIGMON_ERROR);
		}
		if (!(cmp->database->state & TABLES_EXIST)) {
			fprintf(stderr, "ConfigMon tables are not initalized.\n");
			exit(CONFIGMON_ERROR);
		}
		if (get_system_info(cmp, 0)) {
			fprintf(stderr, "ConfigMon system info not available.\n");
			exit(CONFIGMON_ERROR);
		}
	}

	if (sgm_flag) {
		/* This configmon is running on a system acting as an SGM. We
		 * need to make sure that all tables are created, not just the
		 * ones that are valid for this particular system type. 
		 */
		switch (SYS_TYPE(cmp)) {
			case 19:
				setup_table(cmp->database, NODE_TBL, 0);
				break;

			case 27:
				setup_table(cmp->database, IOA_TBL, 0);
				break;

			case 32:
				setup_table(cmp->database, NODE_TBL, 0);
				setup_table(cmp->database, MEMBANK_TBL, 0);
				setup_table(cmp->database, IOA_TBL, 0);
				break;

			default:
				setup_table(cmp->database, CPU_TBL, 0);
				setup_table(cmp->database, NODE_TBL, 0);
				setup_table(cmp->database, MEMBANK_TBL, 0);
				setup_table(cmp->database, IOA_TBL, 0);
				break;
		}
	}

	if (sysinfo_flag || hardware_flag || software_flag) {

		if (!update_flag) {
			/* If we don't already have it, get the current 
			 * system config info.
			 */
			ret = cm_get_sysconfig(cmp, 0, 0);
			if (ret) {
				switch(ret) {
					case 1:
						fprintf(stderr, "Time is greater than current time.\n");
						break;

					case 4:
						fprintf(stderr, "Time is less than "
							"configmon init time.\n");
						break;

					default:
						break;
				}
				exit(-1);
			}
		}
		if (sysinfo_flag) {
			cm_print_sysinfo(cmp, ofp);
		}
		if (hardware_flag) {
			cm_print_hwconfig(cmp, ofp);
		}
		if (software_flag) {
			cm_print_swconfig(cmp, ofp);
		}
	}

	exit_status = cmp->event_type;
	free_configmon(cmp);

#ifdef ALLOC_DEBUB
    /* XXX -- just to see if we forgot anyting...
     */
    alloc_debug = 1;
#endif
	if (KLP) {
		kl_free_klib(KLP);
	}
    free_temp_blocks();
	exit(exit_status);
}

