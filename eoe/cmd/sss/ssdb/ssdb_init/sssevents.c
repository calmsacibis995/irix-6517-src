/*
 * Copyright 1992-1999 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <ssdbapi.h>
#include <ssdberr.h>
#include <sys/systeminfo.h>
#include <netinet/in.h>
#include <netdb.h>
#include <dirent.h>

#include "dbinit.h"


int main (int argc, char *argv[])
{
	char szDefaultUserName[64] = "";             /* Default user name */
	char szDefaultPassword[32] = "";             /* Default password */
	char szDefaultHostName[64] = "localhost";    /* Default host name */
	char szDefaultDatabaseName[64] = "ssdb";     /* Default db name */
	ssdb_Client_Handle client;
	ssdb_Connection_Handle connection;
	ssdb_Request_Handle req_handle;
	ssdb_Error_Handle error_handle;
	CLASS_TYPE *head;
	char name[128];
	DIR *dp;
	struct dirent *dirp;
	FILE *fp;
	DBLIST *dbhead;
	SYSTEM_TABLE system_data;
	SYSTEM_TABLE *system_info_ptr;
	TOOLS *tool_ptr,*tool_dbptr;
	int system_check,tools_record;

	system_info_ptr = &system_data;	
	head = NULL;
	dbhead = NULL;

	if ((dp = opendir("/var/esp/init/eventlist")) == NULL)
	{
		fprintf(stderr,"Database initialization failed: Error opening event initialization files\n");
		exit(1);
	}


	/* Read in default set of events */
	while ((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_name[0] == '.' && dirp->d_name[1] == '\0'
                    || dirp->d_name[1] == '.' && dirp->d_name[2] == '\0')
                	continue;
		else{
		strcpy(name,"/var/esp/init/eventlist/");
		strcat(name,dirp->d_name);
		if ((fp = fopen(name,"r")) == NULL)
		{
			fprintf(stderr,"Database initialization failed: Error opening event initialization file\n");
			exit(1);
		}

		/* Create the list of events */
		if ((head = read_events(head,fp)) == NULL)
		{
			fprintf(stderr,"Database initialization failed: Error reading event initialization file\n");
			exit(1);
		}
		fclose (fp);
		}
	}	

	if(!ssdbInit())
        {
                 fprintf(stderr,"Database initialization failed: API Error %d: \"%s\"\n",
			ssdbGetLastErrorCode(0), ssdbGetLastErrorString(0));
                 exit(1);
        }

        /* Establish a error handle for all recovering SSDB API Errors */

        if ((error_handle = ssdbCreateErrorHandle()) == NULL)
        {
                fprintf(stderr,"Database initialization failed: API Error %d: \"%s\"\n",
                        ssdbGetLastErrorCode(0),ssdbGetLastErrorString(0));
                exit(1);
        }

        /* Establish a new client */

        if ((client = ssdbNewClient(error_handle,szDefaultUserName,szDefaultPassword,0)) == NULL)
	{
                fprintf(stderr,"Database initialization failed: API Error %d: \"%s\"\n",
                        ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                exit(1);
        }


        /* Establish a connection to a database */
        if ((connection = ssdbOpenConnection(error_handle,client,szDefaultHostName,szDefaultDatabaseName,0)) == NULL)
        {
                fprintf(stderr,"Database initialization failed: API Error %d: \"%s\"\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                exit(1);
        }

	/* Read in system information from the live system */
	if (!get_system_info(system_info_ptr))
	{
		fprintf(stderr,"Database initialization failed: Error reading system information\n");
		exit(1);
	}
		
	/* Check if the system information is currently in the database */
	system_check = check_system_info(system_info_ptr,connection,error_handle);

	if (system_check < 0)
	{
		fprintf(stderr,"Database initialization failed: Error reading system information from database\n");
		exit(1);
	}

	/* This means that the database has no record for the system information */
	if (!system_check)
	{
		if (!create_system_record(system_info_ptr,connection,error_handle))
		{
			fprintf(stderr,"Database initialization failed: Error creating system record\n");
			exit(1);
		}
	}

	/* The system id is different that what is in the database */
	if (system_info_ptr->id_changed || system_info_ptr->hostchange)
	{
		if (!update_system_records(system_info_ptr,connection,error_handle))
		{
                        fprintf(stderr,"Database initialization failed: Error updating system records\n");
			exit(1);	
		}
	}

	/* Create a single default action for the system. This will be set to espnotify graphical post */
	if (!create_default_action(system_info_ptr,connection,error_handle))
	{
		fprintf(stderr,"Database initialization failed: Error creating default action entry\n");
		exit(1);
	}

	if ((dbhead = dbread(dbhead,connection,error_handle)) == NULL) 
	{
		fprintf(stderr,"Database initialization failed: Error reading the database fields\n");
		exit(1);
	}


	if (!compare_lists(head,dbhead,system_info_ptr,connection,error_handle))
	{
		fprintf(stderr,"Database initialization failed: Error during dbcompare with event list\n");
		exit(1);
	}

	if ((tool_ptr = read_tools()) == NULL)
	{
		fprintf(stderr,"Database initialization failed: Error reading tools from tools configuration file");
		exit(1);
	}
	if ((tool_dbptr = read_dbtools(connection,error_handle)) == NULL)
	{
		fprintf(stderr,"Database initialization failed: Error reading tools configuration table\n");	
		exit(1);
	}
	if (!compare_tools(tool_ptr,tool_dbptr,connection,error_handle))
	{
		fprintf(stderr, "Error during dbcompare with initialization file\n");
		exit(1);
	}
	free_eventlist(head); 

	if (!create_sssconfig(connection,error_handle,system_info_ptr))
	{
		fprintf(stderr,"Database initialization failed: Error creating ESP Configuration\n");
		exit(1);
	}
	exit(0);
}

TOOLS * read_tools(void)
{
	FILE *fp;
	char buffer[256],check_buffer[255];
        char *p;
	TOOLS *newtool,*tool_ptr;
	int num_fields;
	/* Create a dummy node for the tool config data */

        if ((tool_ptr = (TOOLS *)malloc(sizeof(TOOLS))) == NULL)
                return 0;
        tool_ptr->next = NULL;

	if ((fp = fopen("/var/esp/init/datafiles/toolconfig","r")) == NULL)
                return 0;
	fgets(buffer, 256,fp);
        while (!feof(fp))
        {
		if (buffer[0] != '#' && buffer[0] != '\n')
                {
			buffer[strlen(buffer)-1] = '\0';
			strcpy(check_buffer,buffer);
                        num_fields = 0;
                        p = strtok(check_buffer,":");
                        while(p)
                        {
                                num_fields ++;
                                p = strtok(NULL,":");
                        }
                        if(num_fields == 3)
			{
				if ((newtool = (TOOLS *)malloc(sizeof(TOOLS))) == NULL)
				{
					printf("Error allocating memory\n");
					return 0;
				}
				p = strtok(buffer,":");
				strcpy(newtool->tool_name,p);
				p = strtok(NULL,":");
				strcpy (newtool->tool_option,p);
				p = strtok(NULL,":");
				strcpy (newtool->option_default,p);
				tool_ptr =  add_to_config(tool_ptr,newtool);	
			}
		}
		fgets(buffer, 256,fp);
        }
	return (tool_ptr);
}
CLASS_TYPE * read_events(CLASS_TYPE *head,FILE *fp)
{
	char buffer[256],check_buffer[255];
	char *p;
	int num_fields;
	CLASS_TYPE *newclass;
	/* Create a dummy node for the list for data read from initfile */

	if (!head)
	{
		if ((head = (CLASS_TYPE *)malloc(sizeof(CLASS_TYPE))) == NULL)
			return 0;
		head -> next = NULL;
	}

	fgets(buffer, 256,fp);

	while (!feof(fp))
	{
		if (buffer[0] != '#' && buffer[0] != '\n')
		{
			buffer[strlen(buffer)-1] = '\0';
			strcpy(check_buffer,buffer);
			num_fields = 0;
			p = strtok(check_buffer,":");
			while(p)
			{
				num_fields ++;
				p = strtok(NULL,":");
			}
			if(num_fields == 9)
			{
				if ((newclass = (CLASS_TYPE *)malloc(sizeof(CLASS_TYPE))) == NULL)
				{
					printf("Error allocating memory\n");
					return 0;
				}
				p = strtok(buffer,":");
				newclass->class = atoi(p);	
				p = strtok(NULL,":");
				strcpy (newclass->class_desc,p); 
				p = strtok(NULL,":");
				newclass->type = strtol(p,NULL,(p[0] == '0' && p[1] == 'x') ? 16:10);
				p = strtok(NULL,":");
				strcpy (newclass->type_desc,p);
				p = strtok(NULL,":");
				newclass->throttled = atoi(p);
				p = strtok(NULL,":");
				newclass->throttle_val = atoi(p);
				p = strtok(NULL,":");
				newclass->threshold = atoi(p);
				p = strtok(NULL,":");
				newclass->enabled = atoi(p);
				p = strtok(NULL,":");
				newclass->action_enabled = atoi(p);
				newclass->next = NULL;
				head = add_to_list(head,newclass);
			}
		}
	fgets(buffer, 256,fp); 
	}
	return (head);
}

TOOLS * add_to_config(TOOLS *tool_ptr,TOOLS *newtool)
{
	TOOLS *temphead,*prev;

	if (tool_ptr ->next == NULL)
        {
                newtool->next = tool_ptr;
		return (newtool);
	}
	prev = tool_ptr;
	temphead = tool_ptr->next;
	while (temphead->next != NULL){
                temphead = temphead->next;
                prev = prev->next;
        }
	newtool->next = temphead;
	prev->next = newtool;
	return (tool_ptr);	
}

CLASS_TYPE * add_to_list (CLASS_TYPE *head,CLASS_TYPE *newclass)
{
	CLASS_TYPE *temphead,*prev;

	if (head ->next == NULL)
	{
		newclass->next = head;
		return (newclass);
	}

	prev = head;	
	temphead = head->next;

	while (temphead->next != NULL){
		temphead = temphead->next;
		prev = prev->next;
	}
	newclass->next = temphead;
	prev->next = newclass;
	return (head);
}

DBLIST * add_dblist(DBLIST *dbhead,DBLIST *dblistptr)
{
	DBLIST *temphead,*prev;

	if (dbhead ->next == NULL)
        {
                dblistptr->next = dbhead;
                return (dblistptr);
        }

        prev = dbhead;
        temphead = dbhead->next;

        while (temphead->next != NULL){
                temphead = temphead->next;
                prev = prev->next;
        }
        dblistptr->next = temphead;
        prev->next = dblistptr;
        return (dbhead);
}

int compare_tools(TOOLS *tool_ptr,TOOLS *tool_dbptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{
	int update,exists;
        ssdb_Request_Handle req_handle;
	TOOLS *mydblist,*dblistptr,*newdbhead,*mylisthead;
	mylisthead = tool_ptr;
	newdbhead = tool_dbptr;
	mylisthead = tool_ptr;
	while (mylisthead->next != NULL)
        {
                update = FALSE;
                exists = FALSE;
		mydblist = newdbhead;
                while (mydblist->next != NULL)
                {
                        if (!strcmp(mylisthead->tool_option,mydblist->tool_option))
			{
				exists = TRUE;
				break;
			}
			mydblist = mydblist->next;
                }
		if(!exists)
		{
			/* This tool does not exist. Make a new entry into the table */
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into tool values ('%s','%s','%s')",mylisthead->tool_name,mylisthead->tool_option, mylisthead->option_default)))
                        {
                                fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
					ssdbGetLastErrorString(error_handle));
                                return 0;
                        }
                        ssdbFreeRequest(error_handle,req_handle);
			dblistptr = (TOOLS *)malloc(sizeof(TOOLS));
			strcpy(dblistptr->tool_name,mylisthead->tool_name);	
			strcpy(dblistptr->tool_option,mylisthead->tool_option);	
			strcpy(dblistptr->option_default,mylisthead->option_default);	
			newdbhead = add_to_config(newdbhead,dblistptr);
		}
		mylisthead = mylisthead->next;
	}	
	return 1;	
}

int compare_lists(CLASS_TYPE *listhead,DBLIST *dbhead,SYSTEM_TABLE *system_info_ptr,
		  ssdb_Connection_Handle connection, ssdb_Error_Handle error_handle)
{
	CLASS_TYPE *mylisthead;
	DBLIST *mydblist,*dblistptr,*newdbhead;
	int update,exists;
	ssdb_Request_Handle req_handle;
	mylisthead = listhead;
	newdbhead = dbhead;
	while (mylisthead->next != NULL)
	{
		/* First check if the TYPE exists. Existing may also indicate that the description changed */
		update = FALSE;
		exists = FALSE;
		mydblist = newdbhead;
		while (mydblist->next != NULL)
		{
			if (mylisthead->type == mydblist->type){
				exists = TRUE;
				if (strcmp(mylisthead->type_desc,mydblist->type_desc)){
					update = TRUE;
				}
				break;
			}
			mydblist = mydblist->next;
		}
		if (update)
		{
			/* update the description only or else check the last node here */
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
				"update event_type set type_desc = \"%s\" where type_id = %d",mylisthead->type_desc,mydblist->type))){
                		fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
                        	ssdbGetLastErrorString(error_handle));
                		return 0;
        		}
			ssdbFreeRequest(error_handle,req_handle);
		}
		if (!exists)
		{
			/* This event does not exist. Add the event type to the event type table */
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
				"insert into event_type values('%s',%d,%d,\"%s\",%d,%d,%d,-1,%d)",
				system_info_ptr->sys_id,mylisthead->type,mylisthead->class,mylisthead->type_desc,
				mylisthead->throttled,mylisthead->throttle_val,mylisthead->threshold,mylisthead->enabled)))
			{
				fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
				ssdbGetLastErrorString(error_handle));
				return 0;
			}
			ssdbFreeRequest(error_handle,req_handle);
			update = FALSE;
			exists = FALSE;
			/* Now check if the CLASS is also new. Other possibility is that the CLASS description was changed */
			mydblist = newdbhead;
			while (mydblist->next != NULL)
			{
                        	if (mylisthead->class == mydblist->class){
					exists = TRUE;
					if (strcmp(mylisthead->class_desc,mydblist->class_desc)){
						update = TRUE;
					}
					break;
				}
				mydblist = mydblist->next;
			}
			if (update)
			{
				/* update the description only for the CLASS */
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
					"update event_class set class_desc = \"%s\" where class_id = %d",
					mylisthead->class_desc,mydblist->class)))
				{
					fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
					ssdbGetLastErrorString(error_handle));
					return 0;
				}
				ssdbFreeRequest(error_handle,req_handle);
			}
			if (!exists)
			{
				/* This class does not exist. Add it to the event_class table */
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
				"insert into event_class values ('%s',%d,\"%s\")",system_info_ptr->sys_id,
				mylisthead->class, mylisthead->class_desc)))
				{
					fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
					ssdbGetLastErrorString(error_handle));
					return 0;
				}
				ssdbFreeRequest(error_handle,req_handle);
			}
			dblistptr = (DBLIST *)malloc(sizeof(DBLIST));
			strcpy(dblistptr->sys_id,system_info_ptr->sys_id);
			dblistptr->class = mylisthead->class;
			strcpy(dblistptr->class_desc,mylisthead->class_desc);
			dblistptr->type = mylisthead->type;
			strcpy(dblistptr->type_desc,mylisthead->type_desc);
			newdbhead = add_dblist(newdbhead,dblistptr);

			/* Now create the event_actionref relationship. This will always assume that the new event list will
			   point to a default action. Availmon has a different private action. So this is checked for here.
			   The default action is setup based on ENABLE_DEFAULT_ACTIONS field in the base_events list. 1 is to
			   setup action, 0 is no action. However, if the action has been setup with the UI, there is no effect.
			   This comes into place only when the event is a new event. */

			if(mylisthead->action_enabled)
			{
				if (mylisthead->class == 4000)
				{
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
						"insert into event_actionref values('%s',NULL,%d,%d,2)",system_info_ptr->sys_id,
						mylisthead->class,mylisthead->type)))
					{
						fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
						ssdbGetLastErrorString(error_handle));
						return 0;
					}
				}
				else
				{
					if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
						"insert into event_actionref values('%s',NULL,%d,%d,1)",system_info_ptr->sys_id,
						mylisthead->class,mylisthead->type)))
					{
						fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
						ssdbGetLastErrorString(error_handle));
						return 0;
					}
				}
				ssdbFreeRequest(error_handle,req_handle);
			}
		}	
		else
		{
			/* Event CLASS exists and here look only if the class description has changed */
			
			update = FALSE;
                        mydblist = newdbhead;
                        while (mydblist->next != NULL)
                        {
                                if (mylisthead->class == mydblist->class){
                                        if (strcmp(mylisthead->class_desc,mydblist->class_desc)){
                                                update = TRUE;
                                        }
                                        break;
                                }
                                mydblist = mydblist->next;
                        }
                        if (update)
                        {
                                /* update the description only for the CLASS */
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
					"update event_class set class_desc = \"%s\" where class_id = %d",
					mylisthead->class_desc,mydblist->class)))
                                {
                                        fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
                                        ssdbGetLastErrorString(error_handle));
                                        return 0;
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                        }

		}
		mylisthead = mylisthead->next;
	}
	free_dblist(newdbhead);
	return 1;
}

