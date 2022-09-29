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
**      FileName:       evo_csc.c
*/

#include "evo_diag.h"

/*
 * Forward Function References
 */

__uint32_t  evo_CSC(__uint32_t argc, char **argv);

__uint32_t  evo_cscTest (__uint32_t csc_num, __uint32_t ifmt, __uint32_t ofmt, __uint32_t idif, 
		 __uint32_t odif,  __uint32_t bypass, __uint32_t a_kin, __uint32_t a_kop);

void  _evo_csc_yuv2rgb(void);

void  _evo_csc_rgb2yuv(void);

/*
 * NAME: evo_csc
 *
 * FUNCTION: top level function for csc
 *
 * INPUTS: argc and argv
 *
 * OUTPUTS: -1 if error, 0 if no error
 * 
 */

__uint32_t
evo_csc(__uint32_t argc, char **argv)
{
    __uint32_t	m_errors = 0;
    __uint32_t	badarg = 0;
    __uint32_t	mi_fmt = 1;
    __uint32_t  mo_fmt = 1;
    __uint32_t  m_blank = 0;
    __uint32_t  m_bypass = 0;
    __uint32_t	mi_dif = 0;
    __uint32_t  mo_dif = 0;
    __uint32_t	m_csc_num = 1;
    __uint32_t  m_a_kin = 0;
    __uint32_t	m_a_kop = 0;
    __uint32_t	m_full_scl = 0;


    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'n':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &m_csc_num); /*which CSC into csc_num*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(m_csc_num));
			}
                        break;
                case 'i':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &(mi_fmt)); /*reads input format into fmt, 1=>YUV442/4*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &(mi_fmt));
			}
                        break;
                case 'o':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &mo_fmt); /*reads output format into mo_fmt, 1=>YUV422/4*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &mo_fmt);
			}
                        break;
                case 'd':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &mi_dif); /*Getting input dif enable info, 1=>Interpolation*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &mi_dif);
			}
                        break;
                case 'f':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &mo_dif); /*Getting output dif enable info, 1=>Decimation*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &mo_dif);
			}
                        break;
                case 'b':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &m_bypass); /*Bypass Mode option*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &m_bypass);
			}
                        break;
                case 'k':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &m_a_kin); /*Selecting  MAX X/ alpha for KLUT input*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &m_a_kin);
			}
                        break;
                case 'a':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &m_a_kop); /*Selecting  normal alpha/1/2k for alpha output*/
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &m_a_kop);
			}
                        break;
                default: badarg++; break;
        }
        argc--; argv++;
    }

    if (badarg || argc ) {
	msg_printf(SUM, "\
	Usage: evo_csc -n [1|2] -i [0|1] -o [0|1]  -d [0|1] -f [0|1]\n\
	        -b [0|1] -k [0|1] -a [0|1]  \n\
	-n --> CSC Asic 1[2] \n\
	-i --> Input Format 0-other 1-YUV422/4 \n\
	-o --> Output Format 0-other 1-YUV422/4\n\
	-d --> Input DIF  0-Disable 1-Enable\n\
	-f --> Output DIF 0-Disable 1-Enable\n\
	-b --> Bypass Mode 0-Disable 1-Enable\n\
	-k --> KLUT Input 0-MAX X (Constant Hue) 1-Alpha\n\
	-a --> Alpha Output  0-Normal Alpha 1-Use 1/2k \n\
	");
	return(-1);
    } else {
    	    /*get memory for all buffers*/
    	    evobase->lutbuf = (u_int16_t *)get_chunk((EVO_CSC_MAX_LOAD * sizeof(u_int16_t)) + (EVO_CSC_MAX_COEF*sizeof(u_int16_t)));
    	    evobase->coefbuf = evobase->lutbuf + EVO_CSC_MAX_LOAD;
	    m_errors = evo_cscTest(m_csc_num, mi_fmt, mo_fmt, mi_dif, mo_dif, m_bypass, m_a_kin, m_a_kop);
    }
    return(m_errors);

}


__uint32_t
evo_cscTest(__uint32_t csc_num, __uint32_t ifmt, __uint32_t ofmt, __uint32_t idif, 
		 __uint32_t odif,  __uint32_t bypass, __uint32_t a_kin, __uint32_t a_kop)

