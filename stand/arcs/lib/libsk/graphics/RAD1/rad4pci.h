#ifndef __RAD4PCI_H__
#define __RAD4PCI_H__
/*
*
* Copyright 1998 Psitech, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Psitech, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Psitech, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
#define	RAD4_SETLUT 101
#define	RAD4_SETLUTSS 102
#define RAD4_SETDEPTH8_INDEX 103
#define RAD4_SETDEPTH8_GREY 104
#define RAD4_SETDEPTH8_TRUE 105
#define RAD4_SETDEPTH8_DIRECT 106
#define RAD4_SETDEPTH12_TRUE 107
#define RAD4_SETDEPTH12_DIRECT 108
#define RAD4_SETDEPTH16_TRUE 109
#define RAD4_SETDEPTH16_DIRECT 110
#define RAD4_SETDEPTH24_TRUE 111
#define RAD4_SETDEPTH24_DIRECT 112
#define RAD4_GETTIMING 113
#define RAD4_SETTIMING 114
#define RAD4_SETCURSOR_POS 115
#define RAD4_SETCURSOR_CLUT 116
#define RAD4_SETCURSOR_MAP 117
#define RAD4_RECT_WRITE 118
#define RAD4_RECT_FILL 119
#define RAD4_SET_MASK 120
#define RAD4_RECT_READ 121
#define RAD4_SETCURSOR_ON 122
#define RAD4_SETCURSOR_OFF 123
#define RAD4_SCREEN_ON 124
#define RAD4_SCREEN_OFF 125
#define RAD4_SET_PCI_READ_COMMAND 126
#define RAD4_SET_GAMMA_LUT 127
#define RAD4_DAEMON 128
#define RAD4_WRITE_EEPROM_SECTOR 129
#define RAD4_READ_EEPROM_SECTOR 130
#define RAD4_WRITE_SEPROM 131
#define RAD4_READ_SEPROM 132
#define RAD4_DEBUG_LEVEL 133
#define RAD4_SET_OVERLAY_START 134
#define RAD4_GETTIMING_EX 135
#define RAD4_RECT_COPY 140
#define RAD4_RECT_WRITE_MODE 141

#define RAD4_1280_1K_72		0
#define RAD4_1280_1K_60		1
#define RAD4_1600_1200_60	2
#define RAD4_1600_1200_72	3
#define RAD4_2K_1K_60			4
#define RAD4_1200_1600_76	5
#define RAD4_1600_1200_79	6

#define RAD4_D_OFF	0
#define RAD4_D_ON	1
#define RAD4_D_FORCE	2
typedef struct
{	int start;
	int count;
	int colors[1024];
} rad4_lut_t;

typedef struct
{	int id;	/* the rad4 has up to 256 window types */
	int mask;	/* bits set to 1s are to be ignored */
	int start_addr;	/* where in the lut this window starts */
} rad4_wat_i;

typedef struct
{	int x_size;
	int y_size;
	int	v_rate;
}	rad4_video_info;

typedef struct
{	int x_size;
	int y_size;
	int	v_rate;
	int	index;
}	rad4_video_info_ex;

typedef struct
{	int x;
	int y;
} rad4_cursor_pos;

typedef struct
{	int f_red;
	int f_green;
	int f_blue;
	int b_red;
	int b_green;
	int b_blue;
} rad4_cursor_clut;

typedef struct
{	int xhot;
	int yhot;
	int mask[64*64/(8*sizeof(int))];
	int bits[64*64/(8*sizeof(int))];
} rad4_cursor_map;

typedef enum {CI8,RGB15,RGB24,RGB32} RadColorMode;
typedef struct
{ int mask;
	RadColorMode mode;
  int startx;
  int starty;
  int sizex;
  int sizey;
  unsigned int * buf;
} rad4_rect_write;
 
typedef struct
{ int mask;
  int startx;
  int starty;
  int sizex;
  int sizey;
  int value;
} rad4_rect_fill;
 
typedef struct
{ int mask;
	int mode;
  int startx;
  int starty;
  int sizex;
  int sizey;
  int offsetx;
  int offsety;
} rad4_rect_copy;
 
typedef struct
{ int mode;
  int count;
  int colors[256];
} rad4_write_mode_t;

#define RAD4_EEPROM_SIZE 65536
#define RAD4_SECTOR_SIZE 128
typedef struct
{	unsigned int offset;	/* must be a multiple of RAD4_SECTOR_SIZE */
	unsigned char *data;
} rad4_eeprom_sector;

#define RAD4_SEPROM_SIZE 128
typedef struct
{	unsigned int offset;	/* must be even */
	unsigned int size;	/* number of bytes to read or write, must be even */
	unsigned char *data;
} rad4_seprom;

/* the uppermost 40 bytes of the serial EPROM are available for our use */
#define RAD4_SEPROM_INFO_OFFSET 88
typedef struct
{	unsigned int default_resolution;
	unsigned int available1;
	unsigned int available2;
	unsigned int available3;
	unsigned int available4;
	unsigned int available5;
	unsigned int available6;
	unsigned int available7;
	unsigned int available8;
	unsigned int available9;
} rad4_seprom_info;

#endif /* !__RAD4PCI_H__ */

