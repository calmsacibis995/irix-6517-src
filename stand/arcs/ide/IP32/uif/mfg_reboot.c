
/*
 * mfg_reboot.c - enable mfg to set environment variables in the flash and reboot
 */


#include <sys/cpu.h>
#include <arcs/errno.h>
#include <sys/types.h>
#include <stringlist.h>
#include <libsc.h>
#include <libsk.h>

#include <flash.h>
#include <sys/IP32flash.h>



/*
 * External definitions
 */
extern struct string_list environ_str;
extern char** environ;
extern FlashSegment* flashSeg;


void
mfg_reboot()
{

	int sts;
	char* string, envbuf[32];

	init_env();
	_setenv("SystemPartition","",0);
	_setenv("OSLoader","",0);
	_init_bootenv();

	clear_mace_intmask();

	/* _setenv("mfg_cmd","bootp()/mh.shell.ide",2); */

	Reboot();

}

