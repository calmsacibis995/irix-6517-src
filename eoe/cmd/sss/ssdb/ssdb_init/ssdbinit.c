#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ssdbapi.h>
#include <ssdberr.h>
#include <unistd.h>
#include <stdarg.h>

#define CONNECT 1000
#define TRUE 1
#define FALSE 0
int get_sql_string(FILE *fp, char *sqlbuffer);
int get_sql_type(char *sqlbuffer);
int parse_arguments (int argc, char *argv[]);
int exec_command_line(void);
exec_from_file(void);

static void static_exit();
static char szDefaultUserName[64] = "";             /* Default user name */
static char szDefaultPassword[32] = "";             /* Default password */
static char szDefaultHostName[64] = "localhost";    /* Default host name */
static char szDefaultDatabaseName[64] = "";         /* Default db name */
static char szDefaultSqlString[1024] = "";          /* Default SQL string */
static char szDefaultFileName[64] = "";             /* Default File name */

ssdb_Client_Handle client;
ssdb_Connection_Handle connection;
ssdb_Request_Handle req_handle;
ssdb_Error_Handle error_handle;

int main (int argc, char *argv[])
{
	FILE *fp;
        char *dbname;
	char sqlbuffer[1024];
	int SQL_TYPE;	
	int file = FALSE;
	client = connection = req_handle = error_handle = NULL;

	atexit(static_exit);

	if (geteuid() != 0)
	{
		fprintf (stderr,"Need to be root to run this program\n");
		return 0;
	}
	if (parse_arguments(argc, argv))
	{
		return 0;
        }

	if(strlen(szDefaultFileName))
	{
		file = TRUE;
	}

		/* Initialize SSDB API internal structures */
		if(!ssdbInit())
		{
			printf("SSDB API Error %d: \"%s\"\n",ssdbGetLastErrorCode(0),ssdbGetLastErrorString(0));
			exit(1);
		}

		/* Establish a error handle for all recovering SSDB API Errors */
		if ((error_handle = ssdbCreateErrorHandle()) == NULL)
		{
			printf("SSDB API Error %d: \"%s\" - can't create error handle\n",
				ssdbGetLastErrorCode(0),ssdbGetLastErrorString(0));
			exit(1);
		}

		/* Establish a client handle */
		if ((client = ssdbNewClient(error_handle,NULL,NULL,0)) == NULL)
		{
			printf("SSDB API Error %d: \"%s\" - can't create new client handle\n",
				ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
			exit(1);
		}
		
		if(file)
		{
			if (exec_from_file())
				exit(1);
		}
		else
		{
			if (exec_command_line())
                                exit(1);
		}
	exit(0);
}

int exec_from_file (void)
{
	int SQL_TYPE;
	FILE *fp;
	char *dbname;
	int retcode = 0;

		if ((fp = fopen(szDefaultFileName,"r")) == NULL)
                {
                        printf("Invalid Argument\n");
			return ++retcode;
                }

		while(!feof(fp))
                {
                        if(!get_sql_string (fp, szDefaultSqlString))
                        {
                                SQL_TYPE = get_sql_type(szDefaultSqlString);
                                if (SQL_TYPE == CONNECT)
                                {
                                        dbname = strtok(szDefaultSqlString, " ");
                                        dbname = strtok(NULL, " ");

                                        /* If there is already a connection and the text file is switching to a
                                           another database, close the previous connection and open a new one. */

                                        if (connection)
                                        {
                                                ssdbCloseConnection(error_handle,connection);
                                                connection = NULL;
                                        }

                                        /* Invalid database names will bail this program out */

                                        if ((connection = ssdbOpenConnection(error_handle,client,NULL,dbname,0)) == NULL)
                                        {
                                                printf ("Invalid database\n");
                                                return ++retcode;
                                        }
                                }
                                else
                                {
                                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SQL_TYPE,szDefaultSqlString)))
					{
                                                printf("Error in executing SQL string %s\n", szDefaultSqlString);
                                        	ssdbFreeRequest(error_handle,req_handle);
						retcode++; 
					}
					ssdbFreeRequest(error_handle,req_handle);
                                }
                        }
                }
                fclose(fp);
	return retcode;
}

