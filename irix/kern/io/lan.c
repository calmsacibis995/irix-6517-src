/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA. */
#include	"sys/cmn_err.h"

lan_intr()
{
	cmn_err( CE_NOTE, "local i/o interupt, lan chip - not supported" );
	return 0;
}
fram_intr()
{
	cmn_err( CE_NOTE, "local i/o interupt, frame buffer - not supported" );
	return 0;
}
