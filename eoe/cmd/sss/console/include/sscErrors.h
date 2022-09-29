/* --------------------------------------------------------------------------- */
/* -                             SSCERRORS.H                                 - */
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
#ifndef _SSCERRORS_H_
#define _SSCERRORS_H_

#include "sscStreams.h"

#ifndef SSCAPI
#ifdef _MSC_VER 
#define SSCAPI WINAPI
#endif
#endif

#ifndef SSCAPI
#define SSCAPI
#endif

typedef void* sscErrorHandle;

#define SSCERR_GETERRBLK_ALLOC  0x01965001
#define SSCERR_GETERRBLK_BUSY   0x01965002
#define SSCERR_GETERRBLK_FREE   0x01965003

#ifdef __cplusplus
extern "C" {
#endif

/****      sscErrorGetInfo
 *
 *   Function: Get SSC Error objects library internal info
 *   Return: Internal info from Error objects library
 *   Parameters:
 *   idx - integer value - reference to some internal counter
 *         (see. SSCERR_GETERRBLK_...)
 */
unsigned long SSCAPI sscErrorGetInfo(int idx);


/****      sscCreateErrorObject
 *
 *   Function: To create new error object
 *   Return:   A handle to a new error object or NULL if can't create an object
 */
sscErrorHandle SSCAPI sscCreateErrorObject(void);


/****      sscDeleteErrorObject
 *
 *   Function: To release given error object
 *   Return:   0 if successful return or -1 otherwise 
 *   Parameters:
 *
 *   hError        Handle of an error object to be deleted
 */
int SSCAPI sscDeleteErrorObject(sscErrorHandle hError); 


/****      sscIsErrorObject
 *
 *   Function: Verify if given handle is represented a valid error object handle
 *   Return:   Non zero value (TRUE) if valid error handle or 0 (FALSE) if invalid handle supplied 
 *   Parameters:
 *
 *   hError        Handle of an error object to be verified
 */
int SSCAPI sscIsErrorObject(sscErrorHandle hError);


/****      sscGetErrorStream
 *
 *   Function: To get error log stream of the given error object
 *   Return:   Error log stream handle or NULL if invalid handle supplied
 *   Parameters:
 *
 *   hError        Handle of a given error object
 */
streamHandle SSCAPI sscGetErrorStream(sscErrorHandle hError); 


/****      sscMarkErrorZone
 *
 *   Function: Start of an error time zone in the given error object
 *             sscCreateErrorObject() opens first time zone.
 *   Return:   0 - success, -1 otherwise
 *   Parameters:
 *
 *   hError        Handle of a given error object
 */
int SSCAPI sscMarkErrorZone(sscErrorHandle hError); 


/****      sscWereErrorsInZone
 *
 *   Function: To check if any error was logged in current time zone.
 *   Return:   Zero (FALSE) if no errors was logged or non zero (TRUE) otherwise
 *   Parameters:
 *
 *   hError        Handle of a given error object
 */
int SSCAPI sscWereErrorsInZone(sscErrorHandle hError); 


/****      sscWereErrors
 *
 *   Function: To check if an error was logged in the given error object
 *             since it was created
 *   Return:   Zero (FALSE) if no errors was logged or non zero (TRUE) otherwise
 *   Parameters:
 *
 *   hError        Handle of a given error object
 */
int SSCAPI sscWereErrors(sscErrorHandle hError); 


/****      sscError
 *
 *   Function: To log an error in the given error object
 *   Return:   0 if successful return or -1 otherwise 
 *   Parameters:
 *
 *   hError        Handle of a given error object
 *   errorMessage  Text to be logged 
 */
int  SSCAPI sscError(sscErrorHandle hError, char *msg, ...);
int  SSCAPI sscErrorStream(sscErrorHandle hError,char *msg, ...);

#ifdef __cplusplus
}
#endif

#endif
