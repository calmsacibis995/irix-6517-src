/* $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/fforward/graphics/EXPRESS/RCS/gr2.h,v 1.4 1995/08/23 20:29:31 rchiang Exp $ */
/**************************************************************************
 *								 	  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *								 	  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *								 	  *
 **************************************************************************/
extern int gr2_boardvers(void);
extern int Gr2ProbeGEs(struct gr2_hw *base);
extern int pix_dma(struct gr2_hw *,long, long, long, long, long, int *);
extern int pix_dma_zb(int, int, int, int, int, int *);
extern int Gr2Probe(struct gr2_hw *hw);
extern int Gr2ProbeAll(char **);
void print_xmap_loc(unsigned int);
extern int addrinc(int,int,int,int,int);
extern int do_dma(void);
void mk_dmada(char *, long, long, __psunsigned_t);
extern int is_indyelan(void);

extern int buf1[];
extern int buf2[];
