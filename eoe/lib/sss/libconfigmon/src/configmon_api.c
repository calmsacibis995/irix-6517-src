#ident "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/src/RCS/configmon_api.c,v 1.4 1999/05/27 23:13:43 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include <sss/configmon.h>

int change_event_member_type[] = {
	UNSIGNED_LONG,      /* (0) CE_SYS_ID */
	UNSIGNED,           /* (1) CE_TIME */
	SIGNED              /* (2) CE_TYPE */
};

int change_item_member_type[] = {
	UNSIGNED_LONG,      /* (0) CI_SYS_ID */
	UNSIGNED,           /* (1) CI_TIME */
	SIGNED,             /* (2) CI_TYPE */
	SIGNED,             /* (3) CI_TID */
	SIGNED              /* (4) CI_REC_KEY */
};

int sysinfo_member_type[] = {
	SIGNED,             /*  (0) SYS_REC_KEY */
	UNSIGNED_LONG,      /*  (1) SYS_SYS_ID */
	SIGNED,             /*  (2) SYS_SYS_TYPE */
	STRING_POINTER,     /*  (3) SYS_SERIAL_NUMBER */
	STRING_POINTER,     /*  (4) SYS_HOSTNAME */
	STRING_POINTER,     /*  (5) SYS_IP_ADDRESS */
	SHORT,				/*  (6) SYS_ACTIVE */
	SHORT,				/*  (7) SYS_LOCAL */
	UNSIGNED 			/*  (8) SYS_TIME */
};

int hw_member_type[] = {
	SIGNED,				/*  (0) HW_SEQ */
	SIGNED,				/*  (1) HW_LEVEL */
	SIGNED,				/*  (2) HW_REC_KEY */
	SIGNED,				/*  (3) HW_PARENT_KEY */
	UNSIGNED_LONG,		/*  (4) HW_SYS_ID */
	SIGNED, 			/*  (5) HW_CATEGORY */
	SIGNED, 			/*  (6) HW_TYPE */
	SIGNED, 			/*  (7) HW_INV_CLASS */
	SIGNED, 			/*  (8) HW_INV_TYPE */
	SIGNED, 			/*  (9) HW_INV_CONTROLLER */
	SIGNED, 			/* (10) HW_INV_UNIT */
	SIGNED, 			/* (11) HW_INV_STATE */
	STRING_POINTER, 	/* (12) HW_LOCATION */
	STRING_POINTER, 	/* (13) HW_NAME */
	STRING_POINTER,		/* (14) HW_SERIAL_NUMBER */
	STRING_POINTER,		/* (15) HW_PART_NUMBER */
	STRING_POINTER, 	/* (16) HW_REVISION */
	SIGNED, 			/* (17) HW_ENABLED */
	UNSIGNED, 			/* (18) HW_INSTALL_TIME */
	UNSIGNED, 			/* (19) HW_DEINSTALL_TIME */
	SIGNED,  			/* (20) HW_DATA_TABLE */
	SIGNED,				/* (21) HW_DATA_KEY */
};

int sw_member_type[] = {
	STRING_POINTER,		/* (0) SW_NAME */
	UNSIGNED_LONG,		/* (1) SW_SYS_ID */
	UNSIGNED, 			/* (2) SW_VERSION */
	STRING_POINTER, 	/* (3) SW_DESCRIPTION */
	UNSIGNED, 			/* (4) SW_INSTALL_TIME */
	UNSIGNED, 			/* (5) SW_DEINSTALL_TIME */
	UNSIGNED 			/* (6) SW_REC_KEY */
};

/*
 * alloc_list()
 */
static list_t *
alloc_list(int flag) 
{
	list_t *listp = (list_t*)NULL;

	if (listp = (list_t*)kl_alloc_block(sizeof(list_t), flag)) {
		listp->list_magic = LIST_MAGIC;
	}
	return(listp);
}

/*
 * free_item()
 */
static void
free_item(configmon_t *cmp, cm_hndl_t item, int item_type)
{
	int i;
	database_t *dbp;

	if (!cmp || !item) {
		return;
	}
	dbp = cmp->database;

	switch(item_type) {

		case LIST_TYPE:
			cm_free_list(cmp, item);
			break;

		case CM_EVENT_TYPE:
			free_record_memory(dbp, CM_EVENT_TBL, item);
			break;

		case CHANGE_ITEM_TYPE:
			free_record_memory(dbp, CHANGE_ITEM_TBL, item);
			break;

		case SYSINFO_TYPE:
			free_record_memory(dbp, SYSTEM_TBL, item);
			break;

		case HWCOMPONENT_TYPE:
			free_record_memory(dbp, HWCOMPONENT_TBL, item);
			break;

		case SWCOMPONENT_TYPE:
			free_record_memory(dbp, SWCOMPONENT_TBL, item);
			break;

		default:
			kl_free_block(item);
			break;
	}
}

/*
 * event_history()
 */
