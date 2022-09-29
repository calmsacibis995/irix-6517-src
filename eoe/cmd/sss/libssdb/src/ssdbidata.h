/* ----------------------------------------------------------------- */
/*                           SSDBIDATA.H                             */
/*                    SSDB Internal data definition                  */
/* ----------------------------------------------------------------- */
#ifndef H_SSDBIDATA_H
#define H_SSDBIDATA_H

/* This preprocessor shit really need only for check OS_... definition */
#ifdef OS_UNIX
#define LOCAL_TEMP_DEFINITION_LVA 1
#endif
#ifdef OS_WINDOWS
#ifndef LOCAL_TEMP_DEFINITION_LVA
#define LOCAL_TEMP_DEFINITION_LVA 1
#else
#include <double_os_type_definition.h>  /* Double OS type definition. Please, check carefully -D... */
#endif
#endif
#ifndef LOCAL_TEMP_DEFINITION_LVA
#include <incorrect_os_version_definition.h> /* Some Error. Appear only if OS_... not defined! */
#endif

#include "ssdbthread.h"
#include "mysql.h"
#include "errmsg.h"

/* ----------------------------------------------------------------- */
/* SSDB API max string size definitions                              */
#define SSDB_STATIC_ERRSTR_SIZE  256 /* Max error buffer size (for static error string) */
#define SSDB_MAX_USERNAME_SIZE   128 /* Max User name string size */
#define SSDB_MAX_PASSWORD_SIZE   128 /* Max Password string size */
#define SSDB_MAX_HOSTNAME_SIZE	 64  /* Max Hostname string size */
#define SSDB_MAX_DATABASE_SIZE	 64  /* Max Hostname string size */
#define SSDB_MAX_FIELDNAME_SIZE  36  /* Max length for a field name in table */
#define SSDB_MAX_FIELDS_INREQ    25  /* Max number of fields pre-allocated */
#define SSDB_MAX_LOCKTABLES_CNT  256 /* Max counter of locked tables in LockTable request */

/* SSDB API Version number definition */
#define SSDB_VMAJORNUMBER        1   /* SSDB API Major version number */
#define SSDB_VMINORNUMBER        1   /* SSDB API Minor version number */

#define SSDB_CONNECT_REPTCOUNTER 3 /* Connection repeate counter (if SSDB_CONFLG_REPTCONNECT defined ) */

#define SSDB_REQUEST_STACK_BUFF_SIZE (1024*16) /* buffer size for create SQL string (on stack) */

/* SSDB Internal structure tags definition (effective sizeof(unsigned short)) */
#define SSDB_ISTRC_TAG_CLIENT     (0x1965)  /* "Client" structure TAG */
#define SSDB_ISTRC_TAG_CONNECTION (0x1980)  /* "Connection" structure TAG */
#define SSDB_ISTRC_TAG_REQUEST    (0x1992)  /* "Request" structure TAG */
#define SSDB_ISTRC_TAG_LASTERROR  (0x1939)  /* "LastError" structure TAG */

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif

/* Definitions for easy access to ssdb_slinker structure */
#define lnk_sizeof linker.size_of_strc
#define lnk_tag    linker.tag
#define lnk_next   linker.next
#define lnk_parent linker.parent

/* Definition for convertor function */
typedef int ssdbiu_typeconvert_request(const char *srcmem, char *memdest, int destsize,int field_size);

