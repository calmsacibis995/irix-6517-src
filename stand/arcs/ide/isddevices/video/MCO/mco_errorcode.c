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
 *  MCO - ERRORCODES.
 *
 *  $Revision: 1.1 $
 */

#include "sys/mgrashw.h"
#include "mco_diag.h"

_Test_Info MCOBackEnd[] = {
    {/* DIAG mco_XXXXXXX */         "Undefined MCO VC2", "VCT000"},      /* 0 */
    {/* DIAG mco_vc2addrsbus */ "MCO VC2 Address Bus", "VCT001"},    /* 1 */
    {/* DIAG mco_vc2databus */  "MCO VC2 Data Bus",	"VCT002"},       /* 2 */
    {/* DIAG mco_vc2addrsuniq */ "MCO VC2 Address Uniqness", "VCT003"}, /* 3 */
    {/* DIAG mco_vc2patrn */    "MCO VC2 Pattern", "VCT004"},        /* 4 */
    {/* DIAG mco_vc2internalreg */ "MCO VC2 Register", "VCT005"},    /* 5 */
    {/* DIAG mco_XXXXXXX */	    "Undefined MCO VC2 Sram", "VCT006"}, /* 6 */

    {/* DIAG mco_7162clrpalettewalkbit */ "MCO 7162 DAC Color Palette Walking Bit Data Bus",  "7162DAC001"},  /* 7 */
    {/* DIAG mco_7162clrpaletteaddrUniq */ "MCO 7162 DAC Color Palette Addr Uniq", "7162DAC002"},	/* 8 */
    {/* DIAG mco_7162clrpalettepatrn */ "MCO 7162 DAC Color Palette Patrn",  "7162DAC003"},	/* 9 */
    {/* DIAG mco_7162ctrlreg */ "MCO 7162 DAC Control Reg", "7162DAC004"},	/* 10 */
    {/* DIAG mco_7162modereg */ "MCO 7162 DAC Mode Reg", "7162DAC005"},	/* 11 */
    {/* DIAG mco_7162addrreg */ "MCO 7162 DAC Addr Reg", "7162DAC006"},	/* 12 */

    {/* DIAG mco_473clrpalettewalkbit */ "MCO 473 DAC Color Palette Walking Bit Data Bus",    "473DAC001"},	/* 13 */
    {/* DIAG mco_473clrpaletteaddrUniq */ "MCO 473 DAC Color Palette Addr Uniq",         "473DAC002"},	/* 14 */
    {/* DIAG mco_473clrpalettepatrn */ "MCO 473 DAC Color Palette Patrn",             "473DAC003"},	/* 15 */
    {/* DIAG mco_473overlaywalkbit */ "MCO 473 DAC Overlay Palette Walk Bit",        "473DAC004"},	/* 16 */
    {/* DIAG mco_473overlayaddrUniq */ "MCO 473 DAC Overlay Palette Addr Uniq",       "473DAC005"},	/* 17 */
    {/* DIAG mco_473overlaypatrn */ "MCO 473 DAC Overlay Palette Patrn",           "473DAC006"},	/* 18 */
    {/* DIAG mco_473RAMaddrreg */ "MCO 473 DAC Addr Reg",                        "473DAC007"},	/* 19 */
    {/* DIAG mco_indextest */ "MCO Index Reg", "FPGA002"},	/* 20 */
    {/* DIAG mco_chkcmpbittest */ "MCO ISOS Check Compare Bit", "SWIZ001"},	/* 21 */
    {/* DIAG mco_oschksumtest */ "MCO ISOS Checksum ", "SWIZ002"},	/* 22 */

    {/* DIAG mco_daccrctest */ "MCO 7162A DAC CRC (Pixel Path)",     		"ICOPP001"},	/* 23 */
    {/* DIAG mco_daccrctest */ "MCO 7162B DAC CRC (Pixel Path)",     		"ICOPP002"},	/* 24 */
    {/* DIAG mco_XXXXXXX */ "Undefined MCO Test",     		"MCOX999"},	/* 25 */

};

