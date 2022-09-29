#ifndef SM_TYPES_H
#define SM_TYPES_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.9 $
 */

#include <net/if.h>
#include <netinet/if_ether.h>
#include <net/raw.h>
#include <sys/fddi.h>

typedef struct {u_char b[2]; LFDDI_ADDR ieee;} SMT_STATIONID;
typedef struct {char b[32]; } USER_DATA;

#endif
