#ident  "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <termio.h>
#include <sys/invent.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * clear_dump() -- Clear out the memory dump.  
 *
 *   Do ONLY in the case that you are not on an active system, and the
 *   corefile is /dev/swap. We call this differently depending on
 *   whether we are running a report or not. For now, this will remain
 *   an undocumented feature.
 */
int
clear_dump()
{
	FILE *xfp;
	long long zero = 0LL;

	if (!strcmp(corefile, "/dev/swap")) {
		if (xfp = fopen(corefile, "w")) {
			if (lseek(fileno(xfp), 0, SEEK_SET) >= 0) {
				write(fileno(xfp), (char *)&zero, sizeof(long long));
			}
			fclose(xfp);
		}
	}
	return 1;
}

#define I_BYTE 	1
#define I_HWORD 2
#define I_WORD 	4
#define I_DWORD 8

/*
 * dump_memory()
 */
int
dump_memory(kaddr_t addr, k_uint_t ecount, int flags, FILE *ofp)
{
	int i, j, k, l, s; 
	int short_print = -1, capture_ascii = 0, printblanks;
	int base, ewidth, vaddr_mode, first_time = 1;
	k_uint_t size;
	kaddr_t a;
	unsigned char *buf;
	unsigned char *bp;
	unsigned char *cp;
	char cstr[17];

	if (DEBUG(DC_FUNCTRACE, 4)) {
		fprintf(ofp, "addr=0x%llx, ecount=%lld, flags=0x%x, file=0x%x\n", 
			addr, ecount, flags, ofp);
	}

	/* Determine the mode of addr (virtual/physical/kernelstack/etc.)
	 */
	vaddr_mode = kl_get_addr_mode(addr);
	if (KL_ERROR) {
		return(1);
	}

	/* Make sure there are no conflicts with the flags controlling
	 * the base (octal, hex, decimal) and element size (byte, half 
	 * word, word and double word). Set base and ewidth to the highest
	 * priority flag that is set.
	 */
	if (flags & C_HEX) {
		base = C_HEX;
	}
	else if (flags & C_DECIMAL) {
		base = C_DECIMAL;
	}
	else if (flags & C_OCTAL) {
		base = C_OCTAL;
	}
	else {
		base = C_HEX;
	}

	/* If none of the width flags are set, use the pointer size to
	 * determine what the width should be.
	 */
	if (!(flags & C_DWORD) && !(flags & C_WORD) && 
				!(flags & C_HWORD) && !(flags & C_BYTE)) {
		if (PTRSZ64) {
			flags |= C_DWORD;
		}
		else {
			flags |= C_WORD;
		}
	}
	
	if (flags & C_DWORD) {
		if (addr % 8) {
			if (!(addr % 4)) {
				ewidth = I_WORD;
				size = ecount * 4;
			}
			else if (!(addr % 2)) {
				ewidth = I_HWORD;
				size = ecount * 2;
			}
			else {
				ewidth = I_BYTE;
				size = ecount;
			}
		}
		else {
			ewidth = I_DWORD;
			size = ecount * 8;
		}
	}
	else if (flags & C_WORD) {
		if (addr % 4) {
			if (!(addr % 2)) {
				ewidth = I_HWORD;
				size = ecount * 2;
			}
			else {
				ewidth = I_BYTE;
				size = ecount;
			}
		}
		else {
			ewidth = I_WORD;
			size = ecount * 4;
		}
	}
	else if (flags & C_HWORD) {
		if (addr %2) {
			ewidth = I_BYTE;
			size = ecount;
		}
		else {
			ewidth = I_HWORD;
			size = ecount * 2;
		}
	}
	else if (flags & C_BYTE) {
		ewidth = I_BYTE;
		size = ecount;
	}

	/* Turn on ASCII dump if outputtting in HEX (for words and double 
	 * words only).
	 */
	if ((base == C_HEX) && ((ewidth == I_DWORD) || (ewidth == I_WORD))) {
		capture_ascii = 1;
	}

	buf = (unsigned char *)kl_alloc_block(IO_NBPC, K_TEMP);

	fprintf(ofp, "\nDumping memory starting at ");
	if (vaddr_mode == 4) {
		fprintf(ofp, "physical address : 0x%0llx\n\n", addr);
	} 
	else {
		fprintf(ofp, "address : 0x%0llx\n\n", addr);
	}

	while (size) {
		if (size > IO_NBPC) {
			s = IO_NBPC;
			size -= IO_NBPC;
		} 
		else {
			s = (int)size;
			size = 0;
		}

		a = addr;

		/*
		 * If the current block size extends beyond the end of the K0,
		 * K1, K2 memory, or the kernelstack, adjust size to end the 
		 * dump there.
		 */
		if ((vaddr_mode == 0) && ((a + s) > (K_K0BASE + K_K0SIZE))) {
			s = (int)((K_K0BASE + K_K0SIZE) - a);
			size = 0;
			short_print = 0;
		}
		else if ((vaddr_mode == 1) && ((a + s) > (K_K1BASE + K_K1SIZE))) {
			s = (int)((K_K1BASE + K_K1SIZE) - a);
			size = 0;
			short_print = 1;
		}
		else if ((vaddr_mode == 2) && ((a + s) > (K_K2BASE + K_K2SIZE))) {
			s = (int)((K_K2BASE + K_K2SIZE) - a);
			size = 0;
			short_print = 2;
		}
		else if ((vaddr_mode == 3) && ((a + s) >= K_KERNSTACK)) {
			s = (K_KERNSTACK - a);
		}

		kl_get_block(addr, s, buf, "dump");
		if (KL_ERROR) {
			kl_free_block((k_ptr_t)buf);
			return(-1);
		}

		bp = buf;
		while(bp < (buf + s)) {

			if (PTRSZ64) {
				fprintf(ofp, "%016llx: ", a);
			}
			else {
				fprintf(ofp, "%08llx: ", a);
			}

			a += 16;
			i = 0;
			k = 0;
			printblanks = 0;

			/* Set the character pointer for ASCII string to bp.
			 */
			if (capture_ascii) {
				cp = (unsigned char*)bp;
			}

			while (i < 16) {

				switch (ewidth) {
					case I_DWORD:
						if (base == C_HEX) {
							if (printblanks) {
								fprintf(ofp, "                  "); 
							}
							else {
								fprintf(ofp, "%016llx  ", *(k_int_t*)bp);
							}
						} 
						else if (base == C_DECIMAL) {
							if (printblanks) {
								fprintf(ofp, "                      "); 
							}
							else {
								fprintf(ofp, "%020lld  ", *(k_int_t*)bp);
							}
						} 
						else if (base == C_OCTAL) {
							if (printblanks) {
								fprintf(ofp, "                         "); 
							}
							else {
								fprintf(ofp, "%023llo  ", *(k_int_t*)bp);
							}
						} 
						break;

					case I_WORD:
						if (base == C_HEX) {
							if (printblanks) {
								fprintf(ofp, "          "); 
							}
							else {
								fprintf(ofp, "%08x  ", *(uint*)bp);
							}
						} 
						else if (base == C_DECIMAL) {
							if (printblanks) {
								fprintf(ofp, "             "); 
							}
							else {
								fprintf(ofp, "%011d  ", *(uint*)bp);
							}
						} 
						else if (base == C_OCTAL) {
							if (printblanks) {
								fprintf(ofp, "              "); 
							}
							else {
								fprintf(ofp, "%020o  ", *(uint*)bp);
							}
						} 
						break;

					case I_HWORD:
						if (base == C_HEX) {
							if (printblanks) {
								fprintf(ofp, "      "); 
							}
							else {
								fprintf(ofp, "%04x  ", *(unsigned short*)bp);
							}
						} 
						else if (base == C_DECIMAL) {
							if (printblanks) {
								fprintf(ofp, "        "); 
							}
							else {
								fprintf(ofp, "%06d  ", *(unsigned short*)bp);
							}
						} 
						else if (base == C_OCTAL) {
							if (printblanks) {
								fprintf(ofp, "         "); 
							}
							else {
								fprintf(ofp, "%07o  ", *(unsigned short*)bp);
							}
						} 
						break;

					case I_BYTE:
						if (base == C_HEX) {
							if (printblanks) {
								fprintf(ofp, "   "); 
							}
							else {
								fprintf(ofp, "%02x ", *(unsigned char*)bp);
							}
						} 
						else if (base == C_DECIMAL) {
							if (i == 8) {
								if (PTRSZ64) {
									fprintf(ofp, "\n                  ");
								}
								else {
									fprintf(ofp, "\n          ");
								}
							}
							if (printblanks) {
								fprintf(ofp, "     "); 
							}
							else {
								fprintf(ofp, "%04d ", *(unsigned char*)bp);
							}
						} 
						else if (base == C_OCTAL) {
							if (i == 8) {
								if (PTRSZ64) {
									fprintf(ofp, "\n                  ");
								}
								else {
									fprintf(ofp, "\n          ");
								}
							}
							if (printblanks) {
								fprintf(ofp, "     "); 
							}
							else {
								fprintf(ofp, "%04o ", *(unsigned char*)bp);
							}
						} 
						break;
				}

				/* Get an ASCII representation of the bytes being dumpped
				 */
				if (capture_ascii && !printblanks) {
					for( j = 0; j < ewidth; j++) {
						if (*cp >= 32 && *cp <= 126) {
							cstr[k] = (char)*cp;
						} 
						else {
							cstr[k] = '.';
						}
						k++;
						cp++;
					}
				}

				i += ewidth;
				bp += ewidth;
				if ((i <= 16) && ((uint)bp == ((uint)buf + s))) {
					printblanks = 1;
				}
			}


			/* Dump the ASCII string 
			 */
			if (capture_ascii) {
				cstr[k] = 0;
				fprintf(ofp, "|%s\n", cstr);
			}
			else {
				fprintf(ofp, "\n");
			}

		}
		if (size) {
			addr += IO_NBPC;
		}
	}
	if (short_print != -1) {
		switch(short_print) {
			case 0 : fprintf(ofp, "\nEnd of K0 segment!\n");
					 break;

			case 1 : fprintf(ofp, "\nEnd of K1 segment!\n");
					 break;

			case 2 : fprintf(ofp, "\nEnd of K2 segment!\n");
					 break;
		}
	}
	fprintf(ofp, "\n");
	kl_free_block((k_ptr_t)buf);
	return(0);
}

