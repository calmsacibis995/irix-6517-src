/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * A packet viewer that does nothing.
 */
#include "heap.h"
#include "packetview.h"

static int	npv_anyop(PacketView *);
static void	npv_destroy(PacketView *);

static struct pvops null_pvops = {
	(int (*)(PacketView *, struct snoopheader *, struct tm *)) npv_anyop,
	npv_anyop,
	(int (*)(PacketView *, struct protocol*, char*, int, char*)) npv_anyop,
	npv_anyop,
	(int(*)(PacketView*,void*,int,struct protofield*,char*,int)) npv_anyop,
	npv_anyop,
	(int (*)(PacketView *, char *, int)) npv_anyop,
	npv_anyop,
	(int (*)(PacketView *, unsigned char *, int, int)) npv_anyop,
	npv_destroy,
};

PacketView *
null_packetview()
{
	PacketView *pv;

	pv = new(PacketView);
	pv_init(pv, 0, &null_pvops, "null", -1);
	pv->pv_nullflag = PV_NULLIFY;
	return pv;
}

static void
npv_destroy(PacketView *pv)
{
	delete(pv);
}

/* ARGSUSED */
static int
npv_anyop(PacketView *pv)
{
	return 1;
}
