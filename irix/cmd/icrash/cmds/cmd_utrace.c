#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_utrace.c,v 1.5 1999/11/17 15:26:44 purdy Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/* Number of trace records to print if a count isn't specified */
#define DFLTRECS	20

/* Magic UTRACE and continuation event numbers, EOB modes (from rtmon.h) */
#define UTRACE_EVENT 63000
#define JUMBO_EVENT  63001
#define RT_TSTAMP_EOB_STOP 0
#define RT_TSTAMP_EOB_WRAP 1

/* Internal values, used for evtype array */
#define EVTYPE_NORM		0
#define EVTYPE_JUMBO	1
#define EVTYPE_CONT		2

/* An array of these is passed to the trace merge routine.  One/cpu. */
typedef struct trlist {
	cpuid_t cpu;				/* CPU ID */
	k_ptr_t entry;				/* "Current" buffer entry */
	kaddr_t entries;			/* Buffer pointer from pda */
	uint index;					/* Current pos. in the buffer */
	uint nentries;				/* Number of entries in the buffer */
	uint prev;					/* Previous entry printed */
	char *evtype;				/* Array of event types */
	uint left;				/* Number of entries left */
} tracelist_t;

/*
 * utrace_init() -- Set up data structures for printing UTRACEs
 */
static int
utrace_init(tracelist_t *elt, k_ptr_t pdap)
{
	kaddr_t e;					/* The limit of the current scan */
	int i, len;
	uint sz;					/* Num. of extra entries in current event */
	uint left;					/* How many entries are left on this scan */
	uint entrylen;				/* Length of one trace entry */
	char *evtype;
	k_ptr_t entry;				/* The current trace entry */
	k_ptr_t entries;			/* Whole trace buffer -- too slow otherwise */
	kaddr_t p;					/* Scratch pointer */
	k_ptr_t b;					/* Scratch buffer */
	k_uint_t eobmode;			/* End-of-buffer mode */

	/* There are two kinds of multiple-element events.  In wrap mode,
	   jumbo events are stored as multiple single events, with those
	   past the first having a trace name of JUMBO_EVENT.  If we find
	   one of these, we must scan again.

	   In stop mode, event data is stored contiguously, taking the
	   number of event slots indicated by the "reserved" event field.
	   If the starting point for a scan lies within one of these
	   events, we're done.  This is why we can't just move backwards
	   by decrementing the pointer; we might land in the middle of one
	   of these events.  So we scan the entire array first, recording
	   the type of each event. */

	entrylen = kl_struct_len("tstamp_event_entry");
	elt->entries = kl_kaddr(pdap, "pda_s", "p_tstamp_entries");
	elt->nentries = (kl_kaddr(pdap, "pda_s", "p_tstamp_last") -elt->entries)
		/ entrylen + 1;
	elt->left = elt->nentries;
	elt->entry = kl_alloc_block(entrylen, K_TEMP);

	/* The index is in one of two places, depending on the eobmode */
	eobmode = KL_UINT(pdap, "pda_s", "p_tstamp_eobmode");
	if (eobmode == RT_TSTAMP_EOB_WRAP) {
		elt->index = (kl_kaddr(pdap, "pda_s", "p_tstamp_ptr") -elt->entries)
			/ entrylen;
		elt->evtype = (char *)0; /* Table is not needed in wrap mode */
		return 0;
	}

	/* Stop mode:  index is in shared state, and lookup table is needed */
	p = kl_kaddr(pdap, "pda_s", "p_tstamp_objp");
	p = kl_kaddr_to_ptr(p + kl_member_offset("tstamp_obj_t", "shared_state"));
	len = kl_struct_len("tstamp_shared_state");
	b = kl_alloc_block(len, K_TEMP);
	kl_get_struct(p, len, b, "tstamp_shared_state");
	if (KL_ERROR) {
		kl_free_block(b);
		kl_print_error();
		elt->evtype = (char *)0;
		return 1;
	}
	elt->index = KL_UINT(b, "tstamp_shared_state", "curr_index");
	kl_free_block(b);

	evtype = (char *)kl_alloc_block(elt->nentries, K_TEMP);
	entries = kl_alloc_block(entrylen * elt->nentries, K_TEMP);
	kl_get_struct(elt->entries, entrylen * elt->nentries, 
					entries, "tstamp_event_entry_t");
	if (KL_ERROR) {
		kl_free_block(entries);
		kl_print_error();
		kl_free_block((caddr_t *)evtype);
		elt->evtype = (char *)0;
		return 1;
	}

	for(sz=0, left=elt->nentries, i=elt->index;
		left;
		left--, i = (i+1) % elt->nentries) {

		/* Are we in the middle of a stop-mode vevent */
		if (sz) {
			evtype[i] = EVTYPE_CONT;
			sz--;
			continue;
		}

		/* Get trace number i */
		entry = (char *)entries + i * entrylen; 

		/* Is this a wrap-mode continuation event? */
		if (KL_UINT(entry, "tstamp_event_entry", "evt") == JUMBO_EVENT) {
			evtype[i] = EVTYPE_JUMBO;
			continue;
		}

		/* Is the ths start of a stop-mode vevent? */
		sz = KL_UINT(entry, "tstamp_event_entry", "reserved");
		if (sz > elt->nentries) {
			fprintf(KL_ERRORFP, "Corrupt UTRACE at 0x%llx\n", 
					i * entrylen + elt->entries);
			evtype[i] = EVTYPE_NORM;
			continue;
		}
		
		evtype[i] = EVTYPE_NORM;
	}

	kl_free_block(entries);
	elt->evtype = evtype;
	return 0;
}

