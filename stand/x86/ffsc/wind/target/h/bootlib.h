/* bootLib.h - boot support subroutine library */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01g,22sep92,rrr  added support for c++
01f,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01c,05oct90,shl  added ANSI function prototypes.
                 added copyright notice.
01b,10aug90,dnw  added declaration of bootParamsErrorPrint().
01a,18jul90,dnw  written
*/

#ifndef __INCbootLibh
#define __INCbootLibh

#ifdef __cplusplus
extern "C" {
#endif

/* BOOT_PARAMS is a structure containing all the fields of the VxWorks
 * boot line.  The routines in bootLib convert this structure to and
 * from the boot line ascii string.
 */

#define BOOT_DEV_LEN		20	/* max chars in device name */
#define BOOT_HOST_LEN		20	/* max chars in host name */
#define BOOT_ADDR_LEN		30	/* max chars in net addr */
#define BOOT_FILE_LEN		80	/* max chars in file name */
#define BOOT_USR_LEN		20	/* max chars in user name */
#define BOOT_PASSWORD_LEN	20	/* max chars in password */
#define BOOT_OTHER_LEN		80	/* max chars in "other" field */

#define BOOT_FIELD_LEN		80	/* max chars in boot field */

typedef struct				/* BOOT_PARAMS */
    {
    char bootDev [BOOT_DEV_LEN];	/* boot device code */
    char hostName [BOOT_HOST_LEN];	/* name of host */
    char targetName [BOOT_HOST_LEN];	/* name of target */
    char ead [BOOT_ADDR_LEN];		/* ethernet internet addr */
    char bad [BOOT_ADDR_LEN];		/* backplane internet addr */
    char had [BOOT_ADDR_LEN];		/* host internet addr */
    char gad [BOOT_ADDR_LEN];		/* gateway internet addr */
    char bootFile [BOOT_FILE_LEN];	/* name of boot file */
    char startupScript [BOOT_FILE_LEN];	/* name of startup script file */
    char usr [BOOT_USR_LEN];		/* user name */
    char passwd [BOOT_PASSWORD_LEN];	/* password */
    char other [BOOT_OTHER_LEN];	/* available for applications */
    int  procNum;			/* processor number */
    int  flags;				/* configuration flags */
    } BOOT_PARAMS;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	bootBpAnchorExtract (char *string, char ** pAnchorAdrs);
extern STATUS 	bootNetmaskExtract (char *string, int *pNetmask);
extern STATUS 	bootScanNum (char ** ppString, int *pValue, BOOL hex);
extern STATUS 	bootStructToString (char *paramString, BOOT_PARAMS
		*pBootParams);
extern char *	bootStringToStruct (char *bootString, BOOT_PARAMS *pBootParams);
extern void 	bootParamsErrorPrint (char *bootString, char *pError);
extern void 	bootParamsPrompt (char *string);
extern void 	bootParamsShow (char *paramString);

#else	/* __STDC__ */

extern STATUS 	bootBpAnchorExtract ();
extern STATUS 	bootNetmaskExtract ();
extern STATUS 	bootScanNum ();
extern STATUS 	bootStructToString ();
extern char *	bootStringToStruct ();
extern void 	bootParamsErrorPrint ();
extern void 	bootParamsPrompt ();
extern void 	bootParamsShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCbootLibh */
