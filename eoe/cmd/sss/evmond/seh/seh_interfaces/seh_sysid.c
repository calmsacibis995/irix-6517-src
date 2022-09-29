/*--------------------------------------------------------------------*/
/* get_sysid                                                          */
/*--------------------------------------------------------------------*/


#include <ssdbapi.h>
#include <ssdberr.h>
#include <sss_sysid.h>
#include <common.h>
#include <common_ssdb.h>
#include <limits.h>
#include <seh_errors.h>

extern FPssdbCreateErrorHandle  *fp_ssdbCreateErrorHandle;
extern FPssdbDeleteErrorHandle  *fp_ssdbDeleteErrorHandle;
extern FPssdbNewClient          *fp_ssdbNewClient;
extern FPssdbDeleteClient       *fp_ssdbDeleteClient;
extern FPssdbIsClientValid      *fp_ssdbIsClientValid;
extern FPssdbIsErrorHandleValid *fp_ssdbIsErrorHandleValid;
extern FPssdbOpenConnection     *fp_ssdbOpenConnection;
extern FPssdbCloseConnection    *fp_ssdbCloseConnection;
extern FPssdbSendRequest        *fp_ssdbSendRequest;
extern FPssdbSendRequestTrans   *fp_ssdbSendRequestTrans;
extern FPssdbFreeRequest        *fp_ssdbFreeRequest;
extern FPssdbGetNumRecords      *fp_ssdbGetNumRecords;
extern FPssdbGetNumColumns      *fp_ssdbGetNumColumns;
extern FPssdbGetNextField       *fp_ssdbGetNextField;
extern FPssdbLockTable          *fp_ssdbLockTable;
extern FPssdbUnLockTable        *fp_ssdbUnLockTable;
extern FPssdbGetRow             *fp_ssdbGetRow;
extern FPssdbGetFieldMaxSize    *fp_ssdbGetFieldMaxSize;
extern FPssdbGetLastErrorCode   *fp_ssdbGetLastErrorCode;
extern FPssdbGetLastErrorString *fp_ssdbGetLastErrorString;

/*--------------------------------------------------------------------*/
/* List of systems that are in system table                           */
/*--------------------------------------------------------------------*/
extern int verbose;
system_info_list_t    *pSysInfoList;
static int sgm_or_sem=LOCAL;

/*--------------------------------------------------------------------*/
/* Prototypes for internal functions                                  */
/*--------------------------------------------------------------------*/

static __uint64_t insert_sysinfo(system_info_list_t **, system_info_t *);
static system_info_t *new_sysinfo(void);
static void free_sysinfo(system_info_t *);
static void PrintDBError(char *, ssdb_Error_Handle);
__uint64_t convertsysid(char *);

/*--------------------------------------------------------------------*/
/* Function to initialize and populate pSysInfoList from SSDB         */
/*  First function to be called from SEM/SGM.                         */
/*--------------------------------------------------------------------*/

