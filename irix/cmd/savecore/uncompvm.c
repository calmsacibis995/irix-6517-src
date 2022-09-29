#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dump.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <limits.h>
#include <getopt.h>
#include <ctype.h>

FILE *open_infile(char *, struct stat *);
void compr_flush(FILE *);
void expand_header(dump_hdr_t *, FILE *);
char *decode_type(int);
int get_dir_ent(FILE *, long long *, int *, int *, unsigned long long, int);
int uncompress_block(unsigned char *, unsigned char *, int, long, long *);
void dump_stats(int);

int verbose = 0;
int dump_blocksize;

typedef struct pageinfo_s {
    int			blkno;
    int			nasid;
    int			pfn;
    int			flags;
} pageinfo_t;

pageinfo_t *pagelist;
int pagecount;
int counts[DUMP_TYPEMASK >> DUMP_TYPESHIFT];
int blk_cnt = 0;

void main(int argc, char *argv[])
{
    FILE *infile = 0, *outfile = 0;
    dump_hdr_t dump_hdr;
    unsigned char *uncompr_block, *compr_block;
    char name[PATH_MAX], outname[PATH_MAX];
    int len, read_len;
    long long addr;
    long long memsize;
    unsigned long long mem_start;
    int flags;
    long new_size;
    int i, c;
    int header_mode = 0;
    int error = 0;
    int debug = 0;
    long long maxsize = LONGLONG_MAX;
    uint64_t	block_number  = 0;
    int		print_block   = 0;
    int		hub_reg_count = 0;
    int		hub_dm_count  = 0;
    int		output_file   = 1;
    int		rc;

    while ((c = getopt(argc, argv, "b:fhvds:")) != -1) {
	switch (c) {
	    case 'b':
		output_file = 0;
		if ( !(strcmp( optarg, "check" )) ) {
			verbose = 0;
			break;
		}
		if ( !(strcmp( optarg, "list" )) ) {
			verbose = 1;
			break;
		}
		for ( i=0; i< strlen(optarg); i++ ) {
			if ( isdigit((int)optarg[i]) ) {
				continue;
			}
			error++;
			break;
		}
		if ( error ) {
			break;
		}
		print_block  = 1;
		block_number = strtoul(optarg, 0, 0);
		break;
	    case 'h':
		header_mode = 1;
		output_file = 0;
		break;
	    case 'v':
		verbose = 1;
		break;
	    case 'd':
		debug = 1;
		break;
	    case 's':		/* maximum size of dump in MB */
		maxsize = strtoul(optarg, 0, 0) << 20;
		break;
	    default:
		error++;
	}
    }

    if (((argc - optind) < 1) || error ||
			(header_mode && ((argc - optind)  > 1))) {
	fprintf(stderr,
		"Usage: %s [-hvd] [-b block_number|list|check] [-s max_megabytes] "
		"infile [outfile]\n", argv[0]);
	exit(1);
    }

    strcpy(name, argv[optind]);

    if ((infile = fopen(name, "r")) == NULL) {
	fprintf(stderr, "Couldn't open input file\n");
	exit(1);
    }

    if ((argc - optind) > 1) {
	strcpy(outname, argv[optind+1]);
    } else {
	strcpy(outname, name);

	for (i = strlen(outname) - 1; ((outname[i] != '.') && i); i--)
	    ;

	if (!strcmp(outname + i, ".comp"))
	    *(outname + i) = '\0';
	else
	    strcat(outname, ".uncomp");
    }

    if ( output_file ) 
	if ((outfile = fopen(outname, "w")) == NULL) {
	    fprintf(stderr,
		    "%s: couldn't open output file %s\n", argv[0], outname);
	    exit(1);
	}

    /* Get the header */
    if (fread(&dump_hdr, sizeof(dump_hdr_t), 1, infile) != 1) {
	fprintf(stderr, "%s: Read error fetching header!\n", argv[0]);
	exit(1);
    }

    if (dump_hdr.dmp_magic != DUMP_MAGIC) {
	fprintf(stderr, "%s: %s is not an IRIX compressed dump file!\n",
			argv[0], name);
	exit(1);
    } else if (header_mode) {
	expand_header(&dump_hdr, stdout);
	if (output_file) {
	    printf("Checking blocks\n");
	}
    } else {
	if (output_file) {
	    printf("%s: Expanding dump of '%s'\n", argv[0], dump_hdr.dmp_uname);
	}
    }

    dump_blocksize = dump_hdr.dmp_pg_sz;
    uncompr_block = malloc(dump_blocksize);
    compr_block = malloc(dump_blocksize);

    if( (uncompr_block == NULL) || (compr_block == NULL) ) {
	fprintf(stderr, "%s: out of memory\n",argv[0]);
	exit(1);
    }

    /* Compute memory size in bytes. */
    memsize = dump_hdr.dmp_physmem_sz * 0x100000LL;

    /* Round the memory size to the nearest four megabytes. */
    memsize = ((memsize + 0x1fffffLL) & ~0x1fffffLL);

    if (dump_hdr.dmp_version >= 1)
	mem_start = dump_hdr.dmp_physmem_start;
    else
	mem_start = 0;

    pagecount = memsize / dump_blocksize;

    if (debug) {
	printf("memsize:    0x%llx\n", memsize);
	printf("mem_start:  0x%llx\n", mem_start);
	printf("dmp_pages:  0x%x\n", dump_hdr.dmp_pages);
	printf("pagecount:  0x%x\n", pagecount);

	if (!(pagelist = calloc(sizeof (pageinfo_t), pagecount))) {
	    fprintf(stderr, "%s: Couldn't malloc page array.  Exiting.\n",
		argv[0]);
	}
    }

    /* Seek to the start of the first dumped block. */
    fseek64(infile, dump_hdr.dmp_hdr_sz, SEEK_SET);

    if (get_dir_ent(infile, &addr, &len, &flags, mem_start, blk_cnt) < 0) {
	fprintf(stderr, "Dump appears empty.\n");
	exit(1);
    }

    while (!(flags & DUMP_END)) {

	/*
	 *   skip the pieces that contain the hub register and
	 *   directory memory information: since these are 
	 *   dumped with their real addresses: the resulting
	 *   uncompressed output file would be far too large.
	 *   keep a count of the number of entries that were
	 *   skipped for each type. The resulting number is 
	 *   printed at the end of the execution for uncompvm().
	 */

	if ( flags & ( DUMP_DIRECTORY | DUMP_REGISTERS ) ) { 

		if ( flags & DUMP_DIRECTORY ) {
			hub_dm_count++;
		} else if ( flags & DUMP_REGISTERS ) {
			hub_reg_count++;
		}

		if ( !print_block ) {
			if (verbose) {
				if ( flags & DUMP_DIRECTORY ) {
		    			printf("\tdirectory memory block %d - skipped.\n",
						hub_dm_count);
				} else if ( flags & DUMP_REGISTERS ) {
		    			printf("\thub register block %d - skipped.\n",
						hub_reg_count);
				}
			}

			if ( rc = fseek64(infile, len, SEEK_CUR) ) {
				fprintf(stderr, "%s: seek error: %d\n", argv[0], rc);
				break;
			}
			blk_cnt++;
			if (get_dir_ent(infile, &addr, &len, &flags, mem_start, blk_cnt) < 0) {
				break;
			}
			continue;
		}
	}

	if (debug) {
	    pageinfo_t 	       *pi;

	    if (blk_cnt >= pagecount) {
		printf("Dump size exceeds memory size!\n");
		exit(1);
	    }

	    pi = &pagelist[blk_cnt];

	    pi->blkno = blk_cnt;
	    pi->nasid = addr >> 32 & 0xff;
	    pi->pfn = (addr & 0xffffffffULL) / dump_blocksize;
	    pi->flags = flags;

	    printf("%06d: Node %03d Pg %06d Addr 0x%010llx "
		   "Flg 0x%02x Len 0x%04x %s\n",
		   blk_cnt, pi->nasid, pi->pfn, addr, flags, len,
		   decode_type(flags));

	    counts[(flags & DUMP_TYPEMASK) >> DUMP_TYPESHIFT]++;
	}

	read_len = fread(compr_block, 1, len, infile);

	if (read_len < len) {
	    fprintf(stderr, "%s: read error\n", argv[0]);
	    fprintf(stderr, "Warning: uncompressed file may be incomplete.\n");
	    if (outfile)
		fclose(outfile);
	    exit(1);
	}

	if ( print_block && ( block_number == blk_cnt ) ) {
		printf("\nstart of block %d:\n\taddress: 0x%llx\n\tflags: 0x%x"
			"\n\tsize: %d\n\n%05x ", blk_cnt, addr, flags, len, 0);
		for ( i=0; i<len; i++ ) {
			printf(" %02x", compr_block[i]);
			if ( i+1 == len ) {
				break;
			} else if ( (i%16) == 15 ) {
				printf(" \n%05x ", i+1);
			} else if ( (i%8) == 7 ) {
				printf("   ");
			}
		}
		printf("\n\nend of block %d:\n\taddress: 0x%llx\n\tflags: 0x%x"
			"\n\tsize: %d\n", blk_cnt, addr, flags, len);
		if ((flags & DUMP_COMPMASK) == DUMP_RLE) {
			uncompress_block(compr_block, uncompr_block,
				 flags, len, &new_size);
			printf("\n\nstart of block %d:\n\taddress: 0x%llx\n\tflags: 0x%x"
				"\n\tsize: %d\n\tuncompressed size: %d\n\n%05x ",
				blk_cnt, addr, flags, len, new_size, 0);
			for ( i=0; i<new_size; i++ ) {
				printf(" %02x", uncompr_block[i]);
				if ( i+1 == new_size ) {
					break;
				} else if ( (i%16) == 15 ) {
					printf(" \n%05x ", i+1);
				} else if ( (i%8) == 7 ) {
					printf("   ");
				}
			}
			printf("\n\nend of block %d:\n\taddress: 0x%llx\n\tflags: 0x%x"
				"\n\tsize: %d\n\tuncompressed size: %d\n\n",
				blk_cnt, addr, flags, len, new_size);
		}
	}

	if ( flags & ( DUMP_DIRECTORY | DUMP_REGISTERS ) ) { 
		blk_cnt++;
		if (get_dir_ent(infile, &addr, &len, &flags, mem_start, blk_cnt) < 0) {
			break;
		}
		continue;
	}

	if (addr < maxsize) {
	    /* seek to the new block location */

#ifdef DEBUG_DUMP
	    printf("Seeking to 0x%x - ", (long)(addr & ~0xffll));
#endif

	    if (outfile && fseek64(outfile, (addr & ~0xffll), SEEK_SET)) {
		fprintf(stderr, "%s: seek error\n", argv[0]);
		exit(1);
	    }

#ifdef DEBUG_DUMP
	    printf("%s\n", decode_type(flags));
#endif
			
	    if ((flags & DUMP_COMPMASK) == DUMP_RAW) {

		if (len != dump_blocksize)
		    fprintf(stderr,
			    "%s: 'RAW' blocksize discrepancy: actual %d, expected %d.\n",
			    argv[0], len, dump_blocksize);

		if ( verbose) {
		    	printf("\t%d -> %d RAW\n", len, dump_blocksize);
			if ( outfile ) {
				printf("\twriting 0x%x bytes\n", dump_blocksize);
			}
		}

		if (outfile &&
		    fwrite(compr_block, 1, dump_blocksize, outfile) != dump_blocksize) {
		    fprintf(stderr, "%s: Write error!\n", argv[0]);
		    exit(1);
		}

	    } else {
		uncompress_block(compr_block, uncompr_block,
				 flags, len, &new_size);

		if (new_size != dump_blocksize)
		    fprintf(stderr, "%s: New size (%d) != blocksize ( %d)\n",
			    argv[0], new_size, dump_blocksize);

		if (verbose) {
			printf("\t%d -> %d COMPRESSED\n", len, dump_blocksize);
			if ( outfile ) {
				printf("\twriting 0x%x bytes\n", new_size);
			}
		}

		if (outfile &&
		    fwrite(uncompr_block, 1, new_size, outfile) != new_size) {
		    fprintf(stderr, "%s: Write error!\n", argv[0]);
		    exit (1);
		}
	    }
	}

	blk_cnt++;

	if (! debug && !(blk_cnt & 0xff) && output_file) {
	    printf(".");
	    fflush(stdout);
	}

	if (get_dir_ent(infile, &addr, &len, &flags, mem_start, blk_cnt) < 0)
	    break;
    }

    if (output_file) {
        printf("\nDump included %d pages", blk_cnt);

	if ( hub_reg_count ) {
	    printf(":\n\thub register pages: %d ( skipped )", hub_reg_count);
	}

	if ( hub_dm_count ) {
	   printf(":\n\tdirectory memory pages: %d ( skipped )", hub_dm_count);
	}

	printf(".\n");
    }

    if (debug)
	dump_stats(dump_hdr.dmp_pages);

    if (outfile)
	fclose(outfile);
}



