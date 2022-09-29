/*
 * Copyright 1992-1998 Silicon Graphics, Inc.
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

/*
 * Program Name : espquery.c
 * Executable   : espquery
 * Description  : This program is a command line program that can be used to
 *                perform system support software database operations. The
 *                command however will allow read only SQL queries. The data
 *                is output onto standard out.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ssdbapi.h>
#include <ssdberr.h>

/* globals for parsing input */

static char szDefaultUserName[64] = "";             /* Default user name */
static char szDefaultPassword[32] = "";             /* Default password */
static char szDefaultHostName[64] = "localhost";    /* Default host name */
static char szDefaultDatabaseName[64] = "ssdb";         /* Default db name */
static char szDefaultSqlString[1024] = "";          /* Default SQL string */
static int titles = 1;                              /* Display output with titles */
int SQL_TYPE;
void print_avail_options();
void print_usage();
int print_dbquery_data(ssdb_Client_Handle client,ssdb_Error_Handle error_handle,ssdb_Connection_Handle connection,
		ssdb_Request_Handle req_handle,int titles);
void ssdb_exit(ssdb_Client_Handle client, ssdb_Error_Handle error_handle, ssdb_Connection_Handle connection,
               ssdb_Request_Handle req_handle);

int main (int argc, char *argv[])
{
        ssdb_Client_Handle client;
        ssdb_Connection_Handle connection;
        ssdb_Request_Handle req_handle;
        ssdb_Error_Handle error_handle;
	client = NULL;
	connection = NULL;
	req_handle = NULL;
	error_handle = NULL;


	if (parse_dbquery_args(argc, argv))
	{
		return 0;
	}

	if(!ssdbInit())
	{
		fprintf(stderr,"Database API Error %d: \"%s\"\n",ssdbGetLastErrorCode(0),
                        ssdbGetLastErrorString(0));
		exit(1);
        }

	/* Establish a error handle for all recovering Database API Errors */

        if ((error_handle = ssdbCreateErrorHandle()) == NULL)
	{
                fprintf(stderr,"Database API Error %d: \"%s\" - can't create error handle\n",
                        ssdbGetLastErrorCode(0),ssdbGetLastErrorString(0));
		ssdb_exit(client, error_handle,connection,req_handle);
		exit(1);
        }

	/* Establish a new client */

        if ((client = ssdbNewClient(error_handle,szDefaultUserName,szDefaultPassword,0)) == NULL)
	{
                fprintf(stderr,"Database API Error %d: \"%s\" - can't create new client handle\n",
                        ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
		ssdb_exit(client, error_handle,connection,req_handle);
                exit(1);
        }


	/* Establish a connection to a database */
	if ((connection = ssdbOpenConnection(error_handle,client,szDefaultHostName,szDefaultDatabaseName,0)) == NULL)
	{
                fprintf(stderr,"Database API Error %d: \"%s\" - Invalid/missing database name\n\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
		print_usage();
		ssdb_exit(client, error_handle,connection,req_handle);
                exit(1);
        }

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SQL_TYPE,szDefaultSqlString)))
	{
                fprintf(stderr,"\nDatabase Error %d : %s\n\n",ssdbGetLastErrorCode(error_handle),
                ssdbGetLastErrorString(error_handle));
		ssdb_exit(client, error_handle,connection,req_handle);
                exit(1);
        }

	if(!print_dbquery_data(client,error_handle,connection,req_handle,titles))
	{
		ssdb_exit(client, error_handle,connection,req_handle);
		exit(1);
	}
	else
		ssdb_exit(client, error_handle,connection,req_handle);
        return 0;

}

int parse_dbquery_args(int argc, char *argv[])
{
	int c;
	char tempbuf[1024];
	char *sqptype;
	int retcode = 0;
	if (argc < 2)
	{
		print_usage();
		return ++retcode;
	}
	
	strcpy(szDefaultDatabaseName,argv[argc-1]);
	while ((c = getopt(argc, argv, "n:p:u:s:ht")) != EOF)
	{
		switch (c)
		{
			case '?':
				print_usage();
                                retcode++;
				break; 

			case 'n':
				strcpy (szDefaultHostName,optarg);
				break;
	
			case 'p':
				strcpy (szDefaultPassword,optarg);
				break;

			case 's':
				strcpy (szDefaultSqlString,optarg);
				strcpy (tempbuf,optarg);
				sqptype = strtok(tempbuf, " ");
				if(sqptype && !strcasecmp(sqptype,"select"))
					SQL_TYPE = SSDB_REQTYPE_SELECT;
				else if(sqptype && !strcasecmp(sqptype,"show"))
					SQL_TYPE = SSDB_REQTYPE_SHOW;
				else if(sqptype && !strcasecmp(sqptype,"describe"))
					SQL_TYPE = SSDB_REQTYPE_DESCRIBE;
				else{
					print_avail_options();
					retcode++;
				}
				break;
			
			case 't':
				titles = 0;
				break;

			case 'u':
				strcpy (szDefaultUserName,optarg);
				break;		

			case 'h':
				 print_usage();
                                 retcode++;
                                 break;

			default: print_usage();
				 retcode++;
				 break;
		}
	}
	return retcode;
}

/* Function to print the usage of the dbquery command */

void print_usage()
{
	fprintf (stderr,"\nUsage:\nespquery [-th] [-u username] [-p password] [-n hostname] -s sqlstring dbname\n\n");
}


/* dbquery supports only operations that are query by nature. Operations that alter contents of the database
   are not supported. This function just prints out the operations supported.
*/

void print_avail_options()
{
        fprintf (stderr,"\nThis operation is not supported. Supported SQL options:\n");
        fprintf (stderr,"SELECT\nDESCRIBE\nSHOW\n\n");
}

/* Function to gracefully exit at all times. This will take care to make sure that there are no memory
   leaks due to open connections, Database API structures etc */

void ssdb_exit(ssdb_Client_Handle client, ssdb_Error_Handle error_handle, ssdb_Connection_Handle connection,
               ssdb_Request_Handle req_handle)
{
        if (req_handle)
                ssdbFreeRequest(error_handle,req_handle);

        if (ssdbIsConnectionValid(error_handle,connection))
                ssdbCloseConnection (error_handle,connection);

        if (ssdbIsClientValid(error_handle,client))
                ssdbDeleteClient(error_handle,client);

        if (error_handle)
                ssdbDeleteErrorHandle(error_handle);

        ssdbDone();
}

/* Prints the output is a table format. The option of no titles will print only | seperated fields */

int print_dbquery_data(ssdb_Client_Handle client,ssdb_Error_Handle error_handle,ssdb_Connection_Handle connection,
                   ssdb_Request_Handle req_handle,int titles)
{
	char field_name[64];
	int field_sizes[64],name_sizes[64];
	int number_of_records,number_of_fields,rec_sequence,num_flags,total_length,fields,retcode,length;
	const char **vector_data;
	
	retcode = 0;

	/* get number of records */

	number_of_records = ssdbGetNumRecords(error_handle,req_handle);

        if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
        {
                fprintf (stderr,"Database API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                ssdb_exit(client, error_handle,connection,req_handle);
		return ++retcode;
        }

	if (number_of_records == 0)
	{
		fprintf(stderr,"\n\nEmpty set\n\n");
		return retcode++;
	};


	/* Get the number of fields */

	number_of_fields  = ssdbGetNumColumns(error_handle,req_handle);
	if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
        {
                fprintf (stderr,"Database API Error %d: \"%s\" ssdbGetNumRecords\n",
                ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                ssdb_exit(client, error_handle,connection,req_handle);
                return ++retcode;
        }


	
	/* Read in the maximum sizes of the fields and the field names. This is the header
	   name that is output when the query is executed without -t option */

        total_length = 0;
        for (fields=0; fields < number_of_fields; fields++)
        {
                field_sizes[fields] = ssdbGetFieldMaxSize(error_handle,req_handle,fields);
		if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
		{
			fprintf (stderr,"Database API Error %d: \"%s\" ssdbGetNumRecords\n",
			ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
			ssdb_exit(client, error_handle,connection,req_handle);
			return ++retcode;
        	}

                ssdbGetFieldName(error_handle,req_handle,fields,field_name,sizeof(field_name));
		if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
		{
                        fprintf (stderr,"Database API Error %d: \"%s\" ssdbGetNumRecords\n",
                        ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
                        ssdb_exit(client, error_handle,connection,req_handle);
                        return ++retcode;
                }

	/* Determine the total size of the output table */

                name_sizes[fields] = strlen(field_name);
                if (field_sizes[fields] < name_sizes[fields])
                        total_length = total_length + name_sizes[fields]+1;
                else
                        total_length = total_length + field_sizes[fields]+1;
        }

	
	/* Print header information if the titles option is enables */

	if (titles)
	{
		for (fields=0; fields < total_length+1; fields++)
			printf("-");
			printf("\n");

		printf("|");
		for (fields=0; fields < number_of_fields; fields++)
		{
			ssdbGetFieldName(error_handle,req_handle,fields,field_name,sizeof(field_name));
			if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
			{
				fprintf (stderr,"Database API Error %d: \"%s\" ssdbGetNumRecords\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
				ssdb_exit(client, error_handle,connection,req_handle);
				return ++retcode;
			}
			name_sizes[fields] = strlen(field_name);
			printf("%-*s|",field_sizes[fields],field_name);
		}
		printf("\n");
		for (fields=0; fields < total_length+1; fields++)
			printf("-");
			printf("\n");
	}

       /* Get data back in the form of strings (as a vector of strings). The data is already there
          based on the query. The flag is really meant to be used to justify between numbers and strings. 
	  However, for lack of support at this time on the DB API to indicate type of data coming back
	  everything is justified to the left side. */

	ssdbRequestSeek(error_handle,req_handle,0,0);

        for (rec_sequence = 0; rec_sequence < number_of_records; rec_sequence++)
        {
                vector_data = ssdbGetRow(error_handle,req_handle);
                if (vector_data)
                {
                printf("|");
                num_flags = 0;
                        for (fields = 0; fields < number_of_fields; fields++){
                                if (field_sizes[fields] < name_sizes[fields])
                                        length = name_sizes[fields];
                                else
                                        length = field_sizes[fields];
                                printf (num_flags ? "%*s|" : "%-*s|",length,vector_data[fields] ?
                                (char *)vector_data[fields] : "NULL");
			}
                printf("\n");
                }
        }

	/* print the bottom line if titles option has been chosen */
	if (titles)
	{
		for (fields=0; fields < total_length+1; fields++)
			printf("-");
		printf("\n");
	}
	return retcode;
}
