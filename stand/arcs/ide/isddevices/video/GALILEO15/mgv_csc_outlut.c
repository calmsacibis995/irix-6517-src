/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       mgv_csc_outlut.c
*/

#include "mgv_diag.h"

/*
 * Forward Function References
 */

__uint32_t mgv_csc_OutputLuts(void);
__uint32_t _mgv_csc_TestLut(__uint32_t, __uint32_t, u_int16_t *, u_int16_t *);

/*
 * NAME: mgv_csc_OutputLuts
 *
 * FUNCTION: Writes and reads back walking 0's/1's and different test patterns 
 * onto CSC Output LUTS Y/G, U/B, V/R and Alpha.
 *
 * INPUTS:  None
 *
 * OUTPUTS: -1 if error, 0 if no error
 */


__uint32_t
mgv_csc_OutputLuts(void)
{

      __uint32_t errors = 0, errCode ;
      u_int16_t *walkingBuf, *readBuf ;

      errCode = CSC_OUTPUT_LUT_TEST ;
      msg_printf(SUM, "%s\n", mgv_err[errCode].TestStr);


      if ((walkingBuf = (u_int16_t *)get_chunk(2 * CSC_MAX_LOAD * 
			sizeof(u_int16_t))) == NULL) {
	msg_printf(ERR, "Error in allocating memory for walking buf\n");
	return (-1);
      }
      readBuf = walkingBuf + CSC_MAX_LOAD  ;

      _mgv_csc_setupDefault();

      csc_outlut = 1 ; /* XXX */

      _mgv_csc_LoadBuf(CSC_MAX_LOAD, walkingBuf, CSC_LUT_WALKING_ALL, CSC_YG_OUT); /* XXX */
      /* _mgv_csc_LoadBuf(CSC_MAX_LOAD, walkingBuf, CSC_LUT_WALKING10bit, 4); */

      errors += _mgv_csc_TestLut(CSC_YG_OUT,CSC_YG_READ,walkingBuf, readBuf);
      errors += _mgv_csc_TestLut(CSC_UB_OUT,CSC_UB_READ,walkingBuf, readBuf);
      errors += _mgv_csc_TestLut(CSC_VR_OUT,CSC_VR_READ,walkingBuf, readBuf);
      errors += _mgv_csc_TestLut(CSC_ALPHA_OUT,CSC_ALPHA_READ,walkingBuf, readBuf);

      return(_mgv_reportPassOrFail((&mgv_err[errCode]) ,errors));
}

__uint32_t _mgv_csc_TestLut(__uint32_t loadLUTId, __uint32_t readLUTId, u_int16_t *buf, u_int16_t *readBuf)
{
      __uint32_t errors ;


      _mgv_csc_LoadLut(CSC_LUT_LOAD, loadLUTId, buf);
      _mgv_csc_ReadLut(loadLUTId,readLUTId,readBuf);
      errors = _mgv_csc_VerifyBuf(CSC_MAX_LOAD,loadLUTId, buf, readBuf);


      return (errors);
}