int get_system_info(SYSTEM_TABLE *system_info_ptr)
{

        module_info_t mod_info,low_mod_info;
        clock_t timevar;
        int hostid,ret_val,num_modules,i;
        char ip_buf[32],dest[10];
	char myhost[256];
        char *str = ip_buf;


	/* Sme huge number to get lowest module id */
	low_mod_info.mod_num = 99999999;
	num_modules = get_num_modules();
	for (i = 0; i < num_modules; i++)
	{
		get_module_info(i,&mod_info, sizeof(module_info_t));
		if (mod_info.mod_num < low_mod_info.mod_num)
			memcpy(&low_mod_info,&mod_info,sizeof(module_info_t));
	}
        if (low_mod_info.mod_num < 0 || low_mod_info.mod_num == 99999999)
		return 0;
        else
	{
                sprintf(system_info_ptr->sys_id,"%llX",low_mod_info.serial_num);
		if (low_mod_info.serial_str[0])
			strcpy(system_info_ptr->sys_serial,low_mod_info.serial_str);
		else
			strcpy(system_info_ptr->sys_serial,"Unknown");
        }
        hostid = gethostid();
        sprintf(system_info_ptr->ip_address,"%d.%d.%d.%d",(hostid >> 24) & 0xff,(hostid >> 16) & 0xff, (hostid >> 8) & 0xff, hostid & 0xff);
        if (!gethostname(myhost,256))
		get_actual_host(system_info_ptr,myhost);
        else
                strcpy(system_info_ptr->hostname,"Unknown");
        system_info_ptr->time = time(NULL);
        system_info_ptr->active = 1;
        system_info_ptr->local = 1;
        sysinfo(SI_MACHINE,ip_buf,32);
	str++;
	str++;
	system_info_ptr->sys_type = atoi(str);
	return (1);
}

