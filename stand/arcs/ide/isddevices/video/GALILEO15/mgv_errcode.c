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
 *  File name : video_errcodes.c                                          *
 *  Revision : 1.0
 **************************************************************************/

#include "mgv_diag.h"

/*
 * This structure matches with the constants defined in mgv_diag.h
 * for error codes
 */

mgv_test_info mgv_err[] = {
{/* DIAGS mgv_init */ "Impact Video Initialization Test",		"XXXXX"},  
{/* DIAGS mgv_gpitrig */ "GPI Trigger Test",				"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  

	/* AB1 Error Strings and Codes */
{/* DIAGS mgv_abaddrbus */ "AB1 Dram Address Bus Red Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abaddrbus */ "AB1 Dram Address Bus Green Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abaddrbus */ "AB1 Dram Address Bus Blue Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abaddrbus */ "AB1 Dram Address Bus Alpha Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abdatabus */ "AB1 Dram Data Bus Red Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abdatabus */ "AB1 Dram Data Bus Green Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abdatabus */ "AB1 Dram Data Bus Blue Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abdatabus */ "AB1 Dram Data Bus Alpha Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abpatrn */ "AB1 Dram Pattern Red Channel Test",		"XXXXX"},  
{/* DIAGS mgv_abpatrn */ "AB1 Dram Pattern Green Channel Test",		"XXXXX"},  
{/* DIAGS mgv_abpatrn */ "AB1 Dram Pattern Blue Channel Test",		"XXXXX"},  
{/* DIAGS mgv_abpatrn */ "AB1 Dram Pattern Alpha Channel Test",		"XXXXX"},  
{/* DIAGS mgv_abpatrnslow */ "AB1 Dram Pattern Slow Red Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abpatrnslow */ "AB1 Dram Pattern Slow Green Channel Test", "XXXXX"},  
{/* DIAGS mgv_abpatrnslow */ "AB1 Dram Pattern Slow Blue Channel Test",	"XXXXX"},  
{/* DIAGS mgv_abpatrnslow */ "AB1 Dram Pattern Slow Alpha Channel Test", "XXXXX"},  
{/* DIAGS mgv_abdcb */ "AB1 DCB Test",					"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  

	/* CC1 Error Strings and Codes */
{/* DIAGS mgv_ccaddrbus */ "CC1 Dram Address Bus Test",			"XXXXX"},  
{/* DIAGS mgv_ccdatabus */ "CC1 Dram Data Bus Test",			"XXXXX"},  
{/* DIAGS mgv_ccpatrn */ "CC1 Dram Pattern Test",			"XXXXX"},  
{/* DIAGS mgv_ccdcb */ "CC1 DCB Test",					"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},

	/* VGI1 Error Strings and Codes */
{/* DIAGS mgv_vgiregpatrn */ "VGI1 Register Pattern Test",			"XXXXX"},  
{/* DIAGS mgv_vgiregwalk */ "VGI1 Register Walking Bit Test",		"XXXXX"},  
{/* DIAGS mgv_vgiint */ "VGI1 Interrupt Test",				"XXXXX"},  
{/* DIAGS mgv_vgidma */ "VGI1 DMA Test",				"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  

        /* CSC Error Strings and Codes */
{/* DIAGS mgv_csc_bus */ "CSC  Address Bus Test",                       "XXXXX"},  
{/* DIAGS mgv_csc_bus */ "CSC  Data Bus Test",                          "XXXXX"},  
{/* DIAGS mgv_csc_inlut */ "CSC  Input LUTs Test",                      "XXXXX"},  
{/* DIAGS mgv_csc_outlut */ "CSC  Output LUTs Test",                    "XXXXX"},  
{/* DIAGS mgv_csc_coef */ "CSC  Coefficient Test",                       "XXXXX"},  
{/* DIAGS mgv_csc_x2k */ "CSC  X2K Lut Test",                           "XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  

        /* TMI Error Strings and Codes */
{/* DIAGS mgv_tmi_reg */ "TMI Register Pattern Test",			"XXXXX"},  
{/* DIAGS mgv_tmi_databus */ "TMI Register Data Bus Test",		"XXXXX"},  
{/* DIAGS mgv_tmi_bypass */ "TMI Bye Pass Test",			"XXXXX"},  
{/* DIAGS mgv_tmi_sram_addr */ "TMI SRAM Address Bus Test",		"XXXXX"},  
{/* DIAGS mgv_tmi_sram_patrn */ "TMI SRAM Pattern Test",		"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  
{/* DIAGS null        */ "",						"XXXXX"},  

        /* SCALER Error Strings and Codes */
{/* DIAGS mgv_scl_bus */ "SCL  Address Bus Test",                       "XXXXX"},  
{/* DIAGS mgv_scl_bus */ "SCL  Data Bus Test",                          "XXXXX"},  

};
