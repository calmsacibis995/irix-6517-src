#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/src/RCS/dbaccess.c,v 1.4 1999/05/27 23:13:43 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/systeminfo.h>
#include <unistd.h>

#include <klib.h>
#include <configmon.h>

int database_debug = 0;

/* Array of table names.
 */
char *table_namelist[] = {
	"configmon",      /*  (0) CONFIGMON_TBL */
	"system_info",    /*  (1) SYSTEM_TBL */
	"sysarchive",     /*  (2) SYSARCHIVE_TBL */
	"cm_event",       /*  (3) CM_EVENT_TBL */
	"change_item",    /*  (4) CHANGE_ITEM_TBL */
	"hwcomponent",    /*  (5) HWCOMPONENT_TBL */
	"hwarchive",      /*  (6) HWARCHIVE_TBL */
	"swcomponent",    /*  (7) SWCOMPONENT_TBL */
	"swarchive",      /*  (8) SWARCHIVE_TBL */
	"cpu",            /*  (9) CPU_TBL */
	"node",           /* (10) NODE_TBL */
	"membank",        /* (11) MEMBANK_TBL */
	"ioa",            /* (12) IOA_TBL */
	"processor"};     /* (13) PROCESSOR_TBL */

typedef struct field_s {
	int	  field_type;
	char *name;
} field_t;

field_t change_event_field[] = {
	HEX_STRING,			"sys_id",			/* CE_SYS_ID */
	DECIMAL_UNSIGNED,   "time",             /* CE_TIME */
	DECIMAL,            "type"              /* CE_TYPE */
};

field_t change_item_field[] = {
	HEX_STRING,			"sys_id",			/* CI_SYS_ID */
	DECIMAL_UNSIGNED,   "time",             /* CI_TIME */
	DECIMAL,            "type",             /* CI_TYPE */
	DECIMAL,            "tid",              /* CI_TID */
	DECIMAL,            "rec_key"           /* CI_REC_KEY */
};

field_t system_field[] = {
	DECIMAL,    		"rec_key",			/* SYS_REC_KEY */
	HEX_STRING, 		"sys_id",			/* SYS_SYS_ID */
	DECIMAL,    		"sys_type",			/* SYS_SYS_TYPE */
	CHAR_STRING,    	"serial_number",	/* SYS_SERIAL_NUMBER */
	CHAR_STRING,    	"hostname",			/* SYS_HOSTNAME */            
	CHAR_STRING,    	"ip_address",		/* SYS_IP_ADDRESS */         
	DECIMAL,     		"active",			/* SYS_ACTIVE */              
	DECIMAL,     		"local",			/* SYS_LOCAL */               
	DECIMAL_UNSIGNED,	"time"				/* SYS_TIME */                
};

field_t swcomponent_field[] = {
	CHAR_STRING,		"name",				/* SW_NAME */
	HEX_STRING,         "sys_id",			/* SW_SYS_ID */
	DECIMAL_UNSIGNED,   "version",			/* SW_VERSION */
	CHAR_STRING,        "description",		/* SW_DESCRIPTION */
	DECIMAL_UNSIGNED,   "install_time",		/* SW_INSTALL_TIME */            
	DECIMAL_UNSIGNED,   "deinstall_time",	/* SW_DEINSTALL_TIME */         
	DECIMAL,			"rec_key"			/* SW_REC_KEY */              
};

field_t hwcomponent_field[] = {
	DECIMAL,            "seq",				/* HW_SEQ */
	DECIMAL,            "level",			/* HW_LEVEL */
	DECIMAL,            "rec_key",			/* HW_REC_KEY */
	DECIMAL,            "parent_key",		/* HW_PARENT_KEY */
	HEX_STRING,         "sys_id",			/* HW_SYS_ID */            
	DECIMAL,            "category",			/* HW_CATEGORY */         
	DECIMAL,            "type",				/* HW_TYPE */              
	DECIMAL,            "inv_class",		/* HW_INV_CLASS */               
	DECIMAL,            "inv_type",			/* HW_INV_TYPE */               
	DECIMAL,            "inv_controller",	/* HW_INV_CONTROLLER */
	DECIMAL,            "inv_unit",			/* HW_INV_UNIT */               
	DECIMAL,            "inv_state",		/* HW_INV_STATE */               
	CHAR_STRING,        "location",			/* HW_LOCATION */                
	CHAR_STRING,        "name",				/* HW_NAME */
	CHAR_STRING,        "serial_number",	/* HW_SERIAL_NUMBER */
	CHAR_STRING,        "part_number",		/* HW_PART_NUMBER */
	CHAR_STRING,        "revision",			/* HW_REVISION */
	DECIMAL,            "enabled",			/* HW_ENABLED */            
	DECIMAL_UNSIGNED,   "install_time",		/* HW_INSTALL_TIME */         
	DECIMAL_UNSIGNED,   "deinstall_time",	/* HW_DEINSTALL_TIME */
	DECIMAL,            "data_table",		/* HW_DATA_TABLE */               
	DECIMAL,            "data_key"			/* HW_DATA_KEY */                
};

char *operator[] = {
    "=",                /*  (0) OP_EQUAL */
	"<",				/*  (1) OP_LESS_THAN */
    ">",                /*  (2) OP_GREATER_THAN */
    "<=",               /*  (3) OP_LESS_OR_EQUAL */
    ">=",               /*  (4) OP_GREATER_OR_EQUAL */
    "!="                /*  (5) OP_NOT_EQUAL */
};

/*
 * open_database()
 */
database_t *
open_database(char *dbname, int bounds, int flags, int dsoflg)
{
	database_t *dbp;
	char *configmon;

	dbp = (database_t *)kl_alloc_block(sizeof(database_t), flags);
	dbp->dbname = (char *)kl_str_to_block(dbname, flags);
	dbp->bounds = bounds;
	dbp->flags = flags;
	dbp->dsoflg = dsoflg;

	/* Initialize SSDB API internal structures
	 */
	if(!dbp->dsoflg && !ssdbInit()){
		printf("SSDB API Error %d: \"%s\"\n",
			ssdbGetLastErrorCode(0), ssdbGetLastErrorString(0));
		close_database(dbp);
		return((database_t *)NULL);
	}

	/* Establish a handle for all SSDB API Errors
	 */
	if ((dbp->ssdb_error = ssdbCreateErrorHandle()) == NULL) {
		printf("SSDB API Error %d: \"%s\" - can't create error handle\n",
			ssdbGetLastErrorCode(0),ssdbGetLastErrorString(0));
		close_database(dbp);
		return((database_t *)NULL);
	}

	/* Establish a new client
	 */
	if ((dbp->ssdb_client = 
				ssdbNewClient(dbp->ssdb_error, NULL, NULL, 0)) == NULL) {
		printf("SSDB API Error %d: \"%s\" - can't create new client handle\n",
			ssdbGetLastErrorCode(dbp->ssdb_error),
			ssdbGetLastErrorString(dbp->ssdb_error));
		close_database(dbp);
		return((database_t *)NULL);
	}

	/* Establish a connection to the database
	 */
	if ((dbp->ssdb_connection = ssdbOpenConnection(dbp->ssdb_error, 
					dbp->ssdb_client, NULL, dbname, 0)) == NULL){
		printf("SSDB API Error %d: \"%s\" - can't open connection\n",
			ssdbGetLastErrorCode(dbp->ssdb_error),
			ssdbGetLastErrorString(dbp->ssdb_error));
		close_database(dbp);
		return((database_t *)NULL);
	}

	/* Allocate space for a SQL command buffer 
	 */
	dbp->sqlbuf = kl_alloc_block(1024, ALLOCFLG(dbp->flags));

	dbp->state = DATABASE_EXISTS;

	/* Now try and initialize the configmon tables. We have to kind
	 * of bootstrap the configmon table, since it contains information
	 * about all the other configmon tables.
	 */
	TABLE_NAME(dbp, CONFIGMON_TBL) = table_name(CONFIGMON_TBL, bounds, flags);
	if (table_exists(dbp, CONFIGMON_TBL)) {
		TABLE_STATE(dbp, CONFIGMON_TBL) = VALID_TBL;
		init_tables(dbp);
	}
	return(dbp);
}

/*
 * close_database()
 */
void
close_database(database_t *dbp)
{
	int i;

	update_configmon_table(dbp);

	/* Free the SSDB request. This is necessary to be memory efficient
	 */
	if (dbp->ssdb_req) {
		ssdbFreeRequest(dbp->ssdb_error, dbp->ssdb_req);
	}

	/* Close the connection 
	 */
	if (dbp->ssdb_connection) {
		if (ssdbIsConnectionValid(dbp->ssdb_error, dbp->ssdb_connection)) {
			if(!ssdbCloseConnection(dbp->ssdb_error, dbp->ssdb_connection)) {
				printf("SSDB API Error %d: \"%s\" - can't close connection\n",
					ssdbGetLastErrorCode(dbp->ssdb_error),
					ssdbGetLastErrorString(dbp->ssdb_error));
			}
		}
	}

	/* Distroy the client handle
	 */
	if (dbp->ssdb_client) {
		if (ssdbIsClientValid(dbp->ssdb_error, dbp->ssdb_client)) {
			if (!ssdbDeleteClient(dbp->ssdb_error, dbp->ssdb_client)) {
				printf("SSDB API Error %d: \"%s\" - can't close client\n",
					ssdbGetLastErrorCode(dbp->ssdb_error),
					ssdbGetLastErrorString(dbp->ssdb_error));
			}
		}
	}

	/* Destroy the error handle
	 */
	if (dbp->ssdb_error) {
		ssdbDeleteErrorHandle(dbp->ssdb_error);
	}

	/* Free up all ssdb internal structures
	 */
	if(!dbp->dsoflg && !ssdbDone()){
		printf("SSDB API Error %d: \"%s\"\n",ssdbGetLastErrorCode(0),
		ssdbGetLastErrorString(0));
		return;
	}
	
	if (dbp->sqlbuf) {
		kl_free_block(dbp->sqlbuf);
	}
	if (dbp->dbname) {
		kl_free_block(dbp->dbname);
	}
	for (i = 0; i < MAX_TABLES; i++) {
		free_table_rec(dbp, i);
	}
	kl_free_block(dbp);
}