int qualified_hostname(char *hostname, char *str)
{
	int len;
	len = strlen(hostname);
	if (strlen(str) < len) {
                return(0);
        }

	if (!strncasecmp(str, hostname, len)) {
		if (str[len] == '.') {
                        return(1);
                }
        }
        return(0);
}


void get_actual_host(SYSTEM_TABLE *ptr,char *name)
{
	int i =0, len;
	struct hostent *hostentp;
	char hostname[256], *hnp = (char*)NULL;
        char *cp;
	if (name) {
		strcpy(hostname, name);
	}
	else{
		if(gethostname(hostname, 256) < 0){
			strcpy(ptr->hostname,"Unknown");
			return;
		}
	}
	if (hostentp = gethostbyname(hostname)) {
		if (qualified_hostname(hostname, hostentp->h_name)) {
			strcpy(ptr->hostname,hostentp->h_name);
			return;
		}	
		if (hostentp->h_aliases) {
			cp = hostentp->h_aliases[0];
		}
		else
		{
			strcpy(ptr->hostname,hostentp->h_name);
			return;
		}
		while (cp) {
			if (qualified_hostname(hostname, cp)) {
				strcpy(ptr->hostname,hostentp->h_name);
				return;
			}
			cp = hostentp->h_aliases[i++];
		}
	}
	strcpy(ptr->hostname,hostname);
	return;
}

