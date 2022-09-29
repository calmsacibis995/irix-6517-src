#if defined(IP30)
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef _FLASH_PROM_CMD_H
#define _FLASH_PROM_CMD_H


int _fl_prom(uint16_t *ld_buf, int reset, int beg_seg, int fl_sz);
int _fl_check_bin_image(char *buf, int len);

#define NO_FORMAT		0
#define IP30PROM_BIN_FORMAT	1
#define FPROM_BIN_FORMAT	2

#endif /* _FLASH_PROM_CMD_H */
#endif /* defined(IP30) */
