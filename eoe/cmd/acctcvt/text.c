/*
 * acctcvt/text.c
 *	acctcvt functions for dealing with accounting data in text format
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/extacct.h>
#include <sys/mkdev.h>
#include <time.h>

#include "common.h"


#define DATE_FMT   "%d-%H:%M:%S"


/*
 * WriteHeader_text
 *	Write out a file header in human readable text format. Returns
 *	0 if successful, -1 if not.
 */
int
WriteHeader_text(output_t *Output, header_t *Header)
{
	char timebuf[80];
	int i;
	struct in_addr addr;

	Print(Output,
	      "Version %d.%d\n",
	      Header->fh.sat_major,
	      Header->fh.sat_minor);

	cftime(timebuf, "%a %b %e %T %Z %Y", &Header->fh.sat_start_time);
	Print(Output, "Created %s\n", timebuf);

	if (Header->fh.sat_stop_time != 0) {
		cftime(timebuf,
		       "%a %b %e %T %Z %Y",
		       &Header->fh.sat_stop_time);
		Print(Output, "Closed  %s\n", timebuf);
	}
	else {
		Print(Output, "Closed  <unknown>\n");
	}

	Print(Output, "Size    %d\n", Header->fh.sat_total_bytes);

	Print(Output, "\nTimezone %s\n", Header->tz);
	Print(Output, "Hostname %s\n", Header->hostname);
	Print(Output, "Domain   %s\n", Header->domainname);

	addr.s_addr = Header->fh.sat_host_id;
	Print(Output, "Host ID  %s\n", inet_ntoa(addr));

	if (Header->fh.sat_user_entries > 0) {
		Print(Output, "\nUsers:\n");
		for (i = 0;  i < Header->fh.sat_user_entries; ++i) {
			Print(Output, "    %s\n", Header->users[i]);
		}
	}

	if (Header->fh.sat_group_entries > 0) {
		Print(Output, "\nGroups:\n");
		for (i = 0;  i < Header->fh.sat_group_entries; ++i) {
			Print(Output, "    %s\n", Header->groups[i]);
		}
	}

	if (Header->fh.sat_host_entries > 0) {
		Print(Output, "\nHosts:\n");
		for (i = 0; i < Header->fh.sat_host_entries; ++i) {
			addr.s_addr = Header->hostids[i];
			Print(Output,
			      "    %s, hostid %s\n",
			      Header->hosts[i],
			      inet_ntoa(addr));
		}
	}

	Print(Output,
	      "\n----------------------------------------------------\n\n");

	return 0;
}


/*
 * WriteRecord_text
 *	Writes a canonical data record to the specified output_t
 *	in human readable format.  Information is printed out in
 *	row format.
 *
 *	Returns 0 if successful, -1 if not.
 */
