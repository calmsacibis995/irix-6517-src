#ifndef __DEVCONVERT_H__
#define __DEVCONVERT_H__
#ifdef __cplusplus
extern "C" {
#endif
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"irix/include/devconvert.h $Revision: 1.1 $"


/*	routines to convert among device files referring to
 *	the same device.
*/

/* includes */

#include <sys/types.h>

typedef int dv_bool;
const dv_bool dv_true = 1, dv_false = 0;

char *dev_to_scsiname(dev_t dev_in, char *scsiname_out, int *length_inout);
char *fdes_to_scsiname(int fdes_in, char *scsiname_out, int *length_inout);
char *filename_to_scsiname(const char *filename_in,
			   char *scsiname_out, int *length_inout);

char *dev_to_diskname(dev_t dev_in,
		      dv_bool raw_in, int partno_in,
		      char *diskname_out, int *length_inout);
char *fdes_to_diskname(int fdes_in,
		       dv_bool raw_in, int partno_in,
		       char *diskname_out, int *length_inout);
char *filename_to_diskname(const char *filename_in,
			   dv_bool raw_in, int partno_in,
			   char *diskname_out, int *length_inout);

char *dev_to_tapename(dev_t dev_in,
		      dv_bool raw_in, dv_bool rewind_in, dv_bool byteswap_in,
		      dv_bool var_bsize_in, int density_in, dv_bool compression_in,
		      char *tapename_out, int *length_inout);
char *fdes_to_tapename(int fdes_in,
		       dv_bool raw_in, dv_bool rewind_in, dv_bool byteswap_in,
		       dv_bool var_bsize_in, int density_in,
		       dv_bool compression_in,
		       char *tapename_out, int *length_inout);
char *filename_to_tapename(const char *filename_in,
			   dv_bool raw_in, dv_bool rewind_in, dv_bool byteswap_in,
			   dv_bool var_bsize_in, int density_in,
			   dv_bool compression_in,
			   char *tapename_out, int *length_inout);

/* For floppies, diameter is either 525 for 5 1/4" or 350 for 3 1/2".
 * Capacity is in kbytes.  The valid combinations are:
 *
 *	Diameter	Capacity		Old Name
 *
 *	 525		   360			fdsXdY.48
 *	 525		   720			fdsXdY.96
 *	 525		  1200			fdsXdY.96hi
 *	 350		   720			fdsXdY.3.5
 *	 350		  1440			fdsXdY.3.5hi
 *	 350		 20331			fdsXdY.3.5.20m
 *
 * Or, you may specify diameter and capacity of 0 to get a default
 * device.
 */

char *dev_to_floppyname(dev_t dev_in,
			int diameter, int capacity,
			char *floppyname_out, int *length_inout);
char *fdes_to_floppyname(int fdes_in,
			 int diameter, int capacity,
			 char *floppyname_out, int *length_inout);
char *filename_to_floppyname(const char *filename_in,
			     int diameter, int capacity,
			     char *floppyname_out, int *length_inout);

#ifdef __cplusplus
}
#endif
#endif /* !__DEVCONVERT_H__ */
