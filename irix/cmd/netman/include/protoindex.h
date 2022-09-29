#ifndef PROTOINDEX_H
#define PROTOINDEX_H
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Indexes mapping discriminant field values to higher-layer protocols.
 */
#include "cache.h"

typedef Cache ProtoIndex;

#define	pin_lruhead	c_lruhead
#define	pin_lrutail	c_lrutail
#define	pin_entries	c_entries
#define	pin_freshtime	c_timeout
#define	pin_index	c_index
#define pin_maxsize	c_maxsize

void		pin_create(char *, int, int, ProtoIndex **);
struct protocol	*pin_match(ProtoIndex *, long, long);
void		pin_enter(ProtoIndex *, long, long, struct protocol *);
void		pin_remove(ProtoIndex *, long, long);

#endif
