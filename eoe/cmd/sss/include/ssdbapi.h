/* ----------------------------------------------------------------- */
/*                             SSDBAPI.H                             */
/*                        SSDB API header file                       */
/* ----------------------------------------------------------------- */
#ifndef H_SSDBAPI_H
#define H_SSDBAPI_H

#ifdef _MSC_VER 
#define SSDBAPI _cdecl
#endif

#ifndef SSDBAPI
#define SSDBAPI
#endif

/* ----------------------------------------------------------------- */
/* SSDB Handlers definitions                                         */
typedef void*                   ssdb_Abstract_Handler;    /* Generic SSDB Handler */
typedef ssdb_Abstract_Handler   ssdb_Client_Handle;       /* SSDB Client Handler */
typedef ssdb_Abstract_Handler   ssdb_Connection_Handle;   /* SSDB Connection Handler */
typedef ssdb_Abstract_Handler   ssdb_Request_Handle;      /* SSDB Request Handler */
typedef ssdb_Abstract_Handler   ssdb_Error_Handle;        /* SSDB Error Handler */

/* ----------------------------------------------------------------- */
/* SSDB Get INTERGER parameter names                                 */
#define SSDBPARI_VERSIONMAJOR       1   /* Name for "Get SSDB major version number" request */
#define SSDBPARI_VERSIONMINOR       2   /* Name for "Get SSDB minor version number" request */
#define SSDBPARI_MAXUSERNAMESIZE    3   /* Name for "Get SSDB max User name string size" request */
#define SSDBPAPI_VERSIONMAJOR       SSDBPARI_VERSIONMAJOR    /* Name for "Get SSDB major version number" request */
#define SSDBPAPI_VERSIONMINOR       SSDBPARI_VERSIONMINOR    /* Name for "Get SSDB minor version number" request */
#define SSDBPAPI_MAXUSERNAMESIZE    SSDBPARI_MAXUSERNAMESIZE /* Name for "Get SSDB max User name string size" request */
#define SSDBPAPI_MAXPASSWDSIZE      4   /* Name for "Get SSDB max Password string size" request */
#define SSDBPAPI_MAXHOSTNAMESIZE    5   /* Name for "Get SSDB max Hostname string size" request */
#define SSDBPAPI_MAXDBNAMESIZE      6   /* Name for "Get SSDB max DataBase name string size" request */
#define SSDBPAPI_ISINITEDFLG        7   /* Name for "Get SSDB isInited Flag" request */
#define SSDBPAPI_CONNECTREPTCNT     8   /* Name for "Get SSDB repeat connection counter" request */
#define SSDBPAPI_GLOBALSTATUS       9   /* Name for "Get SSDB global status info" request */
#define SSDBPAPI_MAXLOCKTABLES      10  /* Name for "Get SSDB max locked tables counter" request */
/* ----------------------------------------------------------------- */
/* Sub request numbers for "SSDBPAPI_GLOBALSTATUS" request */
#define SSDBPAPI_GS_CLIENTALLOC     1   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total Client structs allocated" */
#define SSDBPAPI_GS_CLIENTFREE      2   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total Client structs in free list" */
#define SSDBPAPI_GS_CONNALLOC       3   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total Connection structs allocated" */
#define SSDBPAPI_GS_CONNFREE        4   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total Connection structs in free list" */
#define SSDBPAPI_GS_REQUESTALLOC    5   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total Request structs allocated" */
#define SSDBPAPI_GS_REQUESTFREE     6   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total Request structs in free list" */
#define SSDBPAPI_GS_REQINFOALLOC    7   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total ReqInfo structs allocated" */
#define SSDBPAPI_GS_REQRESALLOC     8   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total ReqResultData blocks allocated" */
#define SSDBPAPI_GS_ERRHALLOC       9   /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total ErrorHandle structs allocated" */
#define SSDBPAPI_GS_ERRHFREE        10  /* Sub request number for "SSDBPAPI_GLOBALSTATUS" request - "total ErrorHandle structs in free list" */