/*
 * print_putbuf() -- Print out the putbuf to a specific file descriptor.
 */
void
print_putbuf(FILE *ofp)
{
	int i, j, k, l, pbufsize, psize;
	char *np, *pbuf;
	kaddr_t pbufp;
	struct syment *putbuf_cpusz;
	
	if (!ACTIVE && (K_DUMP_HDR->dmp_version > 2)) {
		pbufsize = K_DUMP_HDR->dmp_putbuf_size;
	} 
	else {
		if (!(putbuf_cpusz = kl_get_sym("putbuf_cpusz", K_TEMP))) {
			pbufsize = 1000;
		} 
		else {
			kl_get_block(putbuf_cpusz->n_value, 4, &psize, "putbuf_cpusz");
			if (KL_ERROR) {
				pbufsize = K_MAXCPUS * 0x800;
			} 
			else {
				pbufsize = K_MAXCPUS * psize;
			}
		}
	}

	np = kl_alloc_block(pbufsize + 500, K_TEMP);
	pbuf = (char *)kl_alloc_block(pbufsize, K_TEMP);
	if(!np || !pbuf || !pbufsize) {
		return;
	}
	kl_get_block(K_PUTBUF, pbufsize, pbuf, "putbuf");
	np[0] = '\t';
	psize = strlen(pbuf);
	for (i = 0, j = 1, l = 1; i < psize; i++) {
		if (pbuf[i] == '\0') {
			/* skip all the NULLs */
		} 
		else if (pbuf[i] == '\n') {
			np[j++] = pbuf[i];
			for (k = 0; k < 4; k++) {
				np[j++] = ' ';
			}
			l = 4;
		} 
		else {
			np[j++] = pbuf[i];
			l++;
		}
	}
	np[j] = '\0';
	fprintf(ofp, "%s", np);
	kl_free_block((k_ptr_t)np);
	kl_free_block((k_ptr_t)pbuf);
	return;
}