/*
 * get_qual() -- Return the specified qualifier from an event.  A
 *				 specialized form of kl_uint().
 */

static k_uint_t
get_qual(k_ptr_t event, int num)
{
	k_uint_t v;
	k_ptr_t source;

	/* Any good way to avoid the hardcoded "8"?  "qual" is array of uint64 */
	source = (k_ptr_t)(ADDR(event, "tstamp_event_entry", "qual") + num*8);
	bcopy(source, &v, sizeof(v));
	return(v);
}

/*
 * is_string() -- Check if a qualifier is a printable string.
 */
static int
is_string(k_uint_t qual)
{
    int i;
    char    *ptr;

    for (i=0, ptr=(char*)&qual; i<sizeof(k_uint_t); ++i, ++ptr) {
        if (!isprint(*ptr))
            return 0;
    }
    return 1;
}

/*
 * print_qual() -- Print a qualifier
 */
static void
print_qual(FILE *fp, k_uint_t qual)
{
    if (is_string(qual))
        fprintf(fp, " '%8.8s'      ", (char*)&qual);
    else
        fprintf(fp, " %016llx", qual);
}

/*
 * utrace_xlate() -- Look up an event number in the translation table
 *					 Returns 1 on success, 0 on failure.
 */

static int
utrace_xlate(k_uint_t num, char *out, int slen)
{
	kaddr_t ptr;				/* pointer into the table */
	k_ptr_t entry;				/* the current entry in it */
	int len;					/* size of one table entry */
	k_uint_t v;					/* the event number in the table */
	kaddr_t s;					/* address if the symbolic name string */

	if ((ptr = kl_sym_addr("utrace_trtbl")) == 0 ||
		(len = kl_struct_len("utr_trtbl_s")) == 0) {
		return 0;				/* translation failed */
	}

	entry = kl_alloc_block(len, K_TEMP); /* buffer for one rec. at a time */

	/* This should probably be a hash table */
	do {
		kl_get_struct(ptr, len, entry, "utr_trtbl_s");
		if (KL_ERROR) {
			kl_print_error();
			kl_free_block(entry);
			return 0;
		}

		/* Did we find it? */
		s = kl_kaddr(entry, "utr_trtbl_s", "name");
		if (s && (v = kl_uint(entry, "utr_trtbl_s", "evt", 0)) == num) {
			kl_free_block(entry);
			kl_get_block(s, slen, out, "name");
			if (KL_ERROR) {
				kl_print_error();
				return 0;
			}
			out[slen-1] = '\0';	/* make sure it's null-terminated */
			return 1;
		}
		ptr += len;
	} while(s);

	kl_free_block(entry);
	return 0;
}