static list_t *
event_history( 
	configmon_t *cmp, 
	k_uint_t sys_id, 
	time_t start_time, 
	time_t end_time, 
	int type)
{
	int i, j, event_count, aflag;
	cm_event_t *event_p;
	list_t *listp;
	void **event_item;
	db_cmd_t *dbcmd;
	char *ptr;
	database_t *dbp;

	if (!cmp) {
		return((list_t *)NULL);
	}
	dbp = cmp->database;
	aflag = ALLOCFLG(dbp->flags);

	/* A valid sys_id must be passed in (no lookup for "localhost" allowed).
	 */
	if (sys_id == 0) {
		return((list_t *)NULL);
	}

	/* Select all of the cm_event_s records and load them into the
	 * item array.
	 */
	dbcmd = alloc_dbcmd(dbp);

	/* Build the query string
	 */
	sprintf(dbp->sqlbuf, "SELECT * from %s WHERE sys_id=\"%llx\"", 
		TABLE_NAME(dbp, CM_EVENT_TBL), sys_id); 
	ptr = NEXTPTR(dbp->sqlbuf);
	if (start_time) {
		sprintf(ptr, " AND time>=%d", start_time);
		ptr = NEXTPTR(ptr);
	}
	if (end_time) {
		if (start_time) {
			sprintf(ptr, " AND");
			ptr += 4;
		}
		sprintf(ptr, " time<=%d", end_time);
	}
	ptr = NEXTPTR(ptr);
	sprintf(ptr, " ORDER BY time");

	/* Setup the command for our query
	 */
	setup_dbcmd(dbcmd, SSDB_REQTYPE_SELECT, CM_EVENT_TBL, dbp->sqlbuf);

	/* Execute the command
	 */
	event_count = select_records(dbcmd);

	/* Free the dbcmd (we don't need it any more)
	 */
	free_dbcmd(dbcmd);

	/* Check to make sure that we found something...if we didn't then
	 * return a NULL list_t pointer.
	 */
	if (!event_count) {
		return((list_t *)NULL);
	}

	/* Allocate a buffer for the event list
	 */
	event_item = (void**)kl_alloc_block((event_count * sizeof(void *)), aflag);

	j = 0;
	for (i = 0; i < event_count; i++) {

		/* Get the next cm_event record and see if it is of the 
		 * type we are looking for...
		 */
		event_p = (cm_event_t *)next_record(dbp, CM_EVENT_TBL);
		if (!(event_p->type & type)) {
			kl_free_block(event_p);
			continue;
		}
		event_item[j] = event_p;
		j++;
	}

	/* Allocate some space for the event list header
	 */
	listp = alloc_list(aflag);

	/* Setup the contents of the list_s struct 
	 */
	listp->list_type = CHANGE_HISTORY_LIST;
	listp->item_type = CM_EVENT_TYPE;
	listp->start_time = start_time;
	listp->end_time = end_time;
	if ((listp->count = j) == 0) {
		kl_free_block(event_item);
		listp->item = (void**)NULL;
	}
	else {
		listp->item = event_item;
	}
	return(listp);
}

/*
 * change_items()
 */
static list_t *
change_items(configmon_t *cmp, time_t time)
{
	database_t *dbp;
	int aflag;
	int i, count;
	void **item;
	db_cmd_t *dbcmd;
	list_t *listp;
	change_item_t *cip, **cipp;

	if (!cmp) {
		return((list_t *)NULL);
	}
	dbp = cmp->database;
	aflag = ALLOCFLG(dbp->flags);

	listp = alloc_list(aflag);

	/* Select all of the cm_event_s records for the time and load
	 * them into the record data array.
	 */
	dbcmd = alloc_dbcmd(dbp);

	sprintf(dbp->sqlbuf, "SELECT * from %s WHERE time=%u ORDER BY type", 
		TABLE_NAME(dbp, CHANGE_ITEM_TBL), time);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_SELECT, CHANGE_ITEM_TBL, dbp->sqlbuf);
	count = select_records(dbcmd);
	free_dbcmd(dbcmd);

	item = (void**)kl_alloc_block(count * sizeof(void *), aflag);

	for (i = 0; i < count; i++) {
		item[i] = next_record(dbp, CHANGE_ITEM_TBL);
	}

	listp->list_type = ITEM_HISTORY_LIST;
	listp->item_type = CHANGE_ITEM_TYPE;
	listp->count = count;
	listp->item = item;
	return(listp);
}

/*
 * get_chage_item()
 */
static cm_hndl_t *
get_chage_item(configmon_t *cmp, change_item_t *cip)
{
	int tid;
	database_t *dbp;
	int count, flag;
	db_cmd_t *dbcmd;
	char *ptr;
	void *item;

	if (!cmp || !cip) {
		return((cm_hndl_t*)NULL);
	}
	dbp = cmp->database;
	flag = ALLOCFLG(dbp->flags);
	
	dbcmd = alloc_dbcmd(dbp);
	tid = cip->tid;
	sprintf(dbp->sqlbuf, "SELECT * FROM %s WHERE sys_id=\"%llx\" AND " 
		"rec_key=%d",
		TABLE_NAME(dbp, tid),
		cip->sys_id,
		cip->rec_key);

	setup_dbcmd(dbcmd, SSDB_REQTYPE_SELECT, tid, dbp->sqlbuf);
	count = select_records(dbcmd);
	free_dbcmd(dbcmd);

	if (count == 0) {

		/* If we didn't get any hits and the table we were doing
		 * the select from was an active table, it might be that the
		 * record we want is no longer there. Just for grins, lets
		 * do a search of the respective archive table for the rec_key
		 * to see if it's there.
		 */
		dbcmd = alloc_dbcmd(dbp);
		switch (tid) {

			case SYSTEM_TBL:
				tid = SYSARCHIVE_TBL;
				break;

			case HWCOMPONENT_TBL:
				tid = HWARCHIVE_TBL;
				break;

			case SWCOMPONENT_TBL:
				tid = SWARCHIVE_TBL;
				break;

			default:
				tid = -1;
				break;
		}
		if (tid >= 0) {
			sprintf(dbp->sqlbuf, "SELECT * FROM %s WHERE sys_id=\"%llx\" AND "
				"rec_key=%d",
				TABLE_NAME(dbp, tid),
				cip->sys_id,
				cip->rec_key);
			setup_dbcmd(dbcmd, SSDB_REQTYPE_SELECT, tid, dbp->sqlbuf);
			count = select_records(dbcmd);
			free_dbcmd(dbcmd);
		}
	}
	if (count != 1) {
		return((cm_hndl_t *)NULL);
		/* XXX -- ??? */
	}

	item = next_record(dbp, tid);
	return(item);
}

/*
 * item_history()
 */
