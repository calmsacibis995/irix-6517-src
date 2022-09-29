#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * icrashdef_cmd() -- Dump out a binary copy of the icrashdef_s struct
 */
int
icrashdef_cmd(command_t cmd)
{
	int nbytes;
	kaddr_t icrashdef;
	k_ptr_t ip;
	int ifp;

	if ((ifp = open(cmd.args[0], O_CREAT|O_RDWR|O_EXCL, 0666)) == -1) {
		fprintf(KL_ERRORFP, "Unable to open output file \'%s\' (errno=%d)\n", 
			cmd.args[0], errno);
		return(1);
	}
	nbytes = kl_struct_len("icrashdef_s");
	icrashdef = kl_sym_addr("icrashdef");
	ip = kl_alloc_block(nbytes, K_TEMP);
	kl_get_block(icrashdef, nbytes, ip, "icrashdef");
	if (KL_ERROR) {
		fprintf(KL_ERRORFP, "Unable to read icrashdef_s struct from "
			"the core file\n");
		kl_free_block(ip);
		return(1);
	}
	if (write(ifp, ip, nbytes) != nbytes) {
		fprintf(KL_ERRORFP, "Unable to write icrashdef_s struct to file"
			"\'%s\' (errno=%d)\n", cmd.args[0], errno);
		kl_free_block(ip);
		return(1);
	}
	kl_free_block(ip);
	return(0);
}

#define _ICRASHDEF_USAGE "outfile"

/*
 * icrashdef_usage() -- Print the usage string for the 'icrashdef' command.
 */
void
icrashdef_usage(command_t cmd)
{
	CMD_USAGE(cmd, _ICRASHDEF_USAGE);
}

/*
 * icrashdef_help() -- Print the help information for the 'icrashdef' command.
 */
void
icrashdef_help(command_t cmd)
{
	CMD_HELP(cmd, _ICRASHDEF_USAGE,
		"Read the icrashdef structure in from the corefile and dump it, in "
		"binary form, to a file on disk. As a percausion, an icrashdef "
		"file will only be created if a file with the same pathname does "
		"not already exist.");
}

/*
 * icrashdef_parse() -- Parse the command line arguments for 'icrashdef'.
 */
int
icrashdef_parse(command_t cmd)
{
	return (C_TRUE);
}
