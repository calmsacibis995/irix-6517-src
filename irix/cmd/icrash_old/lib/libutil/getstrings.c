#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/dump.h>
#include <syslog.h>
#include <stdio.h>

void get_uname(char *uname, dump_hdr_t *dump_hdr, int len)
{
	strncpy(uname, dump_hdr->dmp_uname, len);
}


void get_panicstr(char *panic_mesg, dump_hdr_t *dump_hdr, int len)
{
	strncpy(panic_mesg, dump_hdr->dmp_panic_str, len);
}


/*
 * Largely taken from the kernel's printputbuf function.
 */
void log_putbuf(dump_hdr_t *dump_hdr, int syslogit, FILE *fp)
{
	unsigned int pbi;
	unsigned int cc;
	unsigned int lim;
	char putbuf[DUMP_PUTBUF_LEN];
	unsigned int index = 0;
	unsigned int line = 0;

	if (dump_hdr->dmp_putbufsz <= 0) {
		if (syslogit)
			syslog(LOG_ERR, "log_putbuf: putbufsz (%d) <= 0",
						dump_hdr->dmp_putbufsz);
		else
			fprintf(fp, "log_putbuf: putbufsz (%d) <= 0",
						dump_hdr->dmp_putbufsz);
		return;
	}

	pbi = dump_hdr->dmp_putbufndx % dump_hdr->dmp_putbufsz;
	lim = dump_hdr->dmp_putbufsz;

	while (1) {

		if (index >= DUMP_PUTBUF_LEN)
			break;
		if ((pbi < lim) && (pbi < DUMP_PUTBUF_LEN)) {
			if (cc = dump_hdr->dmp_putbuf[pbi++])
				if (pbi < DUMP_PUTBUF_LEN) {
					putbuf[index++] = cc;
			}
		} else {
			if (lim == dump_hdr->dmp_putbufndx %
						dump_hdr->dmp_putbufsz) {
				break;
			} else {
				lim = dump_hdr->dmp_putbufndx %
						dump_hdr->dmp_putbufsz;
				pbi = 0;
			}
		}
	}

	lim = index;
	putbuf[lim] = '\0';
	index = 0;

	for (pbi = 0; pbi < lim; pbi++) {
		if (putbuf[pbi] == '\n') {
			putbuf[pbi] = '\0';
			if (syslogit)
				syslog(LOG_CRIT, "pb %d: %s", line++,
							&(putbuf[index]));
			else
				fprintf(fp, "    pb %d: %s\n", line++, &(putbuf[index]));
			index = pbi + 1;
		}
	}

	if (index != pbi) {
		if (syslogit)
			syslog(LOG_CRIT, "pb %d: %s", line++, &(putbuf[index]));
		else
			fprintf(fp, "    pb %d: %s\n", line++, &(putbuf[index]));
	}
}

/*
** NOTE:this function formerly in uncompvm.c
**/
void expand_header(dump_hdr_t *dump_hdr, FILE *fp)
{
#define MAXLEN	256
    uint memsize_mb;

    char string[MAXLEN];

    fprintf(fp, "\n                Dump Header Information\n");
    fprintf(fp, "-------------------------------------------------------\n");

    get_uname(string, dump_hdr, MAXLEN);
    fprintf(fp, "  uname:        %s\n", string);

    memsize_mb = dump_hdr->dmp_physmem_sz;
    /* Round to the nearest two MB */
    memsize_mb = ((memsize_mb + 1) & ~(1LL));
    fprintf(fp, "  physical mem: %d megabytes\n", memsize_mb);
    if (dump_hdr->dmp_version >= 1)
	fprintf(fp, "  phys start:   0x%x \n", dump_hdr->dmp_physmem_start);
    fprintf(fp, "  page size:    %d bytes\n", dump_hdr->dmp_pg_sz);
    fprintf(fp, "  dump version: %d\n", dump_hdr->dmp_version);
    fprintf(fp, "  dump size:    %lld k\n", (long long)dump_hdr->dmp_pages *
					dump_hdr->dmp_pg_sz / 1024LL);
    fprintf(fp, "  crash time:   %s", ctime(&(dump_hdr->dmp_crash_time)));

    get_panicstr(string, dump_hdr, MAXLEN);
    fprintf(fp, "  panic string: %s\n", string);

    fprintf(fp, "  kernel putbuf:\n");

    log_putbuf(dump_hdr, 0, fp);
    fprintf(fp, "\n");

}