static list_t *
item_history(db_cmd_t *dbcmd, time_t start_time, time_t end_time)
{
	int i, count, first_new_condition;
	database_t *dbp;
	int aflag;
	list_t *listp;
	void *ip = 0;

	if (!dbcmd) {
		return((list_t*)NULL);
	}
	dbp = dbcmd->dbp;
	aflag = ALLOCFLG(dbp->flags);

	/* Make sure that some search conditions have already been set
	 * by the caller (the conditions must gaurantee that only one 
	 * record will be found in the active table).
	 */
	if ((first_new_condition = dbcmd->next_condition) == 0) {
		return((list_t *)NULL);
	}

	/* Note that there should be some way to check for a search 
	 * condition that finds records for only ONE component (hardware 
	 * or software). However, there really isn't any way to determine 
	 * this based on the query, so.....
	 */

	/* We have to have a data_type, or we wont know which field names
	 * to use in the search query...
	 */
	if (dbcmd->data_type == 0) {
		return((list_t *)NULL);
	}

	/* Allocate a buffer to hold the list header
	 */
	listp = alloc_list(aflag);

	/* Since the first search we perform will be on the active table,
	 * we only need to check for an end_time (if one was provided).
	 */
	switch (dbcmd->data_type) {

		case SYSINFO_TYPE:
			dbcmd->tid = SYSTEM_TBL;
			if (end_time) {
				add_condition(dbcmd, SYS_TIME, 
					OP_LESS_OR_EQUAL, (cm_value_t)end_time);
			}
			break;

		case HWCOMPONENT_TYPE:
			dbcmd->tid = HWCOMPONENT_TBL;
			if (end_time) {
				add_condition(dbcmd, HW_INSTALL_TIME, 
					OP_LESS_OR_EQUAL, (cm_value_t)end_time);
			}
			break;

		case SWCOMPONENT_TYPE:
			dbcmd->tid = SWCOMPONENT_TBL;
			if (end_time) {
				add_condition(dbcmd, SW_INSTALL_TIME, 
					OP_LESS_OR_EQUAL, (cm_value_t)end_time);
			}
			break;

		default:
			kl_free_block(listp);
			return((list_t *)NULL);
	}
	
	if (setup_dbselect(dbcmd)) {
		kl_free_block(listp);
		return((list_t *)NULL);
	}

    if (count = db_docmd(dbcmd)) {

		/* There should only be one record in the active table for any
		 * given item. If more than one record is found, then there
		 * was something wrong with the search conditions that were set.
		 */
		if (count > 1) {
			kl_free_block(listp);
			return((list_t *)NULL);
		}

        /* Get the record from the database. Don't do anything with 
		 * it yet. We need to determine how many total item records 
		 * were found so that we can allocate space in the list_s 
		 * struct.
         */
        ip = get_next_record(dbcmd);
    }

	/* We have to free the command string, since we will be building a new
	 * one for the archive table...
	 */
	free_cmdstr(dbcmd);

	switch (dbcmd->data_type) {

		case SYSINFO_TYPE:
			dbcmd->tid = SYSARCHIVE_TBL;
			if (start_time) {
				add_condition(dbcmd, SYS_TIME, OP_GREATER_OR_EQUAL, 
					(cm_value_t)start_time);
			}
			add_condition(dbcmd, SYS_TIME, OP_ORDER_BY, (cm_value_t)0);
			break;

		case HWCOMPONENT_TYPE:
			dbcmd->tid = HWARCHIVE_TBL;
			if (start_time) {
				add_condition(dbcmd, HW_INSTALL_TIME, OP_GREATER_OR_EQUAL, 
					(cm_value_t)start_time);
			}
			add_condition(dbcmd, HW_INSTALL_TIME, OP_ORDER_BY, (cm_value_t)0);
			break;

		case SWCOMPONENT_TYPE:
			dbcmd->tid = SWARCHIVE_TBL;
			if (start_time) {
				add_condition(dbcmd, SW_INSTALL_TIME, OP_GREATER_OR_EQUAL, 
					(cm_value_t)start_time);
			}
			add_condition(dbcmd, SW_INSTALL_TIME, OP_ORDER_BY, (cm_value_t)0);
			break;

		default:
			kl_free_block(listp);
			return((list_t *)NULL);
	}
	
	/* Set up the dbcmd struct
	 */
	if (setup_dbselect(dbcmd)) {
		kl_free_block(listp);
		return((list_t *)NULL);
	}

	/* Execute the command...
	 */
    count = db_docmd(dbcmd);

    /* Bump the record count by one if there was a match from the current
	 * table. Note that it could be that the current record is the ONLY 
	 * match that is found. It could also be that NO records were found 
	 * in the active table...
     */
    if (ip) {
        count++;
    }

    /* if we found at least one record, go ahead and fill in the rest
     * of the information in the list_s struct.
     */
    if (count) {
        listp->item = (void **) kl_alloc_block(count * sizeof(void*), aflag);
        if (ip) {
            listp->item[count - 1] = ip;
            for (i = 0; i < (count - 1); i++) {
            	listp->item[i] = get_next_record(dbcmd);
            }
        }
        else {
            for (i = 0; i < count; i++) {
            	listp->item[i] = get_next_record(dbcmd);
            }
        }
		listp->list_type = CHANGE_HISTORY_LIST;
        listp->item_type = dbcmd->data_type;
        listp->start_time = start_time;
        listp->end_time = end_time;
        listp->count = count;
    }
    else {
        kl_free_block(listp);
        listp = ((list_t *)NULL);
    }
	return (listp);
}

/*
 * compare_items()
 */