{
    __uint32_t	errors = 0;
    __uint32_t	badarg = 0;
    __uint32_t	itrc = 0; /*default itrc value*/
    __uint32_t	op_black_lvl = 1; /*default black level (RGB)*/
    __uint32_t	op_full_scl = 0; /*default is CCIR*/
    __uint32_t	op_frc_black = 0; /*default is normal*/

    evo_csc_op_ctrl_t csc_op_ctrl; 

    __uint32_t	i, dataBufSize, tmp;
    unsigned char       xp;
    uchar_t value;
    
    /*setting appropriate CSC base address*/
    if (csc_num == 1) {
	EVO_SET_CSC_1_BASE;
    } else if (csc_num == 2) {
    	EVO_SET_CSC_2_BASE;
    }

    if (bypass == 0){
	/*load CSC LUTs first before setting up codes for normal operation*/
	if ((ifmt == 1) && (ofmt == 0)){
		csc_inlut = 1; /*XXX  Check these values*/
		csc_outlut = 1;
		csc_alphalut = 4;
		_evo_csc_yuv2rgb();	
	} else if ((ifmt == 0) && (ofmt == 1)){
		csc_inlut = 1; /*XXX Check these values*/
		csc_outlut = 1;
		csc_alphalut = 1;
		_evo_csc_rgb2yuv();
	}
	/*setting CSC up for normal operation with appropriate parameters*/
   	value = 8; /*disabling bypass mode, setting up CSC for normal operation*/
    	CSC_W1(evobase, CSC_MODE, value);

    	value = CSC_IN_CTRL;
    	CSC_W1(evobase, CSC_ADDR, value);
    	value = 0xff & ((ifmt) | (idif << 1) | (itrc << 2));  
    	CSC_W1(evobase, CSC_DATA, value);

    	value = CSC_OUT_CTRL;
    	CSC_W1(evobase, CSC_ADDR, value);
    	if (ofmt == 1) op_black_lvl = 0;

    	csc_op_ctrl.sel_ofmt       = ofmt;
    	csc_op_ctrl.sel_odif       = odif;
    	csc_op_ctrl.sel_op_black_lvl = op_black_lvl;
    	csc_op_ctrl.sel_op_full_scl   = op_full_scl;
    	csc_op_ctrl.sel_a_kin   = a_kin;
    	csc_op_ctrl.sel_a_kop   = a_kop;
    	csc_op_ctrl.op_frc_black = op_frc_black;
    	csc_op_ctrl.sel_op_full_scl   = op_full_scl;
    	csc_op_ctrl.reserved   = 0;
    	bcopy((char *)&csc_op_ctrl , (char *)&value, sizeof(evo_csc_op_ctrl_t));    
    	CSC_W1(evobase, CSC_DATA, value);
    
    	msg_printf(SUM, "CSC:\n Input: -n %d -i %d; -d %d; -k %d;\n Output: -o %d; -f %d; -b %d; -a %d;\n", 
			csc_num, ifmt, idif, a_kin, ofmt, odif, bypass, a_kop);
    }
    else if (bypass == 1) {
   	value = 10; /*enabling  bypass mode for the CSC, input links are routed to output links*/
    	CSC_W1(evobase, CSC_MODE, value);
    }

    return(errors);
}



/*
 * Program LUTs and coefficients to convert from YUV to RGB 
 */
void _evo_csc_yuv2rgb(void)
{
    u_int16_t *lutbuf, *coefbuf;

    lutbuf = evobase->lutbuf;
    coefbuf = evobase->coefbuf;

    /* Load input LUTS */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_YUV2RGB, CSC_YG_IN);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_YG_IN, lutbuf);

    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_YUV2RGB, CSC_UB_IN);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_UB_IN, lutbuf);

    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_YUV2RGB, CSC_VR_IN);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_VR_IN, lutbuf);

    /* Load output LUTS */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_YUV2RGB, CSC_YG_OUT);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_YG_OUT, lutbuf);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_UB_OUT, lutbuf);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_VR_OUT, lutbuf);

    /* Load coefficients */
    _evo_csc_LoadBuf(EVO_CSC_MAX_COEF, coefbuf, CSC_COEF_YUV2RGB, 0);

    _evo_csc_LoadLut(CSC_COEF_LOAD, CSC_YG_IN, coefbuf);

    /* Load X2K LUT */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_X2KLUT_YUV2RGB, 0);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_XK_IN, lutbuf);

    /* Load alpha LUT */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_YUV2RGB, CSC_ALPHA_OUT);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_ALPHA_OUT, lutbuf);
}

/*
 * Program LUTs and coefficients to convert from RGB to YUV
 */
void _evo_csc_rgb2yuv(void)
{
    u_int16_t *lutbuf, *coefbuf;

    lutbuf = evobase->lutbuf;
    coefbuf = evobase->coefbuf;

    /* Load input LUTS */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_RGB2YUV, CSC_YG_IN);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_YG_IN, lutbuf);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_UB_IN, lutbuf);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_VR_IN, lutbuf);

    /* Load Y output LUT */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_RGB2YUV, CSC_YG_OUT);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_YG_OUT, lutbuf);

    /* Load U and V output LUTS */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_RGB2YUV, CSC_UB_OUT);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_UB_OUT, lutbuf);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_VR_OUT, lutbuf);

    /* Load alpha LUT */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_LUT_RGB2YUV, CSC_ALPHA_OUT);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_ALPHA_OUT, lutbuf);

    /* Load coefficients */
    _evo_csc_LoadBuf(EVO_CSC_MAX_COEF, coefbuf, CSC_COEF_RGB2YUV, 0);
    _evo_csc_LoadLut(CSC_COEF_LOAD, CSC_YG_IN, coefbuf);

    /* Load X2K LUT */
    _evo_csc_LoadBuf(EVO_CSC_MAX_LOAD, lutbuf, CSC_X2KLUT_RGB2YUV, 0);
    _evo_csc_LoadLut(CSC_LUT_LOAD, CSC_XK_IN, lutbuf);
}


