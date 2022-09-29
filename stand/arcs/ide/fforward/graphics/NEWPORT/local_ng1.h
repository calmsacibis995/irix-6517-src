
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 *************************************************************************/

/* ng1visuals.c */
int ng1_setvisual(uint);

/* minigl3.c */
int ng1_planes(int b);
int ng1_drawdepth (int b);
int ng1_color(int c);
int ng1_block(int x0, int y0, int x1, int y1, int dm0_xtra, int dm1_xtra);
int ng1_rgbcolor(int r, int g, int b);
int ng1_line(int x0, int y0, int x1, int y1, int dm0_xtra);
int ng1_polygon(int x0, int y0, int x1, int y1, float color[][3]);
void ng1_writemask (int mask);

/* vram3.c */
int check_bits(unsigned int expect, unsigned int got, int bank_no); 
int check_chip(int cbit, int code);
int check_bank(unsigned int x, unsigned int y);
int print_bank(int bank_no);
int print_chip(int chip_no, int bank_no);
int ng1displayfb(void);

/* rex3.c */
int rex3init(void);

/* bt445.c */
int ng1test_dac_palette( int chip );
int ng1test_palette( int chip, int rev);
int rgberror ( int chip, char *c, int i, int expect, int got );
int set_palette_rgb( int chip, int r, int g, int b);
int set_palette_read_addr( int chip,  int i);
int set_palette_addr( int chip,  int i);
int get_palette_rgb ( int chip, char *r, char *g, char *b);

/* vc2.c */
void vc2_off(void);

/* sram3.c */
int ng1_test_sram(int,char **);

/* libsk */
void Ng1DacInit(struct rex3chip *rex3, struct ng1_info *info);
void Ng1VC2Init(struct rex3chip *rex3, struct ng1_info *info);
void Ng1XmapInit(struct rex3chip *rex3, struct ng1_info *info);
void Ng1CmapInit(struct rex3chip *rex3, struct ng1_info *info);
void rex3Clear(struct rex3chip *rex3, struct ng1_info *info);
void Ng1CursorInit(struct rex3chip *rex3,int,int,int,int,int,int);