__uint64_t init_sysinfolist (ssdb_Client_Handle Client, 
		             ssdb_Error_Handle ErrorHandle, 
		             int qualifier)
{

    system_info_t            *pSysInfo = NULL;
    char                     sqlstring[MAX_STR_LEN];
    ssdb_Connection_Handle   Connection;
    ssdb_Request_Handle      Request;
    __uint32_t               NumRows = 0, NumCols = 0;
    __uint64_t               ErrCode = 0;
    __uint32_t               iTmpVar1 = 0, iTmpVar2 = 0, iTmpVar3 = 0;

    __int32_t                iTemplate[] = {-1, -1, -1, 
					    -1, sizeof(unsigned short),
					    sizeof(unsigned short)};

    __int32_t                iImportant[] = 
    {
	1, 0, 1, 
	0, 1, 1
    };

    __uint32_t               iFieldVal = 0;
    unsigned short           iFieldVal1 = 0;
    int			     flag = 0;
    char                     tmpSysID[20];

    /*----------------------------------------------------------------*/
    /* We can't do anything if ssdb_Client_Handle is NULL.  We don't  */
    /* want to get into the mess of creating a new client handle since*/
    /* we don't have a username & password to connect to DB. If Client*/
    /* Handle is not NULL, we assume it is a valid handle             */
    /*----------------------------------------------------------------*/

    sgm_or_sem=qualifier;

    if ( !fp_ssdbIsClientValid(ErrorHandle, Client) ) {
	if (verbose) PrintDBError("IsClientValid", ErrorHandle);
	return(SEH_ERROR(SEH_MAJ_SSDB_ERR, SEH_MIN_DB_API_ERR));
    }

    /*----------------------------------------------------------------*/
    /* Let's start DB Operations                                      */
    /*----------------------------------------------------------------*/

    /* Get Connection Handle */

    if ( !(Connection = fp_ssdbOpenConnection(ErrorHandle, Client, 
			NULL, SSS_SSDB_DBNAME, 
			(SSDB_CONFLG_RECONNECT | SSDB_CONFLG_REPTCONNECT))) ) {
	if (verbose) PrintDBError("OpenConnection", ErrorHandle);
	return(SEH_ERROR(SEH_MAJ_SSDB_ERR, SEH_MIN_DB_CONNERR));
    }

    /*----------------------------------------------------------------*/
    /* Generate SQL String                                            */
    /*                                                                */
    /* Case 1 :  On SEM, we keep only the data of the local system in */
    /*           memory. A local system can be identified by means of */
    /*           local flag in the DB (local == 1 )                   */
    /* Case 2 :  On SGM, we need to keep all subscribed systems' data */
    /*           in the memory. So, we don't look for local flag      */
    /*----------------------------------------------------------------*/

    if ( qualifier == LOCAL ) {
	sprintf(sqlstring, "SELECT %s,%s,%s,%s,%s,%s FROM %s WHERE %s=1 AND %s=1", 
		SYSTEM_SYSID, SYSTEM_SERIAL, SYSTEM_HOST, SYSTEM_IP, 
		SYSTEM_ACTIVE, SYSTEM_LOCAL, SYSTEM_TABLE, 
		SYSTEM_ACTIVE, SYSTEM_LOCAL);
    } else {
	sprintf(sqlstring,"SELECT %s,%s,%s,%s,%s,%s FROM %s WHERE %s=1",
		SYSTEM_SYSID, SYSTEM_SERIAL, SYSTEM_HOST, SYSTEM_IP, 
		SYSTEM_ACTIVE, SYSTEM_LOCAL, SYSTEM_TABLE, 
		SYSTEM_ACTIVE); 
    }

    /* Get a Request Handle */

    if ( !(Request = fp_ssdbSendRequest(ErrorHandle, Connection, 
		     SSDB_REQTYPE_SELECT, sqlstring)) ) {
	if (verbose) PrintDBError("SendRequest", ErrorHandle);
        ErrCode = SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_XMIT);
	goto end1;
    }

    /* Start retrieving results */

    if ( (NumRows = fp_ssdbGetNumRecords(ErrorHandle, Request)) != 0 ) {

	/*------------------------------------------------------------*/
	/* We have some records.  Lets start retrieving them.  However*/
	/* lets first check if pSysInfoList is NULL or not.  If not   */
	/* NULL, then we already read-in the data once during startup */
	/* and this call is to re-read the data once again which could*/
	/* have been triggered by any one of the following events :   */
	/*   CONFIG_CHANGE or AVAILMON.ID_CHANGE or SUBSCRIBE         */
	/* to either SEM or SGM.  In this case, we need to freeup the */
	/* list we created earlier and recreate a new list with new   */
	/* data in it.  We won't freeup the list if we don't have any */
	/* records.                                                   */
	/*------------------------------------------------------------*/

	if ( pSysInfoList != NULL ) {
	    free_sysinfolist();
	}

	if ( (NumCols = fp_ssdbGetNumColumns(ErrorHandle, Request)) == 0 ) {
	    if (verbose) PrintDBError("GetNumColumns", ErrorHandle);
	    ErrCode = SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_RCV);
	}

	/*------------------------------------------------------------*/
	/* Get all Field Sizes                                        */
	/*------------------------------------------------------------*/

	for ( iTmpVar2 = 0; iTmpVar2 < NumCols; iTmpVar2++) {
	    /*--------------------------------------------------------*/
	    /* we have a template, iTemplate, defined with 0s and -1s */
	    /* If iTemplate[iTmpVar2] < 0, then it is a character fiel*/
	    /* d, which means that we need to get the max size of the */
	    /* field and allocate space for it.  We don't want to do  */
	    /* it everytime we loop thro' the record set.  We will do */
	    /* it only once for the first time and store it in iTemp- */
	    /* late.                                                  */
	    /*                                                        */
	    /* for people who are picky about syntactic etiquette     */
	    /*  I've used iFieldVal only as a place holder for a int  */
	    /*  value.  Don't crib about it.                          */
	    /*--------------------------------------------------------*/

	    if ( iTemplate[iTmpVar2] < 0 ) {
		if ( ((iFieldVal = 
		       fp_ssdbGetFieldMaxSize(ErrorHandle,
					   Request, iTmpVar2)) <= 0) &&
		     iImportant[iTmpVar2])
		{
                    /*-----------------------------------------------*/
                    /* Oops.. We are supposed to get a char field siz*/
		    /* e, but we got an error ! We can't do anything */
		    /* about it but return ErrorCode to SEM.  But,   */
		    /* first, free pSysInfo                          */
                    /*-----------------------------------------------*/

	            if (verbose) PrintDBError("Get Field MaxSize", ErrorHandle);
                    ErrCode = SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
		    goto end;
		}
		iTemplate[iTmpVar2] = iFieldVal;
	    }

	}

	/*------------------------------------------------------------*/
	/* Loop thro' the number of records retrieved                 */
	/*------------------------------------------------------------*/

	for ( iTmpVar1 = 0; iTmpVar1 < NumRows; iTmpVar1++ ) {

	    if ( (pSysInfo = new_sysinfo()) == NULL ) {
		ErrCode = SEH_ERROR(SEH_MAJ_INIT_ERR, SEH_MIN_ALLOC_MEM);
		goto end;
	    }

	    /*--------------------------------------------------------*/
	    /* Loop thro' the number of fields                        */
	    /*--------------------------------------------------------*/

	    for( iTmpVar2 = 0; iTmpVar2 < NumCols; iTmpVar2++ ) {

		iFieldVal1 = 0;
		flag = 0;
		/*----------------------------------------------------*/
		/* Now, retrieve and put the field in the structure   */
		/*----------------------------------------------------*/
                
		switch (iTmpVar2) {
		    case 0:
			if ( !fp_ssdbGetNextField(ErrorHandle, Request, 
					       /*&pSysInfo->system_id,*/
					       tmpSysID,
					       iTemplate[iTmpVar2]+1) ) {
			    flag = 1;
			} else {
			    if ( (pSysInfo->system_id = 
				  convertsysid(tmpSysID)) == 0  &&
				 iImportant[iTmpVar2] ) {
				     ErrCode = SSDBERR_PROCESSRESERR;
				     free_sysinfo(pSysInfo);
				     goto end;
			    }
			}
			break;
		    case 1:
			if ((pSysInfo->serialnum = (char *) 
			     sem_mem_alloc_temp(iTemplate[iTmpVar2]+1)) 
			    != NULL ) {
			    if ( !fp_ssdbGetNextField(ErrorHandle, Request,
						   pSysInfo->serialnum,
						   iTemplate[iTmpVar2]+1)  &&
				 iImportant[iTmpVar2] ) {
				flag = 1;
			    }
			}
			break;
		    case 2:
			if ((pSysInfo->hostname_full = (char *) 
			     sem_mem_alloc_temp(iTemplate[iTmpVar2]+1)) 
			    != NULL ) {
			    if ( !fp_ssdbGetNextField(ErrorHandle, Request,
						   pSysInfo->hostname_full,
						   iTemplate[iTmpVar2]+1)  &&
				 iImportant[iTmpVar2] ) {
				flag = 1;
			    }
			    pSysInfo->hostname_1dot=pSysInfo->hostname_nodots
				= NULL;
			    if(pSysInfo->hostname_full)
				for(iTmpVar3 = 0;
				    pSysInfo->hostname_full[iTmpVar3];
				    iTmpVar3++)
				{
				    if(pSysInfo->hostname_full[iTmpVar3]=='.')
				    {
					if(pSysInfo->hostname_nodots == 
					   NULL)
					{
					    pSysInfo->hostname_nodots=(char *)
						sem_mem_alloc_temp(iTmpVar3+1);
					    strncpy(pSysInfo->
						    hostname_nodots,
						    pSysInfo->
						    hostname_full,
						    iTmpVar3);
					    pSysInfo->hostname_nodots[iTmpVar3]
						=0;
					}
					else if(pSysInfo->hostname_1dot == 
						NULL)
					{
					    pSysInfo->hostname_1dot=(char *)
						sem_mem_alloc_temp(iTmpVar3+1);
					    strncpy(pSysInfo->
						    hostname_1dot,
						    pSysInfo->
						    hostname_full,
						    iTmpVar3);
					    pSysInfo->
						hostname_1dot[iTmpVar3]=0;
					}
				    }
					
				}
			}
			break;
		    case 3:
			if ((pSysInfo->ipaddr = (char *) 
			     sem_mem_alloc_temp(iTemplate[iTmpVar2]+1)) 
			    != NULL ) {
			    if ( !fp_ssdbGetNextField(ErrorHandle, Request,
						   pSysInfo->ipaddr,
						   iTemplate[iTmpVar2]+1) &&
				 iImportant[iTmpVar2]) {
				flag = 1;
			    }
			}
			break;
		    case 4:
			if ( !fp_ssdbGetNextField(ErrorHandle, Request, 
					 &iFieldVal1,    /* unsigned short */
					 iTemplate[iTmpVar2]) ) {
			    flag = 1;
			} else {
			    pSysInfo->active = (__uint32_t) iFieldVal1;
			}
			break;
		    case 5:
			if ( !fp_ssdbGetNextField(ErrorHandle, Request, 
					 &iFieldVal1,    /* unsigned short */
					 iTemplate[iTmpVar2]) ) {
			    flag = 1;
			} else {
			    pSysInfo->local = (__uint32_t) iFieldVal1;
			}
			break;
		    default:
			break;
		}

		if ( flag ) {
		    free_sysinfo(pSysInfo);
	            if (verbose) PrintDBError("GetNextField", ErrorHandle);
		    ErrCode = SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_FLD);
		    goto end;
		}
	    }

	    /*--------------------------------------------------------*/
	    /* Insert pSysInfo into our pSysInfoList                  */
	    /*--------------------------------------------------------*/

	    if ( insert_sysinfo(&pSysInfoList, pSysInfo) != 0 ) {
		free_sysinfo(pSysInfo);
	    }
	}

    } else {
	if (verbose) PrintDBError("GetNumRows", ErrorHandle);
	ErrCode = SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_RCV);
    }

    end:

    /* Close Request Handle that we had before */

    fp_ssdbFreeRequest(ErrorHandle, Request);

    /* Close Connection that we opened */

    end1:
    fp_ssdbCloseConnection(ErrorHandle, Connection);
    return(ErrCode);
}