int
uncompress_block(
	unsigned char *comp_buf,
	unsigned char *uncomp_buf,
	int flags,
	long blk_size,
	long *new_size)
{
    int i;
    unsigned char value;
    unsigned char count;
    unsigned char cur_byte;
    long read_index, write_index;

    read_index = write_index = 0;

    if ((flags & DUMP_COMPMASK) != DUMP_RLE) {
	printf("Error!  Bogus compression type!\n");
	return 1;
    }

    while(read_index < blk_size) {
	cur_byte = comp_buf[read_index++];
	if (cur_byte == 0) {
	    count = comp_buf[read_index++];
	    if (count == 0) {
		uncomp_buf[write_index++] = 0;
	    } else {
		value = comp_buf[read_index++];
		    for (i = 0; i <= count; i++) {
			uncomp_buf[write_index++] = value;
		    }
	    } /* If count == 0 */
	} else { /* if cur_byte == 0 */
		uncomp_buf[write_index++] = cur_byte;
	}

	if (write_index > dump_blocksize) {
		printf("ERROR: Attempted to decompress ( %d ) beyond "
		       "block boundaries ( %d ) - file corrupted!\n",
		       write_index, dump_blocksize);
		exit(1);
	}

    } /* while read_index */

    *new_size = write_index;

    return 0;
}


