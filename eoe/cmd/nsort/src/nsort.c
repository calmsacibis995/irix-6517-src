/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 * nsort.c - multiprocessor sorting program
 *
 *	$Ordinal-Id: nsort.c,v 1.51 1996/12/23 23:47:24 chris Exp $
 */
#ident	"$Revision: 1.1 $"

#include	"otcsort.h"
#include	"merge.h"
#include	<errno.h>
#include	<sys/times.h>
#include	<sys/mman.h>
#include	<sys/cachectl.h>
#include	<string.h>
#include	<malloc.h>

/*#define IDLE_TIME 1*/
char	Nsort_Copyright[] = "Copyright © 1996 Ordinal Technology Corp";
char	Nsort_id[] = "$Ordinal-Revision: 1.51 $";

#define PRINT_TIME(x)	fprintf(stderr, " %2d.%02d", (x) / 100, (x) % 100)
#define PRINT_TIME4(x)	fprintf(stderr, (x < 10*100) ? " %d.%02d" : " %4d", (x) / 100, (x) % 100)
#define PRINT_TIME6(x)	fprintf(stderr, (x < 1000*100) ? " %3d.%02d" : " %6d", (x) / 100, (x) % 100)
#define PRINT_TIME5(x)	fprintf(stderr, (x < 100*100) ? " %2d.%02d" : " %5d", (x) / 100, (x) % 100)
#define PRINT_TIME6(x)	fprintf(stderr, (x < 1000*100) ? " %3d.%02d" : " %6d", (x) / 100, (x) % 100)
#define PRINT_COUNT(x)	fprintf(stderr, ((x) < 1000*10) ? " %5d" : " %4dk", \
					((x) < 1000*10) ? (x) : (x) / 1024)

extern int Merge12;
extern int SprocDelay;
extern int SortpartLoss;

void nsort_final_write(sort_t *sort, byte *base, unsigned size)
{
    if (TRUE || !OUT.file->copies_write)
    {
	set_file_flags(OUT.file, FSYNC);
	if (lseek64(OUT.file->fd, OUT.write_offset, SEEK_SET) != 
	    OUT.write_offset && errno != ESPIPE)
	{
	    die("lseek() for final write failed: %s", strerror(errno));
	}
	OUT.file->stats.busy -= get_time();
	if (write(OUT.file->fd, base, size) != size)
	    die("final write() failed: %s", strerror(errno));
	OUT.file->stats.busy += get_time();
	OUT.write_offset += size;
    }
}

void usage(const char *badarg)
{
    fprintf(stderr, "Nsort: Unrecognized option \"%s\"\n", badarg);
    fprintf(stderr, "usage: nsort [flags and sortspecs]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  flags:\n");
    fprintf(stderr, "    -a#     set number of input aio's\n");
    fprintf(stderr, "    -A#     set number of output aio's\n");
    fprintf(stderr, "    -b#     set number of 512-byte blocks in each input read request\n");
    fprintf(stderr, "    -B#     set number of 512-byte blocks in each output write request\n");
    fprintf(stderr, "    -C#     sort no more than this many records\n");
    fprintf(stderr, "    -g      generate input internally instead of reading it\n");
    fprintf(stderr, "    -S#     set the number of sorting sprocs (in addition to the i/o sprocs)\n");
    fprintf(stderr, "    -t      print task allocation messages\n");
    fprintf(stderr, "    -u#     set the number of 512-byte blocks in each output sub buffer\n");
    fprintf(stderr, "    -v      print sort definition\n");
    fprintf(stderr, "    <filename>   set the input file name\n");
    fprintf(stderr, "    -o <name>    set the output file name\n");
    exit(1);
}

void print_percent(FILE *file, long part, long total)
{
    int percent;

    if (total == 0)
	fprintf(file, "      ");
    else
    {
	percent = ((part * 100.0) / total) + 0.5;
	fprintf(stderr, "%4d%% ", percent);
    }
}