#define MAXRN 20 /* maximum length of the minor release name */
char *
get_releasename()
{
static char relname[MAXRN];
kaddr_t rnaddr;

        rnaddr =kl_sym_addr("uname_releasename");
	if(!rnaddr) return 0;
        kl_get_block(rnaddr, MAXRN, relname, "relname");
	return relname;
}

/*
 * print_utsname() -- Print out utsname structure (uname -a)
 */
void
print_utsname(FILE *ofp)
{
	char *relname=get_releasename();
	k_ptr_t utsnamep;

	if (!(utsnamep = kl_get_utsname(K_TEMP))) {
		fprintf(ofp, "    system name:    UNKNOWN\n");
		fprintf(ofp, "    release:        UNKNOWN (UNKNOWN)\n");
		fprintf(ofp, "    node name:      UNKNOWN\n");
		fprintf(ofp, "    version:        UNKNOWN\n");
		fprintf(ofp, "    machine name:   UNKNOWN\n");
		kl_free_block(relname);
		return;
	}
	fprintf(ofp, "    system name:    %s\n",
		CHAR(utsnamep, "utsname", "sysname"));
	fprintf(ofp, "    release:        %s (%s)\n",
		CHAR(utsnamep, "utsname", "release"), relname ? relname : "UNKNOWN");
	fprintf(ofp, "    node name:      %s\n",
		CHAR(utsnamep, "utsname", "nodename"));
	fprintf(ofp, "    version:        %s\n",
		CHAR(utsnamep, "utsname", "version"));
	fprintf(ofp, "    machine name:   %s\n",
		CHAR(utsnamep, "utsname", "machine"));
	kl_free_block(utsnamep);
}

