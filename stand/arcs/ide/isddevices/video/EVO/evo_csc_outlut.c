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
**      FileName:       evo_csc_outlut.c
*/

#include "evo_diag.h"

/*
 * Forward Function References
 */

__uint32_t evo_csc_OutputLuts(void);
__uint32_t _evo_csc_TestLut(__uint32_t, u_int16_t *, u_int16_t *);

/*
 * NAME: evo_csc_OutputLuts
 *
 * FUNCTION: Writes and reads back walking 0's/1's and different test patterns 
 * onto CSC Output LUTS Y/G, U/B, V/R and Alpha.
 *
 * INPUTS:  None
 *
 * OUTPUTS: -1 if error, 0 if no error
 */


__uint32_t
evo_csc_OutputLuts(void)
{

      __uint32_t errors = 0, errCode ;
      u_int16_t *walkingBuf, *readBuf ;

      errCode = CSC_OUTPUT_LUT_TEST ;
      msg_printf(SUM, "%s\n", evo_err[errCode].TestStr);


      if ((walkingBuf = (u_int16_t *)get_chunk(2 * EVO_CSC_MAX_LOAD * 
			sizeof(u_int16_t))) == NULL) {
	msg_printf(ERR, "Error in allocating memory for walking buf\n");
	return (-1);
      }
      readBuf = walkingBuf + EVO_CSC_MAX_LOAD  ;

      _evo_csc_setupDefault();

      csc_outlut = 1 ; /* XXX */

	/*XXX Loads walkingBuf with patterns*/
      _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, walkingBuf, CSC_LUT_WALKING_ALL, CSC_YG_OUT); /* XXX */
      /* _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, walkingBuf, CSC_LUT_WALKING10bit, 4); */

	/*XXX All LUTs are now Read/Load..code below may need changing*/
      errors += _evo_csc_TestLut(CSC_YG_OUT, walkingBuf, readBuf);
      errors += _evo_csc_TestLut(CSC_UB_OUT, walkingBuf, readBuf);
      errors += _evo_csc_TestLut(CSC_VR_OUT, walkingBuf, readBuf);
      errors += _evo_csc_TestLut(CSC_ALPHA_OUT, walkingBuf, readBuf);

      return(_evo_reportPassOrFail((&evo_err[errCode]) ,errors));
}

__uint32_t _evo_csc_TestLut(__uint32_t loadLUTId, u_int16_t *buf, u_int16_t *readBuf)
{
      __uint32_t errors ;


      _evo_csc_LoadLut(CSC_LUT_LOAD, loadLUTId, buf);
      _evo_csc_ReadLut(CSC_LUT_READ, loadLUTId, readBuf);
      errors = _evo_csc_VerifyBuf(EVO_CSC_MAX_LOAD,loadLUTId, buf, readBuf);


      return (errors);
}