void print_proc_stats(sort_t *sort, sprocstat_t *stat)
{
    unsigned	inuser, outuser, insys, outsys, totuser, totsys;

    /* input user/sys/busy%  */
    inuser = TIMEVAL_TO_TICKS(stat->in_usage.ru_utime);
    PRINT_TIME6(inuser);
    insys = TIMEVAL_TO_TICKS(stat->in_usage.ru_stime);
    PRINT_TIME5(insys);
    print_percent(stderr, inuser + insys, TIMES.list_done);

    totuser = TIMEVAL_TO_TICKS(stat->total_usage.ru_utime);
    totsys = TIMEVAL_TO_TICKS(stat->total_usage.ru_stime);

    /* output user/sys/busy%  */
    outuser = totuser - inuser;
    outsys = totsys - insys;
    PRINT_TIME6(outuser);
    PRINT_TIME5(outsys);
    print_percent(stderr, outuser+outsys, TIMES.write_done - TIMES.list_done);

    PRINT_TIME6(totuser);
    PRINT_TIME5(totsys);
    print_percent(stderr, totuser + totsys, TIMES.write_done);

    if (STATS.wizard)
    {
	/* voluntary and involuntary context switches */
	PRINT_COUNT(stat->total_usage.ru_nvcsw);
	PRINT_COUNT(stat->total_usage.ru_nivcsw);
#if defined(IDLE_TIME)
	if (stat->in_idle)
	    PRINT_TIME5(stat->in_idle);
#endif
    }
    putc('\n', stderr);
}

/*
 * print_file_stats	- display a per-file statistics line with the header:
 *
 *	File Name                     Busy  Wait  MB/sec   Bytes   Xfers
 */
void print_file_stats(filestats_t *stats, fileinfo_t *file, int dir_only, filestats_t *sum)
{
    static char *mode_names[6] = {"", "direct", "buffered", "mapped", "serial"};
    long	ms;
    int		print_len;


    if (file->name != NULL)
    {
	if (dir_only)
	    print_len = strrchr(file->name, '/') - file->name;
	else
	    print_len = strlen(file->name);
	fprintf(stderr, "  %-21.*s %-8.8s", min(print_len, 21), file->name,
					    mode_names[file->io_mode]);
	if (file->io_mode != IOMODE_MAPPED)
	    print_percent(stderr, stats->busy,
				  stats->finish_last - stats->start_first);
	else
	    fprintf(stderr, "      ");
    }
    else
	fprintf(stderr, " All\t\t\t\t      ");
	
    if (file->io_mode == IOMODE_MAPPED)
	fprintf(stderr, "      ");
    else
	PRINT_TIME5(stats->wait);

    /* Megabytes/second == kbytes/millisecond.
     */
#if 0
    ms = TICKS_TO_MS(stats->finish_last - stats->start_first);
#else
    ms = TICKS_TO_MS(stats->busy);
#endif
    if (ms == 0 || file->io_mode == IOMODE_MAPPED)
	fprintf(stderr, "       ");
    else
	fprintf(stderr, " %6.2f", ((float) stats->bytes / 1024) / (float) ms);
    if (stats->bytes < 100*1000)
	fprintf(stderr, "%7d", stats->bytes);
    else if (stats->bytes < 100*1000*1024)
	fprintf(stderr, "%6dK", stats->bytes / 1024);
    else
	fprintf(stderr, "%6dM", stats->bytes / (1024 * 1024));
    if (file->io_mode != IOMODE_MAPPED)
	fprintf(stderr, "%7d", stats->ios);
    putc('\n', stderr);
	
    if (sum != NULL)
    {
	sum->wait += stats->wait;
	sum->bytes += stats->bytes;
	sum->ios += stats->ios;
	/* should the summarized xfer rate be
		total bytes transfered / time from the start of the first i/o
					 on the earliest file to the last i/o
					 on the latest file, or
		figure the transfer rates for each file, then add those up to
					 come up with the overall transfer rate.
	    The prior is the only one sensible for multiple input files,
	    wheres the latter could be better for temps. For now always use
	    the first definition
	 */
	if (sum->start_first > stats->start_first)
	    sum->start_first = stats->start_first;
	if (sum->finish_last < stats->finish_last)
	    sum->finish_last = stats->finish_last;
    }
}

