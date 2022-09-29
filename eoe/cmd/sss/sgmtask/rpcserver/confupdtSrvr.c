/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

#ident           "$Revision: 1.6 $"

/*---------------------------------------------------------------------------*/
/* Include files                                                             */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ssdbapi.h>
#include <ssdberr.h>
#include <syslog.h>
#include "sgmtaskerr.h"
#include "sgmtask.h"
#include "sgmauth.h"

#define  MAXMEMORYALLOC                           4098

typedef struct table_detail_s {
    __uint32_t            headerFlag;
    __uint32_t            numColumns;
    char                  tablename[64];
    char                  *tableheader;
    char                  select[1024];
    char                  outFormat[64];
    struct table_detail_s *next;
} table_details_t;

typedef table_details_t  * TABLE_DETAIL;

#define    CONFIGTABLE                  "configmon"
#define    CONFIGTABLE_TABLENAMES       "table_name"
#define    TABLEDELIMITER               '#'
#define    HEADERDELIMITER              '!'
#define    RECORDDELIMITER              '@'

static int                      ssdb_init_done = 0;
static ssdb_Client_Handle       Client = 0;
static ssdb_Error_Handle        Error  = 0;
static ssdb_Connection_Handle   Connection = 0;
char   mysys_id[20];

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

__uint32_t CONFSSDBInit(int);
void SGMSSDBDone(int);
static int xtractTableData(TABLE_DETAIL, FILE *);
static int getTableHeader(TABLE_DETAIL);
static void printTableData(TABLE_DETAIL, int);
static int initTables(char *, TABLE_DETAIL *);
static TABLE_DETAIL newTableDetail(char *, int);
static int insertTableDetail(TABLE_DETAIL *, TABLE_DETAIL);
static void freeTableData(TABLE_DETAIL);
static int xtract_data(char *, int *, char *);
static __uint32_t xfrdata(char *, int, int, char *, int, char **, int *);
int confupdt_srvr(char *, char **, __uint32_t *, char *, __uint32_t);

int                            ssdbinit = 0;
extern   char *cmd;

__uint32_t CONFSSDBInit(int flag)
{
    __uint32_t   err = 0;
    int          ssdberr = 0;

    if ( ssdb_init_done ) return(0);

    if ( flag ) {
        if ( !ssdbInit() ) {
            ssdberr = ssdbGetLastErrorCode(NULL);
            if ( ssdberr != SSDBERR_ALREADYINIT1  || ssdberr != SSDBERR_ALREADYINIT2 )
                return((ssdberr > 0 ? ssdberr : -1*ssdberr));
        }
    }

    if ( (Error = ssdbCreateErrorHandle()) == NULL ) {
        ssdberr = ssdbGetLastErrorCode(NULL);
        return((ssdberr > 0 ? ssdberr : -1*ssdberr));
    }

    if ( (Client = ssdbNewClient(Error, SSDBUSER, SSDBPASSWORD, 0))
                 == NULL ) {
        ssdberr = ssdbGetLastErrorCode(NULL);
        if (Error) ssdbDeleteErrorHandle(Error);
        Error = 0;
        return((ssdberr > 0 ? ssdberr : -1*ssdberr));
    }

    if ( !(Connection = ssdbOpenConnection(Error, Client,
                        NULL, SSDATABASE, (SSDB_CONFLG_RECONNECT |
                        SSDB_CONFLG_REPTCONNECT))) ) {
        ssdberr = ssdbGetLastErrorCode(NULL);
        if (Client) ssdbDeleteClient(Error, Client);
        if (Error) ssdbDeleteErrorHandle(Error);
        Client = 0;
        Error = 0;
        return((ssdberr > 0 ? ssdberr : -1*ssdberr));
    }

    ssdb_init_done = 1;
    return(0);
}

void CONFSSDBDone(int flag)
{

    if ( !ssdb_init_done ) return;
    if ( Connection ) ssdbCloseConnection(Error, Connection);
    if (Client) ssdbDeleteClient(Error, Client);
    if (Error) ssdbDeleteErrorHandle(Error);
    Error = Connection = Client = 0;
    ssdb_init_done = 0;
    if ( flag ) ssdbDone();
    return;
}

