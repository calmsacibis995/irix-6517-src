#ifndef H_RGPGAPI_H
#define H_RGPGAPI_H

/*
 *               Report Generator Server API header file
 *               =======================================
 *
 *   This API describes how the Report Generator core will communicate the 
 *   Report Generator Server (plugin). Server has to be developed 
 *   strictly following all the requirements of this API.
 *
 *   
 *   Function rgpgInit() is called from RG core every time DSO is loaded 
 *   into memory. Function rgpgDone() is called before unloading DSO from 
 *   memory.
 *   
 *   All report generation functions are called in response of analogous
 *   functions called from a user program to RG core. RG core makes validity 
 *   check for 'hError' and 'session' parameters. Therefore, RG server doesn't
 *   have to check it.
 *   
 *   The concrete specifics of all report generation functions can be found 
 *   in particular server description.
 *   
 *   General scenario is:
 *   
 *   rgpgInit() -- is called after server loaded into memory
 *   
 *       rgpgCreateSesion() -- start new session
 *   
 *           During the report session any number of calls
 *           to the next three function can occur in any order:
 *                - rgpgGetAttribute()
 *                - rgpgSetAttribute()
 *                - rgpgGenerateReport()
 *   
 *       rgpgDeleteSesion() -- end session
 *   
 *           At any point of time between different sessions server can be 
 *           unloaded from the memory. rgpgDone() will be called before 
 *           this unloading. If request comes for this particular server 
 *           it will be loaded again. rgpgInit() will be called each time 
 *           server is loaded into memory.
 *   
 *       rgpgCreateSesion() -- start new session
 *       . . .
 *       rgpgDeleteSesion() -- end session
 *   
 *   rgpgDone() -- called before server is unloaded from memory
 *   
 *   
 *   One session for each requested server is opened for each processed HTML file
 *   or HTML request for Report Generator. It does not matter
 *   how requests are distributed in file. Session is opened before the 
 *   first request to a particular server is processed.
 *   
 *   
 *   General form of HTML request for report is the following:
 *   
 *   
 *   <Desired machine SSS Web Server Internet address>/$sss/RG/server~par1~...~parn?attr1~ext1=value1&...&attrk~extk=valuek
 *   
 *   This request is converted into followind the sequence in current session:
 *        
 *        rgpgSetAttribute  (herr, session, "attr1", "ext1", "value1");
 *        . . .
 *        rgpgSetAttribute  (herr, session, "attrk", "extk", "valuek");
 *        rgpgGenerateReport(herr, session, n, {"par1", ..., "parn"}, outstream);
 *        
 *   A request for a particular attribute value is coded in the following manner:      
 *        
 *   <Desired machine SSS Web Server Internet address>/$sss/RG/server/attrname~extra?attr1~ext1=value1&...&attrk~extk=valuek
 *        
 *   This request is converted into the followind sequence in current session:
 *        
 *        rgpgSetAttribute  (herr, session, "attr1", "ext1", "value1");
 *        . . .
 *        rgpgSetAttribute  (herr, session, "attrk", "extk", "valuek");
 *  ... = rgpgGetAttribute  (herr, session, "attrname", "extra", &typattrib);
 *        
 *     Notes:   1. If extra attribute parameter is missing (together with '~'),   
 *     =====       the corresponding parameter in a function call will be zero.
 *              2. Calling sequence for a report request is the traditional argc,argv
 *                 mechanism like in main(int argc, char *argv[]) classic in C language.
 *                 Additional details of requested report can be coded in attributes 
 *                 setting following the question mark.
 */

#include <sys/types.h>
#include "sscStreams.h"

 
#ifndef RGPGAPI
#define RGPGAPI
#endif

typedef void* sscErrorHandle;
typedef void* rgpgSessionID;

/* Definition for rgpgGetAttribute function */
#define RGATTRTYPE_STATIC       0x01955001 /* Static memory pointer */
#define RGATTRTYPE_SPECIALALLOC 0x01955002 /* Special Alloced */
#define RGATTRTYPE_MALLOCED     0x01955003 /* Alloced trougth malloc() */

