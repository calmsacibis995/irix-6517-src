#ifndef NETGRAPH_XDR_H
#define NETGRAPH_XDR_H
/*
 * Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *  netgraph XDR include file
 *
 *	$Revision: 1.1 $
 */

#include <rpc/types.h>
#include <rpc/xdr.h>


bool_t
open_plain_outfile(char*, FILE**);

void
close_plain_file(FILE*);

bool_t
open_xdr_outfile(char*, FILE**, XDR*, int*, char**);

int
open_xdr_infile(char*, FILE**, XDR*, int*, int*, char**);

bool_t
do_xdr_time(XDR*, struct timeval*);

bool_t
do_xdr_data(XDR*, float*);

bool_t
do_xdr_stripInfo(XDR*, int, char**, int*, int*, int*, int*, int*, 
    int*, int*, float*, float*);

void
close_xdr_file(FILE*, XDR*);

void
print_xdr_pos(XDR*, char*);

unsigned int
get_xdr_pos(XDR*);

bool_t
test_xdr_read(char*, FILE**, XDR*);


#endif
