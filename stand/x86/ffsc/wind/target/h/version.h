/* version.h - VxWorks version information */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
02u,25oct95,ms	 changed release to 5.3
02t,21jun95,ms	 changed release to 5.3-beta
02s,29mar95,kdl  changed release to 5.2.
02r,12nov94,dvs  changed release to 5.2-beta
02q,18apr94,caf  changed to 5.1.1-R4000-beta-1, updated copyright.
02p,20nov93,caf  changed to 5.1.1-R3000-beta-1, updated copyright.
02o,28jul93,caf  changed to 5.1-R3000-alpha-1.
02n,07apr94,caf  changed to 5.1.1-R3000.
02m,19may94,kdl  changed release to 5.1.1 for Am29k
02l,03dec93,pad  changed release to 5.1.1 for Am29K-Beta
02k,11mar94,rrr  vxsim fcs.
02j,22feb94,gae  changed to 1.1.1.
02i,29jun93,gae  vxsim.
02h,26sep93,jmm  changed release to 5.1.1
02g,18aug93,jmm  changed release to 5.1.1 (engineering)
02f,22sep92,kdl  fixed ansi warnings.
02e,22sep92,rrr  added support for c++
02d,22sep92,kdl  added string max lengths; made vxWorksVersion and creationDate 
		 const; added support for c++.
02c,27jul92,jwt  moved missing space to printLogo() instead of VXWORKS_VERSION.
02b,26jul92,jwt  changed to 5.1 (alpha); add missing space in between strings.
02a,04jul92,jcf  cleaned up.
01j,26may92,rrr  the tree shuffle
01i,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01h,25sep91,yao  added 5.0.2-683xx for 683XX.
01g,26apr91,nfs  changed to 5.0.x alpha, updated copyright
01f,05oct90,shl  added copyright notice.
01e,15feb89,dnw  changed to 4.1 alpha
01d,30oct88,jcf  changed to 4.0. (finally)
01c,31mar88,jcf  moved to version 4.0 beta
01b,17nov87,dnw  added IMPORTS
01a,15oct87,jlf  written
*/

#ifndef __INCversionh
#define __INCversionh

#ifdef __cplusplus
extern "C" {
#endif

#define VXWORKS_VERSION "5.3"

#define CREATE_DATE_MAX_LENGTH	40
#define VERSION_MAX_LENGTH	40

IMPORT char * creationDate;
IMPORT char * vxWorksVersion;


#ifdef __cplusplus
}
#endif

#endif /* __INCversionh */
