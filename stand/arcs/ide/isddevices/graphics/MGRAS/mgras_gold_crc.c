/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  MGRAS - INFO.
 *
 *  $Revision: 1.8 $
 */

#include <math.h>
#include <libsc.h>
#include <uif.h>
#include "sys/mgrashw.h"

#include "ide_msg.h"
#include "mgras_diag.h"


input    GoldSignature,  mgras_gold_sign[LAST_CRC_INDEX+1];

void
_mgras_init_gold_crc(void)
{
	input    *GoldSign = mgras_gold_sign;

#if 1
GoldSign->red =   274; GoldSign->green =   669; GoldSign->blue =   330; GoldSign++;       /*   0 */
GoldSign->red =   971; GoldSign->green =   518; GoldSign->blue =   855; GoldSign++;       /*   1 */
GoldSign->red =   781; GoldSign->green =   693; GoldSign->blue =   393; GoldSign++;       /*   2 */
GoldSign->red =     5; GoldSign->green =   217; GoldSign->blue =   282; GoldSign++;       /*   3 */
GoldSign->red =    17; GoldSign->green =   702; GoldSign->blue =   877; GoldSign++;       /*   4 */
GoldSign->red =   519; GoldSign->green =   989; GoldSign->blue =   105; GoldSign++;       /*   5 */
GoldSign->red =    17; GoldSign->green =   702; GoldSign->blue =   877; GoldSign++;       /*   6 */
GoldSign->red =   519; GoldSign->green =   989; GoldSign->blue =   105; GoldSign++;       /*   7 */
GoldSign->red =   920; GoldSign->green =   383; GoldSign->blue =   788; GoldSign++;       /*   8 */
GoldSign->red =   398; GoldSign->green =    28; GoldSign->blue =    16; GoldSign++;       /*   9 */
GoldSign->red =   920; GoldSign->green =   383; GoldSign->blue =   788; GoldSign++;       /*  10 */
GoldSign->red =   398; GoldSign->green =    28; GoldSign->blue =    16; GoldSign++;       /*  11 */
GoldSign->red =    17; GoldSign->green =   702; GoldSign->blue =   877; GoldSign++;       /*  12 */
GoldSign->red =   702; GoldSign->green =   784; GoldSign->blue =   445; GoldSign++;       /*  13 */
GoldSign->red =   801; GoldSign->green =   434; GoldSign->blue =   704; GoldSign++;       /*  14 */
GoldSign->red =   398; GoldSign->green =    28; GoldSign->blue =    16; GoldSign++;       /*  15 */
GoldSign->red =    17; GoldSign->green =   702; GoldSign->blue =   877; GoldSign++;       /*  16 */
GoldSign->red =   702; GoldSign->green =   784; GoldSign->blue =   445; GoldSign++;       /*  17 */
GoldSign->red =   801; GoldSign->green =   434; GoldSign->blue =   704; GoldSign++;       /*  18 */
GoldSign->red =   398; GoldSign->green =    28; GoldSign->blue =    16; GoldSign++;       /*  19 */
GoldSign->red =   691; GoldSign->green =   711; GoldSign->blue =   174; GoldSign++;       /*  20 */
GoldSign->red =   123; GoldSign->green =   282; GoldSign->blue =    69; GoldSign++;       /*  21 */
GoldSign->red =   271; GoldSign->green =   862; GoldSign->blue =   630; GoldSign++;       /*  22 */
GoldSign->red =   716; GoldSign->green =    71; GoldSign->blue =   840; GoldSign++;       /*  23 */
GoldSign->red =   653; GoldSign->green =   111; GoldSign->blue =    45; GoldSign++;       /*  24 */
GoldSign->red =   816; GoldSign->green =    75; GoldSign->blue =   818; GoldSign++;       /*  25 */
GoldSign->red =   101; GoldSign->green =   773; GoldSign->blue =   146; GoldSign++;       /*  26 */
GoldSign->red =   743; GoldSign->green =   610; GoldSign->blue =   220; GoldSign++;       /*  27 */
GoldSign->red =   163; GoldSign->green =   509; GoldSign->blue =  1014; GoldSign++;       /*  28 */
GoldSign->red =   358; GoldSign->green =   767; GoldSign->blue =   255; GoldSign++;       /*  29 */
GoldSign->red =   349; GoldSign->green =   281; GoldSign->blue =   877; GoldSign++;       /*  30 */
GoldSign->red =   436; GoldSign->green =   585; GoldSign->blue =   183; GoldSign++;       /*  31 */
GoldSign->red =   264; GoldSign->green =   296; GoldSign->blue =   312; GoldSign++;       /*  32 */
GoldSign->red =   735; GoldSign->green =   722; GoldSign->blue =   873; GoldSign++;       /*  33 */
GoldSign->red =   664; GoldSign->green =   638; GoldSign->blue =   516; GoldSign++;       /*  34 */
GoldSign->red =    50; GoldSign->green =   849; GoldSign->blue =   736; GoldSign++;       /*  35 */
GoldSign->red =    91; GoldSign->green =   678; GoldSign->blue =   963; GoldSign++;       /*  36 */
GoldSign->red =   800; GoldSign->green =   311; GoldSign->blue =   827; GoldSign++;       /*  37 */
GoldSign->red =   317; GoldSign->green =   154; GoldSign->blue =    34; GoldSign++;       /*  38 */
GoldSign->red =   359; GoldSign->green =   145; GoldSign->blue =   400; GoldSign++;       /*  39 */
GoldSign->red =   242; GoldSign->green =   408; GoldSign->blue =    98; GoldSign++;       /*  40 */
GoldSign->red =   951; GoldSign->green =   443; GoldSign->blue =   159; GoldSign++;       /*  41 */
#endif

}