/* ----------------------------------------------------------------- */
/* SSDB Get STRING parameter names                                   */
#define SSDBPARS_VERDESCRIPTION     1   /* Name for "Get SSDB version descriprion string" request */
#define SSDBPARS_ERRORMSGSTRBYCODE  2   /* Name for "Get SSDB error message string for error code #" request (addParam must be error code) */
#define SSDBPARS_DEFAULTUSERNAME    3   /* Name for "Get SSDB default username string" request */
#define SSDBPARS_DEFAULTPASSWORD    4   /* Name for "Get SSDB default password string" request */
#define SSDBPARS_DEFAULTHOSTNAME    5   /* Name for "Get SSDB default hostname string" request */
#define SSDBPARS_CONTACTADDRESS     6   /* Name for "Get SSDB contact address" request */
/* ----------------------------------------------------------------- */
/* SSDB API Client flags                                             */
#define SSDB_CLIFLG_ENABLEDEFUNAME  0x0001  /* Enable use default "Username" if username param not difined */
#define SSDB_CLIFLG_ENABLEDEFPASWD  0x0002  /* Enable use default "Password" if password param not defined */
/* ----------------------------------------------------------------- */
/* SSDB API Connect flags                                            */
#define SSDB_CONFLG_RECONNECT       0x0001  /* Enable reconnect if link to server was broken */
#define SSDB_CONFLG_REPTCONNECT     0x0002  /* Enable repeate connection if connectet fail */
#define SSDB_CONFLG_DEFAULTHOSTNAME 0x0004  /* Force use for connection default host name - "localhost" */
/* ----------------------------------------------------------------- */
/* SSDB SQL Request type definition                                  */
#define SSDB_REQTYPE_INSERT         1   /* SQL Type: INSERT */
#define SSDB_REQTYPE_SELECT         2   /* SQL Type: SELECT */
#define SSDB_REQTYPE_UPDATE         3   /* SQL Type: UPDATE */
#define SSDB_REQTYPE_DELETE	    4   /* SQL Type: DELETE */
#define SSDB_REQTYPE_CREATE         5   /* SQL Type: CREATE */
#define SSDB_REQTYPE_DROPTABLE      6   /* SQL Type: DROPTABLE */
#define SSDB_REQTYPE_CREATEUSER     7   /* SQL Type: CREATEUSER */
#define SSDB_REQTYPE_DROPUSER       8   /* SQL Type: DROPUSER */
#define SSDB_REQTYPE_ALTER          9   /* SQL Type: ALTER */
#define SSDB_REQTYPE_REPLACE        10  /* SQL Type: REPLACE */
#define SSDB_REQTYPE_SHOW           11  /* SQL Type: SHOW */
#define SSDB_REQTYPE_LOCK           12  /* SQL Type: LOCK */
#define SSDB_REQTYPE_UNLOCK         14  /* SQL Type: UNLOCK */
#define SSDB_REQTYPE_DESCRIBE       15  /* SQL Type: DESCRIBE */
#define SSDB_REQTYPE_DROPDATABASE   16  /* SQL Type: DROP DATABASE */
#define SSDB_REQTYPE_MASK         0xff  /* SSDB mask for request type */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------- */
/* Initialize SSDB API. Must be first API call.
   parameter(s):
    none
   return result(s):
    integer value != 0 - success, else error
   note(s):
    can use  ssdbGetLastErrorCode(NULL) for error detail error code
                  or
             ssdbGetLastErrorString(NULL) for error string
*/
int SSDBAPI ssdbInit(void);
typedef int SSDBAPI FPssdbInit(void);

/* ----------------------------------------------------------------- */
/* Deinitialize SSDB API.
   parameter(s):
    none
   return result(s):
    integer value != 0 - success, else error
   note(s):
    can use  ssdbGetLastErrorCode(NULL) for error detail error code
                  or
             ssdbGetLastErrorString(NULL) for error string
*/
int SSDBAPI ssdbDone(void);
typedef int SSDBAPI FPssdbDone(void);