void print_stats(sort_t *sort)
{
    sprocstat_t		*stat;
    float		empty, used, avg;
    struct part		*part;
    sprocstat_t		*sum;
    unsigned		t;
    unsigned		output_time;
    int			i;
    fileinfo_t		t_file;


    stat = STATS.usages;
    if (getrusage(RUSAGE_SELF, &stat->total_usage) < 0)
	fprintf(stderr, "**getrusage of self failed:%s\n", strerror(errno));
    stat->total_elapsed = TIMES.done;

    /* In a two-pass sort the stats are collected in chk_temp_output().
     * We collect them here in one-pass sorts
     */
    if (!sort->two_pass)
    {
	gather_run_stats(sort, &sort->in_run[0]);
	gather_run_stats(sort, &sort->in_run[1]);
    }

    /* nsort version #, date of most recent source, and the current time */
    t = time(NULL);
    fprintf(stderr, "Nsort version %s using %dK of memory", NsortSourceDate,
		    (sort->end_out_buf - sort->begin_in_run) / 1024);
		    
    if (sort->memory != 0)
	fprintf(stderr, " out of %lldK", sort->memory);
    fprintf(stderr, "\n%s sort", (sort->method == SortMethRecord) ? "Record" 
								  : "Pointer");
    if (sort->compute_hash || sort->radix)
    {
	fputs(" (", stderr);
	if (sort->compute_hash)
	{
	    fputs("hash", stderr);
	    if (sort->radix)
		putc(',', stderr);
	}
	if (sort->radix)
	    fputs("radix", stderr);
	putc(')', stderr);
    }
    if (sort->two_pass)
	fprintf(stderr, " of %d runs", IN.runs_flushed);
    fprintf(stderr, " performed %s", asctime(localtime((time_t *) &t)));

    fprintf(stderr,
	    "\t  Input Phase       Output Phase         Overall\nElapsed  ");
    PRINT_TIME6(TIMES.list_done);
    fputs("            ", stderr);
    output_time = TIMES.write_done - TIMES.list_done;
    PRINT_TIME6(output_time);
    fputs("            ", stderr);
    PRINT_TIME6(TIMES.done);

    fprintf(stderr, "\nI/O Busy ");
    for (i = 0, t = 0; i < IN.n_files; i++)
	t += IN.files[i].stats.busy;
    PRINT_TIME6(t);
    fprintf(stderr, "  ");
    print_percent(stderr, t, TIMES.read_done - TIMES.read_first);
    fprintf(stderr, "    ");
    PRINT_TIME6(OUT.file->stats.busy);
    fprintf(stderr, "  ");
    print_percent(stderr,
		  OUT.file->stats.busy,
		  OUT.file->stats.finish_last - OUT.file->stats.start_first);
    fprintf(stderr, "    ");
    PRINT_TIME6(t + OUT.file->stats.busy);

    if (sort->two_pass && TEMP.n_files > 1)
    {
	int	busy_in;
	int	busy_out;

	fprintf(stderr, "\nTemp Max ");
	busy_in = busy_out = 0;
	for (i = 1; i < TEMP.n_files; i++)
	{
	    if (TEMP.files[busy_in].write_stats.busy <
		TEMP.files[i].write_stats.busy)
		busy_in = i;
	    if (TEMP.files[busy_in].fileinfo.stats.busy <
		TEMP.files[i].fileinfo.stats.busy)
		busy_out = i;
	}
	PRINT_TIME6(TEMP.files[busy_in].write_stats.busy);
	fprintf(stderr, "  ");
	print_percent(stderr, TEMP.files[busy_in].write_stats.busy,
			      TEMP.files[busy_in].write_stats.finish_last -
			      TEMP.files[busy_in].write_stats.start_first);
	fprintf(stderr, "    ");
	PRINT_TIME6(TEMP.files[busy_out].fileinfo.stats.busy);
	fprintf(stderr, "  ");
	print_percent(stderr, TEMP.files[busy_out].fileinfo.stats.busy,
			      TEMP.files[busy_out].fileinfo.stats.finish_last -
			      TEMP.files[busy_out].fileinfo.stats.start_first);
    }
    fprintf(stderr, 
	   "\n%s   User   Sys Busy    User   Sys Busy    User   Sys Busy%s\n",
	    "Sproc", STATS.wizard ? "  Vswch Iswch"
#if defined(IDLE_TIME)
			" InIdle"
#endif
			 : ""
	   );
	
    ordaio_getrusage(&stat[sort->n_sprocs + 1].total_usage);

    sum = stat;
    for (i = 0; i <= (sort->single_process ? 0 : sort->n_sprocs+1); i++, stat++)
    {
	if (sort->single_process)
	    fprintf(stderr, "sort ");
	else if (i == 0)
	    fprintf(stderr, "main ");
	else if (i == sort->n_sprocs + 1)
	{
	    fprintf(stderr, " io  ");
	    stat->in_elapsed = TIMES.list_done;
	    stat->total_elapsed = TIMES.write_done;
	}
	else
	    fprintf(stderr, "%3d  ", i);
	print_proc_stats(sort, stat);

	if (stat == sum)
	    continue;

	sum->in_usage.ru_utime.tv_sec += stat->in_usage.ru_utime.tv_sec;
	sum->in_usage.ru_utime.tv_usec += stat->in_usage.ru_utime.tv_usec;
	sum->in_usage.ru_stime.tv_sec += stat->in_usage.ru_stime.tv_sec;
	sum->in_usage.ru_stime.tv_usec += stat->in_usage.ru_stime.tv_usec;
	sum->total_usage.ru_utime.tv_sec += stat->total_usage.ru_utime.tv_sec;
	sum->total_usage.ru_utime.tv_usec += stat->total_usage.ru_utime.tv_usec;
	sum->total_usage.ru_stime.tv_sec += stat->total_usage.ru_stime.tv_sec;
	sum->total_usage.ru_stime.tv_usec += stat->total_usage.ru_stime.tv_usec;
	if (sum->in_elapsed < stat->in_elapsed)
	    sum->in_elapsed = stat->in_elapsed;
	if (sum->total_elapsed < stat->total_elapsed)
	    sum->total_elapsed = stat->total_elapsed;
	if (sum->total_usage.ru_maxrss < stat->total_usage.ru_maxrss)
	    sum->total_usage.ru_maxrss = stat->total_usage.ru_maxrss;

	sum->total_usage.ru_nvcsw += stat->total_usage.ru_nvcsw;
	sum->total_usage.ru_nivcsw += stat->total_usage.ru_nivcsw;
	sum->total_usage.ru_majflt += stat->total_usage.ru_majflt;
	sum->total_usage.ru_minflt += stat->total_usage.ru_minflt;
	sum->total_usage.ru_inblock += stat->total_usage.ru_inblock;
	sum->total_usage.ru_oublock += stat->total_usage.ru_oublock;
#if defined(IDLE_TIME)
	sum->in_idle += stat->in_idle;
#endif
    }
    if (!sort->single_process)
    {
	fprintf(stderr, "All  ");
	print_proc_stats(sort, sum);
    }

    fprintf(stderr, " Rssmax Majflt Minflt\n%7d %6d %6d\n",
		    sum->total_usage.ru_maxrss,
		    sum->total_usage.ru_majflt, sum->total_usage.ru_minflt);

    if (STATS.wizard && STATS.n_parts != 0)
    {
	t = TICKS_TO_MS(STATS.sort_partition);
	fprintf(stderr, "parts: %d, recs %d, avg %d ms, genl: %d*%d+%d %d ms;"
			"unll: %d; mgl %d mgo %d skp %d\n",
		STATS.n_parts,
		sort->n_recs_per_part,
		t / STATS.n_parts,
		sort->n_sub_lists, sort->sub_items, sort->rem_items,
		TICKS_TO_MS(STATS.genline_time) / STATS.n_parts,
		STATS.unused_item_lines / STATS.n_parts,
		STATS.merge_lines, STATS.merge_outs, STATS.merge_skipped);

	fprintf(stderr,
		"run data %dK; recodes: %d, eov: %d; i/o block in %d, out %d\n",
		sort->in_run_data / 1024,
		TIMES.hard_recodes, TIMES.soft_recodes,
		sum->total_usage.ru_inblock,
		sum->total_usage.ru_oublock);
	if (sort->radix)
	{
	    fprintf(stderr,
		    "radix: deletes %d sorts %d item moves %d\n",
		    STATS.radix_deletes,
		    STATS.backwards_calls,
		    STATS.backwards_moves / sort->item_size);
#if defined(DEBUG1)	/* excluded from optimized - accumulation is too slow */
	    hist_print(STATS.backwards_histogram, "backwards sorts (#recs)",
						  stderr);
	    hist_print(STATS.radixcount_histogram, "radix counts (depth)",
						  stderr);
#endif
	}
	fprintf(stderr, "ss");
	PRINT_TIME6(STATS.serial_scan);
	print_percent(stderr, STATS.serial_scan, TIMES.list_done);
	if (sort->hash_table_size != 0)
	{
	    fprintf(stderr, "\thp");
	    PRINT_TIME6(STATS.hash_partition);
	    print_percent(stderr, STATS.hash_partition, TIMES.list_done);
	    if (STATS.hash_replaces)
		fprintf(stderr, "hovflo: %d, ", STATS.hash_replaces);
	    if (STATS.hash_deletes)
		fprintf(stderr, "hdeletes: %d ", STATS.hash_deletes);
	    fprintf(stderr, "hskip: %d", STATS.hash_stopsearches);

	}
	if (sort->touch_memory == FlagTrue)
	{
	    fprintf(stderr, "\ttch ");
	    PRINT_TIME4(TIMES.touch_cpu);
	}
	if (SortpartLoss)
	{
	    fprintf(stderr, "\tsp loss ");
	    PRINT_TIME4(SortpartLoss / 10);
	}
	if (SprocDelay)
	{
	    fprintf(stderr, "\tsp delay ");
	    PRINT_TIME4(SprocDelay);
	}
	fprintf(stderr, "\nsp");
	PRINT_TIME6(STATS.sort_partition);
	print_percent(stderr, STATS.sort_partition, TIMES.list_done);
	if (sort->two_pass)
	{
	    fprintf(stderr, "\tms");
	    PRINT_TIME6(STATS.serial_in_merge);
	    print_percent(stderr, STATS.serial_in_merge, TIMES.list_done);
	    fprintf(stderr, "\tmp");
	    PRINT_TIME6(STATS.parallel_in_merge);
	    print_percent(stderr, STATS.parallel_in_merge, TIMES.list_done);
	    if (sort->method == SortMethPointer)
	    {
		fprintf(stderr, "\tcp");
		PRINT_TIME6(STATS.input_copy);
		print_percent(stderr, STATS.input_copy, TIMES.list_done);
	    }
	    fprintf(stderr, "\nbp");
	    PRINT_TIME6(STATS.build_tbufs);
	    print_percent(stderr, STATS.build_tbufs, output_time);
	}
	fprintf(stderr, "\tms");
	PRINT_TIME6(STATS.serial_out_merge);
	print_percent(stderr, STATS.serial_out_merge, output_time);
	fprintf(stderr, "\tmp");
	PRINT_TIME6(STATS.parallel_out_merge);
	print_percent(stderr, STATS.parallel_out_merge, output_time);
	if (sort->method == SortMethPointer)
	{
	    fprintf(stderr, "\tcp");
	    PRINT_TIME6(STATS.output_copy);
	    print_percent(stderr, STATS.output_copy, output_time);
	}
	putc('\n', stderr);
    }

    /*
     * Display the per-file statistics
     */
    fprintf(stderr, "File Name\t\tI/O Mode Busy   Wait MB/sec  Bytes  Xfers\n");
    if (STATS.wizard && sort->touch_memory == FlagTrue)
    {
	/* fill dummy filestats structure with touching statistics and
	 * print it out. */

	/* "Wait" is elapsed time */
	t_file.stats.wait = TIMES.touch_done;

	/* touch cpu use divided by elapsed touch time is "Busy" */
	t_file.stats.finish_last = TIMES.touch_done;
	t_file.stats.start_first = 0;
	t_file.stats.busy = TIMES.touch_cpu;	

	/* "Bytes" is the amount of memory touched */
	/* "MB/sec" is the speed of memory touching */
	t_file.stats.bytes = sort->end_out_buf - sort->begin_in_run;

	/* "Xfers" is the number of memory regions */
	t_file.stats.ios =	
	    ROUND_UP_DIV(t_file.stats.bytes, SYS.max_region_size + sizeof(unsigned));
	t_file.name = "Memory Touching";
	t_file.io_mode = IOMODE_UNSPECIFIED;
	print_file_stats(&t_file.stats, &t_file, FALSE, NULL);
    }
    fprintf(stderr, "Input Reads\n");
    memset(&t_file.stats, 0, sizeof(t_file.stats));
    t_file.stats.start_first = ~0;
    t_file.stats.finish_last = 0;
    for (i = 0; i < IN.n_files; i++)
	print_file_stats(&IN.files[i].stats, &IN.files[i], FALSE,
			 (IN.n_files > 1) ? &t_file.stats : NULL);
    if (IN.n_files > 1)
    {
	t_file.name = NULL;
	t_file.io_mode = IOMODE_UNSPECIFIED;
	print_file_stats(&t_file.stats, &t_file, FALSE, NULL);
    }

    if (sort->two_pass)
    {
	fprintf(stderr, "Temporary Writes\n");
	memset(&t_file.stats, 0, sizeof(t_file.stats));
	t_file.stats.start_first = ~0;
	t_file.stats.finish_last = 0;
	t_file.io_mode = IOMODE_DIRECT;
	for (i = 0; i < TEMP.n_files; i++)
	    print_file_stats(&TEMP.files[i].write_stats,
			     &TEMP.files[i].fileinfo,
			     TEMP.files[i].rm_on_close,
			     (TEMP.n_files > 1) ? &t_file.stats : NULL);

	if (TEMP.n_files > 1)
	{
	    t_file.name = NULL;
	    print_file_stats(&t_file.stats, &t_file, FALSE, NULL);
	    memset(&t_file.stats, 0, sizeof(t_file.stats));
	    t_file.stats.start_first = ~0;
	    t_file.stats.finish_last = 0;
	}
	fprintf(stderr, "Temporary Reads\n");
	for (i = 0; i < TEMP.n_files; i++)
	{
	    TEMP.files[i].fileinfo.stats.ios = TEMP.files[i].io_issued;
	    TEMP.files[i].fileinfo.stats.bytes = TEMP.files[i].write_offset;
	    print_file_stats(&TEMP.files[i].fileinfo.stats,
			     &TEMP.files[i].fileinfo,
			     TEMP.files[i].rm_on_close,
			     (TEMP.n_files > 1) ? &t_file.stats : NULL);
	}
	if (TEMP.n_files > 1)
	    print_file_stats(&t_file.stats, &t_file, FALSE, NULL);
    }

    fprintf(stderr, "Output Writes\n");
    print_file_stats(&OUT.file->stats, OUT.file, FALSE, NULL);

    if (sort->hash_table_size)
    {
	if (STATS.hash_details && !sort->two_pass)   /* XXX unavail for >1run */
	{
	    fprintf(stderr, "Partition #  Empty Slots  Used Slots  Average Chain Length\n");
	    empty = used = avg = 0.0;
	    for (i = 0, part = sort->in_run[0].partitions; i < sort->in_run[0].n_parts; i++, part++)
	    {
		fprintf(stderr, "%11d  %11d  %10d  %8d.%04d\n", i,
			part->hash_empty, part->hash_used,
			part->hash_avg_length / 1000,
			part->hash_avg_length % 1000);
		empty += part->hash_empty;
		used += part->hash_used;
		avg += part->hash_avg_length;
	    }
	    avg /= sort->in_run[0].n_parts;
	    fprintf(stderr, "Overall Avg  %11d  %10d  %8d.%04d\n",
		    (int) empty / MERGE.width, (int) used / sort->in_run[0].n_parts,
		    (int) avg / 1000, (int) avg % 1000);
	}
    }
}