#ifdef MFG_USED
void
mgras_disp_gold_crc(void)
{
#ifdef CAPTURE_CRC
	__uint32_t	index;
	input		GoldSignature,	*GoldSign = mgras_gold_sign;

	msg_printf(VRB, "Edit ""mgras_gold_crc.c"" file and replace body of \n \
		_mgras_init_gold_crc with the following statements...\n");
	for (index = 0; index <= LAST_CRC_INDEX; index++, GoldSign++) {
	    msg_printf(VRB, "GoldSign->red = %5d; GoldSign->green = %5d; GoldSign->blue = %5d;",
	    	GoldSign->red, GoldSign->green, GoldSign->blue);
	    msg_printf(VRB, " GoldSign++; \t /* %3d */\n", index);
	}
#else
	msg_printf(ERR, "IDE is not in CRC CAPTURE MODE!!!\n");
#endif
}
#endif /* MFG_USED */

__uint32_t
_check_crc_sig(__uint32_t pattern, __uint32_t index)
{
	__uint32_t error = 0;
	_Test_Info pp_crc[] = {
		{"CRC Check",""}};
	input    *GoldSign = &(mgras_gold_sign[index]);

	msg_printf(DBG, "Starting _check_crc_sig\n");

#ifndef MGRAS_DIAG_SIM
	In   = &Present ;
        Prev = &Previous ;
        ExpSign = &Signature ;
        RcvSign = &HWSignature ;

	RcvSign->red = RcvSign->green = RcvSign->blue = 0x0;

	ResetDacCrc();
	GetSignature();
#ifdef CAPTURE_CRC
	GoldSign->red = RcvSign->red;	
	GoldSign->green = RcvSign->green;	
	GoldSign->blue = RcvSign->blue;	
	msg_printf(VRB, "index = %d\n", index);
	msg_printf(VRB, "mgras_gold_sign %x; GoldSign %x\n", mgras_gold_sign, GoldSign);
	msg_printf(VRB, "RcvSign->red %x; RcvSign->green %x; RcvSign->blue %x\n",
		RcvSign->red, RcvSign->green, RcvSign->blue);
#else
	ExpSign->red = GoldSign->red;
	ExpSign->green = GoldSign->green;
	ExpSign->blue = GoldSign->blue;
	error = CrcCompare();
	CONTINUITY_CHECK((&pp_crc[0]), CmapAll, (pattern & 0xffffffff), error);
#endif
#endif

	return (error);
}

_mgras_crc_test(__uint32_t exp_red, __uint32_t exp_green, __uint32_t exp_blue)
{
   __uint32_t error = 0;

   In   = &Present ;
   Prev = &Previous ;
   ExpSign = &Signature ;
   RcvSign = &HWSignature ;

   RcvSign->red = RcvSign->green = RcvSign->blue = 0x0;

   ResetDacCrc();
   GetSignature();

   ExpSign->red = exp_red;
   ExpSign->green = exp_green;
   ExpSign->blue = exp_blue;

   error = CrcCompare();
   return (error);
}

__uint32_t
mgras_crc_test(int argc, char **argv)
{
   __uint32_t error = 0;
   __uint32_t bad_arg = 0;
   __uint32_t exp_red, exp_green, exp_blue;

   /* get the args */
   argc--; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
		case 'r':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&exp_red);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&exp_red);
			}
			break;
		case 'g':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&exp_green);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&exp_green);
			}
			break;
		case 'b':
			if (argv[0][2]=='\0') {
				atob(&argv[1][0], (int*)&exp_blue);
				argc--; argv++;
			} else {
				atob(&argv[0][2], (int*)&exp_blue);
			}
			break;
		default: bad_arg++; break;
	}
	argc--; argv++;
   }

   if ( bad_arg || argc ) {
	msg_printf(SUM,
	 "Usage: mg_crc_test -r <red> -g <green> -b <blue> \n");
	 return(0);
   }

   error = _mgras_crc_test(exp_red, exp_green, exp_blue);

   return (error);
}