/*--------------------------------------------------------------------*/
/* Internal SSDB Error printing function                              */
/*--------------------------------------------------------------------*/
static void PrintDBError(char *loc, ssdb_Error_Handle ErrorHandle)
{
    printf("Error in ssdb%s: %s\n", loc, fp_ssdbGetLastErrorString(ErrorHandle));
}

/*--------------------------------------------------------------------*/
/* Internal insert list function                                      */
/*--------------------------------------------------------------------*/
static __uint64_t insert_sysinfo(system_info_list_t **SysInfoList,
		                 system_info_t      *SysInfo)
{
    
    system_info_list_t    *pTmpVar; 
    system_info_list_t    *pTmpVar1; 

    if ( SysInfo == NULL ) {
	return( (__uint64_t) -1);
    }

    if ( (pTmpVar = (system_info_list_t *) 
	     sem_mem_alloc_temp(sizeof(system_info_list_t))) != NULL ) {
	pTmpVar->node = SysInfo;
	pTmpVar->next = NULL;
    } else {
	return( (__uint64_t) -1);
    }

    if ( (*SysInfoList) == NULL ) {
	(*SysInfoList) = pTmpVar;
    } else {
	pTmpVar1 = (*SysInfoList);
	while ( pTmpVar1->next != NULL ) {
	    pTmpVar1 = pTmpVar1->next;
	}

	pTmpVar1->next = pTmpVar;
    }

    return( (__uint64_t) 0);

}