/* ----------------------------------------------------------------- */
/* Get SSDB INTEGER parameter
   parameter(s):
    int paramName - parameter name, SSDBPARI_....
    int addParam  - additional specify param (typical zero, but may have specific values)
   return result(s):
    integer value - some parameter
   note(s):
    Didn't change last error code/string variables.
   sample(s):
    int ssdbMajorVersionNumber = ssdbGetParamInt(SSDBPARI_VERSIONMAJOR,0);
    int ssdbMinorVersionNumber = ssdbGetParamInt(SSDBPARI_VERSIONMINOR,0);
    printf("SSDB API version %d.%d\n",ssdbMajorVersionNumber,ssdbMinorVersionNumber);
*/
int SSDBAPI ssdbGetParamInt(int paramName,int addParam);
typedef int SSDBAPI FPssdbGetParamInt(int paramName,int addParam);
/* ----------------------------------------------------------------- */
/* Get SSDB STRING parameter
   parameter(s):
    int paramName - parameter name, SSDBPARS_....
    int addParam  - additional specify param (typical zero, but may have specific values)
   return result(s):
    string pointer - some string parameter
   note(s):
    Didn't change last error code/string variables.
   sample(s):
    printf("SSDB Description:%s\n",ssdbGetParamString(SSDBPARS_VERDESCRIPTION,0));
*/
const char * SSDBAPI ssdbGetParamString(int paramName,int addParam);
typedef const char * SSDBAPI FPssdbGetParamString(int paramName,int addParam);
/* ----------------------------------------------------------------- */
/* Create new SSDB Error Handle
   parameter(s):
    none
   return result(s):
    non zero - valid SSDB error handler, if zero - some error
   sample(s):
    ssdb_Error_Handle err;
    if((err = ssdbCreateErrorHandle()) == 0) error("Can't allocate SSDB error handle");
*/
ssdb_Error_Handle SSDBAPI ssdbCreateErrorHandle(void);
typedef ssdb_Error_Handle SSDBAPI FPssdbCreateErrorHandle(void);
/* ----------------------------------------------------------------- */
/* Delete SSDB Error Handle
   parameter(s):
    ssdb_Error_Handle errHandle - must be valid SSDB Error Handle
   return result(s):
    none
*/ 
int SSDBAPI ssdbDeleteErrorHandle(ssdb_Error_Handle errHandle);
typedef int SSDBAPI FPssdbDeleteErrorHandle(ssdb_Error_Handle errHandle);
/* ----------------------------------------------------------------- */
/* Check Error Handle
   parameter(s):
    ssdb_Error_Handle errHandle - must be valid SSDB Error Handle
   return result(s):
    non zero value if errHandle valid ssdb_Error_Handle, else zero
*/
int SSDBAPI ssdbIsErrorHandleValid(ssdb_Error_Handle errHandle);
typedef int SSDBAPI FPssdbIsErrorHandleValid(ssdb_Error_Handle errHandle);
/* ----------------------------------------------------------------- */
/* Create "Client" context
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    const char *username - User name , can be 0
    const char *password - Password , can be 0
    unsigned long flags  - Client profile flags (SSDB_CLIFLG_...)
   return result(s):
    ssdb_Client_Handle != 0 if success, else some error
 */
ssdb_Client_Handle SSDBAPI ssdbNewClient(ssdb_Error_Handle errHandle,const char *username, const char *password, unsigned long flags);
typedef ssdb_Client_Handle SSDBAPI FPssdbNewClient(ssdb_Error_Handle errHandle,const char *username, const char *password, unsigned long flags);