int check_system_info(SYSTEM_TABLE *system_info_ptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{
        int number_of_records,stringlen,field_number;
	ssdb_Request_Handle req_handle;
	char host[256],ip[64];
	system_info_ptr->hostchange = FALSE;
	system_info_ptr->id_changed = FALSE;
        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select * from system where active = 1 and local = 1")))
        {
                fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return -1;
        }

        number_of_records = ssdbGetNumRecords(error_handle,req_handle);
        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
        {
                fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
        	ssdbFreeRequest(error_handle,req_handle);
                return -1;
        }
	if (!number_of_records)
	{
	    ssdbFreeRequest(error_handle,req_handle);
	    return 0;	
	}

	/* Get the sysid field */
	field_number = 1;
	stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,field_number);
	ssdbRequestSeek(error_handle,req_handle,0,field_number);
	if (!ssdbGetNextField(error_handle,req_handle,&(system_info_ptr->dbsys_id),stringlen+1))
	{
                        fprintf(stderr,"0 API Error %d: \"%s\" Field Error\n",
                        ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                        return -1;
	}	

	/* Get the hostname field */
	field_number = 4;
	stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,field_number);
	ssdbRequestSeek(error_handle,req_handle,0,field_number);
	if (!ssdbGetNextField(error_handle,req_handle,host,stringlen+1))
	{
                        fprintf(stderr,"1 API Error %d: \"%s\" Field Error\n",
                        ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                        return -1;
        }

	/* Get the IP address */
	field_number++;
	stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,field_number);
        ssdbRequestSeek(error_handle,req_handle,0,field_number);
        if (!ssdbGetNextField(error_handle,req_handle,ip,stringlen+1))
        {
                        fprintf(stderr,"2 API Error %d: \"%s\" Field Error\n",
                        ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                        return -1;
        }
        ssdbFreeRequest(error_handle,req_handle);

	if(strcmp(system_info_ptr->hostname,host))
		system_info_ptr->hostchange = TRUE;
	if(strcmp(system_info_ptr->ip_address,ip))
		system_info_ptr->hostchange = TRUE;
	if(strcmp(system_info_ptr->dbsys_id,system_info_ptr->sys_id))
		system_info_ptr->id_changed = TRUE;
	return 1;
}

TOOLS * read_dbtools(ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{

	TOOLS *dblistptr;
        ssdb_Request_Handle req_handle;
        int number_of_records, number_of_fields, rec_sequence,stringlen,nextfield;
	TOOLS *tool_dbptr;
	if ((tool_dbptr = (TOOLS *)malloc(sizeof(TOOLS))) == NULL)
        {
                fprintf (stderr,"Memory allocation failure\n");
                return(0);
        }
	tool_dbptr->next = NULL;
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,"select * from tool")))
        {
                fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return 0;
        }
	number_of_records = ssdbGetNumRecords(error_handle,req_handle);
        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
        {
                fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                return 0;
        }

	number_of_fields  = ssdbGetNumColumns(error_handle,req_handle);
        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS){
                fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                return 0;
        }
	if (number_of_records)
        {
		for (rec_sequence = 0; rec_sequence < number_of_records; rec_sequence++)
		{
			dblistptr = (TOOLS *)malloc(sizeof(TOOLS));
			nextfield = 0;
			stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,nextfield);
			if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
			{
				fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}
			if (!ssdbGetNextField(error_handle,req_handle,dblistptr->tool_name,stringlen+1))
			{
				fprintf(stderr,"API Error %d: \"%s\" Field Error\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}
			nextfield++;
			stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,nextfield);
			if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
			{
				fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}
			if (!ssdbGetNextField(error_handle,req_handle,dblistptr->tool_option,stringlen+1))
			{
				fprintf(stderr,"API Error %d: \"%s\" Field Error\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}
			nextfield++;
			stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,nextfield);
			if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
			{
				fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}
			if (!ssdbGetNextField(error_handle,req_handle,dblistptr->option_default,stringlen+1))
			{
				fprintf(stderr,"API Error %d: \"%s\" Field Error\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}
			tool_dbptr = add_to_config(tool_dbptr,dblistptr);
		}
		ssdbFreeRequest(error_handle,req_handle);
        }
	return(tool_dbptr);	
}

DBLIST * dbread (DBLIST *dbhead,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{
	DBLIST *dblistptr;
	ssdb_Request_Handle req_handle;
	int number_of_records, number_of_fields, rec_sequence,stringlen,nextfield;

	/* Create a dummy node for the list for data read from ssdb */
        if ((dbhead = (DBLIST *)malloc(sizeof(DBLIST))) == NULL)
                return 0;
        dbhead -> next = NULL;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
		"select system.sys_id,event_class.class_id, event_class.class_desc, "
		"event_type.type_id,event_type.type_desc from system,event_class, event_type "
		"where event_class.class_id = event_type.class_id")))
	{
                fprintf(stderr,"API Error %d: \"%s\" - ssdbSendRequest\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return 0;
        }

	number_of_records = ssdbGetNumRecords(error_handle,req_handle);
        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
	{
                fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
		return 0;
        }

	number_of_fields  = ssdbGetNumColumns(error_handle,req_handle);
        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS){
                fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
		return 0;
        }

	if (number_of_records)
	{
		for (rec_sequence = 0; rec_sequence < number_of_records; rec_sequence++)
		{
			dblistptr = (DBLIST *)malloc(sizeof(DBLIST));
			nextfield = 0;
			stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,nextfield);
			if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
			{
				fprintf (stderr, "API Error %d: \"%s\" ssdbGetNumRecords\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}

			if (!ssdbGetNextField(error_handle,req_handle,dblistptr->sys_id,stringlen+1))
			{
				fprintf(stderr,"API Error %d: \"%s\" Field Error\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}

			nextfield++;

			if (!ssdbGetNextField(error_handle,req_handle,&(dblistptr->class),sizeof(dblistptr->class)))
			{
				fprintf(stderr,"API Error %d: \"%s\" Field Error\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}

			nextfield++;

			/* Get the size of the next field since this field is declared as a varchar */

			stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,nextfield);
			if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
			{
				fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}
			
			if (!ssdbGetNextField(error_handle,req_handle,dblistptr->class_desc,stringlen+1))
			{
				fprintf(stderr,"API Error %d: \"%s\" Field Error\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}

			nextfield++;

			if (!ssdbGetNextField(error_handle,req_handle,&(dblistptr->type),sizeof(dblistptr->class)))
			{
				fprintf(stderr,"API Error %d: \"%s\" Field Error\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}

			nextfield++;

			stringlen = ssdbGetFieldMaxSize(error_handle,req_handle,nextfield);
			if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
			{
				fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}

			if (!ssdbGetNextField(error_handle,req_handle,dblistptr->type_desc,stringlen+1))
			{
				fprintf(stderr,"API Error %d: \"%s\" Field Error\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				return 0;
			}
			dbhead = add_dblist(dbhead,dblistptr);
		}
		ssdbFreeRequest(error_handle,req_handle);
	}
	return (dbhead);
}

int create_system_record(SYSTEM_TABLE *system_info_ptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{
	ssdb_Request_Handle req_handle;


                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into system values(NULL, \"%s\",%d,\"%s\",\"%s\",\"%s\",1,1,%d)",
			system_info_ptr->sys_id,system_info_ptr->sys_type, system_info_ptr->sys_serial,
			system_info_ptr->hostname,system_info_ptr->ip_address, system_info_ptr->time)))
                {
                        fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                        ssdbGetLastErrorString(error_handle));
                        return 0;
                }
                ssdbFreeRequest(error_handle,req_handle);
	return 1;
}

/* This function will insert the first action record ONLY. If there are any records, it will simply return with a
   function success */

int create_default_action(SYSTEM_TABLE *system_info_ptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{
        ssdb_Request_Handle req_handle;
	int number_of_records;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT, "select action_id from event_action")))
        {
                fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return 0;
        }

	number_of_records = ssdbGetNumRecords(error_handle,req_handle);
        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
        {
                fprintf (stderr, "API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                ssdbFreeRequest(error_handle,req_handle);
                return 0;
        }
	ssdbFreeRequest(error_handle,req_handle);
	if (number_of_records)
		return number_of_records;


	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into event_action values('%s',NULL,'',1,0,'/usr/bin/espnotify -A \"%%D\"',0,10,'root','Notify sysadmin on console',0,0)",
		system_info_ptr->sys_id)))
	{
                fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return 0;
        }
	ssdbFreeRequest(error_handle,req_handle);

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into event_action values('%s',NULL,'',1,0,'/usr/etc/amformat -a -O %%O %%T,%%D',0,400,'root','Run Amformat',1,0)",
		system_info_ptr->sys_id)))
	{
                fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return 0;
        }
	ssdbFreeRequest(error_handle,req_handle);
	return 1;
}
int update_system_records(SYSTEM_TABLE *system_info_ptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{

	ssdb_Request_Handle req_handle;

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update system set active = 0,local = 0 where sys_id = \"%s\"",system_info_ptr->dbsys_id)))
	{
		fprintf(stderr,"API %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
		ssdbGetLastErrorString(error_handle));
		return 0;
	}

	ssdbFreeRequest(error_handle,req_handle);

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
		"insert into system values(NULL, \"%s\",%d,\"%s\",\"%s\",\"%s\",1,1,%d)",
		system_info_ptr->sys_id,system_info_ptr->sys_type, system_info_ptr->sys_serial, system_info_ptr->hostname,
		system_info_ptr->ip_address, system_info_ptr->time)))
	{
		fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
		ssdbGetLastErrorString(error_handle));
		return 0;
	}

	ssdbFreeRequest(error_handle,req_handle);

	if(system_info_ptr->id_changed){
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update event_class set sys_id = \"%s\" where sys_id = \"%s\"",
		system_info_ptr->sys_id,system_info_ptr->dbsys_id)))
	{
		fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
		ssdbGetLastErrorString(error_handle));
		return 0;
	}

	ssdbFreeRequest(error_handle,req_handle);

        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update event_type set sys_id = \"%s\" where sys_id = \"%s\"",
		system_info_ptr->sys_id,system_info_ptr->dbsys_id)))
	{
		fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
		ssdbGetLastErrorString(error_handle));
		return 0;
	}

        ssdbFreeRequest(error_handle,req_handle);

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update event_actionref set sys_id = \"%s\" where sys_id = \"%s\"",
		system_info_ptr->sys_id,system_info_ptr->dbsys_id)))
	{
		fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
		ssdbGetLastErrorString(error_handle));
		return 0;
	}

	ssdbFreeRequest(error_handle,req_handle);

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
                "update event_action set  sys_id = \"%s\" where sys_id = \"%s\"",
		system_info_ptr->sys_id,system_info_ptr->dbsys_id)))
	{
                fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return 0;
        }
}
	return 1;
}