/* ------------------------------------------------------------------------------------------- */
/* Linker structure - info pointer */
typedef struct ssdb_slinker {
        unsigned long size_of_strc;                      /* size of parent structure */
        unsigned long tag;                               /* structure tag */
        void *next;                                      /* next pointer */
        void *parent;                                    /* Parent - might be something usefull */
} ssdb_SLINKER;
/* ------------------------------------------------------------------------------------------- */
/* Last Error structure */
typedef struct ssdb_lasterr {
        ssdb_SLINKER linker;                              /* Linker */
        int lasterror_code;                               /* Last error code */
        char *last_error_strptr;                          /* Last error string pointer */
        char error_str_buffer[SSDB_STATIC_ERRSTR_SIZE+1]; /* Last error string buffer (for non static strings) */
} ssdb_LASTERR;
/* ------------------------------------------------------------------------------------------- */
/* ssdb field info structure */
typedef struct ssdb_struct_field_info {
        int mysql_type;                                  /* Type of field from MYSQL structure */
        int byte_len;                                    /* Effective size of binary image of this field */
        int len;                                         /* This is max length defined in schema */
        int maxlen;                                      /* This is max length of selected set */
        char name[SSDB_MAX_FIELDNAME_SIZE+1];            /* This is Field name */
	ssdbiu_typeconvert_request *Fpconvert;	         /* function pointer which converts datastream to type */	
} ssdb_FIELDINFO;
/* ------------------------------------------------------------------------------------------- */
/* Request structure */
typedef struct ssdb_struct_Request {
        ssdb_SLINKER linker;                              /* Linker */
	struct ssdb_struct_Request *worklist;             /* Work List */
        void *data_ptr;                                   /* Data pointer */
        int data_size;                                    /* Data size */
        int num_rows;                                     /* number of rows */ 
        int columns;                                      /* number of columns */
	int row_strlength;                                /* for memory allocation (string) */
        int row_bytelength;                               /* row len as blob (for structure mapping) */
        int sql_req_type;                                 /* SQL request type */
	int cur_row_number;                               /* Current row we are in */
	int cur_field_number;                             /* Current field that is to be fetched */
	int field_info_size;                              /* Size of the current field info table */
	ssdb_FIELDINFO field_info[SSDB_MAX_FIELDS_INREQ]; /* Static buffer for field info */	
	ssdb_FIELDINFO *field_info_ptr;                   /* Field info pointer */
	MYSQL_ROW current_row_ptr;                        /* Current ROW pointer */
        MYSQL_RES *res;                                   /* MYSQL Result structure */
} ssdb_REQUEST;
/* ------------------------------------------------------------------------------------------- */
/* Connection structure */
typedef struct ssdb_struct_Connection {
        ssdb_SLINKER linker;        	       	          /* Linker */
        CRITICAL_SECTION connectionCriticalSection;       /* Critical section for access to some connect data */
        int initCritSecFlg;                               /* Critical section init flagLock counter. if >= 2 hard locked */
        int in_connect_flg;                               /* In connect phase flag */
        int in_rquery_flg;                                /* In query phase flag */
        int in_selectdb_flg;                              /* In select database phase flag */
        int in_delete_flg;                                /* In delete phase flag */
        int uname_len;                                    /* User name size (without last 0) */
        int passwd_len;                                   /* Password string size (without last 0) */
        int hostname_len;                                 /* Host name string size (withot last 0) */
        int dbname_len;                                   /* Database name string size (without last 0) */
        int flags;                                        /* Connection flags */
        char username[SSDB_MAX_USERNAME_SIZE+1];          /* User name (copy from client) */
        char password[SSDB_MAX_PASSWORD_SIZE+1];          /* Password (copy from client) */
        char hostname[SSDB_MAX_HOSTNAME_SIZE+1];          /* Host name */
        char dbname[SSDB_MAX_DATABASE_SIZE+1];            /* Database name */
        MYSQL *connected;                                 /* Connected flag */
        MYSQL mysql;                                      /* MYSQL structure (realization specific) */
        ssdb_REQUEST *req_list;                           /* Request(s) list */
} ssdb_CONNECTION;       
/* ------------------------------------------------------------------------------------------- */
/* Client structure */
typedef struct ssdb_struct_Client {
        ssdb_SLINKER linker;                              /* Linker */
        CRITICAL_SECTION clientCriticalSection;           /* Critical section for access to some client's data */
        int initCritSecFlg;                               /* Critical section init flagLock counter. if >= 2 hard locked */
        int in_delete_flg;                                /* In delete phase flag */
        char username[SSDB_MAX_USERNAME_SIZE+1];          /* User name */
        char password[SSDB_MAX_PASSWORD_SIZE+1];          /* Password */
        int uname_len;                                    /* User name size (without last 0) */
        int passwd_len;                                   /* Password string size (without last 0) */
        unsigned long flags;                              /* Client flags */
        ssdb_CONNECTION *conn_list;                       /* List of connections */
} ssdb_CLIENT;
/* ------------------------------------------------------------------------------------------- */
/* Global SSDB statistics structure */
typedef struct ssdb_struct_GlobalStat {
        unsigned long volatile statTotalAllocClients;     /* Total "allocated" Client structures */
        unsigned long volatile statTotalFreeClients;      /* Total "free" Client structures */
	unsigned long volatile statTotalAllocConn;        /* Total "allocated" Connection structures */
	unsigned long volatile statTotalFreeConn;         /* Total "free" Connection structures */
	unsigned long volatile statTotalAllocRequest;     /* Total "allocated" Request structures */
	unsigned long volatile statTotalFreeRequest;      /* Total "free" Request structures */
	unsigned long volatile statTotalAllocReqFielInfo; /* Total "allocated" RequestInfo data blocks */
	unsigned long volatile statTotalAllocReqResult;   /* Total "allocated" RequestResultData blocks */
	unsigned long volatile statTotalAllocErrorHandle; /* Total "allocated" ErrorHandle structures */
	unsigned long volatile statTotalFreeErrorHandle;  /* Total "free" ErrorHandle structures */
} ssdb_GSTATUS;
/* ------------------------------------------------------------------------------------------- */

#ifdef _MSC_VER
#pragma pack(pop)
#endif

/* request processing function prototype */
typedef int ssdbiu_process_XREQ_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);

/* Recuest processing control block */
typedef struct ssdb_struct_reqProc {
        int req_type;
        ssdbiu_process_XREQ_Results *fpProcessor;
} ssdb_REQPROCENTRY;

#endif /* #ifndef H_SSDBIDATA_H */