/*
 * table_name()
 */
char *
table_name(int tid, int bounds, int flags)
{
	char *name;

	if (bounds != -1) {
		name = (char *)kl_alloc_block(strlen(table_namelist[tid]) + 6, flags);
		sprintf(name, "%s_%d", table_namelist[tid], bounds);
	}
	else {
		name = (char *)kl_alloc_block(strlen(table_namelist[tid]) + 1, flags);
		strcpy(name, table_namelist[tid]);
	}
	return(name);
}

/*
 * table_exists()
 */
int
table_exists(database_t *dbp, int tid)
{
	sprintf(dbp->sqlbuf, "describe %s", TABLE_NAME(dbp, tid));

	if ((dbp->ssdb_req = ssdbSendRequest(dbp->ssdb_error, 
			dbp->ssdb_connection, SSDB_REQTYPE_DESCRIBE, dbp->sqlbuf))){

		return(1);
	}
#ifdef SSDB_DEBUG
	printf("table_exists: SSDB Error %d: \"%s\" - ssdbSendRequest\n",
		ssdbGetLastErrorCode(dbp->ssdb_error),
		ssdbGetLastErrorString(dbp->ssdb_error));
#endif
	return(0);
}

/*
 * table_record_count()
 */
int
table_record_count(database_t *dbp, int tid)
{
	int ret, count = 0, num_records, num_fields;
	db_cmd_t *dbcmd;

	if (TABLE_STATE(dbp, tid) != VALID_TBL) {
		return(-1);
	}
	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "SELECT count(*) FROM %s", TABLE_NAME(dbp, tid));
	setup_dbcmd(dbcmd, SSDB_REQTYPE_SELECT, tid, dbp->sqlbuf);
	ret = db_docmd(dbcmd);
	free_dbcmd(dbcmd);
    if (db_get_field(dbcmd, &count, sizeof(count))) {
		printf("SSDB Error %d: \"%s\" - ssdbSendRequest\n",
			ssdbGetLastErrorCode(dbcmd->error),
			ssdbGetLastErrorString(dbcmd->error));
    }
	return(count);
}

/* 
 * setup_table()
 */
int
setup_table(database_t *dbp, int tid, int first_key)
{
	int bounds = dbp->bounds;

	if (tid >= MAX_TABLES) {
		return(1);
	}

	/* If the table is already setup, then just return
	 */
	if (TABLE_STATE(dbp, tid) == VALID_TBL) {
		return(0);
	}

	/* Allocate the table name (if one does not already exist) and 
	 * check to see if the table exists in the database.
	 */
	if (!TABLE_NAME(dbp, tid)) {
		TABLE_NAME(dbp, tid) = table_name(tid, bounds, dbp->flags);
	}
	if (table_exists(dbp, tid)) {

		/* Update the next_key value in the table control record
		 */
		dbp->table_array[tid].next_key = next_table_key(dbp, tid);	

		/* We need to see if there is a record for tid in the configmon
		 * table (if it exists). If there is, then all we need to do is
		 * update the next_key. If there is a configmon table and there
		 * is not an entry for tid, we need to insert one now. If there
		 * is no configmon table (tid IS CONFIGMON_TBL?), then we can do 
		 * nothing more.
		 */
		if (!(tid == CONFIGMON_TBL) && table_exists(dbp, CONFIGMON_TBL)) {
			db_cmd_t *dbcmd;
			char *ptr;

			dbcmd = alloc_dbcmd(dbp);
			sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(%d,",
				TABLE_NAME(dbp, CONFIGMON_TBL), tid);
			ptr = &dbp->sqlbuf[strlen(dbp->sqlbuf)];
			ptr = add_str(ptr, TABLE_NAME(dbp, tid));
			ptr = add_comma(ptr);
			sprintf(ptr, "%d)", dbp->table_array[tid].next_key);
			setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, tid, dbp->sqlbuf);
			db_docmd(dbcmd);
			free_dbcmd(dbcmd);
		}
	}
	else {
		if (create_table(dbp, tid, first_key)) {
			kl_free_block(dbp->table_array[tid].name);
			dbp->table_array[tid].name = (char *)NULL;
			return(1);
		}
		dbp->table_array[tid].next_key = first_key;
	}
	dbp->table_array[tid].state = VALID_TBL;
	dbp->valid_tables++;
	return(0);
}

/*
 * setup_tables()
 */
int
setup_tables(database_t *dbp, int sys_type, int bounds)
{
	setup_table(dbp, CONFIGMON_TBL, 0);
	setup_table(dbp, SYSTEM_TBL, 1);
	setup_table(dbp, SYSARCHIVE_TBL, 0);
	setup_table(dbp, CM_EVENT_TBL, 0);
	setup_table(dbp, CHANGE_ITEM_TBL, 1);
	setup_table(dbp, HWCOMPONENT_TBL, 1);
	setup_table(dbp, HWARCHIVE_TBL, 0);
	setup_table(dbp, SWCOMPONENT_TBL, 1);
	setup_table(dbp, SWARCHIVE_TBL, 0);

	switch (sys_type) {
		
		case 19 :
			setup_table(dbp, CPU_TBL, 1);
			setup_table(dbp, MEMBANK_TBL, 1);
			setup_table(dbp, IOA_TBL, 1);
			break;

		case 27 :
			setup_table(dbp, CPU_TBL, 1);
			setup_table(dbp, NODE_TBL, 1);
			setup_table(dbp, MEMBANK_TBL, 1);
			break;
	}
	return(0);
}

/*
 * db_create_table()
 */
int
db_create_table(database_t *dbp, int tid)
{
	char *table_name;
	char sqlbuffer[1024];

	table_name = TABLE_NAME(dbp, tid);
	switch (tid) {
		case CONFIGMON_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"table_id int,"
				"table_name varchar(255) not null,"
				"next_key int)", table_name);
			break;

		case SYSTEM_TBL:
		case SYSARCHIVE_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"rec_key int,"
				"sys_id char(16) not null,"
				"sys_type int,"
				"serial_number varchar(255) not null,"
				"hostname varchar(255) not null,"
				"ip_address varchar(255) not null,"
				"active smallint not null,"
				"local smallint null,"
				"time int unsigned)", table_name);
			break;

		case CM_EVENT_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"sys_id char(16) not null,"
				"time int,"
				"type int)", table_name);
			break;

		case CHANGE_ITEM_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"sys_id char(16) not null,"
				"time int unsigned,"
				"type int,"
				"table_id int,"
				"rec_key int)", table_name);
			break;

		case HWCOMPONENT_TBL:
		case HWARCHIVE_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"seq int,"
				"level int,"
				"rec_key int,"
				"parent_key int,"
				"sys_id char(16) not null,"
				"category int,"
				"type int,"
				"inv_class int,"
				"inv_type int,"
				"inv_controller int,"
				"inv_unit int,"
				"inv_state int,"
				"location varchar(255) null,"
				"name varchar(255) null,"
				"serial_number varchar(255) null,"
				"part_number varchar(255) null,"
				"revision varchar(255) null,"
				"enabled int,"
				"install_time int,"
				"deinstall_time int,"
				"data_table int,"
				"data_key int)", table_name);
			break;

		case SWCOMPONENT_TBL:
		case SWARCHIVE_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"rec_key int not null,"
				"sys_id char(16) not null,"
				"name varchar(255) not null,"
				"version int unsigned,"
				"install_time int unsigned,"
				"deinstall_time int unsigned,"
				"description varchar(255) null)", table_name);
			break;

		case CPU_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"rec_key int unsigned not null,"
				"sys_id char(16) not null,"
				"comp_rec_key int unsigned not null,"
				"module int unsigned not null,"
				"slot int unsigned not null,"
				"slice int unsigned,"
				"id int unsigned not null,"
				"speed int unsigned not null,"
				"flavor int unsigned not null,"
				"cachesz int unsigned not null,"
				"cachefreq int unsigned not null,"
				"promrev int unsigned,"
				"enabled int unsigned)", table_name);
			break;

		case NODE_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"rec_key int unsigned not null,"
				"sys_id char(16) not null,"
				"comp_rec_key int unsigned not null,"
				"cpu_count int unsigned not null,"
				"memory int not null)", table_name);
			break;

		case MEMBANK_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"rec_key int unsigned not null,"
				"sys_id char(16) not null,"
				"comp_rec_key int unsigned not null,"
				"attr int unsigned not null,"
				"flag int unsigned not null,"
				"bnk_number int unsigned not null,"
				"size int unsigned not null,"
				"enable int unsigned not null)", table_name);
			break;

		case IOA_TBL:
			sprintf(sqlbuffer, "CREATE TABLE %s("
				"rec_key int unsigned not null,"
				"sys_id char(16) not null,"
				"comp_rec_key int unsigned not null,"
				"enable int unsigned not null,"
				"type int unsigned not null,"
				"virtid int unsigned not null,"
				"subtype int unsigned not null)", table_name);
			break;

		case PROCESSOR_TBL:
			break;

		default:
			return(1);
	}
	if (!(dbp->ssdb_req = ssdbSendRequest(dbp->ssdb_error, 
			dbp->ssdb_connection, SSDB_REQTYPE_CREATE, sqlbuffer))){

		printf("SSDB Error %d: \"%s\" - ssdbSendRequest\n",
			ssdbGetLastErrorCode(dbp->ssdb_error),
			ssdbGetLastErrorString(dbp->ssdb_error));
		return(1);
	}
	return(0);
}

/* 
 * create_table()
 */
int
create_table(database_t *dbp, int tid, int key)
{
	db_cmd_t *dbcmd;
	char *ptr;

	if (table_exists(dbp, tid)) {
		return(1);
	}
	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(%d,", 
		TABLE_NAME(dbp, CONFIGMON_TBL), tid);
	ptr = &dbp->sqlbuf[strlen(dbp->sqlbuf)];
	ptr = add_str(ptr, TABLE_NAME(dbp, tid));
	ptr = add_comma(ptr);
	sprintf(ptr, "%d)", key);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, tid, dbp->sqlbuf);
	db_create_table(dbp, tid);
	db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	return(0);
}