/*
 * print_curtime() -- Print out the time of the system crash.
 */
void
print_curtime(FILE *ofp)
{
	time_t curtime;
	char *tbuf;
	
	tbuf = (char *)kl_alloc_block(100, K_TEMP);
	kl_get_block(K_TIME, 4, &curtime, "time");
	cftime(tbuf, "%h  %d %T %Y", &curtime);
	fprintf(ofp, "%s\n", tbuf);
	kl_free_block((k_ptr_t)tbuf);
}

#define INDENT_STR "    "

/*
 * helpformat() -- String format a line to within N - 3 characters, where
 *                 N is based on the winsize.  Return an allocated string.
 *                 Note that this string must be freed up when completed.
 */
char *
helpformat(char *helpstr)
{
	int indentsize = 0, index = 0, tmp = 0, col = 0;
	char *t, buf[1024];
	struct winsize w;

	/* if NULL string, return */
	if (!helpstr) {
		return ((char *)0);
	}

	/* get the window size */
	if (ioctl(fileno(stdout), TIOCGWINSZ, &w) < 0) {
		return ((char *)0);
	}

	indentsize = strlen(INDENT_STR);

	/* set up buffer plus a little extra for carriage returns, if needed */
	t = (char *)malloc(strlen(helpstr) + 256);
	bzero(t, strlen(helpstr) + 256);

	/* Skip all initial whitespace -- do the indentions by hand. */
	while (helpstr && isspace(*helpstr)) {
		helpstr++;
	}
	strcat(t, INDENT_STR);
	index = indentsize;
	col = indentsize;

	/* Keep getting words and putting them into the buffer, or put them on
	 * the next line if they don't fit.
	 */
	while (*helpstr != 0) {
		tmp = 0;
		bzero(&buf, 1024);
		while (*helpstr && !isspace(*helpstr)) {
			buf[tmp++] = *helpstr;
			if (*helpstr == '\n') {
				strcat(t, buf);
				strcat(t, INDENT_STR);
				col = indentsize;
				index += indentsize;
				tmp = 0;
			}
			helpstr++;
		}

		/* if it fits, put it on, otherwise, put in a carriage return */
		if (col + tmp > w.ws_col - 3) {
			t[index++] = '\n';
			strcat(t, INDENT_STR);
			col = indentsize;
			index += indentsize;
		}

		/* put it on, add it up */
		strcat(t, buf);
		index += tmp;
		col += tmp;

		/* put all extra whitespace in (as much as they had in) */
		while (helpstr && isspace(*helpstr)) {
			t[index++] = *helpstr;
			if (*helpstr == '\n') {
				strcat(t, INDENT_STR);
				index += indentsize;
				col = 4;
			}
			else {
				col++;
			}
			helpstr++;
		}
	}

	/* put on a final carriage return and NULL for good measure */
	t[index++] = '\n';
	t[index] = 0;

	return (t);
}

void
addinfo(__uint32_t *accum, __uint32_t *from, int n)
{
	while (n--) {
		*accum++ += *from++;
	}
}

/*
 * list_pid_entries()
 */