/* --------------------------------------------------------------------------- */
/* Definition for return code for rgpgGenerateReport */
#define RGPERR_SUCCESS          0x00000000  /* Success return code */
#define RGPERR_NEEDPERMISSION   0x00000001  /* User/Request need additional permissions */
#define RGPERR_REDIRECTED       0x00000002  /* Redirected (to URL) */
#define RGPERR_GENERICERROR     0x00000003  /* Generic error */

/* Definition for return flags for rgpgGenerateReport */
#define RGPERR_NOCACHE          0x00010000  /* Return code flag: nocache for result page */
#define RGPERR_NOPARSE          0x00020000  /* Return code flag: stop parsing and processing html file (just copy it to output stream) */

#define RGPERR_RETCODE_MASK     0x0000ffff  /* Return code mask */
#define RGPERR_RETFLAG_MASK     0xffff0000  /* Return flags mask */

/* --------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif
/* --------------------------------------------------------------------------- */
/****      rgpgInit
 *
 *   Function: To initialize the report generator server.
 *   Return:   != 0 - report generator server inited successfuly,  0 - errors (check hError stream)
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during
 *                 the execution of this function. Error condition usually not 
 *                 checked by RG core giving this opportunity to client program.
 *                 If client is not interested in the error condition, zero value 
 *                 can be supplied.
 */
int RGPGAPI rgpgInit(sscErrorHandle hError);
typedef int RGPGAPI FPrgpgInit(sscErrorHandle hError);
/* --------------------------------------------------------------------------- */
/****      rgpgDone
 *
 *   Function: To de-initialize the report generator server.
 *   Return:   != 0 - report generator server deinited successfuly,  0 - errors (check hError stream)
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during
 *                 the execution of this function. Error condition usually not 
 *                 checked by RG core giving this opportunity to client program.
 *                 If client is not interested in the error condition, zero value 
 *                 can be supplied.
 */
int RGPGAPI rgpgDone(sscErrorHandle hError);
typedef int RGPGAPI FPrgpgDone(sscErrorHandle hError);
/* --------------------------------------------------------------------------- */
/****      rgpgCreateSesion
 *
 *   Function: To create server specific report session
 *   Return:   Server specific session ID.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during
 *                 the execution of this function. Error condition usually not 
 *                 checked by RG core giving this opportunity to client program.
 *                 If client is not interested in the error condition, zero value 
 *                 can be supplied.
 */
rgpgSessionID RGPGAPI rgpgCreateSesion(sscErrorHandle hError); 
typedef rgpgSessionID RGPGAPI FPrgpgCreateSesion(sscErrorHandle hError); 
/* --------------------------------------------------------------------------- */
/****      rgpgDeleteSesion
 *
 *   Function: To delete server specific report session.
 *   Parameters:
 *
 *   session       Session to be deleted.
 *   hError        An error object reporting an error condition occured during
 *                 the execution of this function. Error condition usually not 
 *                 checked by RG core giving this opportunity to client program.
 *                 If client is not interested in the error condition, zero value 
 *                 can be supplied.
 */
void RGPGAPI rgpgDeleteSesion(sscErrorHandle hError, rgpgSessionID session);
typedef void RGPGAPI FPrgpgDeleteSesion(sscErrorHandle hError, rgpgSessionID session);
/* --------------------------------------------------------------------------- */
/****      rgpgGetAttribute
 *
 *   Function: To get server attribute value.
 *   Return:   String with requested attribute value. Full ownership to the string is returned.
 *             NULL string indicates some error registered in hError object.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during
 *                 the execution of this function. Error condition usually not 
 *                 checked by RG core giving this opportunity to client program.
 *                 If client is not interested in the error condition, zero value 
 *                 can be supplied.
 *   session       Session to be affected.
 *   attributeID   Attribute identifier.
 *   extraAttrSpec Additional parameter detailing the specified attribute. 
 *                 If not required it will be ignored.
 *   attrtype      Response attribute string type:
 *                 RGATTRTYPE_STATIC - static string (RG core must ignore free process for this string)
 *                 RGATTRTYPE_SPECIALALLOC - specific alloced string pointer (core must call rgpgFreeAttributeString(...)
 *                 RGATTRTYPE_MALLOCED - alloced thougth malloc (core must call free(for free string pointer)
 *                 
 */