int
WriteRecord_text(output_t *Output, record_t *Record)
{
	char gidbuf[80];
	char timebuf[80];
	char uidbuf[80];
	int  i;
	static int NotFirst = 0;

	if (NotFirst) {
		Print(Output,
		      "----------------------------------------------------"
		          "\n\n");
	}
	else {
		NotFirst = 1;
	}

	if (Debug & DEBUG_DETAILS) {
		Print(Output, "Record Type %d\n", Record->rectype);
		Print(Output, "    Size         %d\n", Record->size);

		if (Record->gotext) {
			Print(Output,
			      "    Outcome      %d\n",
			      Record->outcome);
			Print(Output,
			      "    Sequence     %d\n",
			      Record->sequence);
			Print(Output,
			      "    Error        %d\n",
			      Record->error);
			Print(Output,
			      "    Syscall      %s\n",
			      sys_call(Record->syscall, Record->subsyscall));
			Print(Output,
			      "    SAT ID       %s\n",
			      UserName(Record->satuid, uidbuf, fmt_text));

			if (Record->cwd != NULL) {
				Print(Output,
				      "    CWD          %s\n",
				      Record->cwd);
			}

			Print(Output,
			      "    PID          %d\n",
			      Record->pid);
			Print(Output,
			      "    Parent PID   %d\n",
			      Record->ppid);

			if (Record->gotowner) {
				Print(Output,
				      "    Real User    %s\n",
				      UserName(Record->ruid, uidbuf,
					       fmt_text));
				Print(Output,
				      "    Real Group   %s\n",
				      GroupName(Record->rgid, gidbuf,
						fmt_text));
			}

			Print(Output,
			      "    Eff User     %s\n",
			      UserName(Record->euid, uidbuf, fmt_text));
			Print(Output,
			      "    Eff Group    %s\n",
			      GroupName(Record->egid, gidbuf, fmt_text));
		}

		Print(Output,
		      "    TTY          %d,%d\n",
		      major(Record->tty), minor(Record->tty));

		if (Record->numgroups > 0) {
			Print(Output,
			      "    Groups       %s\n",
			      GroupName(Record->groups[0], gidbuf, fmt_text));
			for (i = 1;  i < Record->numgroups;  ++i) {
				Print(Output,
				      "                 %s\n",
				      GroupName(Record->groups[i], gidbuf,
						fmt_text));
			}
		}

		if (Record->gotcaps) {
			char *capstring;

			capstring = cap_to_text(&Record->caps, NULL);
			Print(Output,
			      "    Capability   %s\n",
			      capstring);
			cap_free(capstring);
		}

		if (Record->gotpriv) {
			char *capstring;
			char *howstring;

			capstring = cap_value_to_text(Record->priv);
			switch (Record->privhow) {
			    case 0:
				howstring = "-/capability=";
				break;

			    case SAT_SUSERPOSS:
				howstring = "*/capability=";
				break;

			    case SAT_CAPPOSS:
				howstring = "+/capability=";
				break;

			    case (SAT_SUSERPOSS | SAT_CAPPOSS):
				howstring = "+/capability=";
				break;

			    default:
				howstring = "?/capability=";
				break;
			}

			Print(Output,
			      "    Privs Used   %s%s\n",
			      howstring,
			      capstring);
		}

		Print(Output, "\n");
	}

	switch (Record->rectype) {

	    case SAT_PROC_ACCT:
		Print(Output, "Process Accounting\n");
		break;

	    case SAT_SESSION_ACCT:
		Print(Output, "Session Accounting\n");
		break;

	    case SAT_AE_CUSTOM:
		Print(Output, "Custom Accounting record.\n");
		break;

	    default:
		InternalError();
		abort();
	}

	if (Record->gotext) {
		cftime(timebuf, "%x %X", &Record->clock);
		Print(Output,
		      "    Timestamp    %s.%02d\n",
		      timebuf,
		      Record->ticks);
	}

	if (Record->command != NULL) {
		Print(Output,
		      "    Command      %s\n",
		      Record->command);
	}

	if (Record->gotowner) {
		Print(Output,
		      "    User         %s\n",
		      UserName(Record->ouid, uidbuf, fmt_text));
		Print(Output,
		      "    Group        %s\n",
		      GroupName(Record->ogid, gidbuf, fmt_text));
	}
	else {
		Print(Output,
		      "    User         %s\n",
		      UserName(Record->ruid, uidbuf, fmt_text));
		Print(Output,
		      "    Group        %s\n",
		      GroupName(Record->rgid, gidbuf, fmt_text));
	}

	Print(Output, "\n");

	switch (Record->rectype) {

	    case SAT_PROC_ACCT:
		if (Record->gotext) {
			Print(Output,
			      "    Acct Version %d\n",
			      Record->data.p.pa.sat_version);
		}

		Print(Output,
		      "    Flags       ");
		if (Record->data.p.pa.sat_flag == 0) {
			Print(Output, " <none>\n");
		}
		else {
			if (Record->data.p.pa.sat_flag & SPASF_FORK) {
				Print(Output, " FORK");
			}
			if (Record->data.p.pa.sat_flag & SPASF_SU) {
				Print(Output, " SU");
			}
			if (Record->data.p.pa.sat_flag & SPASF_SESSEND) {
				Print(Output, " SESSEND");
			}
			if (Record->data.p.pa.sat_flag & SPASF_CKPT) {
				Print(Output, " CKPT");
			}
			if (Record->data.p.pa.sat_flag & SPASF_SECONDARY) {
				Print(Output, " SECONDARY");
			}
			Print(Output, "\n");
		}

		if (Record->gotext) {
			Print(Output,
			      "    Initial Nice %d\n",
			      Record->data.p.pa.sat_nice);
			Print(Output,
			      "    Sched Disc.  %d\n",
			      Record->data.p.pa.sat_sched);
			Print(Output,
			      "    ASH          0x%016llx\n",
			      Record->data.p.pa.sat_ash);
			Print(Output,
			      "    Project ID   %lld\n",
			      Record->data.p.pa.sat_prid);
		}

		cftime(timebuf, "%x %X", &Record->data.p.pa.sat_btime);
		Print(Output,
		      "    Start Time   %s\n",
		      timebuf);
		Print(Output,
		      "    Elapsed Time %d ticks\n",
		      Record->data.p.pa.sat_etime);
		break;

	    case SAT_SESSION_ACCT:

		/* SVR4 doesn't produce these, so we don't */
		/* have to keep checking the gotext flag   */
		Print(Output,
		      "    Acct Version %d\n",
		      Record->data.s.sa.sat_version);


		Print(Output,
		      "    Flags       ");
		if (Record->data.p.pa.sat_flag == 0) {
			Print(Output, " <none>\n");
		}
		else {
			if (Record->data.s.sa.sat_flag & SSASF_CKPT) {
				Print(Output, " CKPT");
			}
			if (Record->data.s.sa.sat_flag & SSASF_SECONDARY) {
				Print(Output, " SECONDARY");
			}
			if (Record->data.s.sa.sat_flag & SSASF_FLUSHED) {
				Print(Output, " FLUSHED");
			}
			Print(Output, "\n");
		}

		Print(Output,
		      "    Initial Nice %d\n",
		      Record->data.s.sa.sat_nice);
		Print(Output,
		      "    ASH          0x%016llx\n",
		      Record->data.s.sa.sat_ash);
		Print(Output,
		      "    Project ID   %lld\n",
		      Record->data.s.sa.sat_prid);

		cftime(timebuf, "%x %X", &Record->data.s.sa.sat_btime);
		Print(Output,
		      "    Start Time   %s\n",
		      timebuf);
		Print(Output,
		      "    Elapsed Time %d ticks\n",
		      Record->data.s.sa.sat_etime);

		if (Record->data.s.sa.sat_version > 1) {
			Print(Output,
			      "    SPI Length   %d\n",
			      Record->data.s.sa.sat_spilen);
		}

		if (Record->data.s.spi != NULL) {
			switch (Record->data.s.spifmt) {

			    case 1:
			    {
				    acct_spi_t *s =
					(acct_spi_t *) Record->data.s.spi;

				    Print(Output,
					  "\n    Service Provider Info "
					      "(format 1)\n");
				    Print(Output,
					  "        Company   %.8s\n",
					  s->spi_company);
				    Print(Output,
					  "        Initiator %.8s\n",
					  s->spi_initiator);
				    Print(Output,
					  "        Origin    %.16s\n",
					  s->spi_origin);
				    Print(Output,
					  "        Vendor    %.16s\n",
					  s->spi_spi);

				    break;
			    }

			    case 2:
			    {
				    acct_spi_2_t *s =
					(acct_spi_2_t *) Record->data.s.spi;

				    Print(Output,
					  "\n    Service Provider Info "
					      "(format 2)\n");
				    Print(Output,
					  "        Company   %.8s\n",
					  s->spi_company);
				    Print(Output,
					  "        Initiator %.8s\n",
					  s->spi_initiator);
				    Print(Output,
					  "        Origin    %.16s\n",
					  s->spi_origin);
				    Print(Output,
					  "        Vendor    %.16s\n",
					  s->spi_spi);
				    Print(Output,
					  "        Job Name  %.32s\n",
					  s->spi_jobname);
				    Print(Output,
					  "        Sub Time  %lld\n",
					  s->spi_subtime);
				    Print(Output,
					  "        Exec Time %lld\n",
					  s->spi_exectime);
				    Print(Output,
					  "        Wait Time %lld\n",
					  s->spi_waittime);

				    break;
			    }

			    default:
				InternalError();
				abort();
			}
		}

		break;

	    case SAT_AE_CUSTOM:
		if (Record->custom != NULL) {
			Print(Output,
				"    Custom       %s\n",
				Record->custom);
		}
		break;

	    default:
		InternalError();
		abort();
	}

	/* Remaining tokens are common to both accounting record types */
	if (Record->rectype != SAT_AE_CUSTOM) {
		Print(Output, "\n    Timer values\n");
		Print(Output,
			"        User Time        %lld.%09lld secs\n",
			Record->timers.ac_utime / 1000000000LL,
			Record->timers.ac_utime % 1000000000LL);
		Print(Output,
			"        System Time      %lld.%09lld secs\n",
			Record->timers.ac_stime / 1000000000LL,
			Record->timers.ac_stime % 1000000000LL);

		if (Record->gotext) {
			Print(Output,
				"        Block I/O Wait   %lld ns\n",
				Record->timers.ac_bwtime);
			Print(Output,
				"        Raw I/O Wait     %lld ns\n",
				Record->timers.ac_rwtime);
			Print(Output,
				"        Run Queue Wait   %lld ns\n",
				Record->timers.ac_qwtime);
		}

		Print(Output, "\n    Counter values\n");
		Print(Output,
			"        Memory Usage     %lld\n",
			Record->counts.ac_mem);

		if (Record->gotext) {
			Print(Output,
				"        # swaps          %lld\n",
				Record->counts.ac_swaps);
			Print(Output,
				"        # bytes read     %lld\n",
				Record->counts.ac_chr);
			Print(Output,
				"        # bytes written  %lld\n",
				Record->counts.ac_chw);
			Print(Output,
				"        # blocks read    %lld\n",
				Record->counts.ac_br);
			Print(Output,
				"        # blocks written %lld\n",
				Record->counts.ac_bw);
			Print(Output,
				"        # reads          %lld\n",
				Record->counts.ac_syscr);
			Print(Output,
				"        # writes         %lld\n",
				Record->counts.ac_syscw);

		} else {
			Print(Output,
				"        # bytes I/O      %lld\n",
				Record->counts.ac_chr);
			Print(Output,
				"        # blocks I/O     %lld\n",
				Record->counts.ac_br);
		}
	}

	Print(Output, "\n");

	return 0;
}