/* ----------------------------------------------------------------- */
/* Delete "Client" context
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Client_Handle clientHandle - valid Client context handler
   return result(s):
    integer value != 0 - success, else error
   note(s):
    if Client context have opened connection, this call close it
*/
int SSDBAPI ssdbDeleteClient(ssdb_Error_Handle errHandle,ssdb_Client_Handle clientHandle);
typedef int SSDBAPI FPssdbDeleteClient(ssdb_Error_Handle errHandle,ssdb_Client_Handle clientHandle);
/* ----------------------------------------------------------------- */
/* Check "Client" handler context
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Client_Handle clientHandle - valid Client context handler (or not valid)
   return result(s):
    integer value != 0 - Client handler valid, else Client handler not valid
*/
int SSDBAPI ssdbIsClientValid(ssdb_Error_Handle errHandle, ssdb_Client_Handle clientHandle);
typedef int SSDBAPI FPssdbIsClientValid(ssdb_Error_Handle errHandle, ssdb_Client_Handle clientHandle);

/* ----------------------------------------------------------------- */
/* Open Connection to database (in Client context)
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Client_Handle client - Client context handler, must be valid Client context handler
    const char *hostname - host name, can be 0
    const char *dbname - database name, must be valid database name (incorrect symbols "/\:*'`"")
    unsigned long flags - connection specific flags SSDB_CONFLG_....
                                                    SSDB_CONFLG_RECONNECT - enable reconnect if connection fault
                                                    SSDB_CONFLG_REPTCONNECT - enable repeat connection
                                                    SSDB_CONFLG_DEFAULTHOSTNAME - force use default host name "localhost"
   return result(s):
    ssdb_Connection_Handle != 0 if success, else error
*/
ssdb_Connection_Handle SSDBAPI ssdbOpenConnection(ssdb_Error_Handle errHandle,ssdb_Client_Handle client,const char *hostname,const char *dbname, unsigned long flags);
typedef ssdb_Connection_Handle SSDBAPI FPssdbOpenConnection(ssdb_Error_Handle errHandle,ssdb_Client_Handle client,const char *hostname,const char *dbname, unsigned long flags);

/* ----------------------------------------------------------------- */
/* Close Connection to database (in Client context)
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Connection_Handle connection - valid connection handler
   return result(s):
    integer value != 0 - success, else error
*/
int SSDBAPI ssdbCloseConnection(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection);
typedef int SSDBAPI FPssdbCloseConnection(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection);

/* ----------------------------------------------------------------- */
/* Check connection to database (in Client context)
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Connection_Handle connection - valid connection handler
   return result(s):
    integer value != 0 - connected, else not
*/
int SSDBAPI ssdbIsConnectionValid(ssdb_Error_Handle errHandle,ssdb_Connection_Handle connection);
typedef int SSDBAPI FPssdbIsConnectionValid(ssdb_Error_Handle errHandle,ssdb_Connection_Handle connection);

/* ----------------------------------------------------------------- */
/* Execute SQL request (without internal LOCK and UNLOCK calls) (Make request handler)
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Connection_Handle connection - valid connection handler
    int sqlRequestType - request type: SSDB_REQTYPE_...
    char *sqlString - string or format string with SQL request (not have a default value)
   return result(s):
    ssdb_Request_Handle != 0 if success, else error
*/
ssdb_Request_Handle SSDBAPI ssdbSendRequest(ssdb_Error_Handle errHandle,ssdb_Connection_Handle connection, int sqlRequestType, char *sqlString,...);
typedef ssdb_Request_Handle SSDBAPI FPssdbSendRequest(ssdb_Error_Handle errHandle,ssdb_Connection_Handle connection, int sqlRequestType, char *sqlString,...);
/* ----------------------------------------------------------------- */
/* Check request handle
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - request handler
   return result(s):
    integer value != 0 - request handle is valid, else not
*/
int SSDBAPI ssdbIsRequestValid(ssdb_Error_Handle errHandle,ssdb_Request_Handle request);
typedef int SSDBAPI FPssdbIsRequestValid(ssdb_Error_Handle errHandle,ssdb_Request_Handle request);