int
list_pid_entries(int flags, FILE *ofp)
{
	int first_time = TRUE, i, pid_cnt = 0;
	k_ptr_t psp, pep; 
	kaddr_t pid_slot, pid_entry;

	if (DEBUG(DC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "list_pid_entries: flags=0x%x, ofp=0x%x\n",
			flags, ofp); 
	}

	kl_reset_error();

	psp = kl_alloc_block(PID_SLOT_SIZE, K_TEMP);
	pep = kl_alloc_block(PID_ENTRY_SIZE, K_TEMP);

	for (i = 0; i < K_PIDTABSZ; i++) {

		pid_slot = K_PIDTAB + (i * PID_SLOT_SIZE);
		while (pid_slot) {

			kl_get_struct(pid_slot, PID_SLOT_SIZE, psp, "pid_slot");
			if (KL_ERROR) {
				if (DEBUG(DC_GLOBAL, 1)) {
					KL_ERROR |= KLE_BAD_PID_SLOT;
					kl_print_debug("list_pid_entries");
				}
				pid_slot = 0;
				continue;
			}

			/* Get the pointer to the pid_entry struct
			 */
			if (!(pid_entry = kl_kaddr(psp, "pid_slot", "ps_chain"))) {
				pid_slot = 0;
				continue;
			}

			/* Now get the pid_entry struct
			 */
			kl_get_struct(pid_entry, PID_ENTRY_SIZE, pep, "pid_entry");
			if (KL_ERROR) {
				if (DEBUG(DC_GLOBAL, 1)) {
					KL_ERROR |= KLE_BAD_PID_SLOT;
					kl_print_debug("list_pid_entries");
				}
				pid_slot = 0;
				continue;
			}

			if (first_time) {
				first_time = 0;
			}
			else if (flags & C_NEXT) {
				pid_entry_banner(ofp, BANNER|SMAJOR);
			}
			print_pid_entry(pid_entry, pep, flags, ofp);
			pid_cnt++;

			pid_slot = kl_kaddr(pep, "pid_entry", "pe_next");
		}
	}
	kl_free_block(psp);
	kl_free_block(pep);
	return (pid_cnt);
}

/*
 * node_memory_banner()
 */
void
node_memory_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "MODULE  SLOT          NODEID  NASID  MEM_SIZE  "
			"ENABLED\n");
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "================================================"
			"======\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp, "------------------------------------------------"
			"------\n");
	}
}

/*
 * print_node_memory()
 */
int
print_node_memory(FILE *ofp, int nodeid, int flags)
{
	int i, pages = 0, p;
	banks_t *bank;

	/* Verify that we have a valid nodeid
	 */
	if (K_IP == 27) {
		if ((nodeid > (K_NASID_BITMASK + 1)) || !node_memory[nodeid]) {
			return(0);
		}
	}
	else {
		if (nodeid > 0) {
			return(0);
		}
	}

	fprintf(ofp, "%6d  %-12s  %6d  %5d  %8d  %s\n",
		node_memory[nodeid]->n_module,
		node_memory[nodeid]->n_slot,
		node_memory[nodeid]->n_nodeid,
		node_memory[nodeid]->n_nasid,
		node_memory[nodeid]->n_memsize,
		(node_memory[nodeid]->n_flag & INVENT_ENABLED) ?
			   "      Y" : "      N");

	if (flags & (C_FULL|C_LIST)) {
		fprintf(ofp, "\n");
		if (node_memory[nodeid]->n_numbanks) {
			bank = (banks_t*)&node_memory[nodeid]->n_bank;
			for (i = 0; i < node_memory[nodeid]->n_numbanks; i++) {
				if (bank[i].b_size) {
					pages += print_memory_bank_info(ofp, i, &bank[i], flags);
				}
			}
		}
		else if (flags & C_LIST) {
			kaddr_t addr;

			fprintf(ofp, "\n");
			if (K_IP == 27) {
				p = K_MEM_PER_NODE / K_NBPC;
				addr = ((k_uint_t)node_memory[nodeid]->n_nasid << 
							K_NASID_SHIFT);
			}
			else {
				p = (node_memory[nodeid]->n_memsize * 0x100000) / K_NBPC;
				addr = K_RAM_OFFSET;
			}
			fprintf(ofp, "Memory from node %d found in vmcore image:\n\n", 
				node_memory[nodeid]->n_nodeid);
			pages += print_dump_page_range(ofp, addr, p, 0);
		}
		fprintf(ofp, "\n");
	}
	return(pages);
}

/*
 * print_dump_page_range()
 *
 * Print out the range of kernel addresses (K0) that are contained in
 * the vmcore image.
 */