/*
 * PrintHeading
 *	Prints the column heading to the specified output_t
 *	for text_acctcom format.
 */
void
PrintHeading(output_t *Output)
{
	/* First line. */
	Print(Output, "\nCOMMAND   ");
	Print(Output, "%8s ", " ");
	if (option & TTY)
                Print(Output, "%5s ", " ");
	Print(Output, "START       END             REAL      CPU ");
	if (option & SEPTIME)
		Print(Output, "  (SECS) ");
	if (option & IORW) {
		if (combine) {
			Print(Output, "   CHARS ");
			Print(Output, "  BLOCKS ");
		} else {
			Print(Output, "   CHARS ");
			Print(Output, "   CHARS ");
			Print(Output, "  BLOCKS ");
			Print(Output, "  BLOCKS ");
		}
	}
	if ((option & MEANSIZE) || !option)
		Print(Output, "%10s ", "MEAN");
	if (option & KCOREMIN)
		Print(Output, "%10s ", "KCORE");
	if (option & STATUS)
		Print(Output, "%4s ", " ");
	if (option & ASH)
		Print(Output, "  %16s ", "ARRAY");
	if (option & PRID)
		Print(Output, " PROJECT ");
	if (option & GID)
		Print(Output, "%8s ", " ");
	if (option & AIO) {
		if (combine) {
			Print(Output, " LOGICAL ");
		}
		else {
			Print(Output, " LOGICAL ");
			Print(Output, " LOGICAL ");
		}
	}
	if (option & PID)
		Print(Output, "%13s ", " ");
	if (option & WAITTIME) {
		Print(Output, "RUN QUEU ");
		if (combine) {
			Print(Output, "  IOWAIT ");
		}
		else {
			Print(Output, "  IOWAIT ");
			Print(Output, "  (SECS) ");
		}
	}
	Print(Output, "\n");

	/* Second line. */
	Print(Output, "NAME      USER     ");
	if (option & TTY)
                Print(Output, "TTY   ");
	Print(Output, "TIME        TIME          (SECS) ");
	if (option & SEPTIME) {
		Print(Output, "     SYS ");
		Print(Output, "    USER ");
	}
	else
		Print(Output, "  (SECS) ");
	if (option & IORW) {
		if (combine) {
			Print(Output, "  TRNSFD ");
			Print(Output, "  TRNSFD ");
		} else {
			Print(Output, "    READ ");
			Print(Output, " WRITTEN ");
			Print(Output, "    READ ");
			Print(Output, " WRITTEN ");
		}
	}
	if ((option & MEANSIZE) || !option)
		Print(Output, "%10s ", "SIZE(K)");
	if (option & KCOREMIN)
		Print(Output, "%10s ", "MIN");
	if (option & STATUS)
		Print(Output, "STAT ");
	if (option & ASH)
		Print(Output, "    SESSION HANDLE ");
	if (option & PRID)
		Print(Output, "%8s ", "ID");
	if (option & GID)
		Print(Output, "   GROUP ");
	if (option & AIO) {
		if (combine) {
			Print(Output, "    REQS ");
		}
		else {
			Print(Output, "   READS ");
			Print(Output, "  WRITES ");
		}
	}
	if (option & PID)
		Print(Output, "   PID   PPID ");
	if (option & WAITTIME) {
		Print(Output, "WAIT (S) ");
		if (combine) {
			Print(Output, "  (SECS) ");
		}
		else {
			Print(Output, "   BLOCK ");
			Print(Output, "     RAW ");
		}
	}
	if (option & SPI)
		Print(Output, "     SPI ");
	Print(Output, "\n");

	return;
}