/*
 * next_table_key()
 */
int
next_table_key(database_t *dbp, int tid)
{
	int ret, table_id, next_key = 0, ret_val;
	char table_name[255];
	configmon_rec_t *cmp;

	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "SELECT * FROM %s WHERE tid=%d", 
		TABLE_NAME(dbp, CONFIGMON_TBL), tid);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_SELECT, CONFIGMON_TBL, dbp->sqlbuf);
	db_docmd(dbcmd);
	if (cmp = get_configmon_data(dbcmd)) {
		next_key = cmp->next_key;
		kl_free_block(cmp);
	}
	else {
		/* If the configmon table does not exist, or there isn't an
		 * entry for tid, then we have to determine the highest record
		 * key that currently exists in the table (plus its archive table
		 * if one exists) and return one more than that.
		 *
		 * Note that this should only happen when the first time the
		 * configmon tables are initialized. In that case, the system
		 * table will have already been created earlier on in the init
		 * process.
		 */
		 switch(tid) {
			case SYSTEM_TBL:
				/* XXX -- for now, we'll just return 2 since the record
				 * that already exists should contain 1. We REALLY should
				 * build a query and select all system (and archived system)
				 * records and determine the next key that way.
				 */
				next_key = 2;
				break;

			default:
				break;
		 }
	}
	free_dbcmd(dbcmd);
	return(next_key);
}

/* 
 * init_tables()
 */
void
init_tables(database_t *dbp)
{
	int i, tid, next_key, ret_val;
	char table_name[255];
	configmon_rec_t *cmp;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
  	sprintf(dbp->sqlbuf, "SELECT * from %s", TABLE_NAME(dbp, CONFIGMON_TBL));
	setup_dbcmd(dbcmd, SSDB_REQTYPE_SELECT, CONFIGMON_TBL, dbp->sqlbuf);
	select_records(dbcmd);
	free_dbcmd(dbcmd);
	while (cmp = next_record(dbp, CONFIGMON_TBL)) {
		if (!dbp->table_array[cmp->table_id].name) {
			dbp->table_array[cmp->table_id].name = 
				(char *)kl_str_to_block(cmp->table_name, dbp->flags);
		}
		dbp->table_array[cmp->table_id].state = VALID_TBL;
		dbp->table_array[cmp->table_id].next_key = cmp->next_key;
		dbp->valid_tables++;
		free_configmon_rec(cmp);
	}
	if (dbp->valid_tables) {
		dbp->state |= TABLES_EXIST;
	}
}

/*
 * update_configmon_table()
 */
void
update_configmon_table(database_t *dbp)
{
	db_cmd_t *dbcmd;
	int i;

	dbcmd = alloc_dbcmd(dbp);

	for (i = 0; i < MAX_TABLES; i++) {
		if (TABLE_STATE(dbp, i) == VALID_TBL) {
			sprintf(dbp->sqlbuf, "UPDATE %s set next_key=%d WHERE table_id=%d",
				TABLE_NAME(dbp, CONFIGMON_TBL), 
				dbp->table_array[i].next_key, i);
			setup_dbcmd(dbcmd, SSDB_REQTYPE_UPDATE, i, dbp->sqlbuf);
			db_docmd(dbcmd);
			clean_dbcmd(dbcmd);
		}
	}
	free_dbcmd(dbcmd);
}

/*
 * clean_table_rec()
 */
void
clean_table_rec(database_t *dbp, int tid)
{
	int i;

	if (TABLE_STATE(dbp, tid) != VALID_TBL) {
		return;
	}

	for (i = 0; i < RECORD_COUNT(dbp, tid); i++) {
		if (DATA_ARRAY(dbp, tid)[i]) {

			switch (tid) {

				case SYSTEM_TBL: {

					system_info_t *sip = DATA_ARRAY(dbp, tid)[i];

					free_system_info(sip);
					break;
				}

				case HWCOMPONENT_TBL : {

					hw_component_t *cur = HWCOMPONENT_ARRAY(dbp)[i];

					if (cur->c_location) {
						kl_free_block(cur->c_location);
					}
					if (cur->c_name) {
						kl_free_block(cur->c_name);
					}
					if (cur->c_serial_number) {
						kl_free_block(cur->c_serial_number);
					}
					if (cur->c_part_number) {
						kl_free_block(cur->c_part_number);
					}
					if (cur->c_revision) {
						kl_free_block(cur->c_revision);
					}
					kl_free_block(cur);
					break;
				}

				case SWCOMPONENT_TBL : {
					sw_component_t *cur = SWCOMPONENT_ARRAY(dbp)[i];

					free_swcomponent(cur);
					break;
				}

				default :
					kl_free_block(DATA_ARRAY(dbp, tid)[i]);
					break;
			}
			DATA_ARRAY(dbp, tid)[i] = (void *)NULL;
		}
	}
	if (DATA_PTR(dbp, tid)) {
		kl_free_block(DATA_PTR(dbp, tid));
		DATA_PTR(dbp, tid) = (void **)NULL;
	}
	RECORD_COUNT(dbp, tid) = 0;
	NEXT_RECORD(dbp, tid) = 0;
}

/*
 * free_table_rec()
 */
void
free_table_rec(database_t *dbp, int tid)
{
	void *ptr;
	int i;

	if (dbp->table_array[tid].name) {
		kl_free_block(dbp->table_array[tid].name);
	}
	clean_table_rec(dbp, tid);
}

/*
 * alloc_dbcmd()
 */
db_cmd_t *
alloc_dbcmd(database_t *dbp)
{
	int i;
	db_cmd_t *dbcmd;

	dbcmd = (db_cmd_t *)kl_alloc_block(sizeof(db_cmd_t), ALLOCFLG(dbp->flags));
	dbcmd->dbp = dbp;

	/* Initialize the 'field' field in the condition array to an invalid
	 * value (-1);
	 */
	for (i = 0; i < MAX_CONDITIONS; i++) {
		dbcmd->condition[i].field = -1;
	}

	/* Setup for SQL query
	 */
	dbcmd->client = dbp->ssdb_client;
	dbcmd->connection = dbp->ssdb_connection;
	dbcmd->error = ssdbCreateErrorHandle();
	return(dbcmd);
}

/*
 * setup_dbcmd()
 */
void
setup_dbcmd(db_cmd_t *dbcmd, int type, int tid, char *cmdstr)
{
	dbcmd->command_type = type;
	dbcmd->tid = tid;
	if (cmdstr) {
		dbcmd->cmdstr = 
			(char *)kl_str_to_block(cmdstr, ALLOCFLG(dbcmd->dbp->flags));
	}
}

/*
 * free_cmdstr()
 */
void
free_cmdstr(db_cmd_t *dbcmd)
{
	if (dbcmd->cmdstr) {
		kl_free_block(dbcmd->cmdstr);
		dbcmd->cmdstr = (char *)NULL;
	}
}

/*
 * clean_dbcmd()
 */
void
clean_dbcmd(db_cmd_t *dbcmd)
{
	int i;

	if (dbcmd->cmdstr) {
		kl_free_block(dbcmd->cmdstr);
		dbcmd->cmdstr = (char *)NULL;
	}
	if (dbcmd->request) {
		ssdbFreeRequest(dbcmd->error, dbcmd->request);
		dbcmd->request = (ssdb_Request_Handle)NULL;
	}
	dbcmd->command_type = 0;
	dbcmd->tid = -1;
	dbcmd->data_type = 0;
	dbcmd->db_flags = 0;
	dbcmd->db_sys_id = 0;
	dbcmd->db_key = 0;
	dbcmd->db_time = 0;

	for (i = 0; i < dbcmd->next_condition; i++) {
		dbcmd->condition[i].field = -1;
		dbcmd->condition[i].operator = 0;
		dbcmd->condition[i].value = 0;
	}
	dbcmd->next_condition = 0;
}

/*
 * free_dbcmd()
 */
void
free_dbcmd(db_cmd_t *dbcmd)
{
	clean_dbcmd(dbcmd);

	/* Destroy the error handle
	 */
	if (dbcmd->error) {
		ssdbDeleteErrorHandle(dbcmd->error);
	}
	kl_free_block(dbcmd);
}

/*
 * setup_select_str()
 */
int
setup_select_str(db_cmd_t *dbcmd)
{
	database_t *dbp = dbcmd->dbp;
	int tid = dbcmd->tid;
	char sqlstr[1024];
	char *ptr;

	sprintf(sqlstr, "SELECT * FROM %s", TABLE_NAME(dbp, tid));

	if ((tid == SYSTEM_TBL) && (dbcmd->db_flags & LOCALHOST)) {
		strcat(sqlstr, " WHERE local != 0");
	}
	else {
		ptr = &sqlstr[strlen(sqlstr)];
		if (dbcmd->db_sys_id || dbcmd->db_key || dbcmd->db_time) {
			strcat(ptr, " WHERE");
			ptr = &ptr[strlen(ptr)];
			if (dbcmd->db_sys_id) {
				sprintf(ptr, " sys_id = \"%llx\"", dbcmd->db_sys_id);
				ptr = &ptr[strlen(ptr)];
			}
			if (dbcmd->db_key) {
				if (dbcmd->db_sys_id) {
					strcat(ptr, " AND");
					ptr = &ptr[strlen(ptr)];
				}
				if (dbcmd->db_flags & PARENT_KEY) {
					sprintf(ptr, " parent_key = %d", dbcmd->db_key);
					ptr = &ptr[strlen(ptr)];
				}
				else {
					sprintf(ptr, " rec_key = %d", dbcmd->db_key);
					ptr = &ptr[strlen(ptr)];
				}
			}
			if (dbcmd->db_time) {
				if (dbcmd->db_sys_id || dbcmd->db_key) {
					strcat(ptr, " AND");
					ptr = &ptr[strlen(ptr)];
				}
				if (dbcmd->db_flags & TIME) {
					sprintf(ptr, " time = %d", dbcmd->db_time);
				}
				else if (dbcmd->db_flags & DEINSTALL_TIME) {
					sprintf(ptr, " deinstall_time = %d", dbcmd->db_time);
				}
				else {
					sprintf(ptr, " install_time = %d", dbcmd->db_time);
				}
				ptr = &ptr[strlen(ptr)];
			}
		}
		if (tid == HWCOMPONENT_TBL) {
			strcat(ptr, " ORDER BY seq");
		}
		else if (tid == SWCOMPONENT_TBL) {
			strcat(ptr, " ORDER BY name");
		}
	}
	dbcmd->cmdstr = 
		(char *)kl_str_to_block(sqlstr, ALLOCFLG(dbcmd->dbp->flags));
	return(0);
}