/*
 * utrace_print() -- Print a (possibly jumbo) event
 */
/* Todo: support decoding via librtmon? */
void
utrace_print(FILE *ofp, tracelist_t *elt, int flags)
{
	static char string[33];		/* Temp. formatting space */
	k_uint_t num;				/* The event number */
	kaddr_t index;				/* Current position in the buffer */
	k_ptr_t event;				/* The event we're working on */
	k_uint_t c;					/* A counter */
	k_uint_t c2;				/* Keep track of when we need a newline */
	int len;					/* Length of a tstamp entry */

	len = kl_struct_len("tstamp_event_entry");
	event = elt->entry;
	index = elt->index;
	num = kl_uint(event, "tstamp_event_entry", "evt", 0);

	/* Is it a special UTRACE event? */
	if (num == UTRACE_EVENT) {
		k_uint_t tmp;
		/* Name is in event->qual[2] */
		tmp = get_qual(event, 2);
		strncpy(string, (char *)&tmp, sizeof(tmp));
		string[sizeof(tmp)] = '\0';	   /* make sure it's null-terminated */
	}
	/* Is this event slot empty? */
	else if (num == 0) {
		strcpy(string, "<empty>");
	}
	/* Can we find it in the translation table? */
	else if(utrace_xlate(num, string, sizeof(string))) {
		/* Could clean up the name by stripping out EV_ and EVENT_ */
		;
	}
	/* If all else fails, just format it in decimal */
	else {
		sprintf(string, "Evt %lld", num);
	}

	fprintf(ofp, "%-23s%4d %016llx",
            string,
            elt->cpu,
            KL_UINT(event, "tstamp_event_entry", "tstamp"));
    print_qual(ofp, get_qual(event, 0));
    print_qual(ofp, get_qual(event, 1));
    if (num != UTRACE_EVENT) {
        fprintf(ofp, "\n%23s%4s %16s", "", "", "");
        print_qual(ofp, get_qual(event, 2));
        print_qual(ofp, get_qual(event, 3));
    }
	if (flags & C_FULL) {
		fprintf(ofp, "\n    ADDR %llx: EVT=%lld, CPU=%lld, RESERVED=%lld",
				elt->entries + elt->index*len,
				num,
				KL_UINT(event, "tstamp_event_entry", "cpu"),
				KL_UINT(event, "tstamp_event_entry", "reserved"));
		if (num == UTRACE_EVENT) 
			fprintf(ofp, "\n    QUAL[2]=0x%016llx, QUAL[3]=%016llx",
					get_qual(event, 2),
					get_qual(event, 3));
	}

    /* Dump remaining jumbo event data (if any) */
	c = KL_UINT(event, "tstamp_event_entry", "reserved");
	event = kl_alloc_block(len, K_TEMP);
	c2 = 0;
    for (; c; c--) {
		int c3;
		k_uint_t v;
        index = (index + 1) % elt->nentries;
		kl_get_struct(index * len + elt->entries, len,
					  event, "tstamp_event_entry_t");
		if (KL_ERROR) {
			fprintf(ofp, "\n");
			kl_print_error();
			kl_free_block(event);
			return;
		}
        for (c3=0; c3<len; c3 += sizeof(v), c2++) {
            if (c2 % 3 == 0)
                fprintf(ofp, "\n%27s", "");
			bcopy((k_uint_t)event + c3, &v, sizeof(v));
            fprintf(ofp, " %016llx", v);
        }
    }

    /* Dump any continuation event data, five doublewords/event.
       But don't go beyond the previous event printed. */
	index = (index + 1) % elt->nentries;
	kl_get_struct(index * len + elt->entries, len,
				  event, "tstamp_event_entry_t");
	if (KL_ERROR) {
		fprintf(ofp, "\n");
		kl_print_error();
		kl_free_block(event);
		return;
	}
	num = KL_UINT(event, "tstamp_event_entry", "evt");
    while (num == JUMBO_EVENT && index != elt->prev) {
		k_uint_t v;
        for (c=kl_member_offset("tstamp_event_entry", "tstamp");
			 c<len; c += sizeof(v), c2++) {
            if (c2 % 3 == 0)
                fprintf(ofp, "\n%27s", "");
			bcopy((k_uint_t)event + c, &v, sizeof(v));
            fprintf(ofp, " %016llx", v);
        }
        index = (index + 1) % elt->nentries;
		kl_get_struct(index * len + elt->entries, len, 
					  event, "tstamp_event_entry_t");
		if (KL_ERROR) {
			fprintf(ofp, "\n");
			kl_print_error();
			kl_free_block(event);
			return;
		}
		num = KL_UINT(event, "tstamp_event_entry", "evt");
    }
    fprintf(ofp, "\n");

	kl_free_block(event);
}