/*--------------------------------------------------------------------*/
/* Internal function to malloc space for new system_info_t            */
/*--------------------------------------------------------------------*/
static system_info_t  *new_sysinfo(void)
{
    system_info_t  *pTmpSysInfo = NULL;

    if ((pTmpSysInfo = 
	 (system_info_t *)sem_mem_alloc_temp(sizeof(system_info_t)))!= NULL) 
    {
	pTmpSysInfo->system_id = 0;
	pTmpSysInfo->hostname_full=pTmpSysInfo->hostname_1dot=
	    pTmpSysInfo->hostname_nodots=NULL;
	pTmpSysInfo->serialnum = NULL;
	pTmpSysInfo->ipaddr    = NULL;
	pTmpSysInfo->active    = -1;
	pTmpSysInfo->local     = -1;
    }

    return (pTmpSysInfo);
}

/*--------------------------------------------------------------------*/
/* Internal function to free space created for system_info_t          */
/*--------------------------------------------------------------------*/
static void free_sysinfo(system_info_t  *pSysInfo)
{
    if ( pSysInfo ) {

	if (pSysInfo->hostname_full) 
	    sem_mem_free(pSysInfo->hostname_full);

	if(pSysInfo->hostname_1dot)
	    sem_mem_free(pSysInfo->hostname_1dot);

	if(pSysInfo->hostname_nodots)
	    sem_mem_free(pSysInfo->hostname_nodots);

        if (pSysInfo->serialnum)
	    sem_mem_free(pSysInfo->serialnum);

        if (pSysInfo->ipaddr)
	    sem_mem_free(pSysInfo->ipaddr);
	    
	sem_mem_free(pSysInfo);
    }
}