/*
 * add_condition()
 */
int
add_condition(db_cmd_t *dbcmd, int field, int operator, cm_value_t value)
{
	int i, c;

#ifdef CM_DEBUG
	if (database_debug > 1) {
		fprintf(stderr, "dbcmd=0x%x, field=%d, operator=%d, value=0x%llx\n",
			dbcmd, field, operator, value);
	}
#endif

	/* Make sure we have valid data...
	 */
	if ((c = dbcmd->next_condition) > LAST_CONDITION) {
		return(1);
	}
	switch(dbcmd->data_type) {
		case SYSINFO_TYPE:
			if ((field < 0) || (field >= SYS_INVALID)) {
				return(1);
			}
			break;

		case HWCOMPONENT_TYPE:
			if ((field < 0) || (field >= HW_INVALID)) {
				return(1);
			}
			break;

		case SWCOMPONENT_TYPE:
			if ((field < 0) || (field >= SW_INVALID)) {
				return(1);
			}
			break;

		default:
			return(1);
	}

	/* If the operator is OP_ORDER_BY then we have to make sure that
	 * there isn't an OP_ORDER_BY already set (having two would result
	 * in an invalid query string and could cause a core dump).
	 */
	if (operator == OP_ORDER_BY) {
		for (i = 0; i < dbcmd->next_condition; i++) {
			if (dbcmd->condition[c].operator == OP_ORDER_BY) {
				return(1);
			}
		}
	}

	/* Go ahead and add the condition...
	 */
	dbcmd->next_condition++;
	dbcmd->condition[c].field = field;
	dbcmd->condition[c].operator = operator;
	dbcmd->condition[c].value = value;
	return(0);
}

/*
 * setup_dbselect()
 */
int
setup_dbselect(db_cmd_t *dbcmd) 
{
	int i = 0, condition_cnt = 0, orderby_idx = -1;
	int field, op, data_type, field_type;
	cm_value_t v;
	database_t *dbp = dbcmd->dbp;
	int aflag = ALLOCFLG(dbp->flags);
	char *ptr;

	/* Set the SSDB command type, just to make sure it is correct.
	 */
	dbcmd->command_type = SSDB_REQTYPE_SELECT;

	/* Check to see if there is a OP_ORDER_BY operator set. If there 
	 * is, we need to make sure it is handled last (even if it isn't
	 * the last condition that was set). Also, make sure that only
	 * ONE such operator has been set (this should never happen).
	 */
	for (i = 0; i < dbcmd->next_condition; i++) {
		if (dbcmd->condition[i].operator == OP_ORDER_BY) {
			if (orderby_idx != -1) {
				return(1);
			}
			orderby_idx = i;
		}
	}

	sprintf(dbp->sqlbuf, "SELECT * FROM %s", TABLE_NAME(dbp, dbcmd->tid));
	ptr = NEXTPTR(dbp->sqlbuf);

	/* Check to see if there are any conditions that need to be included
	 * in the SELECT statement...
	 */
	if (dbcmd->next_condition) {

		/* Add the WHERE clause if there are any conditions BESIDES
		 * the OP_ORDER_BY condition, and then add the conditions.
		 */
		if ((orderby_idx == -1) || (dbcmd->next_condition > 1)) {

			sprintf(ptr, " WHERE ");
			ptr += 7;

			for (i = 0; i < dbcmd->next_condition; i++) {

				/* Just in case, make sure we actually have a condition to
				 * set...
				 */
				if ((field = dbcmd->condition[i].field) == -1) {
					return(1);
				}

				/* Get the operator and the value
				 */
				op = dbcmd->condition[i].operator;
				v = dbcmd->condition[i].value;

				/* If the current operator is the OP_ORDER_BY operator, then
				 * skip it for now...it has to go at the end of the select
				 * statement.
				 */
				if (op == OP_ORDER_BY) {
					continue;
				}

				/* If there is already one or more condtions, then add the 
				 * AND clause.
				 */
				if (condition_cnt) {
					sprintf(ptr, " AND ");
					ptr += 5;
				}
				condition_cnt++;

				switch (dbcmd->tid) {

					case SYSTEM_TBL:
					case SYSARCHIVE_TBL:
						sprintf(ptr, "%s%s", 
							system_field[field].name, operator[op]);
						data_type = cm_field_type(SYSINFO_TYPE, field);
						field_type = system_field[field].field_type;
						break;

					case HWCOMPONENT_TBL:
					case HWARCHIVE_TBL:
						sprintf(ptr, "%s%s", 
							hwcomponent_field[field].name, operator[op]);
						data_type = cm_field_type(HWCOMPONENT_TYPE, field);
						field_type = hwcomponent_field[field].field_type;
						break;

					case SWCOMPONENT_TBL:
					case SWARCHIVE_TBL:
						sprintf(ptr, "%s%s", 
							swcomponent_field[field].name, operator[op]);
						data_type = cm_field_type(SWCOMPONENT_TYPE, field);
						field_type = swcomponent_field[field].field_type;
						break;
				}
				ptr = NEXTPTR(ptr);

				switch (field_type) {

					case CHAR_STRING:
						if (data_type == STRING_POINTER) {
							sprintf(ptr, "\"%s\"", (char *)v);
						}
						else {
							sprintf(ptr, "\"<%llu>\"", (cm_value_t)v);
						}
						break;

					case HEX_STRING:
						if ((data_type == LONG) ||
									(data_type == UNSIGNED_LONG)) {
							sprintf(ptr, "\"%llx\"", 
								(unsigned long long)v);
						}
						else if ((data_type == SIGNED) || 
									(data_type == UNSIGNED)) {
							sprintf(ptr, "\"%x\"", (unsigned)v);
						}
						else if ((data_type == SHORT) || 
									(data_type == UNSIGNED_SHORT)) {
							sprintf(ptr, "\"%x\"", (unsigned)v);
						}
						break;

					case DECIMAL:
					case DECIMAL_UNSIGNED:

						switch (data_type) {

							case VOID_POINTER:
								break;

							case STRING_POINTER:
								sprintf(ptr, "%s", (char *)v);
								break;

							case CHARACTER:
								sprintf(ptr, "%c", (char)v);
								break;

							case SIGNED_CHAR:
								sprintf(ptr, "%d", (signed char)v);
								break;

							case UNSIGNED_CHAR:
								sprintf(ptr, "%u", (unsigned char)v);
								break;

							case SHORT:
								sprintf(ptr, "%hd", (short)v);
								break;

							case UNSIGNED_SHORT:
								sprintf(ptr, "%hu", (unsigned short)v);
								break;

							case SIGNED:
								sprintf(ptr, "%d", (int)v);
								break;

							case UNSIGNED:
								sprintf(ptr, "%u", (unsigned)v);
								break;

							case LONG:
								sprintf(ptr, "%lld", (long long)v);
								break;

							case UNSIGNED_LONG:
								sprintf(ptr, "%llu", (unsigned long long)v);
								break;

							default:
								break;
						}
				}
				ptr = NEXTPTR(ptr);
			}
		}
	}
	
	if (orderby_idx != -1) {
		field = dbcmd->condition[orderby_idx].field;
		switch (dbcmd->tid) {

			case SYSTEM_TBL:
			case SYSARCHIVE_TBL:
				sprintf(ptr, " ORDER BY %s", system_field[field].name);
				break;

			case HWCOMPONENT_TBL:
			case HWARCHIVE_TBL:
				sprintf(ptr, " ORDER BY %s", hwcomponent_field[field].name);
				break;

			case SWCOMPONENT_TBL:
			case SWARCHIVE_TBL:
				sprintf(ptr, " ORDER BY %s", swcomponent_field[field].name);
				break;
		}
	}

	dbcmd->cmdstr = (char *)kl_str_to_block(dbp->sqlbuf, aflag);
#ifdef CM_DEBUG
    if (database_debug >= 1) {
		fprintf(stderr, "cmdstr=\"%s\"\n", dbcmd->cmdstr);
	}
#endif
	return(0);
}

/*
 * setup_dbquery()
 */
int
setup_dbquery(
	db_cmd_t *dbcmd, 
	int tid,
	k_uint_t sys_id, 
	int flags, 
	int key, 
	time_t time)
{
	int flg = ALLOCFLG(dbcmd->dbp->flags);

	/* Set the table ID
	 */
	dbcmd->tid = tid;

	/* Set flag values ONLY for non-default behaviors
	 */
	if (((dbcmd->db_sys_id = sys_id) == 0) && (flags & LOCALHOST)) {
		dbcmd->db_flags |= LOCALHOST;
	}
	if ((dbcmd->db_key = key) && (flags & PARENT_KEY)) {
		dbcmd->db_flags |= PARENT_KEY;
	}
	if (dbcmd->db_time = time) {
		if (flags & TIME) {
			dbcmd->db_flags |= TIME;
		}
		else if (flags & DEINSTALL_TIME) {
			dbcmd->db_flags |= DEINSTALL_TIME;
		}
	}
	dbcmd->command_type = SSDB_REQTYPE_SELECT;
	setup_select_str(dbcmd);
	return(0);
}


/* 
 * lock_tables()
 */
void
lock_tables(database_t *dbp)
{
	int i, table_cnt = 0;
	char qstr[1024];
	char *ptr;

	qstr[0] = 0;
	strcat(qstr, "LOCK TABLES ");
	for (i = 0; i < MAX_TABLES; i++) {
		if (table_exists(dbp, i)) {
			ptr = &qstr[strlen(qstr)];
			if (table_cnt) {
				sprintf(ptr, ", %s WRITE", TABLE_NAME(dbp, i));
			}
			else {
				sprintf(ptr, "%s WRITE", TABLE_NAME(dbp, i));
			}
			table_cnt++;
		}
	}
	if (qstr[0]) {
		if (!(dbp->ssdb_req = ssdbSendRequest(dbp->ssdb_error,
				 dbp->ssdb_connection, SSDB_REQTYPE_LOCK, qstr))){
#ifdef SSDB_DEBUG
			printf("lock_tables: SSDB Error %d: \"%s\" - ssdbSendRequest\n",
				ssdbGetLastErrorCode(dbp->ssdb_error),
				ssdbGetLastErrorString(dbp->ssdb_error));
#endif
		}
	}
}