/*
 * "Advance" to the previous entry in the buffer, skipping to the
 * beginning of jumbo and continuation events
 */

#define NEXT_ENTRY(E, entrylen) \
{ \
	(E).prev = (E).index; \
	do { \
		(E).index = (E).index ? (E).index - 1 : (E).nentries - 1; \
		kl_get_struct((E).index * (entrylen) + (E).entries, \
					  entrylen, (E).entry, "tstamp_event_entry_t"); \
		if (KL_ERROR) { \
			kl_print_error(); \
			goto done; \
		} \
	} while ((E).evtype ? ((E).evtype[(E).index] == EVTYPE_CONT) : \
		(KL_UINT((E).entry, "tstamp_event_entry", "evt") == JUMBO_EVENT)); \
}


/*
 * utrace_merge() -- Merge and print utrace buffers
 */

void
utrace_merge(FILE *ofp, tracelist_t *list, int cpu_cnt, kaddr_t rec_cnt, 
			 int flags)
{
	int i, hiidx;				/* array indices */
	k_ptr_t pdap;				/* ptr. to pda buffer */
	int ut_entrylen;			/* length of a utrace_entry_t */
	k_uint_t hiclk, clk;		/* rtc clock values */

	ut_entrylen = kl_struct_len("tstamp_event_entry");

	fprintf(ofp, "%-23s%4s %16s %16s %16s\n", "ID", "CPU", "REAL TIME CLOCK",
			"INFO WORD 1,3", "INFO WORD 2,4");

	/* Get the utrace info. from the PDAs */
	pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
	for (i=0; i<cpu_cnt; i++) {
		(void)kl_get_pda_s((kaddr_t)list[i].cpu, pdap);
		if (KL_ERROR) {
			kl_print_error();
			goto done;
		}
		else {
			if (utrace_init(&list[i], pdap))
				goto done;
			NEXT_ENTRY(list[i], ut_entrylen);
		}
	}

	/* Print'em! */
	while(rec_cnt) {
		/* Scan the current records for each CPU to get the latest rtc */
		hiclk = 0;
		for (i=0; i<cpu_cnt; i++) {
			if (!list[i].left) continue;
			clk = kl_uint(list[i].entry, "tstamp_event_entry", "tstamp", 0);
			if (clk > hiclk) {
				hiclk = clk;
				hiidx = i;
			}
		}
		if (hiclk == 0) break;

		/* Print the record, and advance to the prev. rec. on that CPU */
		utrace_print(ofp, &list[hiidx], flags);
		NEXT_ENTRY(list[hiidx], ut_entrylen);
		list[hiidx].left--;
		rec_cnt--;
	}

done:							/* called from NEXT_ENTRY() macro */
	kl_free_block(pdap);
	for (i=0; i<cpu_cnt; i++) {
		if (list[i].entry)
			kl_free_block(list[i].entry);
		if (list[i].evtype)
			kl_free_block((caddr_t *)(list[i].evtype));
	}
}


/*
 * utrace_cmd() -- Run the 'utrace' command.  
 */
