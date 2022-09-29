#ifndef RGPSSDBCALLS_INCLUDED
#define RGPSSDBCALLS_INCLUDED

#include <sscErrors.h>
#include <sscPair.h>
#include "rgpFormat.h"

/* SSDB specific types declarations */

#define  OP_INSERT_EVENT     0
#define  OP_UPDATE_EVENT     1
#define  OP_VALIDATE_INSERT  2
#define  OP_VALIDATE_UPDATE  3
#define  OP_VALIDATE_DELETE  4
#define  OP_DELETE_CUSTOM    5
#define  OP_DELETE_ANY       6

#define  OP_INSERT_ACTION    0
#define  OP_UPDATE_ACTION    1
#define  OP_VALIDATE_INSERT  2
#define  OP_VALIDATE_UPDATE  3
#define  OP_DELETE_ACTION    4

#define  ENUMIDX_INIT       -1
#define  ENUMIDX_DONE       -2

/* Return ErrorCodes  */

#define  SSDB_ERR                    -1 
#define  ENUM_ERR                    -2
#define  TYPE_ERR                    -3
#define  SQLR_ERR                    -4

#define  EVENTID_OUTOFRANGE          -5
#define  EVENT_ALREADY_EXIST         -6
#define  EVENT_NOTFOUND              -7
#define  EVENTDSCR_ALREADY_EXIST     -8
#define  EVENTDSCR_EMPTY             -9
#define  CLASSDSCR_ALREADY_EXIST    -10
#define  CLASS_NOTFOUND             -11
#define  CLASSDSCR_EMPTY            -12
#define  THCOUNT_OUTOFRANGE         -13   
#define  THTIMEOUT_OUTOFRANGE       -14
#define  CALL_NOTEMPLEMNTED         -15

#define  ACTIONCMD_EMPTY            -16
#define  ACTIONDSCR_EMPTY           -17
#define  DSMTIMEOUT_OUTOFRANGE      -18
#define  DSMTHROT_OUTOFRANGE        -19
#define  DSMFREQ_OUTOFRANGE         -20
#define  DSMRETRYCOUNT_OUTOFRANGE   -21
#define  ACTION_NOTFOUND            -22
#define  ACTION_ALREADY_EXIST       -23

#define  CLASS_NONCUSTOM            -24
#define  NOTHING_SELECTED           -25

/* Types */
#define VALTYPE_UNKNOWN          0x0000 
#define VALTYPE_NULL             0x0001
#define VALTYPE_ALL              0x0002 
#define VALTYPE_VALBLANK         0x0003  
#define VALTYPE_VARBLANK         0x0004
#define VALTYPE_BADINTFORMAT     0x0005
#define VALTYPE_SINGINTVALUE     0x0006
#define VALTYPE_SINGINTVARNAME   0x0007 
#define VALTYPE_SINGSTRVALUE     0x0008
#define VALTYPE_SINGSTRVARNAME   0x0009
#define VALTYPE_MULTINTVARNAME   0x000A
#define VALTYPE_MULTSTRVARNAME   0x000B
#define VALTYPE_VARNOTFOUND      0x000C

/* Enumeration Callback Proc declarations */

typedef  int (*EnumProc ) ( void       * pContext,     
                            int          idx, 
                            CMDPAIR    * pPairs,  
                            int          nPairs);


int  get_sys_id             ( sscErrorHandle hError, char *sysid , int   sysidlen );
int  get_global_setup_flags ( sscErrorHandle hError, int  *pLog  , int  *pFilter, int *pAction );
int  set_global_setup_flags ( sscErrorHandle hError, int  Logging, int   Filter , int   Action );

/*  enumeration  */

int enum_events ( sscErrorHandle hError    ,
                  const char *   szSysID   , 
                  const char *   szClassID , 
                  const char *   szEventID ,
                  const char *   szActionID,
                  const char *   szOrdered ,
                  CMDPAIR    *   Vars      , 
                  int            Varc      ,  
                  EnumProc       pProc     , 
                  void         * pProcData);
                  
int enum_classes( sscErrorHandle hError    ,
                  const char *   szSysID   , 
                  const char *   szClassID , 
                  const char *   szEventID ,
                  const char *   szActionID,
                  const char *   szOrdered ,
                  CMDPAIR    *   Vars      , 
                  int            Varc      ,  
                  EnumProc       pProc     , 
                  void         * pProcData);

int enum_actions( sscErrorHandle hError    ,
                  const char *   szSysID   , 
                  const char *   szClassID , 
                  const char *   szEventID ,
                  const char *   szActionID,
                  const char *   szCustom  ,
                  CMDPAIR    *   Vars      , 
                  int            Varc      ,  
                  EnumProc       pProc     , 
                  void         * pProcData );

int update_event( sscErrorHandle  hError    , 
                  const char    * sys_id    ,
                  const char    * class_id  ,
                  const char    * class_dscr,
                  const char    * event_type, 
                  const char    * event_dscr,
                  const char    * thcount   ,
                  const char    * thtimeout ,
                  const char    * enabled   ,
                  int           * newclassid, 
                  int           * neweventid,
                  CMDPAIR       * Vars      , 
                  int             Varc      ,
                  int             op       );

int update_action ( sscErrorHandle  hError     , 
                    const char    * sys_id     ,
                    const char    * action_id  ,
                    const char    * action_desc, 
                    const char    * action_cmd , 
                    const char    * dsmthrottle,
                    const char    * dsmfreq    ,                     
                    const char    * retrycount ,
                    const char    * timeout    ,
                    const char    * userstring ,
                    const char    * hostname   ,
                    int           * newactionid,
                    CMDPAIR       * Vars       , 
                    int             Varc       ,
                    int             op         );
            
int update_actionref ( sscErrorHandle  hError   , 
                       const char    * sys_id   ,
                       const char    * event_id ,
                       const char    * action_id,
                       CMDPAIR       * Vars     , 
                       int             Varc     ,
                       int             op       );
                    
int delete_events   (  sscErrorHandle  hError   , 
                       const char    * sys_id   ,
                       const char    * event_id ,
                       CMDPAIR       * Vars     , 
                       int             Varc     ,
                       int             op       );

int delete_actions  (  sscErrorHandle  hError   , 
                       const char    * sys_id   ,
                       const char    * action_id,
                       CMDPAIR       * Vars     , 
                       int             Varc     ,
                       int             op       );

int ResolveIntValue ( CMDPAIR *Vars, int Varc, const char * pID, int         *pVal );
int ResolveStrValue ( CMDPAIR *Vars, int Varc, const char * pID, const char **pVal ); 

#endif /* RGPSSDBCALLS_INCLUDED */