/* 
 * unlock_tables()
 */
void
unlock_tables(database_t *dbp)
{
	ssdbUnLockTable(dbp->ssdb_error, dbp->ssdb_connection);
}

/*
 * db_docmd()
 */
int
db_docmd(db_cmd_t *dbcmd)
{
	int num_records = 0;
	int num_fields = 0;
	database_t *dbp = dbcmd->dbp;

	if (!(dbcmd->request = ssdbSendRequest(dbcmd->error, 
				dbcmd->connection, dbcmd->command_type, dbcmd->cmdstr))){

#ifdef SSDB_DEBUG
		printf("db_docmd: SSDB Error %d: \"%s\" - ssdbSendRequest\n",
			ssdbGetLastErrorCode(dbp->ssdb_error),
			ssdbGetLastErrorString(dbp->ssdb_error));
#endif
		return(-1);
	}

    /* Get the number of records returned from the query
	 */
	num_records = ssdbGetNumRecords(dbcmd->error, dbcmd->request);
	if(ssdbGetLastErrorCode(dbcmd->error) != SSDBERR_SUCCESS) {
#ifdef CM_DEBUG
		printf("db_docmd: SSDB Error %d: \"%s\" ssdbGetNumRecords\n",
			ssdbGetLastErrorCode(dbcmd->error),
			ssdbGetLastErrorString(dbcmd->error));
#endif
		return(-1);
	}

	/* If this was a select, make sure to get the number of collumns
	 * and update the table record
	 */
	if (dbcmd->command_type == SSDB_REQTYPE_SELECT) {
		/* Get the number of columns (fields) returned from the query
		 */
		num_fields = ssdbGetNumColumns(dbcmd->error, dbcmd->request);
		if(ssdbGetLastErrorCode(dbcmd->error) != SSDBERR_SUCCESS) {
#ifdef CM_DEBUG
			printf("db_docmd: SSDB Error %d: \"%s\" ssdbGetNumColumns\n",
				ssdbGetLastErrorCode(dbcmd->error),
				ssdbGetLastErrorString(dbcmd->error));
#endif
			return(-1);
		}
		NUM_RECORDS(dbcmd->dbp, dbcmd->tid) = num_records;
		NUM_FIELDS(dbcmd->dbp, dbcmd->tid) = num_fields;
	}
	return(num_records);
}

/*
 * db_select_records()
 */
int
db_select_records(db_cmd_t *dbcmd)
{
	database_t *dbp = dbcmd->dbp;
	int tid = dbcmd->tid;

	/* Send a SELECT request to the database server
	 */
	if (!(dbcmd->request = ssdbSendRequest(dbcmd->error, dbcmd->connection, 
								SSDB_REQTYPE_SELECT, dbcmd->cmdstr))) {
#ifdef CM_DEBUG

		fprintf(stderr, "db_select_records: %s\n", dbcmd->cmdstr);

		printf("SSDB Error %d: \"%s\" - ssdbSendRequest\n",
			ssdbGetLastErrorCode(dbcmd->error),
			ssdbGetLastErrorString(dbcmd->error));
#endif
		return(1);
	}

	/* Get the number of records returned from the query
	 */
	NUM_RECORDS(dbp, tid) = ssdbGetNumRecords(dbcmd->error, dbcmd->request);
	if(ssdbGetLastErrorCode(dbcmd->error) != SSDBERR_SUCCESS) {
#ifdef CM_DEBUG
		printf ("SSDB API Error %d: \"%s\" ssdbGetNumRecords\n",
			ssdbGetLastErrorCode(dbcmd->error),
			ssdbGetLastErrorString(dbcmd->error));
#endif
		return(1);
	}

	/* Get the number of fields per record returned from the query
	 */
	NUM_FIELDS(dbp, tid) = ssdbGetNumColumns(dbcmd->error, dbcmd->request);
	if(ssdbGetLastErrorCode(dbcmd->error) != SSDBERR_SUCCESS) {
#ifdef CM_DEBUG
		printf ("SSDB API Error %d: \"%s\" ssdbGetNumColumns\n",
			ssdbGetLastErrorCode(dbcmd->error), 
			ssdbGetLastErrorString(dbcmd->error));
#endif
		return(1);
	}
	return(0);
}

/*
 * select_records()
 */
int
select_records(db_cmd_t *dbcmd)
{
	int i = 0;
	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);
	int tid = dbcmd->tid;
	void **ptr, *p;

	if (TABLE_STATE(dbp, tid) != VALID_TBL) {
		return(0);
	}

	/* Free up any blocks of memory currently allocated for this table
	 * and prepare to read in a new batch of records from the database.
	 */
	clean_table_rec(dbp, tid);

	db_select_records(dbcmd);

	/* Allocate a buffer large enough to hold pointers for the number of
	 * records selected.
	 */
	ptr = kl_alloc_block((NUM_RECORDS(dbp, tid) * sizeof(void *)), flag);

	/* Get all of the records that were selected...
	 */
	while (p = get_next_record(dbcmd)) {
		ptr[i] = p;
		i++;

		if (NUM_RECORDS(dbp, tid) == 0) {
			break;
		}
	}

	DATA_PTR(dbp, tid) = ptr;
	RECORD_COUNT(dbp, tid) = i;

	/* Clean up a little
	 */
	NUM_RECORDS(dbp, tid) = 0;
	NUM_FIELDS(dbp, tid) = 0;
	return(i);
}

/*
 * insert_record()
 */
int
insert_record(database_t *dbp, int tid, void *datap)
{
	int ret_val;

	switch(tid) {

		case CONFIGMON_TBL :
			break;

		case SYSTEM_TBL :
		case SYSARCHIVE_TBL :
			ret_val = put_system_data(dbp, tid, (system_info_t *)datap);
			break;

		case CM_EVENT_TBL :
			ret_val = put_cm_event(dbp, (cm_event_t *)datap);
			break;

		case CHANGE_ITEM_TBL :
			ret_val = put_change_item(dbp, (change_item_t *)datap);
			break;

		case HWCOMPONENT_TBL :
		case HWARCHIVE_TBL :
			ret_val = put_hwcomponent(dbp, tid, (hw_component_t *)datap);
			break;

		case SWCOMPONENT_TBL :
		case SWARCHIVE_TBL :
			ret_val = put_swcomponent(dbp, tid, (sw_component_t *)datap);
			break;

		case CPU_TBL :
			ret_val = put_cpu_data(dbp, (cpu_data_t *)datap);
			break;

		case NODE_TBL :
			ret_val = put_node_data(dbp, (node_data_t *)datap);
			break;

		case MEMBANK_TBL :
			ret_val = put_membank_data(dbp, (membank_data_t *)datap);
			break;

		case IOA_TBL :
			ret_val = put_ioa_data(dbp, (ioa_data_t *)datap);
			break;

		case PROCESSOR_TBL :
			break;

		default:
			break;
	}
	return(ret_val);
}

/*
 * db_delete_records()
 */
int
db_delete_records(database_t *dbp, int tid, k_uint_t sys_id, int key)
{
	int count;
	char *ptr;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);

	sprintf(dbp->sqlbuf, "DELETE FROM %s WHERE sys_id=\"%llx\"", 
		TABLE_NAME(dbp, tid),
		sys_id);
	ptr = &dbp->sqlbuf[strlen(dbp->sqlbuf)];

	switch (tid) {

		/* These tables don't have a rec_key field
		 */
		case CONFIGMON_TBL:
		case CM_EVENT_TBL:
			break;

		/* Check to see if we are trying to delete a particular record (key)
		 */
		case SYSTEM_TBL:
		case SYSARCHIVE_TBL:
		case CHANGE_ITEM_TBL:
		case HWCOMPONENT_TBL:
		case HWARCHIVE_TBL:
		case SWCOMPONENT_TBL:
		case SWARCHIVE_TBL:
		case CPU_TBL:
		case NODE_TBL:
		case MEMBANK_TBL:
		case IOA_TBL:
			if (key) {
				sprintf(ptr, " AND rec_key=%d", key);
			}
			break;

		case PROCESSOR_TBL:
			break;

		default:
			break;

	}
	setup_dbcmd(dbcmd, SSDB_REQTYPE_DELETE, tid, dbp->sqlbuf);
	count = db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	return(count);
}

/*
 * delete_records()
 */
int
delete_records(database_t *dbp, int tid, k_uint_t sys_id, int key)
{
	int delete_count = 0;
	db_cmd_t *dbcmd;
	void *p;

	if (!sys_id) {
		return(1);
	}

	clean_table_rec(dbp, tid);

	delete_count = db_delete_records(dbp, tid, sys_id, key);
	return(delete_count);
}

/* 
 * free_record_memory()
 */
void
free_record_memory(database_t *dbp, int tid, k_ptr_t p)
{
	switch(tid) {
		case CONFIGMON_TBL:
			free_configmon_rec((configmon_rec_t *)p);
			break;

		case CM_EVENT_TBL:
		case CHANGE_ITEM_TBL:
			kl_free_block(p);
			break;

		case SYSTEM_TBL:
		case SYSARCHIVE_TBL:
			free_system_info(p);
			break;

		case HWCOMPONENT_TBL:
		case HWARCHIVE_TBL:
			free_hwcomponents((hw_component_t*)p);
			break;

		case SWCOMPONENT_TBL:
		case SWARCHIVE_TBL:
			free_swcomponents((sw_component_t*)p);
			break;

		case CPU_TBL:
		case NODE_TBL:
		case MEMBANK_TBL:
		case IOA_TBL:
			kl_free_block(p);
			break;
	}
}