static int CONFAUTHIGetAuth(char *hname, char *authstr)
{
    char                sqlstr[1024];
    ssdb_Request_Handle req = 0;
    char                tmphname[512];
    int                 NumRows = 0, NumCols = 0;
    __uint32_t          err = 0;
    int                 offset = 0;
    const char          **vector;
    int                 i = 0;

    if ( !hname || !authstr ) return(0);

    if ( !SGMNETIGetOffHostName(hname, tmphname) ) return(0);

    if ( !(req = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
	   "select auth_key,auth_mask,auth_encr,auth_pwd from sss_auth where auth_host like '%s%s'",
	   tmphname, "%%"))) {
	return(0);
    }

    if ( (NumRows = ssdbGetNumRecords(Error, req)) == 0 ) {
        goto end;
    } else if ( NumRows > 1 ) goto end;

    if ( (NumCols = ssdbGetNumColumns(Error, req)) == 0 ) {
	goto end;
    }

    ssdbRequestSeek(Error, req, 0, 0);
    vector = ssdbGetRow(Error, req);
    if ( vector ) {
        for(i = 0; i < NumCols; i++ ) {
	    offset += snprintf(authstr+offset, AUTHSIZE-offset, "%s:", vector[i]);
        }	
    }
    end:
    ssdbFreeRequest(Error, req);
    return(offset);
}


static int print_string(FILE *fp, char *ptr)
{
    int    found_squote = 0;
    int    found_dquote = 0;
    int    byteswritten = 0;
    int    i = 0;

    if ( !ptr || !fp ) return(0);

    while ( *(ptr+i) ) {
	if ( *(ptr+i) == '\'' ) found_squote++;
	else if ( *(ptr+i) == '"' ) found_dquote++;
	i++;
    }

    if ( (!found_squote && !found_dquote) ||
	 (found_dquote && !found_squote) ) {
	byteswritten = fprintf(fp, "'%s'", ptr);
    } else if ( found_squote && !found_dquote ) {
	byteswritten = fprintf(fp, "\"%s\"", ptr);
    } else {
	/* we found both types of quotes.  Lets replace one */
	i = 0;
	while ( *(ptr+i) ) {
	    if ( *(ptr+i) == '"' ) *(ptr+i) = '\'';
	    i++;
	}
	byteswritten = fprintf(fp, "\"%s\"", ptr);
    }

    return(byteswritten);
}


/*---------------------------------------------------------------------------*/
/* Name    : xtractTableData                                                 */
/* Purpose : Pulls out data from DB and puts it in a file for all req. tables*/
/* Inputs  : Error Handle, Connection Handle and File Handle                 */
/* Outputs : integer, 1 or 0                                                 */
/*---------------------------------------------------------------------------*/

static int xtractTableData (tabledata, fp)
    TABLE_DETAIL           tabledata;
    FILE                   *fp;            /* Open file handle */
{
    int                      ErrCode = 0;
    int                      i = 0, j = 0, k = 0, NumRows;
    int                      nobytes = 0;
    const char               **vector;
    ssdb_Request_Handle      Request;
    TABLE_DETAIL             tmpTableDetail;

    if ( fp == NULL || tabledata == NULL ) 
	return(0);
    
    tmpTableDetail =  tabledata;

    nobytes += fprintf(fp, "S %s\n", mysys_id);
    while ( tmpTableDetail != NULL ) {
	if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
					 tmpTableDetail->select))) {
	    ErrCode++;
	    goto end2;
	}

	if ( (NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) {
	} 

        nobytes += fprintf(fp, "%c %s %d %d %d\n", TABLEDELIMITER, 
		    tmpTableDetail->tablename, tmpTableDetail->headerFlag,
		    tmpTableDetail->numColumns, NumRows);

	if (tmpTableDetail->headerFlag && tmpTableDetail->tableheader != NULL) {
	    nobytes += fprintf(fp, "%c %s\n", HEADERDELIMITER,
				       tmpTableDetail->tableheader);
	}

	ssdbRequestSeek(Error, Request, 0, 0);
	j = 0;
	while ( j < NumRows ) {
	    vector = ssdbGetRow(Error, Request);
	    if ( vector ) {
		nobytes += fprintf(fp, "%c ", RECORDDELIMITER);
		for ( k = 0; k < tmpTableDetail->numColumns; k++ ) {
		    nobytes += fprintf(fp, "%c", ( (k == 0) ? ' ' : ',') );
		    switch (tmpTableDetail->outFormat[k]) {
			case '0':
			    nobytes += fprintf(fp, "NULL");
			    break;
			case '1':
			    /*nobytes += fprintf(fp, "'%s'", vector[k]);*/
			    nobytes += print_string(fp, (char *) vector[k]);
			    break;
			default:
			    nobytes += fprintf(fp, "%s", vector[k]);
			    break;
		    }
		}
		nobytes += fprintf(fp, "\n");
	    }
	    j++;
	}
	ssdbFreeRequest(Error, Request);
	tmpTableDetail = tmpTableDetail->next;
    }

    end2:
        if ( Request ) ssdbFreeRequest(Error, Request);
        if ( ErrCode ) return (0);
        return(nobytes);
}

