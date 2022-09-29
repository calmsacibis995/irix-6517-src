/* --------------------------------------------------------------------------- */
/* -                               RGAPI.H                                   - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
/*                  Report Generator API Header File                           */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/*   Report Generator (RG) can be linked to a client application either        */
/*   statically, or dynamically. For static linking this header file           */
/*   should be included to accsess RG capabilities.                            */
/* --------------------------------------------------------------------------- */
#ifndef H_RGAPI_H
#define H_RGAPI_H

#include <sys/types.h>
#include "sscErrors.h"
#include "rgPluginAPI.h"

typedef void* srgSessionID;

#define RGCORE_VERSION_MAJOR 2 /* Report Generator Core major version number */
#define RGCORE_VERSION_MINOR 0 /* Report Generator Core minor version number */

#ifdef __cplusplus
extern "C" {
#endif


/*                 Initializing the Report Generator
 *                 =================================
 */

/****      srgInit
 *
 *   Function: To initialize the Report Generator.
 *             Function has to be called once before using the Report Generator.
 *   Return:   != 0 - success,  == 0 - error
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 */
int srgInit(sscErrorHandle hError); 

/****      srgDone
 *
 *   Function: To deinitialize the Report Generator.
 *             This function has to be called last
 *   Return:   != 0 - success,  == 0 - error
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 */
int srgDone(sscErrorHandle hError);


/*                 Report Generator tuning
 *                 =======================
 */

/****      srgGetAttribute
 *
 *   Function: To get the Report Generator attribute value.
 *   Return:   String with requested attribute value. 
 *             NULL string indicates some error registered in hError object.
 *             Type of memory allocation will appear in restype parameter.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 *   attributeID   Attribute identifiers are defined in 'rgAttributeID's at the bottom of this file.
 *   extraAttrSpec Additional parameter detailing the specified attribute. 
 *                 If not required it will be ignored.
 *   restype       Response attribute string type:
 *                 RGATTRTYPE_STATIC - static string (RG core must ignore free process for this string)
 *                 RGATTRTYPE_SPECIALALLOC - specific alloced string pointer (core must call rgpgFreeAttributeString(...)
 *                 RGATTRTYPE_MALLOCED - alloced thougth malloc (core must call free(for free string pointer)
 */
char *srgGetAttribute(sscErrorHandle hError, const char *attributeID, const char *extraAttrSpec, int *restype); 

/****      srgFreeAttribute
 *
 *   Function: To free the Report Generator attribute value.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 *   attributeID   Attribute identifiers are defined in 'rgAttributeID's at the bottom of this file.
 *   extraAttrSpec Additional parameter detailing the specified attribute. 
 *                 If not required it will be ignored.
 *   str           resource string (returned from srgGetAttribute call with restype parameter equ RGATTRTYPE_SPECIALALLOC)
 *   restype       Response attribute string type:
 *                 RGATTRTYPE_STATIC - static string (RG core must ignore free process for this string)
 *                 RGATTRTYPE_SPECIALALLOC - specific alloced string pointer (core must call rgpgFreeAttributeString(...)
 *                 RGATTRTYPE_MALLOCED - alloced thougth malloc (core must call free(for free string pointer)
 */
void srgFreeAttribute(sscErrorHandle hError, const char *attributeID, const char *extraAttrSpec,char *str,int restype);

/****      srgSetAttribute
 *
 *   Function: Set the Report Generator attribute value.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 *   attributeID   Attribute identifier.
 *   extraAttrSpec Additional parameter detailing the specified attribute. 
 *                 If not required it will be ignored.
 *   value         String representing new value of attribute
 */
void srgSetAttribute(sscErrorHandle hError, const char *attributeID, const char *extraAttrSpec, const char *value); 


/*           Generating the Report and tuning the Report Generator Server
 *           ============================================================
 */


/****      srgCreateReportSession
 *
 *   Function: To create new report session.
 *   Return:   A new Report Generator session ID or NULL if unsuccessful.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 *   serverName    Specification of a plugin '.so' file.
 */
srgSessionID srgCreateReportSession(sscErrorHandle hError, const char *serverName); 

/****      srgDeleteReportSession
 *
 *   Function: To close the report session.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 *   hSession      Report Generator session ID.
 */
void srgDeleteReportSession(sscErrorHandle hError, srgSessionID hSession); 

/****      srgGetReportAttribute
 *
 *   Function: To get the Report Server attribute value.
 *   Return:   String with requested attribute value. Full ownership to the string is returned.
 *             NULL string indicates some error registered in hError object.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 *   hSession      Report Generator session ID.
 *   attributeID   Attribute identifier.
 *   extraAttrSpec Additional parameter detailing the specified attribute. 
 *                 If not required it will be ignored.
 *   restype       Response attribute string type:
 *                 RGATTRTYPE_STATIC - static string (RG core must ignore free process for this string)
 *                 RGATTRTYPE_SPECIALALLOC - specific alloced string pointer (core must call rgpgFreeAttributeString(...)
 *                 RGATTRTYPE_MALLOCED - alloced thougth malloc (core must call free(for free string pointer)
 */
char *srgGetReportAttribute(sscErrorHandle hError,srgSessionID hSession, const char *attributeID,const char *extraAttrSpec, int *restype); 

/****      srgFreeReportAttributeString
 * 
 *   Function: Free attribute string (previously returned via srgGetAttribute function call)
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
 *   restype       Response attribute string type:
 *                 RGATTRTYPE_STATIC - static string (RG core must ignore free process for this string)
 *                 RGATTRTYPE_SPECIALALLOC - specific alloced string pointer (core must call rgpgFreeAttributeString(...)
 *                 RGATTRTYPE_MALLOCED - alloced thougth malloc (core must call free(for free string pointer)
 *
 */
void srgFreeReportAttributeString(sscErrorHandle hError,srgSessionID hSession,const char *attributeID,const char *extraAttrSpec,char *attrString,int restype);

/****      srgSetReportAttribute
 *
 *   Function: To set the Report Server attribute value.
 *   Parameters:
 *
 *   hError        An error object reporting an error condition occured during the 
 *                 execution of this function. It has to be checked after return 
 *                 from the function. If a user is not interested in the error condition, 
 *                 zero value can be supplied.
 *   hSession      Report Generator session ID.
 *   attributeID   Attribute identifier. 
 *   extraAttrSpec Additional parameter detailing the specified attribute. 
 *                 If not required it will be ignored.
 *   value         String representing new value of attribute
 */
void srgSetReportAttribute(sscErrorHandle hError, srgSessionID hSession, const char *attributeID, const char *extraAttrSpec, const char *value); 

/****      srgGenerateReport
 *
 *   Function: To generate report. Detailed specification can be found in particular server description.
 *   Parameters:
 *
 *   hError           An error object reporting an error condition occured during the 
 *                    execution of this function. It has to be checked after return 
 *                    from the function. If a user is not interested in the error condition, 
 *                    zero value can be supplied.
 *   hSession         Report Generator session ID.
 *   argc             Number of arguments in 'argv' vector.
 *   argv             Arguments vector. argv[0] - server name, argv[1],...argv[argc-1] - parameters of report 
 *   rawCommandString "Unprocessed" command string from HTTP request
 *   result           Stream for result output. 
 *
 *   Note:            for return code specification see rgPluginAPI.h file
 */
int srgGenerateReport(sscErrorHandle hError, srgSessionID hSession, int argc, char* argv[], char *rawCommandString, streamHandle result);

#ifdef __cplusplus
}
#endif
#endif /* #ifndef H_RGAPI_H */