/*
 * WriteRecord_acctcom
 *	Writes a canonical data record to the specified output_t
 *	in human readable text_acctcom format.  Information is
 *	printed out in columns similarly to the output of acctcom.
 *
 *	Returns 0 if successful, -1 if not.
 */
int
WriteRecord_acctcom(output_t *Output, record_t *Record)
{
	double     cpu;
	time_t     endtime;
	char       gidbuf[80];
 	int        i;
	double     mem;
	int        print_hdr = 0;
	static int recno = 0;
	char       timebuf[80];
	int        top_rec;   /* Number of the record at top of the page. */
	char       uidbuf[80];

	recno++;
	if (!no_header && recno == 1)
		print_hdr = 1;
	else if (hdr_page)
	{
		/* NOTE - The integer division is essential here. */
		top_rec = (recno / rppage) * rppage + 1;
		if (recno == top_rec)
			print_hdr = 1;
	}

	if (print_hdr)
		PrintHeading(Output);
	
	if (Debug & DEBUG_DETAILS) {
		Print(Output, "Record Type %d\n", Record->rectype);
		Print(Output, "    Size         %d\n", Record->size);

		if (Record->gotext) {
			Print(Output,
			      "    Outcome      %d\n",
			      Record->outcome);
			Print(Output,
			      "    Sequence     %d\n",
			      Record->sequence);
			Print(Output,
			      "    Error        %d\n",
			      Record->error);
			Print(Output,
			      "    Syscall      %s\n",
			      sys_call(Record->syscall, Record->subsyscall));
			Print(Output,
			      "    SAT ID       %s\n",
			      UserName(Record->satuid, uidbuf, fmt_acctcom));

			if (Record->gotowner) {
				Print(Output,
				      "    Real User    %s\n",
				      UserName(Record->ruid, uidbuf,
					       fmt_acctcom));
				Print(Output,
				      "    Real Group   %s\n",
				      GroupName(Record->rgid, gidbuf,
						fmt_acctcom));
			}

			Print(Output,
			      "    Eff User     %s\n",
			      UserName(Record->euid, uidbuf, fmt_acctcom));
			Print(Output,
			      "    Eff Group    %s\n",
			      GroupName(Record->egid, gidbuf, fmt_acctcom));
		}

		if (Record->numgroups > 0) {
			Print(Output,
			      "    Groups       %s\n",
			      GroupName(Record->groups[0], gidbuf,
					fmt_acctcom));
			for (i = 1;  i < Record->numgroups;  ++i) {
				Print(Output,
				      "                 %s\n",
				      GroupName(Record->groups[i], gidbuf,
						fmt_acctcom));
			}
		}

		if (Record->gotcaps) {
			char *capstring;

			capstring = cap_to_text(&Record->caps, NULL);
			Print(Output, "    Capability   %s\n", capstring);
			cap_free(capstring);
		}

		if (Record->gotpriv) {
			char *capstring;
			char *howstring;

			capstring = cap_value_to_text(Record->priv);
			switch (Record->privhow) {
			    case 0:
				howstring = "-/capability=";
				break;

			    case SAT_SUSERPOSS:
				howstring = "*/capability=";
				break;

			    case SAT_CAPPOSS:
				howstring = "+/capability=";
				break;

			    case (SAT_SUSERPOSS | SAT_CAPPOSS):
				howstring = "+/capability=";
				break;

			    default:
				howstring = "?/capability=";
				break;
			}

			Print(Output,
			      "    Privs Used   %s%s\n",
			      howstring, capstring);
		}

		Print(Output, "\n");
	}

	switch (Record->rectype) {
	    case SAT_PROC_ACCT:
		if (Record->command != NULL)
			Print(Output, "%-9.9s ", Record->command);
		break;

	    case SAT_SESSION_ACCT:
		if (Record->command != NULL)
			Print(Output, "@%-8.8s ", Record->command);
		break;

	    case SAT_AE_CUSTOM:
		Print(Output, "Custom Accounting record.\n");
		return 0;

	    default:
		InternalError();
		abort();
	}

	if (Record->gotowner) {
		Print(Output,
		      "%-8.8s ",
		      UserName(Record->ouid, uidbuf, fmt_acctcom));
	}
	else {
		Print(Output,
		      "%-8.8s ",
		      UserName(Record->ruid, uidbuf, fmt_acctcom));
	}

	if (option & TTY) {
		Print(Output,
		      "%2d,%2d ",
		      major(Record->tty), minor(Record->tty));
	}
	
	if (Record->rectype == SAT_PROC_ACCT) {
		cftime(timebuf, DATE_FMT, &Record->data.p.pa.sat_btime);
		Print(Output, "%s ", timebuf);
		endtime = Record->data.p.pa.sat_btime +
			Record->data.p.pa.sat_etime / HZ;
		cftime(timebuf, DATE_FMT, &endtime); 
		Print(Output, "%s ", timebuf);
		Print(Output,
		      "%8.2f ",
		      (double) Record->data.p.pa.sat_etime / HZ);
	}
	else { /* SAT_SESSION_ACCT */
		cftime(timebuf, DATE_FMT, &Record->data.s.sa.sat_btime);
		Print(Output, "%s ", timebuf);
		endtime = Record->data.s.sa.sat_btime +
			Record->data.s.sa.sat_etime / HZ;
		cftime(timebuf, DATE_FMT, &endtime); 
		Print(Output, "%s ", timebuf);
		Print(Output,
		      "%8.2f ",
		      (double) Record->data.s.sa.sat_etime / HZ);
	}

	/* Remaining tokens are common to both accounting record types. */

	if (option & SEPTIME) {
		Print(Output,
		      "%8.2f ",
		      (double) Record->timers.ac_stime / NSEC_PER_SEC);
		Print(Output,
		      "%8.2f ",
		      (double) Record->timers.ac_utime / NSEC_PER_SEC);
	}
	else {
		Print(Output,
		      "%8.2f ",
		      (double) (Record->timers.ac_stime +
		      Record->timers.ac_utime) / NSEC_PER_SEC);
	}

	if (option & IORW) {
		if (Record->gotext) {
			if (combine) {
				Print(Output,
				      "%8lld ",
				      Record->counts.ac_chr +
				      Record->counts.ac_chw);
				Print(Output,
				      "%8lld ",
				      Record->counts.ac_br +
				      Record->counts.ac_bw);
			}
			else {
				Print(Output,
				      "%8lld ",
				      Record->counts.ac_chr);
				Print(Output,
				      "%8lld ",
				      Record->counts.ac_chw);
				Print(Output,
				      "%8lld ",
				      Record->counts.ac_br);
				Print(Output,
				      "%8lld ",
				      Record->counts.ac_bw);
			}
		}
		else {
			Print(Output, "%8lld ", Record->counts.ac_chr);
			Print(Output, "%8lld ", Record->counts.ac_br);
		}
	}
	
	/* cpu is in ticks. */
	cpu = (double) (Record->timers.ac_utime + Record->timers.ac_stime) /
		TICK;
	mem = KCORE(Record->counts.ac_mem / cpu);

	if ((option & MEANSIZE) || !option)
		Print(Output, "%10.2f ", mem);

	if (option & KCOREMIN)
		Print(Output, "%10.2f ", MINT(KCORE(Record->counts.ac_mem)));

	if (option & STATUS)
		Print(Output, " %3d ", Record->status);

	if (option & ASH)
		Print(Output, "0x%016llx ", Record->data.p.pa.sat_ash);
		
	if (option & PRID)
		Print(Output, "%8lld ", Record->data.p.pa.sat_prid);

	if (option & GID) {
		if (Record->gotowner) {
			Print(Output,
			      "%8.8s ",
			      GroupName(Record->ogid, gidbuf, fmt_acctcom));
		}
		else {
			Print(Output,
			      "%8.8s ",
			      GroupName(Record->rgid, gidbuf, fmt_acctcom));
		}
	}
	
	if (option & AIO) {
		if (combine) {
			Print(Output,
			      "%8lld ",
			      Record->counts.ac_syscr +
			      Record->counts.ac_syscw);
		} else {
			Print(Output, "%8lld ", Record->counts.ac_syscr);
			Print(Output, "%8lld ", Record->counts.ac_syscw);
		}
	}

	if (option & PID)
		Print(Output, "%6d %6d ", Record->pid, Record->ppid);

	if (option & WAITTIME) {
		Print(Output,
		      "%8.2f ",
		      (double) Record->timers.ac_qwtime / NSEC_PER_SEC);
		if (combine) {
			Print(Output,
			      "%8.2f ",
			      (double) (Record->timers.ac_bwtime +
					Record->timers.ac_rwtime) /
			      NSEC_PER_SEC);
		}
		else {
			Print(Output,
			      "%8.2f ",
			      (double) Record->timers.ac_bwtime /
			      NSEC_PER_SEC);
			Print(Output,
			      "%8.2f ",
			      (double) Record->timers.ac_rwtime /
			      NSEC_PER_SEC);
		}
	}

	if (option & SPI &&
	    Record->rectype == SAT_SESSION_ACCT &&
	    Record->data.s.spi != NULL) {
		switch (Record->data.s.spifmt) {
		    case 1:
		    {
			acct_spi_t *s = (acct_spi_t *) Record->data.s.spi;
			Print(Output, "%.8s ", s->spi_company);
			Print(Output, "%.8s ", s->spi_initiator);
			Print(Output, "%.16s ", s->spi_origin);
			Print(Output, "%.16s ", s->spi_spi);
			break;
		    }
			
		    case 2:
		    {
			acct_spi_2_t *s = (acct_spi_2_t *) Record->data.s.spi;
			Print(Output, "%.8s ", s->spi_company);
			Print(Output, "%.8s ", s->spi_initiator);
			Print(Output, "%.16s ", s->spi_origin);
			Print(Output, "%.16s ", s->spi_spi);
			Print(Output, "%.32s ", s->spi_jobname);
			Print(Output, "%lld ", s->spi_subtime);
			Print(Output, "%lld ", s->spi_exectime);
			Print(Output, "%lld ", s->spi_waittime);
			break;
		    }

		    default:
			InternalError();
			abort();
		}
	}
	
	Print(Output, "\n");
	
	return 0;
}


