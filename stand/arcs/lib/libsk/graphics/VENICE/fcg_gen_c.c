/**************************************************************************
 *									  *
 *		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 *  fcg_gen_c.c $Revision: 1.1 $
 *
 *  This program reads a fcg_cpcmds.B file and generates a fcg_cmdmapper.c file.
 */

#include <stdio.h>
#include <errno.h>
#include <strings.h>


#define FCG_SIZE 64

unsigned int ucode[FCG_SIZE];

void read_fcg_B(char *ifname);
void write_cfile(char *ofname);


main(int argc, char **argv)
{
    if (argc != 3)
    {
	fprintf(stderr, "Usage: %s infile.B outfile.c", argv[0]);
	exit(0);
    }

    read_fcg_B(argv[1]);

    write_cfile(argv[2]);
}


void read_fcg_B(char *ifname)
{
    FILE *ifp;
    int i, j;

    if ((ifp = fopen(ifname, "r")) == NULL)
    {
	fprintf(stderr, "Error: unable to open %s", ifname);
	exit(1);
    }

    for (i = 0; i < FCG_SIZE; i++)
    {
	fscanf(ifp, "%d\n", &j);
	ucode[i] = (j > 0)? j-1 : 0;
    }

    fclose(ifp);
}


void write_cfile(char *ofname)
{
    FILE *ofp;
    int i;

    if ((ofp = fopen(ofname, "w")) == NULL)
    {
	fprintf(stderr, "Error: unable to open/create %s", ofname);
	exit(1);
    }

    fprintf(ofp,"int fcg_cmdmap[%d] = {\n",FCG_SIZE);

    for (i=0; i<FCG_SIZE; i++) {
	if (i < FCG_SIZE - 1) {
		fprintf(ofp,"%d,\n",ucode[i]);
	}
	else {
		fprintf(ofp,"%d\n",ucode[i]);
	}
    }

    fprintf(ofp,"};\n");

    fclose(ofp);
}