/*--------------------------------------------------------------------*/
/* Externally visible free_sysinfolist function which counters new_sys*/
/* info functionality                                                 */
/*--------------------------------------------------------------------*/

void free_sysinfolist()
{
    system_info_list_t   *pTmpVar1;
    system_info_list_t   *pTmpVar2;

    pTmpVar1 = pSysInfoList;


    while ( pTmpVar1 != NULL ) {
	pTmpVar2 = pTmpVar1->next;
	free_sysinfo(pTmpVar1->node);
	sem_mem_free(pTmpVar1);
	pTmpVar1 = pTmpVar2;
    }
    pSysInfoList = NULL;
}

int print_sysinfolist(char *orgBuffer, int length)
{
    system_info_list_t  *pTmpVar1;
    int  nobytes = 0;
    int  i = 1;

    if ( !length || !orgBuffer ) return(0);

    pTmpVar1 = pSysInfoList;

    while ( pTmpVar1 != NULL ) {
	nobytes += snprintf(orgBuffer+nobytes, length, 
			    "  System %2d          : %X, %s (%s)\n",
			    i, pTmpVar1->node->system_id,
			    pTmpVar1->node->hostname_full,
                            (pTmpVar1->node->local==1 ? "local":"remote"));
	i++;
	pTmpVar1 = pTmpVar1->next;
    }

    return(nobytes);
}