/* ----------------------------------------------------------------- */
/* Lock Table in database
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Connection_Handle connection - valid connection handler
    char *table_names - string or format string with name(s) of table(s), must be non zero len
   return result(s):
    integer value != 0 - success, else error
*/
int SSDBAPI ssdbLockTable(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection, char *table_names,...);
typedef int SSDBAPI FPssdbLockTable(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection, char *table_names,...);

/* ----------------------------------------------------------------- */
/* Unlock Table in database
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Connection_Handle connection - valid connection handler
   return result(s):
    integer value != 0 - success, else error
*/
int SSDBAPI ssdbUnLockTable(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection);
typedef int SSDBAPI FPssdbUnLockTable(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection);

/* ----------------------------------------------------------------- */
/* Execute Transaction request (with LOCK and UNLOCK) (Make new Request handler)
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Connection_Handle connection - valid connection handler
    int sqlRequestType - request type: SSDB_REQTYPE_...
    const char *table_names
    char *sqlString - string or format string with SQL request
   return result(s):
    ssdb_Request_Handle - Request handler != 0 - success, else error
*/
ssdb_Request_Handle SSDBAPI ssdbSendRequestTrans(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection, int sqlRequestType, const char *table_names, char *sqlString,...);
typedef ssdb_Request_Handle SSDBAPI FPssdbSendRequestTrans(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection, int sqlRequestType, const char *table_names, char *sqlString,...);
/* ----------------------------------------------------------------- */
/* Free Request handler
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
   return result(s):
    integer value != 0 if success, else error
    (possible to check last execution status by  ssdbGetLastErrorCode)
*/
int SSDBAPI ssdbFreeRequest(ssdb_Error_Handle errHandle, ssdb_Request_Handle request);
typedef int SSDBAPI FPssdbFreeRequest(ssdb_Error_Handle errHandle, ssdb_Request_Handle request);

/* ----------------------------------------------------------------- */
/* Get number of record in Request
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
   return result(s):
    integer value - counter of records in Request
*/
int SSDBAPI ssdbGetNumRecords(ssdb_Error_Handle errHandle, ssdb_Request_Handle request);
typedef int SSDBAPI FPssdbGetNumRecords(ssdb_Error_Handle errHandle, ssdb_Request_Handle request);

/* ----------------------------------------------------------------- */
/* Get number of colomns in Request
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
   return result(s):
    integer value - counter of columns in Request
*/
int SSDBAPI ssdbGetNumColumns(ssdb_Error_Handle errHandle, ssdb_Request_Handle request);
typedef int SSDBAPI FPssdbGetNumColumns(ssdb_Error_Handle errHandle, ssdb_Request_Handle request);

/* ----------------------------------------------------------------- */
/* Get actual field size (sizeof)
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
    int field_number - The field in the record counting from 0
   return result(s):
    int - Size of the selected field if success else 0 on failure
*/
int SSDBAPI ssdbGetFieldSize(ssdb_Error_Handle errHandle, ssdb_Request_Handle request, int field_number);
typedef int SSDBAPI FPssdbGetFieldSize(ssdb_Error_Handle errHandle, ssdb_Request_Handle request, int field_number);

/* ----------------------------------------------------------------- */
/* Get database field size (as defined in database)
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
    int field_number - The field in the record counting from 0
   return result(s):
    int - Size of the selected field if success else 0 on failure
int SSDBAPI ssdbGetFieldTableSize(ssdb_Error_Handle errHandle, ssdb_Request_Handle request, int field_number);
typedef int SSDBAPI FPssdbGetFieldTableSize(ssdb_Error_Handle errHandle, ssdb_Request_Handle request, int field_number);
*/
/* ----------------------------------------------------------------- */
/* Get effective max field size (max size not more then database size)
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
    int field_number - The field in the record counting from 0
   return result(s):
    int - Size of the selected field if success else 0 on failure
*/
int SSDBAPI ssdbGetFieldMaxSize(ssdb_Error_Handle errHandle, ssdb_Request_Handle request, int field_number);
typedef int SSDBAPI FPssdbGetFieldMaxSize(ssdb_Error_Handle errHandle, ssdb_Request_Handle request, int field_number);