int
utrace_cmd(command_t cmd)
{
	kaddr_t num_recs;			/* number of utrace records to print */
	kaddr_t value;
	uint_t arg = 0, i;
	tracelist_t *list;			/* list of CPUs to dump */
	tracelist_t *p, *s, *d;		/* and indexes to it */
	k_ptr_t pdap;				/* ptr. to pda buffer */

	/* Are UTRACEs present in this kernel? */
	if (kl_sym_addr("utrace_bufsize") == 0) {
		fprintf(cmd.ofp, "This kernel was compiled without utraces\n");
		return 0;
	}

	if (cmd.flags & C_ALL) {	/* do all CPUs */
		if (cmd.nargs == 0) {
			num_recs = DFLTRECS;   /* do default num. of records */
		}
		else if (cmd.nargs == 1) { /* do specified number of records */
			kl_get_value(cmd.args[arg++], NULL, 0, &num_recs);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PDA;
				kl_print_error();
				return 0;
			}
		}
		else {
			utrace_usage(cmd);	/* tried to specify cpu# and -a */
			return 0;
		}

		/* Allocate and fill in the list of all CPUs */
		list = kl_alloc_block(K_MAXCPUS * sizeof(tracelist_t), K_TEMP);
		for(p=list, i=0; i < K_MAXCPUS; i++, p++)
			p->cpu = i;
	}

	else {						/* do only specified CPUs */
		if (cmd.nargs == 0) {
			utrace_usage(cmd);	/* need cpu_list _or_ -a */
			return 0;
		}
		else if (cmd.nargs == 1) {
			num_recs = DFLTRECS;   /* single arg. means default count */
		}
		else if (cmd.nargs > 1) { /* non-default num. of records */
			kl_get_value(cmd.args[arg++], NULL, 0, &num_recs);
		}

		/* Allocate and fill in the list of CPUs */
		list = kl_alloc_block((cmd.nargs - arg) * sizeof(tracelist_t), K_TEMP);
		for(p=list; arg < cmd.nargs; arg++) {
			kl_get_value(cmd.args[arg], NULL, K_MAXCPUS, &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_CPUID;
				kl_print_error();
				continue;
			}
			p->cpu = (cpuid_t)value;
			if (p->cpu < 0 || p->cpu >= K_MAXCPUS) {
				KL_SET_ERROR_NVAL(KLE_BAD_CPUID, p->cpu, 0);
				kl_print_error();
				continue;
			}
			p++;
		}
	}

	/* Are traces active on the selected CPUs? */
	pdap = kl_alloc_block(PDA_S_SIZE, K_TEMP);
	for (s=d=list; s<p; s++) {
		*d = *s;
		(void)kl_get_pda_s((kaddr_t)s->cpu, pdap);
		if (KL_ERROR) {
			kl_print_error();
			continue;
		}
		if (!kl_kaddr(pdap, "pda_s", "p_tstamp_objp")) {
			fprintf(cmd.ofp, "Traces are not active on CPU %d\n", s->cpu);
		}
		else {
			d++;
		}
	}
	kl_free_block((caddr_t *)pdap);

    /* Merge 'em, if any valid CPUs */
	if (d - list)
		utrace_merge(cmd.ofp, list, d - list, num_recs, cmd.flags);
	kl_free_block((caddr_t *)list);

	return(0);
}

#define _UTRACE_USAGE "[-a] [-f] [-w outfile] [count] [cpu_list]"

/*
 * utrace_usage() -- Print the usage string for the 'utrace' command.
 */
void
utrace_usage(command_t cmd)
{
    CMD_USAGE(cmd, _UTRACE_USAGE);
}

/*
 * utrace_help() -- Print the help information for the 'utrace' command.
 */
void
utrace_help(command_t cmd)
{
    CMD_HELP(cmd, _UTRACE_USAGE,
        "Dump the specified CPU(s)' utrace buffers, merging "
		"them into one global time sequence.  The traces are shown "
		"in reverse chronological order.  If no count is given, 20 "
		"traces are shown by default (a single argument is taken as "
		"a CPU number, not a count.)  If the -a argument is "
		"present, dump all the cpus' buffers.  If the -f argument "
		"is present, show each trace's address and all its fields.");
/* XXX needed: a way to show the next n entries (UNICOS crash: ut +20) */
}

/*
 * utrace_parse() -- Parse the command line arguments for 'utrace'.
 */
int
utrace_parse(command_t cmd)
{
	return (C_MAYBE|C_ALL|C_FULL|C_WRITE);
}