#if 0
int compress_block(char *old_addr, char *new_addr, long old_length)
{
    int read_index;
    int count;
    unsigned char value;
    unsigned char cur_byte;
    int write_index;

    /* If the block should happen to "compress" to larger than the
	buffer size, allocate a larger one and change cur_buf_size */

    write_index = read_index = 0;

    while ( read_index < old_length) {
	if (read_index == 0) {
	    cur_byte = value = old_addr[read_index];
	    count = 0;
	} else {
	    if (count == 255) {
		if (write_index + 3 > dump_blocksize)
		    return dump_blocksize;
		new_addr[write_index++] = 0;
		new_addr[write_index++] = count;
		new_addr[write_index++] = value;
		value = cur_byte = old_addr[read_index];
		count = 0;
	    } else {
		if ((cur_byte = old_addr[read_index]) == value) {
		    count++;
		} else {
		    if (count > 1) {
			if (write_index + 3 > dump_blocksize)
			    return dump_blocksize;
			new_addr[write_index++] = 0;
			new_addr[write_index++] = count;
			new_addr[write_index++] = value;
		    } else if (count == 1) {
			if (value == 0) {
			    if (write_index + 4 > dump_blocksize)
				return dump_blocksize;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			} else {
			    if (write_index + 2 > dump_blocksize)
				return dump_blocksize;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			}
		    } else { /* count == 0 */
			if (value == 0) {
			    if (write_index + 2 > dump_blocksize)
				return dump_blocksize;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			} else {
			    if (write_index + 1 > dump_blocksize)
				return dump_blocksize;
			    new_addr[write_index++] = value;
			}
		    } /* if count > 1 */
		    value = cur_byte;
		    count = 0;
		} /* if byte == value */
	    } /* if count == 255 */
	} /* if read_index == 0 */
	read_index++;
    }
    if (count > 1) {
	if (write_index + 3 > dump_blocksize)
	    return dump_blocksize;
	new_addr[write_index++] = 0;
	new_addr[write_index++] = count;
	new_addr[write_index++] = value;
    } else if (count == 1) {
	if (value == 0) {
	    if (write_index + 4 > dump_blocksize)
		return dump_blocksize;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	} else {
	    if (write_index + 2 > dump_blocksize)
		return dump_blocksize;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	}
    } else { /* count == 0 */
	if (value == 0) {
	    if (write_index + 2 > dump_blocksize)
		return dump_blocksize;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	} else {
	    if (write_index + 1 > dump_blocksize)
		return dump_blocksize;
	    new_addr[write_index++] = value;
	}
    } /* if count > 1 */
    value = cur_byte;
    count = 0;

    return write_index;
}
#endif /* 0 */