/* ----------------------------------------------------------------- */
/* Get Field name in the table
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
    int field_number - The field in the record counting from 0
    char *field_name - A field name buffer to copy the field into.
    int field_name_size - size of 'field name buffer'
   return result(s):
    int - non zero on success, 0 on failure
*/
int SSDBAPI ssdbGetFieldName(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int field_number,char *field_name,int field_name_size);
typedef int SSDBAPI FPssdbGetFieldName(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int field_number,char *field_name,int field_name_size);
/* ----------------------------------------------------------------- */
/* Get the value of the field specified
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
    void *field - The address of the field where the result converted value is stored in
    int sizeof_field - Size of the type of field being converted to 
   return result(s):
    int - non zero on success, 0 on failure
*/
int SSDBAPI ssdbGetNextField(ssdb_Error_Handle errHandle, ssdb_Request_Handle request, void *field, int sizeof_field);
typedef int SSDBAPI FPssdbGetNextField(ssdb_Error_Handle errHandle, ssdb_Request_Handle request, void *field, int sizeof_field);

/* ----------------------------------------------------------------- */

/* Seek to a particular column in a particular row in the result query set
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
    int row - Row number starting from 0 in the result selected set
    int col - field number starting from 0 in the result selected set
   return result(s):
    int - non zero on success, 0 on failure
*/
int SSDBAPI ssdbRequestSeek(ssdb_Error_Handle errHandle, ssdb_Request_Handle request,int row, int col);
typedef int SSDBAPI FPssdbRequestSeek(ssdb_Error_Handle errHandle, ssdb_Request_Handle request,int row, int col);

/* ----------------------------------------------------------------- */
/* Get selected row as a array of string. Caller needs to convert array of strings to fields to
   required type. 
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
    ssdb_Request_Handle request - valid request handler
    int row - Row number starting from 0 in the result selected set
   return result(s):
    const char ** - Pointer to array of strings, each string representing a field. A zero value
                    is indicative of an error.
*/
const char ** SSDBAPI ssdbGetRow(ssdb_Error_Handle errHandle, ssdb_Request_Handle request); 
typedef const char ** SSDBAPI FPssdbGetRow(ssdb_Error_Handle errHandle, ssdb_Request_Handle request);
const char ** SSDBAPI ssdbGetRowWithSeek(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int row);
typedef const char ** SSDBAPI FPssdbGetRowWithSeek(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int row);

/* ----------------------------------------------------------------- */
/* Get Last Error code value
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
   return result(s):
    integer value - last error code (SSDBERR_...)
   note(s):
    SSDBERR_SUCCESS - not error
    IMPORTANT: This call dosn't change last error code and last error string values
*/
int SSDBAPI ssdbGetLastErrorCode(ssdb_Error_Handle errHandle);
typedef int SSDBAPI FPssdbGetLastErrorCode(ssdb_Error_Handle errHandle);

/* ----------------------------------------------------------------- */
/* Get Last Error string
   parameter(s):
    ssdb_Error_Handle errHandle - valid SSDB Error Handle or NULL for access to global SSDB error storage,
                                  (NULL make sence only for single thread applications, in opposite case
                                   possible to have a overload problems)
   return result(s):
    const char * - error string pointer or NULL if no error
   note(s):
    IMPORTANT: This call dosn't change last error code and last error string values
*/
const char* SSDBAPI ssdbGetLastErrorString(ssdb_Error_Handle errHandle);
typedef const char* SSDBAPI FPssdbGetLastErrorString(ssdb_Error_Handle errHandle);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef H_SSDBAPI_H */