void free_eventlist(CLASS_TYPE *head)
{
        CLASS_TYPE *node_to_free;
        CLASS_TYPE *newclass = head;
        while (newclass->next != NULL)
        {
		node_to_free = newclass;
		newclass = newclass->next;
		free(node_to_free);
        }
	free (newclass);
}

void free_dblist(DBLIST *head)
{
        DBLIST *node_to_free;
        DBLIST *newclass = head;
        while (newclass->next != NULL)
        {
		node_to_free = newclass;
		newclass = newclass->next;
		free(node_to_free);
        }
	free (newclass);
}


int create_sssconfig(ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle,SYSTEM_TABLE *system_info_ptr)
{
        ssdb_Request_Handle req_handle;
        int number_of_records;

        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,"select mode from sss_config")))
        {
                fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return 0;
        }

	number_of_records = ssdbGetNumRecords(error_handle,req_handle);
        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
        {
                fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                ssdbFreeRequest(error_handle,req_handle);
                return 0;
	}
	ssdbFreeRequest(error_handle,req_handle);
	if(!number_of_records)
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into sss_config values(1,0,0,0)")))
		{

			fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
			ssdbGetLastErrorString(error_handle));
			return 0;
		}
        }
	
	ssdbFreeRequest(error_handle,req_handle);
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,"select enabled from ssrv_ipaddr")))
        {
                fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
                return 0;
        }

        number_of_records = ssdbGetNumRecords(error_handle,req_handle);
        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
        {
                fprintf (stderr,"API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                ssdbFreeRequest(error_handle,req_handle);
                return 0;
        }
	ssdbFreeRequest(error_handle,req_handle);
	if (!number_of_records)
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into ssrv_ipaddr values('*.*.*.*',0)")))
		{

			fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
			ssdbGetLastErrorString(error_handle));
			return 0;
		}

		ssdbFreeRequest(error_handle,req_handle);
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into ssrv_ipaddr values('127.0.0.0',1)")))
		{

				fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
				ssdbGetLastErrorString(error_handle));
				return 0;
		}
		ssdbFreeRequest(error_handle,req_handle);

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into ssrv_ipaddr values('127.0.0.1',1)")))
		{

				fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
				ssdbGetLastErrorString(error_handle));
				return 0;
		}

		ssdbFreeRequest(error_handle,req_handle);

		/* if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into ssrv_ipaddr values('%s',1)",system_info_ptr->ip_address)))
		{

				fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
				ssdbGetLastErrorString(error_handle));
				return 0;
		}
		ssdbFreeRequest(error_handle,req_handle); */

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into key_table values('userentry',null,'000D59B5CEF44105403F59095B28FE','000753281F85B31842')")))
		{

				fprintf(stderr,"API Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
				ssdbGetLastErrorString(error_handle));
				return 0;
		}
		ssdbFreeRequest(error_handle,req_handle);
	}

		
		return 1;
}	

void printlist(CLASS_TYPE *head)
{
        CLASS_TYPE *newclass = head;
        while (newclass->next != NULL)
        {
                printf("%d\t%s\t%X\t%s\t%d\t%d\t%d\n",newclass->class,newclass->class_desc,newclass->type,newclass->type_desc,
                        newclass->throttled,newclass->throttle_val,newclass->threshold);
                newclass = newclass->next;
        }
}

void print_tools(TOOLS *head)
{
        TOOLS *newclass = head;
        while (newclass->next != NULL)
        {
                printf("%s\t%s\t%s",newclass->tool_name,newclass->tool_option,newclass->option_default);
                newclass = newclass->next;
        }
}


void printdblist(DBLIST *head)
{
        DBLIST *newdblist = head;
        while (newdblist->next != NULL)
        {
                printf("%d\t%s\t%d\t%s\n",newdblist->class,newdblist->class_desc,newdblist->type,newdblist->type_desc);
                newdblist = newdblist->next;
        }
}
