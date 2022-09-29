/*
 * pgstubs.c
 * 
 * stub routines for type conversion
 */

#include <libsc.h>
#include <libsk.h>
#include <libkl.h>

void *
init_sncfg_scsi_drive(void *a, int b)
{
	return(init_klcfg_scsi_drive(a, b)) ;
	return 1 ;
}

void *
init_sncfg_ioc3()
{
	init_klcfg_ioc3() ;
	return 1 ;
}

void *
init_sncfg_enet()
{
	init_klcfg_enet() ;
	return 1 ;
}

void *
init_sncfg_tty()
{
	init_klcfg_tty() ;
	return 1 ;
}

void *
init_sncfg_pckm()
{
	init_klcfg_pckm() ;
	return 1 ;
}

void *
init_sncfg_gdev()
{
	init_klcfg_gdev() ;
	return 1 ;
}