int main(int argc, char *argv[])
{
    unsigned		i;
    extern char		Merge_id[];
    sort_t		*sort;

#if defined(DEBUG_MALLOC)
    mallopt(M_DEBUG, 1);
    mallopt(M_FREEHD, 1);
    mallopt(M_CLRONFREE, 0xdb);
#endif

    Ordinal_Title = "Nsort";
    Ordinal_Cleanup = nsort_cleanup;
    sort = sort_alloc();

    read_defaults(sort);

    /* get arguments
     */
    for (i = 1; i < argc; i++)
    {
	if (argv[1][0] != '-' || argv[1][1] == '\0')
	    another_infile(sort, argv[1]);
	else if (nsort_statement(argv[1]))
	    parse_statement(sort, argv[1], (char *) NULL);
	else switch (argv[1][1])
	{
	  case '8': Merge12 = TRUE; break;
	  case 'C': sort->n_recs = (i8) get_number(argv[1] + 2);
		    CHECK_POS(sort->n_recs); break;
#if defined(DEMO_SUPPORT)
	  case 'D': sort->recode = (recode_t) recode_demo; CHECK_END; break;
#endif
	  case 'P': IN_RUN_PARTS = atoi(argv[1] + 2); break;
	  case 'o': CHECK_END; if (i == argc - 1) usage(argv[1]);
		    assign_outfile(sort, argv[2]);
		    i++; argv++;
		    break;
	  case 'O': sort->old_recode = 1; break;
	  case 't': Print_task = TRUE; CHECK_END; break;
	  case 'T': Print_task = (argv[1][2] == '\0') ? 2 : 3; break;
	  case 'u': COPY.target_size = atoi(argv[1] + 2) * 512;
		    CHECK_POS(COPY.target_size); break;
	  case 'v': sort->print_definition = TRUE; break;
	  case 'x': sort->test = 1; break;
	  default: usage(argv[1]);
	}

	argv++;
    }

    if (prepare_sort_def(sort))
	exit(1);

    /*
    ** Initialize for function pointers appropriate for this kind of sort
    */
    sort->preparer = prepare_shared;
    sort->worker_main = (taskmain_t) sortproc_main;
    sort->reader = io_input;
    sort->writer = io_output;

    if (OUT.file->copies_write)
	sort->write_output = OUT.file->sync ? ordaio_fsync : ordaio_nop;
    else
	sort->write_output =
	    (OUT.file->io_mode == IOMODE_SERIAL ? ord_write : ordaio_write);
    sort->get_error = aio_error;
    sort->get_return = aio_return;
    sort->final_write = nsort_final_write;

    do_sort(sort); 

    if (STATS.usages)
	print_stats(sort);

    sort_free(sort);

    return (0);
}