static int
compare_items(cm_hndl_t item1, cm_hndl_t item2, int type, int field) 
{
	int data_type, ret_val = 0;
	cm_value_t v1, v2;

	if (!item1 || !item2 || !type) {
		/* XXX -- Set error!?!
		 */
		return(0);
	}

	data_type = cm_field_type(type, field);
	v1 = cm_field(item1, type, field);
	v2 = cm_field(item2, type, field);

	switch(data_type) {

		case VOID_POINTER:
			break;

		case STRING_POINTER:
			if (v1 && v2) {
				/*
				ret_val = strcasecmp((char *)v1, (char *)v2);
				*/
				ret_val = strcmp((char *)v1, (char *)v2);
			}
			break;

		case CHARACTER:
			if ((char)v1 < (char)v2) {
				ret_val = -1;
			}
			else if ((char)v1 > (char)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		case SIGNED_CHAR:
			if ((signed char)v1 < (signed char)v2) {
				ret_val = -1;
			}
			else if ((signed char)v1 > (signed char)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		case UNSIGNED_CHAR:
			if ((unsigned char)v1 < (unsigned char)v2) {
				ret_val = -1;
			}
			else if ((unsigned char)v1 > (unsigned char)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		case SHORT:
			if ((short)v1 < (short)v2) {
				ret_val = -1;
			}
			else if ((short)v1 > (short)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		case UNSIGNED_SHORT:
			if ((unsigned short)v1 < (unsigned short)v2) {
				ret_val = -1;
			}
			else if ((unsigned short)v1 > (unsigned short)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		case SIGNED:
			if ((int)v1 < (int)v2) {
				ret_val = -1;
			}
			else if ((int)v1 > (int)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		case UNSIGNED:
			if ((unsigned int)v1 < (unsigned int)v2) {
				ret_val = -1;
			}
			else if ((unsigned int)v1 > (unsigned int)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		case LONG:
			if ((long long)v1 < (long long)v2) {
				ret_val = -1;
			}
			else if ((long long)v1 > (long long)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		case UNSIGNED_LONG:
			if ((unsigned long long)v1 < (unsigned long long)v2) {
				ret_val = -1;
			}
			else if ((unsigned long long)v1 > (unsigned long long)v2) {
				ret_val = 1;
			}
			else {
				ret_val = 0;
			}
			break;

		default:
			break;
	}
	return(ret_val);
}

/*
 * sort_list()
 */
static int
sort_list(db_cmd_t *dbcmd, list_t *listp)
{
	int count;
	void **list; 
	int i, j, field, type, result;
	cm_hndl_t temp;

	if (!dbcmd || !listp) {
		/* XXX -- set an error!?!
		 */
		return(0);
	}
	count = listp->count;
	list = listp->item;

	/* Check to see if there was an ORDER BY clause in the select statement.
	 */
	for (i = 0; i < dbcmd->next_condition; i++) {
		if (dbcmd->condition[i].operator == OP_ORDER_BY) {
			break;
		}
	}
	if (i == dbcmd->next_condition) {
		/* We don't have to do anything, the list does not need
		 * sorting...
		 */
		return(0);
	}

	type = dbcmd->data_type;
	field = dbcmd->condition[i].field;

	for (i = 0; i < count; i++) {
		for (j = 0; j < (count - i - 1); j++) {
			result = compare_items(list[j], list[j + 1], type, field);
			if (result > 0) {
				temp = list[j];
				list[j] = list[j + 1];
				list[j + 1] = temp;
			}
		}
	}
	return(0);
}

/*
 * select_list()
 */
static list_t *
select_list(db_cmd_t *dbcmd, int flag)
{
	int i, active_count = 0, archive_count = 0;
	database_t *dbp;
	int aflag;
	list_t *listp;
	void **item_list;

	/* Make sure there is an actual dbcmd handle...
	 */
	if (!dbcmd) {
		return((list_t *)NULL);
	}
	dbp = dbcmd->dbp;
	aflag = ALLOCFLG(dbp->flags);

	/* Make sure that some search conditions have already been set
	 * by the caller.
	 */
	if (dbcmd->next_condition == 0) {
		return((list_t *)NULL);
	}

	/* Make sure the flag being passed in is valid for this function
	 */
	if ((flag != ACTIVE_FLG) && (flag != ARCHIVE_FLG) && (flag != ALL_FLG)) {
		return((list_t *)NULL);
	}

	/* We have to have a data_type, or we don't know which table to do
	 * the select on or which field names to use in the search query...
	 * Also, make sure that our data_type is one of the ones we allow 
	 * selects on (SYSINFO_TYPE, HWCOMPONENT_TYPE, and SWCOMPONENT_TYPE).
	 */
	if ((dbcmd->data_type == 0) || ((dbcmd->data_type != SYSINFO_TYPE) && 
				(dbcmd->data_type != HWCOMPONENT_TYPE) && 
				(dbcmd->data_type != SWCOMPONENT_TYPE))) {

		/* XXX -- set error value
		 */
		return((list_t *)NULL);
	}

	/* Allocate space for the list_s struct.
	 */
	listp = alloc_list(aflag);

	/* If the ACTIVE_FLG is set, then do the select on the active table
	 */
	if (flag & ACTIVE_FLG) {
		/* Use the data_type to determine which active table to do the select
		 * on and set the tid in the dbcmd struct.
		 */
		switch (dbcmd->data_type) {

			case SYSINFO_TYPE:
				dbcmd->tid = SYSTEM_TBL;
				break;

			case HWCOMPONENT_TYPE:
				dbcmd->tid = HWCOMPONENT_TBL;
				break;

			case SWCOMPONENT_TYPE:
				dbcmd->tid = SWCOMPONENT_TBL;
				break;
		}
		
		/* Setup for the select...
		 */
		if (setup_dbselect(dbcmd)) {
			kl_free_block(listp);
			return((list_t *)NULL);
		}

		/* Do the select...
		 */
		if (active_count = db_docmd(dbcmd)) {

			/* Allocate a list to hold the records selected. It's possible
			 * that more records will be selected from the respective
			 * archive table if ARCHIVE_FLG is set, so we may need to 
			 * allocate a new, larger list and free this one. We wont know 
			 * until we get there.
			 */
			item_list = (void**)
				kl_alloc_block(active_count * sizeof(cm_hndl_t*), aflag);
			for (i = 0; i < active_count; i++) {
				item_list[i] = get_next_record(dbcmd);
			}
		}
	}

    /* If the ARCHIVE_FLG is set, then do the select on the active table
	 */
	if (flag & ARCHIVE_FLG) {
		/* We have to free the command string, since we will be building a 
		 * new one for the archive table. 
		 */
		free_cmdstr(dbcmd);

		/* Set the tid to the approprate archive table...
		 */
		switch (dbcmd->data_type) {

			case SYSINFO_TYPE:
				dbcmd->tid = SYSARCHIVE_TBL;
				break;

			case HWCOMPONENT_TYPE:
				dbcmd->tid = HWARCHIVE_TBL;
				break;

			case SWCOMPONENT_TYPE:
				dbcmd->tid = SWARCHIVE_TBL;
				break;
		}
		
		/* Setup for the select...
		 */
		if (setup_dbselect(dbcmd)) {
			kl_free_block(listp);
			return((list_t *)NULL);
		}

		/* Do the select on the archive table. If any records were found,
		 * then allocate an item_list to hold them, plus any records that
		 * were found in the active table...each grouping will be sorted.
		 * the two groupings need to be merged into one sorted list. That
		 * will be done below...
		 */
		if (archive_count = db_docmd(dbcmd)) {

			/* Allocate space for all of the records that were found
			 * (archive and active if there were any).
			 */
			listp->item = (void **)
					kl_alloc_block((active_count + archive_count) * 
					sizeof(void *), aflag);
			if (active_count) {
				for (i = 0; i < active_count; i++) {
					listp->item[i] = item_list[i];
				}
				kl_free_block(item_list);
			}
			else {
				i = 0;
			}
			for (; i < (active_count  + archive_count); i++) {
				listp->item[i] = get_next_record(dbcmd);
			}
		} 
	}

	if (active_count && !archive_count) {
		/* Assign the item_list pointer directly to listp->item.
		 */
		listp->item = item_list;
	}

	if (!active_count && !archive_count) {
		/* We didn't find any records...
		 */
		kl_free_block(listp);
		return((list_t *)NULL);
	}

	/* We found some records...fill in the list header details...
	 */
	listp->list_type = ITEM_LIST;
	listp->item_type = dbcmd->data_type;
	listp->start_time = 0;
	listp->end_time = 0;
	listp->count = (active_count + archive_count);

	/* We need to make sure that the list is sorted if there is an 
	 * ORDER BY clause (because there could be two groups of sorted 
	 * records in the list -- one from the active table and one from 
	 * the archive table). We also need to make sure that when 
	 * ordering by string fields that we sort the way WE want to 
	 * and NOT the way ssdb wants us to.
	 */
	sort_list(dbcmd, listp);
	return (listp);
}

/*
 * cm_init()
 */
int
cm_init()
{
	/* Initialize the klib memory allocator. Note that if
	 * kl_init_mempool() is not called, malloc() and free()
	 * will be used when allocating or freeing blocks of
	 * memory. Or, another set of functions can be registered
	 * instead.
	 */
	kl_init_mempool(0, 0, 0);
	return(0);
}

/*
 * cm_free()
 */
void
cm_free()
{
	/* Make a call to kl_free_mempool() to free up all memory allocated
	 * via malloc() and memalign(). This is necessary when the configmon
	 * library is contained in a dso that gets unloaded from memory.
	 */
	kl_free_mempool();
}

/*
 * cm_alloc_config()
 */
cm_hndl_t
cm_alloc_config(char *dbname, int dsoflg)
{
	int flag = K_TEMP;
	configmon_t *cmp;

	if (!(cmp = alloc_configmon(dbname, -1, flag, dsoflg))) {
		return((cm_hndl_t)NULL);
	}
	return(cmp);
}

/*
 * cm_free_config()
 */
void
cm_free_config(cm_hndl_t config)
{
	free_configmon((configmon_t *)config);
}

/*
 * cm_get_sysconfig()
 */
int 
cm_get_sysconfig(cm_hndl_t config, k_uint_t sys_id, time_t time)
{
	configmon_t *cmp = (configmon_t *)config;

	if (setup_configmon(cmp, sys_id)) {
		return(1);
	}

	/* We only need to get the database sysconfig data...
	 */
	return(db_sysconfig(cmp, time));
}

/* 
 * cm_sysinfo()
 */
cm_hndl_t
cm_sysinfo(cm_hndl_t config)
{
	return((cm_hndl_t)((configmon_t *)config)->sysinfo);
}

/*
 * cm_sysinfo_field_type()
 */
static int
cm_sysinfo_field_type(int field)
{
	if ((field >= 0) && (field < SYS_INVALID)) {
		return(sysinfo_member_type[field]);
	}
	return(INVALID_TYPE);
}

/*
 * cm_sysinfo_field()
 */
static cm_value_t
cm_sysinfo_field(cm_hndl_t *sysinfo, int field)
{
	cm_value_t value = 0;

	if (!sysinfo) {
		return((cm_value_t)NULL);
	}

	switch (field) {

		case SYS_REC_KEY:
			value = (cm_value_t)((system_info_t*)sysinfo)->rec_key;
			break;

		case SYS_SYS_ID:
			value = (cm_value_t)((system_info_t*)sysinfo)->sys_id;
			break;

		case SYS_SYS_TYPE:
			value = (cm_value_t)((system_info_t*)sysinfo)->sys_type;
			break;

		case SYS_SERIAL_NUMBER:
			value = (cm_value_t)((system_info_t*)sysinfo)->serial_number;
			break;

		case SYS_HOSTNAME:
			value = (cm_value_t)((system_info_t*)sysinfo)->hostname;
			break;

		case SYS_IP_ADDRESS:
			value = (cm_value_t)((system_info_t*)sysinfo)->ip_address;
			break;

		case SYS_ACTIVE:
			value = (cm_value_t)((system_info_t*)sysinfo)->active;
			break;

		case SYS_LOCAL:
			value = (cm_value_t)((system_info_t*)sysinfo)->local;
			break;

		case SYS_TIME:
			value = (cm_value_t)((system_info_t*)sysinfo)->time;
			break;

		default:
			break;
	}
	return(value);
}

/*
 * cm_first_hwcmp()
 */
cm_hndl_t 
cm_first_hwcmp(cm_hndl_t config)
{ 
	configmon_t *cmp = (configmon_t *)config;
	
	if (!cmp || !cmp->db_sysconfig) {
		return((cm_hndl_t)NULL);
	}
	return(cmp->db_sysconfig->hwcmp_root);
}

/*
 * _first_hwcmp()
 */
static cm_hndl_t 
_first_hwcmp(cm_hndl_t cur_comp)
{
	cm_hndl_t prev_comp, next_comp;

	prev_comp = cur_comp;
	while (next_comp = cm_get_hwcmp(prev_comp, CM_PREV)) {
		prev_comp = next_comp;
	}
	return(prev_comp);
}

/*
 * _last_hwcmp()
 */
static cm_hndl_t 
_last_hwcmp(cm_hndl_t cur_comp)
{
	cm_hndl_t prev_comp, next_comp;

	prev_comp = cur_comp;
	while (next_comp = cm_get_hwcmp(prev_comp, CM_NEXT)) {
		prev_comp = next_comp;
	}
	return(prev_comp);
}

/*
 * cm_get_hwcmp()
 */
cm_hndl_t
cm_get_hwcmp(cm_hndl_t cur_hwcmp, int operation)
{
    cm_hndl_t return_hwcmp = (cm_hndl_t)NULL;

    switch (operation) {
        case CM_FIRST:
            return_hwcmp = _first_hwcmp(cur_hwcmp);
            break;

        case CM_LAST:
            return_hwcmp = _last_hwcmp(cur_hwcmp);
            break;

        case CM_NEXT:
            return_hwcmp = (cm_hndl_t)next_hwcmp((hw_component_t *)cur_hwcmp);
            break;

        case CM_PREV:
            return_hwcmp = (cm_hndl_t)prev_hwcmp((hw_component_t *)cur_hwcmp);
            break;

        case CM_NEXT_PEER: {
		    hw_component_t *hwcmp = (hw_component_t *)cur_hwcmp;

			if ((hwcmp->c_next != hwcmp) &&  hwcmp->c_parent &&
					(hwcmp->c_next != hwcmp->c_parent->c_cmplist)) {
				return_hwcmp = (cm_hndl_t)hwcmp->c_next;
			}
            break;
		}

        case CM_PREV_PEER: {
		    hw_component_t *hwcmp = (hw_component_t *)cur_hwcmp;

			if ((hwcmp->c_prev != hwcmp) && hwcmp->c_parent && 
					(hwcmp != hwcmp->c_parent->c_cmplist)) {
				return_hwcmp = (cm_hndl_t)hwcmp->c_prev;
			}
            break;
		}

        case CM_LEVEL_UP:
            return_hwcmp = 
				(cm_hndl_t)(((hw_component_t *)cur_hwcmp)->c_parent);
            break;

        case CM_LEVEL_DOWN:
            return_hwcmp = 
				(cm_hndl_t)(((hw_component_t *)cur_hwcmp)->c_cmplist);
            break;

        default:
            break;

    }
    return(return_hwcmp);
}

/*
 * cm_hwcmp_field_type()
 */
static int
cm_hwcmp_field_type(int field)
{
	if ((field >= 0) && (field < HW_INVALID)) {
		return(hw_member_type[field]);
	}
	return(INVALID_TYPE);
}

/*
 * cm_hwcmp_field()
 */
static cm_value_t
cm_hwcmp_field(cm_hndl_t *hwcmp, int field)
{
	cm_value_t value = 0;

	switch (field) {

		case HW_SEQ:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_seq;
			break;

		case HW_LEVEL:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_level;
			break;

		case HW_REC_KEY:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_rec_key;
			break;

		case HW_PARENT_KEY:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_parent_key;
			break;

		case HW_SYS_ID:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_sys_id;
			break;

		case HW_CATEGORY:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_category;
			break;

		case HW_TYPE:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_type;
			break;

		case HW_INV_CLASS:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_inv_class;
			break;

		case HW_INV_TYPE:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_inv_type;
			break;

		case HW_INV_CONTROLLER:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_inv_controller;
			break;

		case HW_INV_UNIT:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_inv_unit;
			break;

		case HW_INV_STATE:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_inv_state;
			break;

		case HW_LOCATION:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_location;
			break;

		case HW_NAME:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_name;
			break;

		case HW_SERIAL_NUMBER:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_serial_number;
			break;

		case HW_PART_NUMBER:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_part_number;
			break;

		case HW_REVISION:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_revision;
			break;

		case HW_ENABLED:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_enabled;
			break;

		case HW_INSTALL_TIME:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_install_time;
			break;

		case HW_DEINSTALL_TIME:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_deinstall_time;
			break;

		case HW_DATA_TABLE:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_data_table;
			break;

		case HW_DATA_KEY:
			value = (cm_value_t)((hw_component_t*)hwcmp)->c_data_key;
			break;

		default:
			break;
	}
	return(value);
}

/*
 * cm_first_swcmp()
 */
cm_hndl_t
cm_first_swcmp(cm_hndl_t config)
{
	configmon_t *cmp = (configmon_t *)config;
	sw_component_t *swcp_root;
	
	if (!cmp->db_sysconfig) {
		return((cm_hndl_t)NULL);
	}
	swcp_root = cmp->db_sysconfig->swcmp_root;
	return((cm_hndl_t)first_swcomponent(swcp_root));
}


/*
 * _first_swcmp()
 */
static cm_hndl_t
_first_swcmp(cm_hndl_t cur_swcmp)
{
	cm_hndl_t prev_comp, next_comp;

	prev_comp = cur_swcmp;
	while (next_comp = cm_get_swcmp(prev_comp, CM_PREV)) {
		prev_comp = next_comp;
	}
	return(prev_comp);
}

/*
 * _last_swcmp()
 */
static cm_hndl_t 
_last_swcmp(cm_hndl_t cur_swcmp)
{
	cm_hndl_t prev_comp, next_comp;

	prev_comp = cur_swcmp;
	while (next_comp = cm_get_swcmp(prev_comp, CM_NEXT)) {
		prev_comp = next_comp;
	}
	return(prev_comp);
}

/*
 * cm_get_swcmp()
 */
cm_hndl_t
cm_get_swcmp(cm_hndl_t cur_swcmp, int operation)
{
    cm_hndl_t return_swcmp = (cm_hndl_t)NULL;

    switch (operation) {
        case CM_FIRST:
            return_swcmp = _first_swcmp(cur_swcmp);
            break;

        case CM_LAST:
            return_swcmp = _last_swcmp(cur_swcmp);
            break;

        case CM_NEXT:
            return_swcmp = 
				(cm_hndl_t)next_swcomponent((sw_component_t *)cur_swcmp);
            break;

        case CM_PREV:
            return_swcmp = 
				(cm_hndl_t)prev_swcomponent((sw_component_t *)cur_swcmp);
            break;

		default:
			break;

	}
	return(return_swcmp);
}

/*
 * cm_swcmp_field_type()
 */
static int
cm_swcmp_field_type(int field)
{
	if ((field >= 0) && (field < SW_INVALID)) {
		return(sw_member_type[field]);
	}
	return(INVALID_TYPE);
}

/*
 * cm_swcmp_field()
 */
static cm_value_t
cm_swcmp_field(cm_hndl_t swcmp, int field)
{
	cm_value_t value = 0;

	switch (field) {

		case SW_NAME:
			value = (cm_value_t)((sw_component_t*)swcmp)->sw_name;
			break;

		case SW_SYS_ID:
			value = (cm_value_t)((sw_component_t*)swcmp)->sw_sys_id;
			break;

		case SW_VERSION:
			value = (cm_value_t)((sw_component_t*)swcmp)->sw_version;
			break;

		case SW_DESCRIPTION:
			value = (cm_value_t)((sw_component_t*)swcmp)->sw_description;
			break;

		case SW_INSTALL_TIME:
			value = (cm_value_t)((sw_component_t*)swcmp)->sw_install_time;
			break;

		case SW_DEINSTALL_TIME:
			value = (cm_value_t)((sw_component_t*)swcmp)->sw_deinstall_time;
			break;

		case SW_REC_KEY:
			value = (cm_value_t)((sw_component_t*)swcmp)->sw_key;
			break;

		default:
			break;
	}
	return(value);
}

/*
 * cm_config_event_field_type()
 */
static int
cm_config_event_field_type(int field)
{
	if ((field >= 0) && (field < CE_INVALID)) {
		return(change_event_member_type[field]);
	}
	return(INVALID_TYPE);
}

/*
 * cm_config_event_field()
 */
static cm_value_t
cm_config_event_field(cm_hndl_t *cep, int field)
{
	cm_value_t value = 0;

	switch (field) {

		case CE_SYS_ID:
			value = (cm_value_t)((cm_event_t*)cep)->sys_id;
			break;

		case CE_TIME:
			value = (cm_value_t)((cm_event_t*)cep)->time;
			break;

		case CE_TYPE:
			value = (cm_value_t)((cm_event_t*)cep)->type;
			break;

		default:
			break;
	}
	return(value);
}

/*
 * cm_change_item_type()
 */
int 
cm_change_item_type(cm_hndl_t item)
{
	int type = 0;
	change_item_t *cip = (change_item_t*)item;

	if (!cip) {
		return(0);
	}
	switch(cip->tid) {

		case SYSTEM_TBL:
			type = SYSINFO_TYPE;
			break;

		case HWCOMPONENT_TBL:
		case HWARCHIVE_TBL:
			type = HWCOMPONENT_TYPE;
			break;

		case SWCOMPONENT_TBL:
		case SWARCHIVE_TBL:
			type = SWCOMPONENT_TYPE;
			break;
	}
	return(type);
}

/*
 * cm_change_item_field_type()
 */
static int
cm_change_item_field_type(int field)
{
	if ((field >= 0) && (field < CI_INVALID)) {
		return(change_item_member_type[field]);
	}
	return(INVALID_TYPE);
}

/*
 * cm_change_item_field()
 */
static  cm_value_t
cm_change_item_field(cm_hndl_t *cip, int field)
{
	cm_value_t value = 0;

	if (!cip) {
		return((cm_value_t)NULL);
	}

	switch (field) {

		case CI_SYS_ID:
			value = (cm_value_t)((change_item_t*)cip)->sys_id;
			break;

		case CI_TIME:
			value = (cm_value_t)((change_item_t*)cip)->time;
			break;

		case CI_TYPE:
			value = (cm_value_t)((change_item_t*)cip)->type;
			break;

		case CI_TID:
			value = (cm_value_t)((change_item_t*)cip)->tid;
			break;

		case CI_REC_KEY:
			value = (cm_value_t)((change_item_t*)cip)->rec_key;
			break;

		default:
			break;
	}
	return(value);
}

/*
 * cm_field_type()
 */
int
cm_field_type(int type, int data_tag)
{
    int field_type = 0;

	if (type == 0) {
		return(0);
	}

    switch (type) {

        case CM_EVENT_TYPE:
            field_type = cm_config_event_field_type(data_tag);
            break;

        case CHANGE_ITEM_TYPE:
            field_type = cm_change_item_field_type(data_tag);
            break;

        case SYSINFO_TYPE:
            field_type = cm_sysinfo_field_type(data_tag);
            break;

        case HWCOMPONENT_TYPE:
            field_type = cm_hwcmp_field_type(data_tag);
            break;

        case SWCOMPONENT_TYPE:
            field_type = cm_swcmp_field_type(data_tag);
            break;

        default:
            break;
    }
    return(field_type);
}

/*
 * cm_field()
 */
cm_value_t
cm_field(cm_hndl_t item, int type, int data_tag)
{
    cm_value_t data = 0;

	if (!item) {
		return((cm_value_t)0);
	}

    switch (type) {

        case CM_EVENT_TYPE:
            data = cm_config_event_field(item, data_tag);
            break;

        case CHANGE_ITEM_TYPE:
            data = cm_change_item_field(item, data_tag);
            break;

        case SYSINFO_TYPE:
            data = cm_sysinfo_field(item, data_tag);
            break;

        case HWCOMPONENT_TYPE:
            data = cm_hwcmp_field(item, data_tag);
            break;

        case SWCOMPONENT_TYPE:
            data = cm_swcmp_field(item, data_tag);
            break;

        default:
            break;
    }
    return(data);
}

/**
 ** Functions that return a handle to a list of items
 **/

/* 
 * cm_select_list()
 */
cm_hndl_t
cm_select_list(cm_hndl_t dbcmd, int flag)
{
	cm_hndl_t list;

	if (!dbcmd) {
		return((cm_hndl_t)NULL);
	}
	list = select_list(dbcmd, flag);
	return(list);
}

/*
 * cm_item_history()
 */
cm_hndl_t
cm_item_history(cm_hndl_t dbcmd_h, time_t start_time, time_t end_time)
{
	cm_hndl_t ich;
	db_cmd_t *dbcmd = (db_cmd_t *)dbcmd_h;

	if (!dbcmd) {
		return((cm_hndl_t)NULL);
	}
	return((cm_hndl_t)item_history(dbcmd, start_time, end_time));
}

/*
 * cm_event_history()
 */
cm_hndl_t
cm_event_history(
	cm_hndl_t config, 
	k_uint_t sys_id,
	time_t start_time, 
	time_t end_time, 
	int type)
{
	configmon_t *cmp = (configmon_t *)config;
	list_t *listp;

	if (!cmp) {
		return((cm_hndl_t)NULL);
	}
	listp = event_history(cmp, sys_id, start_time, end_time, type);
	return((cm_hndl_t)listp);
}

/*
 * cm_change_items()
 */
cm_hndl_t 
cm_change_items(cm_hndl_t config, time_t time)
{
	configmon_t *cmp = (configmon_t *)config;
	list_t *listp;

	if (!cmp) {
		return((cm_hndl_t)NULL);
	}
	listp = change_items(cmp, time);
	return((cm_hndl_t)listp);
}

/*
 * cm_free_list()
 */
void
cm_free_list(cm_hndl_t config, cm_hndl_t listp) 
{
	int i;
	database_t *dbp;
	list_t *lp = (list_t *)listp;

	if (!config || !listp) {
		return;
	}
	dbp = ((configmon_t*)config)->database;

	if (lp->item) {
		for (i = 0; i < lp->count; i++) {
			switch(lp->item_type) {
				case CM_EVENT_TYPE:
					free_record_memory(dbp, CM_EVENT_TBL, lp->item[i]);
					break;

				case CHANGE_ITEM_TYPE:
					free_record_memory(dbp, CHANGE_ITEM_TBL, lp->item[i]);
					break;

				case SYSINFO_TYPE:
					free_record_memory(dbp, SYSTEM_TBL, lp->item[i]);
					break;

				case HWCOMPONENT_TYPE:
					free_record_memory(dbp, HWCOMPONENT_TBL, lp->item[i]);
					break;

				case SWCOMPONENT_TYPE:
					free_record_memory(dbp, SWCOMPONENT_TBL, lp->item[i]);
					break;

				default:
					break;
			}
		}
		kl_free_block(lp->item);
	}
	kl_free_block(lp);
}

/** 
 ** List navagation and access routines
 **/

/*
 * cm_list_type()
 */
int
cm_list_type(cm_hndl_t listp)
{
	list_t *lp = (list_t *)listp;

	if (!lp) {
		return(0);
	}

	/* Check the list magic number...
	 */
	if (((list_t*)listp)->list_magic != LIST_MAGIC) {
		return(0);
	}
	return(lp->list_type);
}

/*
 * cm_list_count()
 */
int
cm_list_count(cm_hndl_t listp)
{
	list_t *lp = (list_t *)listp;

	if (!lp) {
		return(0);
	}

	/* Check the list magic number...
	 */
	if (((list_t*)listp)->list_magic != LIST_MAGIC) {
		return(0);
	}
	return(lp->count);
}

/*
 * cm_first_item()
 */
static cm_hndl_t 
cm_first_item(list_t *lp)
{
	cm_hndl_t item = (cm_hndl_t)NULL;

	if (!lp) {
		return((cm_hndl_t)NULL);
	}

	if (lp->count) {
		lp->index = 0;
		item = (cm_hndl_t)lp->item[lp->index];
	}
	return(item);
}

/*
 * cm_last_item()
 */
static cm_hndl_t 
cm_last_item(list_t *lp)
{
	cm_hndl_t item = (cm_hndl_t)NULL;

	if (!lp) {
		return((cm_hndl_t)NULL);
	}

	if (lp->count) {
		lp->index = (lp->count - 1);
		item = (cm_hndl_t)lp->item[lp->index];
	}
	return(item);
}

/*
 * cm_next_item()
 */
static cm_hndl_t 
cm_next_item(list_t *lp)
{
	cm_hndl_t item = (cm_hndl_t)NULL;

	if (!lp) {
		return((cm_hndl_t)NULL);
	}

	if (lp->index < (lp->count - 1)) {
		lp->index++;
		item = (cm_hndl_t)lp->item[lp->index];
	}
	return(item);
}

/*
 * cm_prev_item()
 */
static cm_hndl_t 
cm_prev_item(list_t *lp)
{
	cm_hndl_t item = (cm_hndl_t)NULL;

	if (!lp) {
		return((cm_hndl_t)NULL);
	}

	if (lp->index) {
		lp->index--;
		item = (cm_hndl_t)lp->item[lp->index];
	}
	return(item);
}

/*
 * cm_item()
 */
cm_hndl_t
cm_item(cm_hndl_t listp, int operation)
{
    cm_hndl_t next_item;

	/* Make sure there is a list pointer...
	 */
	if (!listp) {
		return((cm_hndl_t)NULL);
	}

	/* Check the list magic number...
	 */
	if (((list_t*)listp)->list_magic != LIST_MAGIC) {
		return((cm_hndl_t)NULL);
	}

    switch (operation) {

        case CM_FIRST:
            next_item = cm_first_item(listp);
            break;

        case CM_LAST:
            next_item = cm_last_item(listp);
            break;

        case CM_NEXT:
            next_item = cm_next_item(listp);
            break;

        case CM_PREV:
            next_item = cm_prev_item(listp);
            break;

        default:
			next_item = (cm_hndl_t)NULL;
            break;

    }
    return(next_item);
}

/*
 * cm_get_change_item()
 */
cm_hndl_t
cm_get_change_item(cm_hndl_t config, cm_hndl_t ip)
{
	configmon_t *cmp = (configmon_t *)config;
	cm_hndl_t item;
	change_item_t *cip = (change_item_t*)ip;

	if (!cmp || !cip) {
		return((cm_hndl_t)NULL);
	}
	item = get_chage_item(cmp, cip);
	return(item);
}

/*
 * cm_free_item()
 */
void
cm_free_item(cm_hndl_t config, cm_hndl_t item, int item_type)
{
	configmon_t *cmp = (configmon_t *)config;

	if (!cmp || !item) {
		return;
	}
	free_item(cmp, item, item_type);
}

/*
 * cm_alloc_dbcmd()
 */
cm_hndl_t
cm_alloc_dbcmd(cm_hndl_t config, int data_type)
{
	db_cmd_t *dbcmd;
	configmon_t *cmp = (configmon_t *)config;

	if (!cmp) {
		return((cm_hndl_t)NULL);
	}
	dbcmd = alloc_dbcmd(cmp->database);
	dbcmd->data_type = data_type;
	return((cm_hndl_t)dbcmd);
}

/*
 * cm_free_dbcmd()
 */
void
cm_free_dbcmd(cm_hndl_t dbcmd)
{
	if (!dbcmd) {
		return;
	}
	free_dbcmd((db_cmd_t *)dbcmd);
}

/*
 * cm_add_condition()
 */
int
cm_add_condition(cm_hndl_t dbcmd, int field, int operator, cm_value_t value)
{
	if (!dbcmd) {
		return(-1);
	}
	return(add_condition((db_cmd_t *)dbcmd, field, operator, value));
}