/*
 * find_record()
 *
 * Find the record in a table that matches for both sys_id AND record key.
 * In theory, we should never get back more than one record from the
 * select_records() call (we may get NO records). Since the record key 
 * will not be a KEY field in the database, it's possible that a duplicate 
 * will exist (this should be considered a BUG though!). Note that the 
 * select_records() call should be used when multiple records need to
 * be found (i.e., all records for a particular sys_id).
 */
void *
find_record(db_cmd_t *dbcmd)
{
	void *ptr = (void *)NULL;

	database_t *dbp = dbcmd->dbp;
	int tid = dbcmd->tid;

	/* If we get back any number of records besides one, then return
	 * a NULL pointer.
	 */
	if (select_records(dbcmd) != 1) {
		return((void *)NULL);
	}

	ptr = next_record(dbp, tid);
	clean_table_rec(dbp, tid);
	return(ptr);
}

/*
 * next_record()
 *
 * Returns the next record in the table_array and automaticly zeros
 * out the record pointer. It is the responsibility of the caller to
 * then free the data block containing the record data.
 */
void *
next_record(database_t *dbp, int tid)
{
	void *ptr = (void *)NULL;

	if (NEXT_RECORD(dbp, tid) < RECORD_COUNT(dbp, tid)) {
		ptr = DATA_ARRAY(dbp, tid)[NEXT_RECORD(dbp, tid)];
		DATA_ARRAY(dbp, tid)[NEXT_RECORD(dbp, tid)] = (void *)NULL;
		NEXT_RECORD(dbp, tid)++;
	}
	return(ptr);
}  

/*
 * next_key()
 *
 * Returns the next key for a particular table (tid). Note that if a
 * table cannot have new records inserted into it (it is an archive
 * table or is a data table not supported by the local system type),
 * a key value of zero will be returned.
 */
int
next_key(database_t *dbp, int tid)
{
	int key = 0;

	if (dbp->table_array[tid].next_key) {
		key = dbp->table_array[tid].next_key;
		dbp->table_array[tid].next_key++;
	}
	return(key);
}

/*
 * db_get_field()
 */
int
db_get_field(db_cmd_t *dbcmd, k_ptr_t ptr, int size)
{
	int tid = dbcmd->tid;
	database_t *dbp = dbcmd->dbp;

	if (!ssdbGetNextField(dbcmd->error, dbcmd->request, ptr, size)) {
		return(1);
	}
	NEXT_FIELD(dbp, tid)++;
	if (NEXT_FIELD(dbp, tid) == NUM_FIELDS(dbp, tid)) {
		 NEXT_FIELD(dbp, tid) = 0;
	}
	return(0);
}

/*
 * db_get_string()
 */
char *
db_get_string(db_cmd_t *dbcmd)
{
	int len;
	char *field = (char *)NULL;
	char strbuf[256];
	int tid = dbcmd->tid;
	database_t *dbp = dbcmd->dbp;

	bzero(strbuf, 256);

	len = ssdbGetFieldMaxSize(dbcmd->error, 
			dbcmd->request, NEXT_FIELD(dbp, tid));
	if (ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS) {
		if (ssdbGetNextField(dbcmd->error, dbcmd->request, strbuf, len + 1)) {

			/* Note that we must check to make sure there actually
			 * is data in the string. If the first byte is non-NULL, 
			 * then go ahead and alloc a block and copy the string 
			 * there.
			 */
			if (strbuf[0]) {
				field = (char *)kl_str_to_block(strbuf, ALLOCFLG(dbp->flags));
			}
		}
	}
	NEXT_FIELD(dbp, tid)++;
	if (NEXT_FIELD(dbp, tid) == NUM_FIELDS(dbp, tid)) {
		 NEXT_FIELD(dbp, tid) = 0;
	}
	return(field);
}

/*
 * add_str()
 */
char *
add_str(char *ptr, char *str)
{
	if (str) {
		sprintf(ptr, "\"%s\"", str);
	}
	else {
		sprintf(ptr, "\"\"");
	}
	ptr = &ptr[strlen(ptr)];
	return(ptr);
}

/*
 * add_comma()
 */
char *
add_comma(char *ptr)
{
	*ptr++ = ',';
	return(ptr);
}

/*
 * get_configmon_data()
 */
configmon_rec_t *
get_configmon_data(db_cmd_t *dbcmd)
{
	int ret_val;
	char table_name[256];
	configmon_rec_t *cmp;

	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);

	cmp = (configmon_rec_t *)kl_alloc_block(sizeof(configmon_rec_t), flag);

	if (db_get_field(dbcmd, &cmp->table_id, sizeof(cmp->table_id))) {
		goto configmon_error;
	}
	cmp->table_name = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto configmon_error;
	}
	if (db_get_field(dbcmd, &cmp->next_key, sizeof(cmp->next_key))) {
		goto configmon_error;
	}
	NUM_RECORDS(dbp, CONFIGMON_TBL)--;
	return(cmp);

configmon_error:
	kl_free_block(cmp);
	return((configmon_rec_t *)NULL);
}

/* 
 * free_configmon_rec()
 */
void
free_configmon_rec(configmon_rec_t *cmp)
{
	if (cmp->table_name) {
		kl_free_block(cmp->table_name);
	}
	kl_free_block(cmp);
}

/*
 * get_system_data()
 */
system_info_t *
get_system_data(db_cmd_t *dbcmd)
{
	short active, local;
	int ret_val, rec_key, sys_type;
	char system_id[17];
	char serial_number[17];
	char hostname[64];
	char ip_address[64];
	time_t time;
	system_info_t *sysinfop;

	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);
	k_uint_t sys_id = dbcmd->db_sys_id;
	int key = dbcmd->db_key;

	sysinfop = (system_info_t*)kl_alloc_block(sizeof(system_info_t), flag);

	if (db_get_field(dbcmd, &sysinfop->rec_key, sizeof(sysinfop->rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, system_id, 17)) {
		goto handle_error;
	}
	sysinfop->sys_id = STRTOULL(system_id);

	if (db_get_field(dbcmd, &sysinfop->sys_type, sizeof(sysinfop->sys_type))) {
		goto handle_error;
	}
	sysinfop->serial_number = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	sysinfop->hostname = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	sysinfop->ip_address = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &sysinfop->active, sizeof(sysinfop->active))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &sysinfop->local, sizeof(sysinfop->local))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &sysinfop->time, sizeof(sysinfop->time))) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, SYSTEM_TBL)--;
	return(sysinfop);

handle_error:
	kl_free_block(sysinfop);
	return((system_info_t *)NULL);
}

/*
 * put_system_data()
 */
int
put_system_data(database_t *dbp, int tid, system_info_t *sip)
{
	int ret = 0;
	db_cmd_t *dbcmd;
	char *ptr;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(%d,\"%llx\",%d,",
		TABLE_NAME(dbp, tid),
		sip->rec_key,
		sip->sys_id,
		sip->sys_type);
	ptr = &dbp->sqlbuf[strlen(dbp->sqlbuf)];
	ptr = add_str(ptr, sip->serial_number);
	ptr = add_comma(ptr);
	ptr = add_str(ptr, sip->hostname);
	ptr = add_comma(ptr);
	ptr = add_str(ptr, sip->ip_address);
	ptr = add_comma(ptr);
	sprintf(ptr, "%hd,%hd,%u)",
		sip->active,
		sip->local,
		sip->time);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, tid, dbp->sqlbuf);

	ret = db_docmd(dbcmd);

	free_dbcmd(dbcmd);
	return(ret);
}

/*
 * get_hwcomponent()
 */
hw_component_t *
get_hwcomponent(db_cmd_t *dbcmd)
{
	int ret_val, len, nextfield;
	hw_component_t *cp;
	char system_id[17];
	char location[255];
	char name[255];
	char serial_number[255];
	char part_number[255];
	char revision[255];

	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);
	int tid = dbcmd->tid;
	k_uint_t sys_id = dbcmd->db_sys_id;
	int key = dbcmd->db_key;
	time_t time = dbcmd->db_time;

	cp = (hw_component_t *)kl_alloc_block(sizeof(hw_component_t), flag);

	if (db_get_field(dbcmd, &cp->c_seq, sizeof(cp->c_seq))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_level, sizeof(cp->c_level))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_rec_key, sizeof(cp->c_rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_parent_key, sizeof(cp->c_parent_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, system_id, 17)) {
		goto handle_error;
	}
	cp->c_sys_id = STRTOULL(system_id);

	if (db_get_field(dbcmd, &cp->c_category, sizeof(cp->c_category))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_type, sizeof(cp->c_type))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_inv_class, sizeof(cp->c_inv_class))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_inv_type, sizeof(cp->c_inv_type))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, 
				&cp->c_inv_controller, sizeof(cp->c_inv_controller))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_inv_unit, sizeof(cp->c_inv_unit))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_inv_state, sizeof(cp->c_inv_state))) {
		goto handle_error;
	}
	cp->c_location = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	cp->c_name = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	cp->c_serial_number = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	cp->c_part_number = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	cp->c_revision = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_enabled, sizeof(cp->c_enabled))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_install_time, sizeof(cp->c_install_time))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_deinstall_time, 
						sizeof(cp->c_deinstall_time))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_data_table, sizeof(cp->c_data_table))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->c_data_key, sizeof(cp->c_data_key))) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, tid)--;
	return(cp);

handle_error:
	kl_free_block(cp);
	return((hw_component_t *)NULL);
}

/*
 * put_hwcomponent()
 */
int
put_hwcomponent(database_t *dbp, int tid, hw_component_t *cp)
{
	int ret = 0;
	db_cmd_t *dbcmd;
	char *ptr;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(%d,%d,%d,%d,\"%llx\","
		"%d,%d,%d,%d,%d,%d,%d,",
		TABLE_NAME(dbp, tid), 
		cp->c_seq,
		cp->c_level,
		cp->c_rec_key,
		cp->c_parent_key,
		cp->c_sys_id,
		cp->c_category,
		cp->c_type,
		cp->c_inv_class,
		cp->c_inv_type,
		cp->c_inv_controller,
		cp->c_inv_unit,
		cp->c_inv_state);
	ptr = &dbp->sqlbuf[strlen(dbp->sqlbuf)];
	ptr = add_str(ptr, cp->c_location);
	ptr = add_comma(ptr);
	ptr = add_str(ptr, cp->c_name);
	ptr = add_comma(ptr);
	ptr = add_str(ptr, cp->c_serial_number);
	ptr = add_comma(ptr);
	ptr = add_str(ptr, cp->c_part_number);
	ptr = add_comma(ptr);
	ptr = add_str(ptr, cp->c_revision);
	ptr = add_comma(ptr);
	sprintf(ptr, "%d,%d,%d,%d,%d)",
		cp->c_enabled,
		cp->c_install_time,
		cp->c_deinstall_time,
		cp->c_data_table,
		cp->c_data_key);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, tid, dbp->sqlbuf);
	ret = db_docmd(dbcmd);

	free_dbcmd(dbcmd);
	return(ret);
}