char *RGPGAPI rgpgGetAttribute(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec,
int *attrtype); 
typedef char *RGPGAPI FPrgpgGetAttribute(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec,int *attrtype); 
/* --------------------------------------------------------------------------- */
/****      rgpgFreeAttributeString
 * 
 *   Function: Free attribute string (previously returned via rgpgGetAttribute function call)
 *
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during
 *                 the execution of this function. Error condition usually not 
 *                 checked by RG core giving this opportunity to client program.
 *                 If client is not interested in the error condition, zero value 
 *                 can be supplied.
 *   session       Session to be affected.
 *   attributeID   Attribute identifier.
 *   extraAttrSpec Additional parameter detailing the specified attribute. 
 *                 If not required it will be ignored.
 *   attrString    attribute string for deallocation (previously returned trougth rgpgGetAttribute
 *   attrtype      Response attribute string type:
 *                 RGATTRTYPE_STATIC - static string (RG core must ignore free process for this string)
 *                 RGATTRTYPE_SPECIALALLOC - specific alloced string pointer (core must call rgpgFreeAttributeString(...)
 *                 RGATTRTYPE_MALLOCED - alloced thougth malloc (core must call free(for free string pointer)
 * 
 *
 */
void RGPGAPI rgpgFreeAttributeString(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec,char *attrString,int attrtype);
typedef void RGPGAPI FPrgpgFreeAttributeString(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec,char *attrString,int attrtype);
/* --------------------------------------------------------------------------- */
/****      rgpgSetAttribute
 *
 *   Function: To set server attribute value.
 *   Return:   Size in bytes actually transfered between user and RG data space.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during
 *                 the execution of this function. Error condition usually not 
 *                 checked by RG core giving this opportunity to client program.
 *                 If client is not interested in the error condition, zero value 
 *                 can be supplied.
 *   session       Session to be affected.
 *   attributeID   Attribute identifier. 
 *   extraAttrSpec Additional parameter detailing the specified attribute. 
 *                 If not required it will be ignored.
 *   value         String representing new value of attribute
 */
void  RGPGAPI rgpgSetAttribute(sscErrorHandle hError, rgpgSessionID session, const char *attributeID, const char *extraAttrSpec, const char *value); 
typedef void  RGPGAPI FPrgpgSetAttribute(sscErrorHandle hError, rgpgSessionID session, const char *attributeID, const char *extraAttrSpec, const char *value); 
/* --------------------------------------------------------------------------- */
/****      rgpgGenerateReport
 *
 *   Function:       To generate report. Detailed specification can be found
 *                   in a particular server description.
 *   Return:         Stream with requested report. Full ownership to the stream is returned.
 *                   integer value - RGPERR_...
 *                   RGPERR_REDIRECTED - result stream contained redirected url
 *   Parameters:
 *
 *   hError           An error object reporting an error condition occured during
 *                    the execution of this function. Error condition usually not 
 *                    checked by RG core giving this opportunity to client program.
 *                    If client is not interested in the error condition, zero value 
 *                    can be supplied.
 *   session          Session to be affected.
 *   argc             Number of arguments in 'argv' vector.
 *   argv             Arguments vector. argv[0] - server name, argv[1],...argv[argc-1] - parameters of report 
 *   rawCommandString "Unprocessed" command string
 *   result           Stream for result output. 
 */
int RGPGAPI rgpgGenerateReport(sscErrorHandle hError, rgpgSessionID session, int argc, char* argv[], char *rawCommandString, streamHandle result, char *userAccessMask); 
typedef int RGPGAPI FPrgpgGenerateReport(sscErrorHandle hError, rgpgSessionID session, int argc, char* argv[], char *rawCommandString, streamHandle result, char *userAccessMask); 

#ifdef __cplusplus
}
#endif
#endif /* #ifndef H_RGPGAPI_H */
