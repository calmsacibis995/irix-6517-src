#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_stream.c,v 1.19 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/* Local variables
 */
extern stream_rec_t strmtab[];
extern int strmtab_valid;
extern int strcnt;

/*
 * stream_banner()
 */
void
stream_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64) {
			fprintf (ofp, "          STREAM               WRQ  "
				"           VNODE  WOFF              FLAGS\n");
		}
		else {
			fprintf (ofp, "  STREAM                       WRQ  "
				"           VNODE  WOFF              FLAGS\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf (ofp, "==============================================="
			"==============================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf (ofp, "-----------------------------------------------"
			"------------------------------\n");
	}
}

/*
 * print_stream() -- Print out stdata struct data.
 */
void
print_stream(kaddr_t stdata, k_ptr_t stp, stream_rec_t *srec, 
int flags, FILE *ofp)
{
	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(ofp, "print_stream: stdata=0x%llx, stp=0x%x, srec=0x%x\n",
			stdata, stp, srec);
		fprintf(ofp, "              flags=0x%x, ofp=0x%x\n", flags, ofp);
	}

	indent_it(flags, ofp)
	FPRINTF_KADDR(ofp, "  ", stdata);
	fprintf(ofp, "%16llx  ", kl_kaddr(stp, "stdata", "sd_wrq"));
	fprintf(ofp, "%16llx  ", kl_kaddr(stp, "stdata", "sd_vnode"));
	fprintf(ofp, "%4llu   ", KL_UINT(stp, "stdata", "sd_wroff"));
	fprintf(ofp, "%16llx\n", KL_UINT(stp, "stdata", "sd_flag"));

	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		if (srec) {

			file_rec_t *file = srec->file;

			while (file) {
				fprintf(ofp, "FILE = 0x%llx, ", file->fp);
				fprintf(ofp, "VNODE = 0x%llx\n", file->vp);
				fprintf(ofp, "SNODE = 0x%llx, ", file->sp);
				fprintf(ofp, "COMMVP = 0x%llx\n", file->cvp);
				fprintf(ofp, "\n");
				file = file->next;
			}
		}
	}

	if (flags & C_NEXT) {
		fprintf(ofp, "\n");
		do_queues(kl_kaddr(stp, "stdata", "sd_wrq"), flags, ofp);
	}
}

/*
 * list_streams() -- Dump out a listing of all active streams.
 */
int
list_streams(int flags, FILE *ofp)
{
	int i, first_time = 1;
	k_ptr_t strp;

	if (build_stream_tab()) {
		if (DEBUG(DC_GLOBAL, 1)) {
		}
		return(0);
	}

	strp = kl_alloc_block(STDATA_SIZE, K_TEMP);
	for (i = 0; i < strcnt; i++) {
		if (flags & (C_FULL|C_NEXT)) {
			if (!first_time) {
				stream_banner(ofp, BANNER|SMAJOR);
			} else {
				first_time = 0;
			}
		}

		kl_get_struct(strmtab[i].str, STDATA_SIZE, strp, "stdata");
		if (!KL_ERROR) {
			print_stream(strmtab[i].str, strp, &strmtab[i], flags, ofp);
		}
	}
	kl_free_block(strp);
	return(strcnt);
}

/*
 * stream_cmd() -- Dump information about a streams.
 */
int
stream_cmd(command_t cmd)
{
	int i, j, k, stdata_cnt = 0;
	kaddr_t stream = 0;
	stream_rec_t *srec = (stream_rec_t*)NULL;
	k_ptr_t strp;

	strp = kl_alloc_block(STDATA_SIZE, K_TEMP);

	if (ACTIVE || !strmtab_valid) {
		fprintf(cmd.ofp, "Building stream table...\n");
		build_stream_tab();
	}
	stream_banner(cmd.ofp, BANNER|SMAJOR);

	if (cmd.nargs) {
		for (i = 0; i < cmd.nargs; i++) {
			GET_VALUE(cmd.args[i], &stream);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_STDATA;
				kl_print_error();
				continue;
			}
			kl_get_struct(stream, STDATA_SIZE, strp, "stdata");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_STDATA;
				kl_print_error();
			}
			else {
#ifdef XXX
				if (!(srec = find_stream_rec(stream)) && !(cmd.flags & C_ALL)) {
					KL_SET_ERROR_NVAL(KLE_BAD_STREAM, stream, 2);
					kl_print_error();
					continue;
				}
#endif
				print_stream(stream, strp, srec, cmd.flags, cmd.ofp);
				stdata_cnt++;
				if ((cmd.flags & (C_FULL|C_NEXT)) && (i < (cmd.nargs - 1))) {
					stream_banner(cmd.ofp, BANNER|SMAJOR);
				}
			}
		}
	}
	else {
		stdata_cnt = list_streams(cmd.flags, cmd.ofp);
	}
	stream_banner(cmd.ofp, SMAJOR);
	fprintf(cmd.ofp, "%d stdata struct%s found\n", 
		stdata_cnt, (stdata_cnt != 1) ? "s" : "");
	kl_free_block(strp);
	return(0);
}

#define _STREAM_USAGE "[-a] [-f] [-n] [-w outfile] [stream_list]"

/*
 * stream_usage() -- Print the usage string for the 'stream' command.
 */
void
stream_usage(command_t cmd)
{
	CMD_USAGE(cmd, _STREAM_USAGE);
}

/*
 * stream_help() -- Print the help information for the 'stream' command.
 */
void
stream_help(command_t cmd)
{
	CMD_HELP(cmd, _STREAM_USAGE,
		"Display the stdata structure for each virtual address included "
		"in stream_list.  If no entries are specified, display all "
		"Streams that are currently allocated.  If the next option (-n) "
		"is specified, a linked list of queues that are associated with "
		"the stream will also be displayed.");
}

/*
 * stream_parse() -- Parse the command line arguments for 'stream'.
 */
int
stream_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_NEXT|C_ALL);
}