int
print_dump_page_range(FILE *ofp, kaddr_t start_addr, int pages, int flags)
{
	int i, first_valid = 0, pages_found = 0;
	kaddr_t addr, first_addr, last_addr = 0;

	addr = start_addr;
	for (i = 0; i < pages; i++, addr += K_NBPC) {
		if (klib_page_in_dump(LIBMEM_DATA, addr)) {
			if (!first_valid) {
				first_addr = addr;
				first_valid = 1;
			}
			last_addr = addr;
			pages_found++;
			continue;
		}
		else {
			if (first_valid) {
				fprintf(ofp, "  0x%llx", first_addr|K_K0BASE);
				fprintf(ofp, "-->0x%llx\n", 
						(last_addr + K_NBPC - 1)|K_K0BASE);
				first_valid = first_addr = last_addr = 0;
			}
			else {
				continue;
			}
		}
	}

	/* If every page in range is in the dump, we will fall through
	 * to here without having printed out anything...so we have to
	 * print out the info here.
	 */
	if (first_valid) {
		fprintf(ofp, "  0x%llx", first_addr|K_K0BASE);
		fprintf(ofp, "-->0x%llx\n", 
				(last_addr + K_NBPC - 1)|K_K0BASE);
	}
	if (pages_found) {
		fprintf(ofp, "\n");
	}
	if (pages_found == 1) {
		fprintf(ofp, "  1 page found in vmcore image\n\n");
	}
	else {
		fprintf(ofp, "  %d pages found in vmcore image\n\n", pages_found);
	}
	return(pages_found);
}

/*
 * print_memory_bank_info()
 */
int
print_memory_bank_info(FILE *ofp, int j, banks_t *bank, int flags)
{
    int i, p, pages = 0;

	fprintf(ofp, "  BANK %d contains %d MB of memory\n", j, bank->b_size);

	if (flags & C_LIST) {
		kaddr_t addr, first_addr, last_addr = 0;
		p = (bank->b_size * 0x100000) / K_NBPC;
		fprintf(ofp, "\n");
		pages = print_dump_page_range(ofp, bank->b_paddr, p, flags);
		fprintf(ofp, "\n");
	}
	return(pages);
}

/*
 * list_dump_memory()
 */
int
list_dump_memory(FILE *ofp)
{
	int i, j, p, pages = 0;
	kaddr_t addr;

	if (K_IP == 27) {
		int nasid;
		kaddr_t value, nodepda;

		for (i = 0; i < K_NUMNODES; i++) {
			/* Get the address of the nodepda_s struct for this
			 * particular node. We can get the nasid from its address.
			 * Once we have the nasid, we can set the base address
			 * for our range search.
			 */
			value = (K_NODEPDAINDR + (i * K_NBPW));
			kl_get_kaddr(value, &nodepda, "nodepda_s");
			if (KL_ERROR) {
				continue;
			}
			nasid = kl_addr_to_nasid(nodepda);
			addr = ((k_uint_t)nasid << K_NASID_SHIFT);
			p = K_MEM_PER_NODE / K_NBPC;
			fprintf(ofp, "Memory from node %d found in vmcore image:\n\n", i);
			pages += print_dump_page_range(ofp, addr, p, 0);
		}
	}
	else {
		pages += print_dump_page_range(ofp, K_RAM_OFFSET, K_PHYSMEM, 0);
	}
	return(pages);
}

/*
 * list_system_memory()
 */
int
list_system_memory(FILE *ofp, int flags)
{
	int n, j, first_time = 1, pages = 0;
	banks_t *bank;

	fprintf(ofp, "\n\n");
	fprintf(ofp, "NODE MEMORY:\n\n");
	node_memory_banner(ofp, BANNER|SMAJOR);
	if (K_IP == 27) {
		for (n = 0; n < (K_NASID_BITMASK + 1); n++) {
			if (node_memory[n]) {
				if (DEBUG(DC_GLOBAL, 1) || flags & (C_FULL|C_LIST)) {
					if (first_time) {
						first_time = 0;
					}
					else {
						node_memory_banner(ofp, BANNER|SMAJOR);
					}
				}
				pages += print_node_memory(ofp, n, flags);
			}
		}
	}
	else {
		pages = print_node_memory(ofp, 0, flags);
	}
	node_memory_banner(ofp, SMAJOR);
	return(pages);
}