int
get_dir_ent(
	FILE *infile,
	long long *addr,
	int *new_size,
	int *flags,
	unsigned long long mem_start,
	int	blk_cnt)
{
    dump_dir_ent_t entry;

    if (fread(&entry, sizeof(dump_dir_ent_t), 1, infile) != 1) {
        fprintf(stderr, "uncompvm: read error\n");
        fprintf(stderr, "Warning: uncompressed file may be incomplete.\n");
        return -1;
    }

    *flags = entry.flags;
    *addr  = entry.addr_lo - mem_start;
    *addr |= ((long long)(entry.addr_hi) << 32);
    *new_size = entry.length;

    if (verbose)
        printf("block %6d: address: 0x%llx; flags: 0x%x; size: %d\n",
               blk_cnt, *addr, *flags, *new_size);

    return 0;
}


static int cmpfn(pageinfo_t *p1, pageinfo_t *p2)
{
    return (p1->nasid != p2->nasid ?
	    p1->nasid - p2->nasid :
	    p1->pfn != p2->pfn ?
	    p1->pfn - p2->pfn :
	    p1->blkno - p2->blkno);
}

void
dump_stats(int dump_pages)
{
    int		i, j;

    qsort(pagelist, blk_cnt, sizeof (pageinfo_t), (int (*)()) cmpfn);

    printf("Duplicate pages found in this dump:\n");

    for (i = 1; i < blk_cnt; i++)
	if (pagelist[i - 1].nasid == pagelist[i].nasid &&
	    pagelist[i - 1].pfn   == pagelist[i].pfn)
	    printf("    Block %06d/%06d  NASID %03d pfn %06d %s/%s\n",
		   pagelist[i - 1].blkno,
		   pagelist[i].blkno,
		   pagelist[i].nasid, pagelist[i].pfn,
		   decode_type(pagelist[i - 1].flags),
		   decode_type(pagelist[i].flags));

    printf("Range of addresses included in this dump:\n");

    for (i = 0; i < blk_cnt; i = j) {
	j = i + 1;
	while (j < blk_cnt &&
	       pagelist[j].nasid == pagelist[j - 1].nasid &&
	       pagelist[j].pfn <= pagelist[j - 1].pfn + 1)
	    j++;
	printf("    0x%010llx - 0x%010llx (nasid %03d ",
	       pagelist[i].pfn * (long long) dump_blocksize,
	       (pagelist[j - 1].pfn + 1) * (long long) dump_blocksize - 1,
	       pagelist[i].nasid);
	if (j > i + 1)
	    printf("pfn %06d - %06d)\n",
		   pagelist[i].pfn, pagelist[j - 1].pfn);
	else
	    printf("pfn %06d)\n", pagelist[i].pfn);
    }

    for (i = 1; i < (DUMP_TYPEMASK >> DUMP_TYPESHIFT); i <<= 1) {
	printf("%3d%% (%d)\t%s\n",
	       (counts[i] * 100 / blk_cnt),
	       counts[i], decode_type(i << DUMP_TYPESHIFT));
    }

    printf("\n%d%% compression.\n\n", 100 - dump_pages * 100
	   / pagecount);
}


char *decode_type(int flags)
{
    switch(flags & DUMP_TYPEMASK) {
    case DUMP_DIRECT:
	return ("\"Direct\"");
    case DUMP_SELECTED:
	return ("\"Selected\"");
    case DUMP_INUSE:
	return ("\"In Use\"");
    default:
	return ("\"Free\"");
    }
}