/*---------------------------------------------------------------------------*/
/* Name    : getTableHeader                                                  */
/* Purpose : Takes the tables structure and populates it with select stmt.   */
/* Inputs  : Error Handle, Client Handle and table structure                 */
/* Outputs : 1 (success) or 0 (failure)                                      */
/*---------------------------------------------------------------------------*/


static int getTableHeader (tabledata)
     TABLE_DETAIL       tabledata;
{
    int    ErrCode = 0;
    char   sqlstmt[1024];
    char   tmpBuffer[4096];
    int    i, NumCols, NumRows;
    int    j, k;
    int    offset, autoflag, sqloffset;
    const  char **vector;
    ssdb_Request_Handle         Request = 0;
    TABLE_DETAIL                tmpTableDetail = NULL;

    if ( tabledata == NULL ) return(0);
    tmpTableDetail = tabledata;

    while (tmpTableDetail != NULL) {
	sprintf(sqlstmt, "show columns from %s", tmpTableDetail->tablename);
	if ( !(Request = ssdbSendRequest(Error, Connection, 
			 SSDB_REQTYPE_SHOW, sqlstmt))) {
	       ErrCode++;
	       goto end;
        }

	if ( (NumCols = ssdbGetNumColumns(Error, Request)) == 0 ) {
	    ErrCode++;
	    goto end;
	} 

	if ( (NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) {
	    ErrCode++;
	    goto end;
	}

	tmpTableDetail->numColumns = NumRows;

	ssdbRequestSeek(Error, Request, 0, 0);
	tmpBuffer[0] = '(';
	offset = 1;
	sqloffset = sprintf(tmpTableDetail->select, "select");

	for ( j = 0; j < NumRows; j++ ) {
	    autoflag = 0;
	    vector = ssdbGetRow(Error, Request);
	    if ( vector ) {
		/* Hack for configmon's system_info table.  A new table is introduced
		   (Table system_info) which is EXACTLY same as  system table */

		if ( !strncasecmp(vector[0], "local", 5) && 
		     !strcasecmp(tmpTableDetail->tablename, "system_info") ) {
		    sqloffset += sprintf(&tmpTableDetail->select[sqloffset],"%c 0 as %s",
			              ((j == 0) ? ' ' : ','), vector[0]);
		} else {
		    sqloffset += sprintf(&tmpTableDetail->select[sqloffset],"%c %s",
			              ((j == 0) ? ' ' : ','), vector[0]);
                }

		if ( strcmp(vector[5], "auto_increment") == 0 ) 
		    autoflag = 1;

		if ( tmpTableDetail->headerFlag ) {
		    offset += sprintf(&tmpBuffer[offset], "%s %s ", vector[0],
				      vector[1]);
		    if ( strlen(vector[2]) == 0 ) 
			offset += sprintf(&tmpBuffer[offset], "not null ");

                    if ( autoflag )
			offset += sprintf(&tmpBuffer[offset],"auto_increment ");

		    if ( strcmp(vector[3], "PRI") == 0 ) 
			offset += sprintf(&tmpBuffer[offset], "primary key");
		    offset += sprintf(&tmpBuffer[offset], ",");
		}

		if ( !autoflag ) {
		    if ( strncmp(vector[1], "char", 4) == 0 ||
		         strncmp(vector[1], "varchar", 7) == 0 ) {
		        tmpTableDetail->outFormat[j] = '1';  /* Character */
                    } else {
			tmpTableDetail->outFormat[j] = '2';  /* Any Int   */
		    }
		} else 
		    tmpTableDetail->outFormat[j] = '0';
	    }
	}

	if ( tmpTableDetail->headerFlag ) {
	    tmpBuffer[offset-1] = ')';
	    tmpBuffer[offset] = '\0';
	    tmpTableDetail->tableheader = strdup(tmpBuffer);
	}
	sprintf(&tmpTableDetail->select[sqloffset], " from %s where sys_id = '%s'%c", 
		 tmpTableDetail->tablename,mysys_id, '\0');
	ssdbFreeRequest(Error, Request);
	tmpTableDetail = tmpTableDetail->next;
    }

    end:
	if ( Request ) ssdbFreeRequest(Error, Request);
	if ( ErrCode ) return (0);
	return(1);
}

#ifdef DEBUG
/*---------------------------------------------------------------------------*/
/* Name    : printTableData                                                  */
/* Purpose : Debug function                                                  */
/* Inputs  : Not relevent                                                    */
/* Outputs : Not relevent                                                    */
/*---------------------------------------------------------------------------*/

static void printTableData(TABLE_DETAIL tabledata, int ntables)
{
    TABLE_DETAIL    tmpTableDetail;
    int             i = 0;

    if ( tabledata != NULL ) {
	tmpTableDetail = tabledata;
	for ( i = 0; i < ntables; i++ ) {
	    fprintf(stdout, "Details of : %s\n", tmpTableDetail->tablename);
	    fprintf(stdout, "\tnumColumns  : %d\n",tmpTableDetail->numColumns);
	    fprintf(stdout, "\ttableheader : %s\n",tmpTableDetail->tableheader);
	    fprintf(stdout, "\tselect      : %s\n",tmpTableDetail->select);
	    fprintf(stdout, "\toutFormat   : %s\n",tmpTableDetail->outFormat);
	    tmpTableDetail++;
	}
    }
}

#endif

static __uint32_t getSysID(void)
{
    ssdb_Request_Handle    Request = 0;
    int                    NumRows = 0, NumCols = 0;
    const char             **vector;
    int                    retCode = 0;

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
	    "select sys_id from system where active = 1 and local = 1"))) {
	return(ssdbGetLastErrorCode(Error));
    }

    if ( ((NumCols = ssdbGetNumColumns(Error, Request)) == 0 ) ||
	 ((NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) ) {
	retCode = ssdbGetLastErrorCode(Error);
	ssdbFreeRequest(Error, Request);
	return(retCode);
    }

    ssdbRequestSeek(Error, Request, 0, 0);
    vector = ssdbGetRow(Error, Request);
    if ( vector ) strcpy(mysys_id, vector[0]);
    ssdbFreeRequest(Error, Request);
    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : initTables                                                      */
/* Purpose : Reads client parameters and initializes the Table structure     */
/* Inputs  : Error Handle, Connection Handle, Client & Server data structures*/
/* Outputs : 1 (success) or 0 (failure)                                      */
/*---------------------------------------------------------------------------*/

static int initTables(clientdata, tabledetail)
        char *clientdata;
	TABLE_DETAIL *tabledetail;
{
    char               sqlstmt[1024];
    const char         **vector;
    ssdb_Request_Handle Request;
    int                i = 0;
    int                NumCols = 0, NumRows = 0;
    int                retCode = 1;
    int                headerFlag = 0;
    TABLE_DETAIL       tmpTableDetail;

    /* Form SQL Request */

    sprintf(sqlstmt, "SELECT %s from %s where %s != '%s'", CONFIGTABLE_TABLENAMES, 
	    CONFIGTABLE, CONFIGTABLE_TABLENAMES, CONFIGTABLE);

    /* Send request to SSDB */

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
				     sqlstmt))) {
	return(ssdbGetLastErrorCode(Error));
    }

    /* Look for NumCols, NumRows. If they are 0, then return 1 */

    if ( ((NumCols = ssdbGetNumColumns(Error, Request)) == 0 ) ||
	 ((NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) ) {
	retCode = ssdbGetLastErrorCode(Error);
	ssdbFreeRequest(Error, Request);
	return(retCode);
    }

    /* Otherwise, start reading from SSDB each column.  Look for the */
    /* value read in clientdata->tabledata. */

    ssdbRequestSeek(Error, Request, 0, 0);

    for ( i = 0; i < NumRows; i++ ) {
	vector = ssdbGetRow(Error, Request);
	if (vector) {
	    if ( (clientdata != NULL) &&
		 (strstr(clientdata, vector[0]) == NULL) ) {
		headerFlag = 1;
	    } else {
		headerFlag = 0;
	    }

	    if ( (tmpTableDetail = newTableDetail((char *) vector[0], headerFlag))
		 == NULL ) {
		ssdbFreeRequest(Error, Request);
		return(ERR(SERVERERR, OSERR,MALLOCERR));
            } 

	    insertTableDetail( tabledetail, tmpTableDetail );
	}
    }

    ssdbFreeRequest(Error, Request);
    return(0);
}