/*
 * get_swcomponent()
 */
sw_component_t *
get_swcomponent(db_cmd_t *dbcmd)
{
	sw_component_t *swcp;
	char system_id[17];
	char s[256];
	char *c, *rec_key, *sysid, *name, *version, *idate, *ddate, *desc, *p;
	time_t curtime, test_curtime;

	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);
	int tid = dbcmd->tid;
	k_uint_t sys_id = dbcmd->db_sys_id;
	int key = dbcmd->db_key;
	time_t time = dbcmd->db_time;

	swcp = (sw_component_t *)kl_alloc_block(sizeof(sw_component_t), flag);

	if (db_get_field(dbcmd, &swcp->sw_key, sizeof(swcp->sw_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, system_id, 17)) {
		goto handle_error;
	}
	swcp->sw_sys_id = STRTOULL(system_id);

	swcp->sw_name = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &swcp->sw_version, sizeof(swcp->sw_version))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &swcp->sw_install_time, 
				sizeof(swcp->sw_install_time))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &swcp->sw_deinstall_time, 
				sizeof(swcp->sw_deinstall_time))) {
		goto handle_error;
	}
	swcp->sw_description = db_get_string(dbcmd);
	if (!(ssdbGetLastErrorCode(dbcmd->error) == SSDBERR_SUCCESS)) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, tid)--;
	return(swcp);

handle_error:
	kl_free_block(swcp);
	return((sw_component_t *)NULL);
}

/*
 * put_swcomponent()
 */
int
put_swcomponent(database_t *dbp, int tid, sw_component_t *swcp)
{
	int ret = 0;
	db_cmd_t *dbcmd;
	char *ptr;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(%d,\"%llx\",",
		TABLE_NAME(dbp, tid), 
		swcp->sw_key,
		swcp->sw_sys_id);
	ptr = &dbp->sqlbuf[strlen(dbp->sqlbuf)];
	ptr = add_str(ptr, swcp->sw_name);
	ptr = add_comma(ptr);
	sprintf(ptr, "%u,%u,%u,", 
		swcp->sw_version, 
		swcp->sw_install_time, 
		swcp->sw_deinstall_time);
	ptr = &ptr[strlen(ptr)];
	ptr = add_str(ptr, swcp->sw_description);
	*ptr++ = ')';
	*ptr = 0;
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, tid, dbp->sqlbuf);

	ret = db_docmd(dbcmd);

	free_dbcmd(dbcmd);
	return(ret);
}

/*
 * get_cpu_data()
 */
cpu_data_t *
get_cpu_data(db_cmd_t *dbcmd)
{
	int ret_val;
	char system_id[17];
	cpu_data_t *cpup;

	database_t *dbp = dbcmd->dbp;
	k_uint_t sys_id = dbcmd->db_sys_id;
	int flag = ALLOCFLG( dbp->flags);
	int key = dbcmd->db_key;

	cpup = (cpu_data_t *)kl_alloc_block(sizeof(cpu_data_t), flag);

	if (db_get_field(dbcmd, &cpup->cpu_rec_key, sizeof(cpup->cpu_rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, system_id, 17)) {
		goto handle_error;
	}
	cpup->sys_id = STRTOULL(system_id);

	if (db_get_field(dbcmd, &cpup->comp_rec_key, sizeof(cpup->comp_rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_module, sizeof(cpup->cpu_module))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_slot, sizeof(cpup->cpu_slot))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_slice, sizeof(cpup->cpu_slice))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_id, sizeof(cpup->cpu_id))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_speed, sizeof(cpup->cpu_speed))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_flavor, sizeof(cpup->cpu_flavor))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_cachesz, sizeof(cpup->cpu_cachesz))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_cachefreq, 
				sizeof(cpup->cpu_cachefreq))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_promrev, sizeof(cpup->cpu_promrev))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cpup->cpu_enabled, sizeof(cpup->cpu_enabled))) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, CPU_TBL)--;
	return(cpup);

handle_error:
	kl_free_block(cpup);
	return((cpu_data_t *)NULL);
}

/*
 * put_cpu_data()
 */
int
put_cpu_data(database_t *dbp, cpu_data_t *cdp)
{
	int ret = 0;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s "
		"VALUES(%u,\"%llx\",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u)", 
		TABLE_NAME(dbp, CPU_TBL), 
		cdp->cpu_rec_key,
		cdp->sys_id,
		cdp->comp_rec_key,
		cdp->cpu_module,
		cdp->cpu_slot,
		cdp->cpu_slice,
		cdp->cpu_id,
		cdp->cpu_speed,
		cdp->cpu_flavor,
		cdp->cpu_cachesz,
		cdp->cpu_cachefreq,
		cdp->cpu_promrev,
		cdp->cpu_enabled);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, CPU_TBL, dbp->sqlbuf);
	ret = db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	return(ret);
}

/*
 * get_node_data()
 */
node_data_t *
get_node_data(db_cmd_t *dbcmd)
{
	int ret_val;
	char system_id[17];
	node_data_t *ndp;

	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);
	k_uint_t sys_id = dbcmd->db_sys_id;
	int key = dbcmd->db_key;

	ndp = (node_data_t *)kl_alloc_block(sizeof(node_data_t), flag);

	if (db_get_field(dbcmd, &ndp->rec_key, sizeof(ndp->rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, system_id, 17)) {
		goto handle_error;
	}
	ndp->sys_id = STRTOULL(system_id);

	if (db_get_field(dbcmd, &ndp->comp_rec_key, sizeof(ndp->comp_rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &ndp->n_cpu_count, sizeof(ndp->n_cpu_count))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &ndp->n_memory, sizeof(ndp->n_memory))) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, NODE_TBL)--;
	return(ndp);

handle_error:
	kl_free_block(ndp);
	return((node_data_t *)NULL);
}

/*
 * put_node_data()
 */
int
put_node_data(database_t *dbp, node_data_t *ndp)
{
	int ret = 0;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(%u,\"%llx\",%u,%u,%u)", 
		TABLE_NAME(dbp, NODE_TBL), 
		ndp->rec_key,
		ndp->sys_id,
		ndp->comp_rec_key,
		ndp->n_cpu_count,
		ndp->n_memory);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, NODE_TBL, dbp->sqlbuf);

	ret = db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	return(ret);
}

/*
 * get_membank_data()
 */
membank_data_t *
get_membank_data(db_cmd_t *dbcmd)
{
	int ret_val;
	char system_id[17];
	membank_data_t *mbp;

	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);
	k_uint_t sys_id = dbcmd->db_sys_id;
	int key = dbcmd->db_key;

	mbp = (membank_data_t *)kl_alloc_block(sizeof(membank_data_t), flag);

	if (db_get_field(dbcmd, &mbp->mb_rec_key, sizeof(mbp->mb_rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, system_id, 17)) {
		goto handle_error;
	}
	mbp->sys_id = STRTOULL(system_id);

	if (db_get_field(dbcmd, &mbp->comp_rec_key, sizeof(mbp->comp_rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &mbp->mb_attr, sizeof(mbp->mb_attr))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &mbp->mb_flag, sizeof(mbp->mb_flag))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &mbp->mb_bnk_number, sizeof(mbp->mb_bnk_number))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &mbp->mb_size, sizeof(mbp->mb_size))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &mbp->mb_enable, sizeof(mbp->mb_enable))) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, MEMBANK_TBL)--;
	return(mbp);

handle_error:
	kl_free_block(mbp);
	return((membank_data_t *)NULL);
}

/*
 * put_membank_data()
 */
int
put_membank_data(database_t *dbp, membank_data_t *mbp)
{
	int ret = 0;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, 
		"INSERT INTO %s VALUES(%u,\"%llx\",%u,%u,%u,%u,%u,%u)", 
		TABLE_NAME(dbp, MEMBANK_TBL), 
		mbp->mb_rec_key,
		mbp->sys_id,
		mbp->comp_rec_key,
		mbp->mb_attr,
		mbp->mb_flag,
		mbp->mb_bnk_number,
		mbp->mb_size,
		mbp->mb_enable);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, MEMBANK_TBL, dbp->sqlbuf);
	ret = db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	return(ret);
}

/*
 * get_ioa_data()
 */
ioa_data_t *
get_ioa_data(db_cmd_t *dbcmd)
{
	int ret_val;
	char system_id[17];
	ioa_data_t *ioap;

	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);
	k_uint_t sys_id = dbcmd->db_sys_id;
	int key = dbcmd->db_key;

	ioap = (ioa_data_t *)kl_alloc_block(sizeof(ioa_data_t), flag);

	if (db_get_field(dbcmd, &ioap->ioa_rec_key, sizeof(ioap->ioa_rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &ioap->sys_id, sizeof(ioap->sys_id))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &ioap->comp_rec_key, sizeof(ioap->comp_rec_key))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &ioap->ioa_enable, sizeof(ioap->sys_id))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &ioap->ioa_type, sizeof(ioap->ioa_type))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &ioap->ioa_virtid, sizeof(ioap->ioa_virtid))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &ioap->ioa_subtype, sizeof(ioap->ioa_subtype))) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, IOA_TBL)--;
	return(ioap);

handle_error:
	kl_free_block(ioap);
	return((ioa_data_t *)NULL);
}

/*
 * put_ioa_data()
 */