/*
 * WriteRecord_awk
 *	Writes a canonical data record to the specified output_t
 *	in text_awk format.  Returns 0 if successful, -1 if not.
 *
 *	The output is in ascii, but it is not too human readable
 *	since things are not lined up in columns.  Instead, the
 *	fields are just separated by a delimiter that is user
 *	defined.  This output can be fed into a post-processing
 *	tool like awk, hence the name text_awk.
 *
 *	The first few fields of this output match the columns
 *	produced by the command 'acctcom -fitmk'.  This is done
 *	so that a customer's awk script can handle both types
 *	of report.
 *
 *	NOTE - There is currently a bug in acctcvt in that it does
 *      not process the SAT exit status value (SAT_STATUS_TOKEN).
 *	When that bug is resolved, this function should print out
 *	the exit status value (the value printed by the -f option
 *	in acctcom).  Feb. 26, 1999.
 */
int
WriteRecord_awk(output_t *Output, record_t *Record)
{
	double     cpu;
	time_t     endtime;
	char       gidbuf[80];
	double     mem;
	char       timebuf[80];
	char       uidbuf[80];

	switch (Record->rectype) {
	    case SAT_PROC_ACCT:
		if (Record->command != NULL)
			Print(Output, "%s", Record->command);
		break;

	    case SAT_SESSION_ACCT:
		if (Record->command != NULL)
			Print(Output, "@%s", Record->command);
		break;

	    case SAT_AE_CUSTOM:
		Print(Output, "Custom Accounting record.\n");
		return 0;

	    default:
		InternalError();
		abort();
	}

	if (Record->gotowner) {
		Print(Output,
		      "%s%s",
		      fieldsep, UserName(Record->ouid, uidbuf, fmt_awk));
	}
	else {
		Print(Output,
		      "%s%s",
		      fieldsep, UserName(Record->ruid, uidbuf, fmt_awk));
	}

	Print(Output,
	      "%s%d_%d",
	      fieldsep, major(Record->tty), minor(Record->tty));
	
	if (Record->rectype == SAT_PROC_ACCT) {
		cftime(timebuf, DATE_FMT, &Record->data.p.pa.sat_btime);
		Print(Output, "%s%s", fieldsep, timebuf);
		endtime = Record->data.p.pa.sat_btime +
			Record->data.p.pa.sat_etime / HZ;
		cftime(timebuf, DATE_FMT, &endtime); 
		Print(Output, "%s%s", fieldsep, timebuf);
		Print(Output,
		      "%s%.2f",
		      fieldsep, (double) Record->data.p.pa.sat_etime / HZ);
	}
	else { /* SAT_SESSION_ACCT */
		cftime(timebuf, DATE_FMT, &Record->data.s.sa.sat_btime);
		Print(Output, "%s%s", fieldsep, timebuf);
		endtime = Record->data.s.sa.sat_btime +
			Record->data.s.sa.sat_etime / HZ;
		cftime(timebuf, DATE_FMT, &endtime); 
		Print(Output, "%s%s", fieldsep, timebuf);
		Print(Output,
		      "%s%.2f",
		      fieldsep, (double) Record->data.s.sa.sat_etime / HZ);
	}

	/* Remaining tokens are common to both accounting record types. */

	Print(Output,
	      "%s%.2f",
	      fieldsep, (double) Record->timers.ac_stime / NSEC_PER_SEC);
	Print(Output,
	      "%s%.2f",
	      fieldsep, (double) Record->timers.ac_utime / NSEC_PER_SEC);

	Print(Output,
	      "%s%lld",
	      fieldsep, Record->counts.ac_chr + Record->counts.ac_chw);
	Print(Output,
	      "%s%lld",
	      fieldsep, Record->counts.ac_br + Record->counts.ac_bw);

	/* cpu is in ticks. */
	cpu = (double) (Record->timers.ac_utime + Record->timers.ac_stime) /
		TICK;
	mem = KCORE(Record->counts.ac_mem / cpu);
	Print(Output, "%s%.2f", fieldsep, mem);
	Print(Output, "%s%.2f", fieldsep, MINT(KCORE(Record->counts.ac_mem)));

	Print(Output, "%s%lld", fieldsep, Record->counts.ac_chr);
	Print(Output, "%s%lld", fieldsep, Record->counts.ac_chw);
	Print(Output, "%s%lld", fieldsep, Record->counts.ac_br);
	Print(Output, "%s%lld", fieldsep, Record->counts.ac_bw);

	Print(Output, "%s0x%llx", fieldsep, Record->data.p.pa.sat_ash);
	Print(Output, "%s%lld", fieldsep, Record->data.p.pa.sat_prid);

	if (Record->gotowner) {
		Print(Output,
		      "%s%s",
		      fieldsep, GroupName(Record->ogid, gidbuf, fmt_awk));
	}
	else {
		Print(Output,
		      "%s%s",
		      fieldsep, GroupName(Record->rgid, gidbuf, fmt_awk));
	}

	Print(Output, "%s%lld", fieldsep, Record->counts.ac_syscr);
	Print(Output, "%s%lld", fieldsep, Record->counts.ac_syscw);

	Print(Output,
	      "%s%d%s%d",
	      fieldsep, Record->pid, fieldsep, Record->ppid);

	Print(Output,
	      "%s%.2f",
	      fieldsep, (double) Record->timers.ac_qwtime / NSEC_PER_SEC);
	Print(Output,
	      "%s%.2f",
	      fieldsep, (double) Record->timers.ac_bwtime / NSEC_PER_SEC);
	Print(Output,
	      "%s%.2f",
	      fieldsep, (double) Record->timers.ac_rwtime / NSEC_PER_SEC);

	if (Record->rectype == SAT_SESSION_ACCT &&
	    Record->data.s.spi != NULL) {
		switch (Record->data.s.spifmt) {
		case 1:
		{
			acct_spi_t *s = (acct_spi_t *) Record->data.s.spi;
			Print(Output, "%s%.8s", fieldsep, s->spi_company);
			Print(Output, "%s%.8s", fieldsep, s->spi_initiator);
			Print(Output, "%s%.16s", fieldsep, s->spi_origin);
			Print(Output, "%s%.16s", fieldsep, s->spi_spi);
			break;
		}
			
		case 2:
		{
			acct_spi_2_t *s = (acct_spi_2_t *) Record->data.s.spi;
			Print(Output, "%s%.8s", fieldsep, s->spi_company);
			Print(Output, "%s%.8s", fieldsep, s->spi_initiator);
			Print(Output, "%s%.16s", fieldsep, s->spi_origin);
			Print(Output, "%s%.16s", fieldsep, s->spi_spi);
			Print(Output, "%s%.32s", fieldsep, s->spi_jobname);
			Print(Output, "%s%lld", fieldsep, s->spi_subtime);
			Print(Output, "%s%lld", fieldsep, s->spi_exectime);
			Print(Output, "%s%lld", fieldsep, s->spi_waittime);
			break;
		}

		default:
			InternalError();
			abort();
		}
	}
	
	Print(Output, "\n");
	
	return 0;
}