/*---------------------------------------------------------------------------*/
/* Name    : newTableDetail                                                  */
/* Purpose : Creates a new table node                                        */
/* Inputs  : Table name and headerFlag (whether a header needs to be included*/
/* Outputs : pointer to New Table Detail or NULL                             */
/*---------------------------------------------------------------------------*/

static TABLE_DETAIL newTableDetail(char *tablename, int headerFlag)
{
    TABLE_DETAIL    tmp;

    if ( tablename == NULL ) return (NULL);

    if ( (tmp = (TABLE_DETAIL) calloc(1, sizeof(table_details_t))) == NULL ) return(NULL);

    tmp->headerFlag = headerFlag;
    strcpy(tmp->tablename, tablename);
    tmp->next = NULL;

    return(tmp);
}

/*---------------------------------------------------------------------------*/
/* Name    : insertTableDetail                                               */
/* Purpose : inserts a node into a list                                      */
/* Inputs  : A list and a node                                               */
/* Outputs : 0 for success and 1 otherwise                                   */
/*---------------------------------------------------------------------------*/

static int insertTableDetail(TABLE_DETAIL *table, TABLE_DETAIL node)
{
    TABLE_DETAIL tmp;

    if ( node == NULL ) return(1);

    tmp = (*table);

    if ( tmp == NULL ) {
	(*table) = node;
    } else {
	while ( tmp->next != NULL ) tmp = tmp->next;
	tmp->next = node;
    }

    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : freeTableData                                                   */
/* Purpose : frees up a table data structure/list                            */
/* Inputs  : table list                                                      */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

static void freeTableData(TABLE_DETAIL tabledata)
{
    TABLE_DETAIL   tmpTableDetail;
    TABLE_DETAIL   tmpTableDetail1;
    int            i;

    if ( tabledata != NULL ) {
	tmpTableDetail = tabledata;
	while ( tmpTableDetail != NULL ) {
	    tmpTableDetail1 = tmpTableDetail->next;
	    free(tmpTableDetail);
	    tmpTableDetail = tmpTableDetail1;
	}
    }
}

/*---------------------------------------------------------------------------*/
/* Name    : validate_filename                                               */
/* Purpose : validate that this is our file                                  */
/* Inputs  : filename to validate                                            */
/* Return  : 0 if OK, non 0 if refuse                                        */
/*---------------------------------------------------------------------------*/

static int validate_filename ( char * filename )
{
    int    n;
    int    i;
    char * p;
    
    /* so we are allow only 
    "/tmp/sGmCXXXXXX" and 
    "/var/tmp/sGmCXXXXXX" format for our filenames
    */
    
    /* check prefix /tmp/sGmC */
    n = strlen("/tmp/sGmC");
    if ( strncmp ( filename, "/tmp/sGmC", n ) != 0 )
    { /* check prefix /var/tmp/sGmC */
      n = strlen("/var/tmp/sGmC");
      if ( strncmp ( filename, "/var/tmp/sGmC", n ) != 0 )
      { /* refuse, wrong prefix */
        return -1;
      }
    }
    
    p = filename + n;
    
    if ( strlen ( p ) > 6 ) 
         return -1;  /* no more then 6 characters */

    if ( strchr ( p, '.' ) != NULL ) 
         return -1;  /* no dots */
         
    if ( strchr ( p, '/' ) != NULL ) 
         return -1;  /* no slash */     

    if ( strchr ( p, '\\' ) != NULL ) 
         return -1;  /* no slashes */        

    if ( strchr ( p, '*' ) != NULL ) 
         return -1;  /* no stars */        

    if ( strchr ( p, '?' ) != NULL ) 
         return -1;  

    if ( strchr ( p, '{' ) != NULL ) 
         return -1;  

    if ( strchr ( p, '}' ) != NULL ) 
         return -1;  
    
    return 0;
}


static int xtract_data(char *ptr, int *datalen, char *file)
{
    __uint32_t    err = 0;
    char          filename[512];
    int           err1 = 0;
    TABLE_DETAIL  tables = NULL;
    struct stat   statb;
    int           nobytes = 0;
    FILE          *fp = NULL;

    if ( !ptr ) return (SSSERR(SERVERERR, PARAMERR, INVALIDARGS));

    if ( (err1 = initTables(ptr, &tables)) != 0 ) {
	return(SSSERR(SERVERERR, SSDBERR, (err1<0 ? -1*err1:err1)));
    }

    if ( (err1 = getSysID()) != 0 ) {
	err = SSSERR(SERVERERR, SSDBERR, (err1<0 ? -1*err1:err1));
	goto end;
    }

    if ( !getTableHeader(tables) ) {
	err = SSSERR(SERVERERR, SSDBERR, GETTABLEHEADER);
	goto end;
    }

    /* Get a filename */

    if ( stat("/tmp", &statb) == 0 ) {
	snprintf(filename, sizeof(filename), "/tmp/sGmCXXXXXX");
    } else if ( stat("/var/tmp", &statb) == 0 ) {
	snprintf(filename, sizeof(filename), "/var/tmp/sGmCXXXXXX");
    } else {
	err = ERR(SERVERERR, OSERR, errno);
	goto end;
    }

    mktemp(filename);
    
    /* pass it through validation routine */
    if ( validate_filename ( filename ) != 0 )
    {
	err = SSSERR(SERVERERR, PARAMERR, INVALIDARGS);
	goto end;
    }
    
    nobytes = 0;

    if ( (fp = fopen(filename, "w")) == NULL ) {
	err = ERR(SERVERERR, OSERR, errno);
	goto end;
    }

    if ( (nobytes = xtractTableData(tables, fp)) == 0 ) {
	err = SSSERR(SERVERERR, SSDBERR, XTRACTDATA);
	goto end;
    }

  end:

    if ( fp ) 
         fclose(fp);

    freeTableData(tables);
    strcpy(file, filename);
    *datalen = nobytes;
    return(err);
}

static __uint32_t xfrdata(char *file, int bytestotransfer, int fileoffset,
               char *header, int headerlen, char **retPtr, int *datalen)
{
    int             fildes;
    struct stat     statbuf;
    int             bytestoread;
    int             bytesremaining;
    int             tmp;
    char            *buf;
    int             attempts;


    if ( !file ) return(ERR(SERVERERR, PARAMERR, INVALIDARGS));

    if ( stat(file, &statbuf) < 0 ) {
	return(ERR(SERVERERR, OSERR, errno));
    }

    if ( !(statbuf.st_mode & S_IFREG) ) {
	return(ERR(SERVERERR, PARAMERR, NOTAREGULARFILE));
    }

    /* validate that this is our filename */

    if ( statbuf.st_size <= fileoffset ) {
	return(ERR(SERVERERR, PARAMERR, INVALIDOFFSET));
    }

    if ( (fildes = open(file, O_RDONLY)) < 0 ) {
	return(ERR(SERVERERR, OSERR, errno));
    }

    bytesremaining = statbuf.st_size - fileoffset;
    lseek(fildes, fileoffset, SEEK_SET);

    if ( bytestotransfer > 0 ) {
	bytestoread = ( (bytesremaining-bytestotransfer) > 0 ?
			 bytestotransfer : bytesremaining );
    } else {
	bytestoread = ( (bytesremaining-MAXMEMORYALLOC) > 0 ? MAXMEMORYALLOC :
			 bytesremaining);
    }

    if ( (*retPtr = (char *) calloc(bytestoread+10, 1)) == NULL ) {
	return(ERR(SERVERERR, OSERR, MALLOCERR));
    }

    tmp = 0;
    attempts = 0;
    buf = *retPtr;

    while ( attempts < 10 ) {
	tmp += read(fildes, buf, bytestoread);
	if ( tmp < bytestoread ) {
	    buf += tmp;
	    bytestoread -= tmp;
	} else break;

	attempts++;
    }

    snprintf(header, headerlen, "XFR %d %d %d %s DATA:",
		     tmp, bytesremaining-tmp, fileoffset+tmp, file);

    *datalen = tmp;
    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : xtractdata                                                      */
/* Purpose : Main entry point for PROCESSPROC                                */
/* Inputs  : client and server structures                                    */
/* Outputs : 1 for success or 0 otherwise                                    */
/*---------------------------------------------------------------------------*/

int confupdt_srvr(char *SGMHost, char **dPtr, __uint32_t *dLen,
                  char *data, __uint32_t datalen)
{
    int                    err = 0;
    char                   authStr[AUTHSIZE];
    char                   filename[512];
    char                   header[1024];
    char                   *mydata = NULL;
    char                   *retdata = NULL;
    int                    authlen = 0, authType = 0;
    int                    nobytes = 0;
    int                    ntables = 0;
    int                    bytestosend = 0, offset = 0;
    FILE                   *fp;


    memset(authStr, 0, AUTHSIZE);

    if ( !SGMHost || !data || datalen <= 0 )
        return(ERR(SERVERERR, PARAMERR, INCOMPLETEDATA));

    if ( dPtr == NULL || dLen == NULL ) 
	return(ERR(SERVERERR, PARAMERR, INVALIDDATAPTR));

    if ( (err = CONFSSDBInit(1)) != 0 ) {
	return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if ( (authlen = CONFAUTHIGetAuth(SGMHost, authStr)) == 0)
	return(ERR(SERVERERR, AUTHERR, CANTGETAUTHINFO));

    if ( !unpack_data(authStr, authlen, &mydata, 0, NULL, data, datalen, 0) ) {
	return(ERR(SERVERERR, AUTHERR, CANTGETAUTHINFO));
    }

    if ( !mydata )
	return ( ERR(SERVERERR, PARAMERR, INCOMPLETEDATA));

    if ( !strncasecmp(mydata, "XTRACT ", 7) ) {
	err = xtract_data(mydata+7, &nobytes, filename);
        if ( err ) goto end;
        if ( !nobytes ) goto end;
        sscanf(mydata+7, "%d %d", &bytestosend, &ntables);
        
        err = xfrdata(filename, bytestosend, offset, header, sizeof(header), 
                                                     &retdata, &nobytes);

    } else if ( !strncasecmp(mydata, "XFR ", 4) ) {
        sscanf(mydata+4, "%d %d %s", &bytestosend, &offset, filename);
        /* validate filename */
        if ( validate_filename ( filename ) == 0 )
        {
          err = xfrdata(filename, bytestosend, offset, header, sizeof(header),
                                                       &retdata, &nobytes);
	} else {
	  err = (ERR(SERVERERR, PARAMERR, INVALIDARGS));
	}		 

    } else if ( !strncasecmp(mydata, "CLEAN ", 6) ) {
	sscanf(mydata+6, "%s", filename);
        /* validate filename */
        if ( validate_filename ( filename ) == 0 )
        {
	  unlink(filename);
   	  err = 0;
	} else {
	  err = (ERR(SERVERERR, PARAMERR, INVALIDARGS));
	}		 
	*dPtr = NULL;
	*dLen = 0;
	goto end;
    }

    SGMIfree("xxx", data);
    *dLen = pack_data(dPtr, authStr, authlen, header, strlen(header)+1,
		      retdata, nobytes, authType);
    SGMIfree("xxx", retdata);

 end:
    CONFSSDBDone(1);
    return(err);
 
}