/*--------------------------------------------------------------------*/
/* Internal function to convert character sys_id to an integer        */
/*--------------------------------------------------------------------*/
__uint64_t convertsysid(char *sysID)
{
    char        *ptr = NULL;
    __uint64_t  tmpSysID = 0;

    if ( sysID == NULL ) return (0);

    if ( (tmpSysID = strtoull(sysID, &ptr, 16)) == 0 ) {

	/*------------------------------------------------------------*/
	/* If tmpSysID is 0, then no integer can be formed from sysID */
	/*------------------------------------------------------------*/

	return(0);
    } else if ( strlen(ptr) != 0 ) {

	/*------------------------------------------------------------*/
	/* If strlen(ptr) is not 0, then, the string contains some    */
	/* other non-hex characters. Return an error                  */
	/*------------------------------------------------------------*/

	return(0);
    } else if ( errno == ERANGE ) {

	/*------------------------------------------------------------*/
	/* If tmpSysID is ULONGLONG_MAX and errno is set, then there's*/
	/* an overflow condition.  Return an error                    */
	/*------------------------------------------------------------*/
	return(0);
    } else
        return(tmpSysID);
}

/*--------------------------------------------------------------------*/
/* Called by SGM/SEM to get system information for a given host. This */
/* function returns back a system_info_t structure for the given host */
/*--------------------------------------------------------------------*/

__uint64_t sgm_get_sysid(system_info_t **pSysInfo, char *hostname)
{
    system_info_list_t  *pTmpList;
    system_info_t       *pTmpNode;
    __uint64_t          tmpSysID = 0;

    pTmpList = pSysInfoList;
    (*pSysInfo) = NULL;

    /*----------------------------------------------------------------*/
    /* If hostname is null, return the first record found for the     */
    /* system whose local flag is set                                 */
    /* Otherwise, traverse thro' the list to find the host !          */
    /*----------------------------------------------------------------*/

    if (!hostname || (strlen(hostname) == 0) || 
	!strcasecmp(hostname,"localhost")) {
	while ( pTmpList != NULL ) {
	    pTmpNode = pTmpList->node;
	    if (pTmpNode->local == 1) {
		(*pSysInfo) = pTmpNode;
		return(0);
	    }
	    pTmpList = pTmpList->next;
	}
    } else {
	while ( pTmpList != NULL ) {
	    pTmpNode = pTmpList->node;

	    /* 
	     * In SGM case, the fully qualified hostname should be
	     * checked, irrespective of what came in had a dot in it
	     * or not.
	     * This has the side-effect of making the SEM very forgiving
	     * and the SGM very unforgiving.
	     */
	    if((sgm_or_sem == LOCAL         && 
		((pTmpNode->hostname_nodots &&
		  !strcasecmp(hostname,pTmpNode->hostname_nodots)) ||
		 (pTmpNode->hostname_1dot   &&
		  !strcasecmp(hostname,pTmpNode->hostname_1dot))   ||
		 (pTmpNode->hostname_full   &&
		  !strcasecmp(hostname,pTmpNode->hostname_full)))) ||
	       (pTmpNode->hostname_full     && 
		!strcasecmp(hostname, pTmpNode->hostname_full)))
	    {
		(*pSysInfo) = pTmpNode;
		return(0);
	    }

	    /*------------------------------------------------------*/
	    /* As per my discussions earlier with Venki and Doug, it*/
	    /* is decided that a SYS-ID can also be passed as a     */
	    /* hostname and sgm_get_sysid function needs to check if*/
	    /* the char string matches any system_id.  Here, I am   */
	    /* assuming that the char string represents a hex value */
	    /* without leading 0x.                                  */
	    /*------------------------------------------------------*/

            if ( (tmpSysID = convertsysid(hostname)) != 0 ) {
		if ( pTmpNode->system_id == tmpSysID ) {
		    (*pSysInfo) = pTmpNode;
		    return(0);
		}
	    }

	    pTmpList = pTmpList->next;
	}
    }

    return( (__uint64_t) SEH_ERROR(SEH_MAJ_SSDB_ERR,SEH_MIN_DB_MISS_REC) );

}