int
put_ioa_data(database_t *dbp, ioa_data_t *idp)
{
	int ret = 0;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(%u,\"%llx\",%u,%u,%u,%u,%u)", 
		TABLE_NAME(dbp, IOA_TBL), 
		idp->ioa_rec_key,
		idp->sys_id,
		idp->comp_rec_key,
		idp->ioa_enable,
		idp->ioa_type,
		idp->ioa_virtid,
		idp->ioa_subtype);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, IOA_TBL, dbp->sqlbuf);
	ret = db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	return(ret);
}

/*
 * get_next_record()
 */
void *
get_next_record(db_cmd_t *dbcmd)
{
	void *ptr;

	switch (dbcmd->tid) {
		case CONFIGMON_TBL:
			ptr = get_configmon_data(dbcmd);
			break;

		case SYSTEM_TBL:
		case SYSARCHIVE_TBL:
			ptr = get_system_data(dbcmd);
			break;

		case CM_EVENT_TBL:
			ptr = get_cm_event(dbcmd);
			break;

		case CHANGE_ITEM_TBL:
			ptr = get_change_item(dbcmd);
			break;

		case HWCOMPONENT_TBL:
		case HWARCHIVE_TBL:
			ptr = get_hwcomponent(dbcmd);
			break;

		case SWCOMPONENT_TBL:
		case SWARCHIVE_TBL:
			ptr = get_swcomponent(dbcmd);
			break;

		case CPU_TBL:
			ptr = get_cpu_data(dbcmd);
			break;

		case NODE_TBL:
			ptr = get_node_data(dbcmd);
			break;

		case MEMBANK_TBL:
			ptr = get_membank_data(dbcmd);
			break;

		case IOA_TBL:
			ptr = get_ioa_data(dbcmd);
			break;
	}
	return(ptr);
}

/*
 * record_sys_id()
 */
k_uint_t
record_sys_id(int tid, void *datap)
{
	k_uint_t sys_id = 0;

	switch(tid) {

		case CONFIGMON_TBL :
			break;

		case SYSTEM_TBL :
		case SYSARCHIVE_TBL :
			sys_id = ((system_info_t *)datap)->sys_id;
			break;

		case CHANGE_ITEM_TBL :
			break;

		case HWCOMPONENT_TBL :
		case HWARCHIVE_TBL :
			sys_id = ((hw_component_t *)datap)->c_sys_id;
			break;

		case SWCOMPONENT_TBL :
		case SWARCHIVE_TBL :
			sys_id = ((sw_component_t *)datap)->sw_sys_id;
			break;

		case CPU_TBL :
			sys_id = ((cpu_data_t *)datap)->sys_id;
			break;

		case NODE_TBL :
			sys_id = ((node_data_t *)datap)->sys_id;
			break;

		case MEMBANK_TBL :
			sys_id = ((membank_data_t *)datap)->sys_id;
			break;

		case IOA_TBL :
			sys_id = ((ioa_data_t *)datap)->sys_id;
			break;

		case PROCESSOR_TBL :
			break;

		default:
			break;
	}
	return(sys_id);
}

/*
 * record_key()
 */
int
record_key(int tid, void *datap)
{
	int key = 0;

	switch(tid) {

		case CONFIGMON_TBL :
			break;

		case SYSTEM_TBL :
		case SYSARCHIVE_TBL :
			key = ((system_info_t *)datap)->rec_key;
			break;

		case CHANGE_ITEM_TBL :
			break;

		case HWCOMPONENT_TBL :
		case HWARCHIVE_TBL :
			key = ((hw_component_t *)datap)->c_rec_key;
			break;

		case SWCOMPONENT_TBL :
		case SWARCHIVE_TBL :
			key = ((sw_component_t *)datap)->sw_key;
			break;

		case CPU_TBL :
			key = ((cpu_data_t *)datap)->cpu_rec_key;
			break;

		case NODE_TBL :
			key = ((node_data_t *)datap)->rec_key;
			break;

		case MEMBANK_TBL :
			key = ((membank_data_t *)datap)->mb_rec_key;
			break;

		case IOA_TBL :
			key = ((ioa_data_t *)datap)->ioa_rec_key;
			break;

		case PROCESSOR_TBL :
			break;

		default:
			break;
	}
	return(key);
}

/* 
 * alloc_change_item()
 */
change_item_t *
alloc_change_item(
	k_uint_t sys_id, 
	time_t time, 
	int type, 
	int tid, 
	int key, 
	int flags)
{
	int flag = ALLOCFLG(flags);
	change_item_t *cip;

	cip = (change_item_t *)kl_alloc_block(sizeof(change_item_t), flag);

	cip->sys_id = sys_id;
	cip->time = time;
	cip->type = type;
	cip->tid = tid;
	cip->rec_key = key;
	return(cip);
}

/*
 * get_change_item()
 */
change_item_t *
get_change_item(db_cmd_t *dbcmd)
{
	int ret_val;
	char system_id[17];
	change_item_t *cip;

	database_t *dbp = dbcmd->dbp;
	int flag = ALLOCFLG(dbp->flags);
	k_uint_t sys_id = dbcmd->db_sys_id;
	time_t time = dbcmd->db_time;

	cip = (change_item_t *)kl_alloc_block(sizeof(change_item_t), flag);

	if (db_get_field(dbcmd, system_id, 17)) {
		goto handle_error;
	}
	cip->sys_id = STRTOULL(system_id);

	if (db_get_field(dbcmd, &cip->time, sizeof(cip->time))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cip->type, sizeof(cip->type))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cip->tid, sizeof(cip->tid))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cip->rec_key, sizeof(cip->rec_key))) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, CHANGE_ITEM_TBL)--;
	return(cip);

handle_error:
	kl_free_block(cip);
	return((change_item_t *)NULL);
}

/*
 * put_change_item()
 */
int 
put_change_item(database_t *dbp, change_item_t *cip)
{
	int ret = 0;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(\"%llx\",%u,%d,%d,%d)", 
		TABLE_NAME(dbp, CHANGE_ITEM_TBL), 
		cip->sys_id, cip->time, cip->type, cip->tid, cip->rec_key);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, CHANGE_ITEM_TBL, dbp->sqlbuf);
	ret = db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	return(ret);
}

/*
 * insert_change_item()
 */
int
insert_change_item(
	database_t *dbp, 
	k_uint_t sys_id, 
	time_t time, 
	int type, 
	int tid, 
	int key) 
{
	int ret_val;
	change_item_t *cip;

	cip = alloc_change_item(sys_id, time, type, tid, key, dbp->flags);
	ret_val = put_change_item(dbp, cip);
	kl_free_block(cip);
	return(ret_val);
}

/* 
 * alloc_cm_event()
 */
cm_event_t *
alloc_cm_event(
	k_uint_t sys_id, 
	time_t time, 
	int type, 
	int flags)
{
	cm_event_t *cp;

	cp = (cm_event_t *)kl_alloc_block(sizeof(cm_event_t), flags);

	cp->sys_id = sys_id;
	cp->time = time;
	cp->type = type;
	return(cp);
}

/*
 * get_cm_event()
 */
cm_event_t *
get_cm_event(db_cmd_t *dbcmd)
{
	int ret_val;
	char system_id[17];
	cm_event_t *cp;

	int aflag = ALLOCFLG(dbcmd->dbp->flags);
	database_t *dbp = dbcmd->dbp;
	k_uint_t sys_id = dbcmd->db_sys_id;
	k_uint_t time = dbcmd->db_time;

	cp = (cm_event_t *)kl_alloc_block(sizeof(cm_event_t), aflag);

    if (db_get_field(dbcmd, system_id, 17)) {
		goto handle_error;
	}
	cp->sys_id = STRTOULL(system_id);

	if (db_get_field(dbcmd, &cp->time, sizeof(cp->time))) {
		goto handle_error;
	}
	if (db_get_field(dbcmd, &cp->type, sizeof(cp->type))) {
		goto handle_error;
	}
	NUM_RECORDS(dbp, CM_EVENT_TBL)--;
	return(cp);

handle_error:
	kl_free_block(cp);
	return((cm_event_t *)NULL);
}

/* 
 * put_cm_event()
 */
int
put_cm_event(database_t *dbp, cm_event_t *cp)
{
	int ret = 0;
	db_cmd_t *dbcmd;

	dbcmd = alloc_dbcmd(dbp);
	sprintf(dbp->sqlbuf, "INSERT INTO %s VALUES(\"%llx\",%u,%d)", 
		TABLE_NAME(dbp, CM_EVENT_TBL), cp->sys_id, cp->time, cp->type);
	setup_dbcmd(dbcmd, SSDB_REQTYPE_INSERT, CM_EVENT_TBL, dbp->sqlbuf);
	ret = db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	return(ret);
}

/* 
 * insert_cm_event()
 */
int
insert_cm_event( 
	database_t *dbp, 
	k_uint_t sys_id, 
	time_t time, 
	int type, 
	int flags)
{
	int ret_val;
	cm_event_t *cp;

	cp = alloc_cm_event(sys_id, time, type, flags);
	ret_val = put_cm_event(dbp, cp);
	kl_free_block(cp);
	return(ret_val);
}

/*
 * db_cleanup()
 */
void
db_cleanup(char *dbname, int bounds, int dsoflg)
{
	int i, found = 0;
	db_cmd_t *dbcmd;
	database_t *dbp;
	char *ptr;

	if (!(dbp = open_database(dbname, bounds, K_TEMP, dsoflg))) {
		return;
	}

	dbcmd = alloc_dbcmd(dbp);

	sprintf(dbp->sqlbuf, "DROP TABLE "); 
	for (i = 0; i < MAX_TABLES; i++) {
		if (dbp->table_array[i].state == VALID_TBL) {
			ptr = &dbp->sqlbuf[strlen(dbp->sqlbuf)];

			if (found && (i < (MAX_TABLES - 1))) {
				sprintf(ptr, ",%s", TABLE_NAME(dbp, i));
			}
			else {
				sprintf(ptr, "%s", TABLE_NAME(dbp, i));
			}
			found++;
		}
	}
	setup_dbcmd(dbcmd, SSDB_REQTYPE_DROPTABLE, 0, dbp->sqlbuf);
	db_docmd(dbcmd);
	free_dbcmd(dbcmd);
	close_database(dbp);
}
