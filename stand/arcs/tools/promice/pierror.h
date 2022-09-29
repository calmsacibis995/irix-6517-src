/*	PiError.h - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - error codes and messages
*/

#define	PGE_NOE	0		/* NO ERROR */
#define	PGE_RES	1		/* no resource available */
#define	PGE_BSY	2		/* interface is busy */
#define	PGE_TMR	3		/* timer expired */
#define	PGE_HDO	4		/* host data overflow */
#define	PGE_NDA	5		/* no data available */
#define	PGE_UIF	6		/* un implemented feature */
#define	PGE_SYN	7		/* error in script syntax */
#define	PGE_CMD	8		/* no script to process command */
#define	PGE_NUM 9		/* to many numeric items in script */
#define	PGE_LST 10		/* too many items for numeric list */
#define	PGE_INP 11		/* expected item not in input */
#define	PGE_FNM 12		/* file name error */
#define	PGE_BRT	13		/* invalid baud rate */
#define	PGE_ROM	14		/* invalid ROM size */
#define	PGE_WDS	15		/* invalid word size */
#define	PGE_IDL	16		/* invalid unit ID list */
#define	PGE_OPN	17		/* open failed */
#define	PGE_SKP	18		/* unable to skip file data */
#define	PGE_IOE	19		/* device I/O error */
#define	PGE_BPN	20		/* bad port name */
#define	PGE_EOF	21		/* end-o-file */
#define	PGE_CFT	22		/* bad paramter to `picmd()` */
#define	PGE_COM	23		/* communication error */
#define	PGE_NTL	24		/* name is too long */
#define	PGE_TMO	25		/* timed out */
#define	PGE_NYI	26		/* unimplemented feature */
#define	PGE_VFE	27		/* verify failed */
#define	PGE_BIG	28		/* ID out of range */
#define	PGE_AOR	29		/* data address out of range */
#define	PGE_CFG	30		/* ran out of config space */
#define	PGE_BAA	31		/* bad arguments */
#define	PGE_CHK	32		/* bad checksum in record */
#define	PGE_UNF	33		/* feature not supported */
#define	PGE_DRV	34		/* bad argument for driver call */
#define	PGE_BAD	35		/* bad data in hex record */
#define	PGE_LOK	36		/* unit is locked */
#define	PGE_BCF 37		/* bad unit in config */
#define	PGE_PWR	38		/* mem size 0 check power on unit */
#define	PGE_USR	39		/* operation terminated at user request */
#define	PGE_OVR	40		/* data over-run on serial i/o */
#define	PGE_KEY 41		/* no key assigned */
#define	PGE_EMU	42		/* unit is emulating */
#define	PGE_LOD	43		/* unit is in load mode */
#define	PGE_NOP	44		/* no operation */
#define	PGE_XXX	45		/* no link to PromICE */
#define	PGE_N31	46		/* no AI 3.1 */
#define	PGE_BAF	47		/* must define word size first */
#define	PGE_MAX	48		/* Max value for `pxerror` */

#ifdef	PDGLOB
char *pxerrmsg[] =
		{
		"No error - you should never see this",
		"Command failed - Interface is not available or not active",
		"Command failed - Interface is BUSY",
		"Command failed - Timer expired while waiting",
		"Command failed - DataOverflow - lost host data",
		"Command failed - No data available from the target",
		"Command failed - Feature not implemented",
		"LoadICE parser internal error #1 - report to GEI",
		"Illegal command - check your input syntax",
		"LoadICE parser internal error #2 - report to GEI",
		"Too many arguments supplied - check command syntax",
		"Expected argument not supplied - check command syntax",
		"Filename error - check path or spelling",
		"Invalid baud-rate - only 1200,2400,4800,9600,19200 & 57600 allowed",
		"Invalid ROM size - example: use like: 27c010, 128k or 131072",
		"Invalid word-size - must be a multiple of 8, like 8, 16, 24, 32 etc.",
		"Invalid ID list - check syntax, I can't figure out your wordsize",
		"Open failed - check filename, path or spelling of name",
		"Unable to skip file data - disk I/O error",
		"Device I/O error - I don't know what happened",
		"Bad port name - check serial or parallel port name",
		"End-o-File - unexpected end of file encountered",
		"Bad parameters to picmd() - report to GEI",
		"Communication error - cycle power on your units and retry",
		"Name too long - filename can't be this long",
		"Timed out waiting for response\n try 'slow' in ini file if using serial port",
		"FEATURE NOT IMPLEMENTED YET! - sorry!",
		"Verify failed - transmission error, check host cabling",
		"Invalid unit ID - check to see how may units there are?",
		"Address out of range\n can't load this data in this ROM (configuration)",
		"LoadICE internal error #3 - report to GEI",
		"Bad arguments - check command syntax for parameters",
		"Bad checksum in record - hex record checksum doesn't compute",
		"Feature not supported on this unit\n call 1-800-PROMICE for upgrade!",
		"Bad argument for driver call - report to GEI",
		"Bad data in hex record\n corrupted data file or wrong type file",
		"Unit is LOCKED - sorry!",
		"Not enough units to emulate the word-size or configuration\n check syntax of 'word'  command",
		"Unit can't detect any RAM,\n check power level to the Emulator & retry",
		"Operation terminated by user - as you wished!",
		"Data over-run - your host is too busy\n try 'slow' in ini file if using serial port",
		"Key NOT Assigned - press some other key",
		"Can't do while Emulating! - must do 'stop' first",
		"Not emulating! - do a 'go' first",
		"No operation! - you terminated the operation",
		"No link with units - I am not talking to the Target",
		"Must have AI Rev3.1 for this to work!\n call 1-800-PROMICE for upgrade!",
		"Must include 'word=' statement BEFORE the 'fill' to work"
		};
#else
extern char *pxerrmsg[];
#endif
extern long pxerror;
extern char *pi_estr1;
extern char *pi_estr2;
extern char *pi_eloc;
extern long pi_enum1;
extern long pi_enum2;
extern short pi_eflags;
#define PIE_NUM	0x0001			/* display numbers */

#ifdef ANSI
void pierror(void);
#else
void pierror();
#endif
extern short pi_endx;