int exec_command_line(void)
{
	int SQL_TYPE;
	int retcode = 0;
	SQL_TYPE = get_sql_type(szDefaultSqlString);
	/* If there is already a connection and the text file is switching to a
	another database, close the previous connection and open a new one. */

	if (connection)
	{
		ssdbCloseConnection(error_handle,connection);
		connection = NULL;
	}

	/* Invalid database names will bail this program out */

	if ((connection = ssdbOpenConnection(error_handle,client,NULL,szDefaultDatabaseName,0)) == NULL)
	{
		printf ("Invalid database");
		return ++retcode;
	}

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SQL_TYPE,szDefaultSqlString)))
	{
		printf("Error in executing SQL string %s\n", szDefaultSqlString);
		ssdbFreeRequest(error_handle,req_handle);
		return ++retcode;
	}
	ssdbFreeRequest(error_handle,req_handle);
	return 0;
}

void print_usage()
{
        printf ("\nUsage: initssdb [-d database name] [-f filename] [-s sqlstring]\n\n");
}

int parse_arguments (int argc, char *argv[])
{
	int c;
	int retcode = 0;
	if (argc < 2)
	{
		print_usage();
		return ++retcode;
	}

	while ((c = getopt(argc, argv, "d:f:s:")) != EOF)
	{
		switch (c)
		{
			case '?':
				print_usage();
                                return ++retcode;
			case 'd':
				strcpy(szDefaultDatabaseName,optarg);
				break;
			case 's':
				strcpy (szDefaultSqlString,optarg);
				break;
			case 'f':
				strcpy (szDefaultFileName,optarg);
				break;
			default :
				print_usage();
				return ++retcode;
		}
	}
	return retcode;
}
int get_sql_string (FILE *fp, char *sqlbuffer)
{
	char buffer[256];
	char readbuffer[1024];
	char *k,*p;
	bzero(readbuffer,1024);
	fgets(buffer, 256,fp);
	while(!feof(fp))
	{
		buffer[strlen(buffer)-1] = '\0';
		if(strlen(buffer) > 1)
		{
			k = strtok(buffer,"\t");
			while (!k)
				k = strtok(NULL,"\t");
			strcat(readbuffer,k);
			p = strstr(readbuffer,";");
			if(p)
			{
				readbuffer[strlen(readbuffer) - 1] = '\0';
				strcpy(sqlbuffer,readbuffer);
				return 0;
			}
			strcat(readbuffer," ");
		}
		fgets(buffer, 256,fp);
	}	
	return 1;
}

int get_sql_type (char *sqlbuffer)
{
	char buffer[1024];
	char *ptr_to_sqltype;
	strcpy (buffer, sqlbuffer);
	ptr_to_sqltype = strtok(buffer, " ");
	if(!strcasecmp(ptr_to_sqltype,"select"))
		return(SSDB_REQTYPE_SELECT);
	if(!strcasecmp(ptr_to_sqltype,"insert"))
		return (SSDB_REQTYPE_INSERT);
	if(!strcasecmp(ptr_to_sqltype,"delete"))
		return (SSDB_REQTYPE_DELETE);
	if(!strcasecmp(ptr_to_sqltype,"create"))
		return (SSDB_REQTYPE_CREATE);
	if(!strcasecmp(ptr_to_sqltype,"update"))
		return (SSDB_REQTYPE_UPDATE);
	if(!strcasecmp(ptr_to_sqltype,"alter"))
		return (SSDB_REQTYPE_ALTER);
	if(!strcasecmp(ptr_to_sqltype,"connect"))
		return (CONNECT);
	if(!strcasecmp(ptr_to_sqltype,"lock"))
		return (SSDB_REQTYPE_LOCK);
	if(!strcasecmp(ptr_to_sqltype,"unlock"))
		return (SSDB_REQTYPE_UNLOCK);
	if(!strcasecmp(ptr_to_sqltype,"drop"))
	{
		ptr_to_sqltype = strtok(NULL," ");
		if (!strcasecmp(ptr_to_sqltype,"table"))
			return (SSDB_REQTYPE_DROPTABLE);
		if (!strcasecmp(ptr_to_sqltype,"database"))
			return (SSDB_REQTYPE_DROPDATABASE);
	}
	return 0;
}

static void static_exit()
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
