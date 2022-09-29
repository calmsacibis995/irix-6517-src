/***********************************************************************\
*	File:		prom_config.h					*
*									*
*	Contains the data structure definitions for the NVRAM		*
*	configuration table.						*
*									*
\***********************************************************************/

#ifndef _IP21PROM_CONFIG_H_
#define _IP21PROM_CONFIG_H_

#define CFG_MAXUNITS	8		/* Maximum number of units per brd */
#define CFG_SIZE	4		/* size of config entry */

/*
 * Offsets into NVRAM used to retrieve information.
 */
#define BCF_CLASS	0
#define BCF_TYPE	1
#define BCF_REV	2
#define BCF_STATUS	5
#define BCF_DIAGVAL	6
#define BCF_UNIT(_unit, _off)  ((_unit)*CF_SIZE + _off)

#define CF_STATUS	0
#define CF_DIAGVAL	1

/*
 * Bit values for various flags used in the config structure's
 * status field.
 */
#define	CFS_DISABLE	1		/* Set if software disabled device */
#define CFS_PHYS_PRES	2		/* Set if device is found by scan */
#define CFS_LOG_PRES	4		/* Set if device was prev. found */
#define CFS_BUSTED	8		/* Set if diags for device failed */

#if LANGUAGE_C
/*
 * Hardware configuration data structure. This information is 
 * saved in NVRAM so that the PROM can check the system for unintended
 * changes to the configuration.  
 */

typedef struct {
	uchar	  cf_status;		/* Status info for this unit */
	uchar	  cf_diagval;		/* Return value from diags */
	ushort	  cf_pad;		/* Leftover space */
} config_t;

typedef struct {
	uchar	  bcf_class;		/* Class containing this board */
	uchar	  bcf_type;		/* Type of board */
	ushort	  bcf_rev;		/* Board's revision level */
	config_t  bcf_status;		/* Status of global board-level logic */
	config_t  bcf_unit[CFG_MAXUNITS]; /* Status of units on board */
} brdconfig_t;

#endif /* LANGUAGE_C */

#endif /* _IP21PROM_CONFIG_H_ */
