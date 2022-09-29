#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_search.c,v 1.18 1999/11/19 01:26:43 lucc Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <klib/klib.h>
#include "icrash.h"

kaddr_t curadr;
FILE *of;
static int curflags;

static void
match_found(int off)
{
kaddr_t adr=curadr+off;
int k;

	fprintf(of, "MATCH: found at ");
	if (is_kseg2(adr) || is_kseg1(adr)) {
		fprintf(of, "0x%llx\n", adr);
	} 
	else if (IS_SMALLMEM(adr)) {
		if (IS_PHYSICAL(adr)) {
			fprintf(of, "0x%llx\n", 
				(adr) | K_K0BASE);
		} 
		else {
			fprintf(of, "0x%llx\n", 
				(adr));
		}
	} 
	else {
		fprintf(of, "physical address "
			"0x%llx\n", (adr));
	}

}

#define PAGECHUNK 128 /* 2 megs on a 16k page system.  multiple of 2 */

/*
 * search_memory()
 */
int
search_memory(unsigned char *pattern, unsigned char *mask, int nbytes,
	kaddr_t start_addr, k_uint_t length, int flags, FILE *ofp)
{
	int vaddr_mode, align;
	unsigned i, k, s;
	kaddr_t j, addr, size; 
	unsigned char *buf;
	kaddr_t KPmaxclick,KPptr0,KPptr1;
	__uint32_t maxclick;
	int Itemp,Itemp1;
	int oneper, percount=0;
	int chunkbsize=PAGECHUNK*K_PAGESZ;
	int doprct=isatty(fileno(stderr));
	
	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "pattern=0x%x, mask=0x%x, nbytes=%d, "
			"start_addr=0x%llx length=%lld\n", 
			pattern, mask, nbytes, start_addr, length);
	}

	vaddr_mode = kl_get_addr_mode(start_addr);
	if (KL_ERROR) {
		kl_print_error();
		return(1);
	}

	fprintf(ofp, "\n");
	if (IS_PHYSICAL(start_addr)) {
		if (IS_SMALLMEM(start_addr)) {
			fprintf(ofp, "  START = %08llx\n", (start_addr|K_K0BASE));
		} 
		else {
			fprintf(ofp, "  START = %08llx (Physical Address)\n",
				start_addr);
		}
	} 
	else {
		fprintf(ofp, "  START = %08llx\n", start_addr);
	}
	fprintf(ofp, " LENGTH = %lld bytes ", length);

	if (length < K_PAGESZ) {
		fprintf(ofp, "\n");
	} 
	else {
	int p=(length+K_PAGESZ)/K_PAGESZ;
		fprintf(ofp, "(%lld%s page%s)\n", 
			length/K_PAGESZ, (length%K_PAGESZ) ? "+" : "", 
			(((length/K_PAGESZ) == 1) && 
			!(length%K_PAGESZ)) ? "" : "s");
		oneper=p/100;
	}

	fprintf(ofp, "PATTERN = ");
	if (flags & C_STRING) {
		fprintf(ofp, "\"");
		for (i = 0; i < nbytes; i++) {
			fprintf(ofp, "%c", pattern[i]);
			if (!((i + 1) % 60)) {
				fprintf(ofp, "\n          ");
			}
		}
		fprintf(ofp, "\"");
	} 
	else {
		for (i = 0; i < nbytes; i++) {
			fprintf(ofp, "%02x", pattern[i]);
			if (!((i + 1) % 24)) {
				fprintf(ofp, "\n          ");
			} 
			else if (!((i + 1) % 4)) {
				fprintf(ofp, " ");
			}
		}
	}

	fprintf(ofp, "\n");
	if (!(flags & C_STRING)) {
		fprintf(ofp, "   MASK = ");
		for (i = 0; i < nbytes; i++) {
			fprintf(ofp, "%02x", mask[i]);
			if (!((i + 1) % 24)) {
				fprintf(ofp, "\n          ");
			} 
			else if (!((i + 1) % 4)) {
				fprintf(ofp, " ");
			}
		}
		fprintf(ofp, "\n\n");
	} 
	else {
		fprintf(ofp, "\n");
	}

	size = length;
	addr = start_addr;
	Itemp1 = Pnum((is_kseg0(addr) ? k0_to_phys(addr) : k1_to_phys(addr)));
	KPmaxclick = kl_sym_addr("maxclick");
	kl_get_block(KPmaxclick, 4, &maxclick, "maxclick");

	/* setup memory serach step according to user supplied allignment */
	if (flags & C_BYTE) {
		align=1;
	} 
	else if (flags & C_HWORD) {
		align=2;
	} 
	else if (flags & C_WORD) {
		align=4;
	} 
	else if (flags & C_DWORD) {
		align=8;
	} 
	else {
		align=8;
	}

	curflags=flags;
	of=ofp;

	/* is this is a vmcore and we are lookign at kseg0 then use the pageindex */
	if(is_kseg0(addr) && !ACTIVE) {

	int i,found=0;
	ptableindex *pp;
	kaddr_t paddr=k0_to_phys(addr);
	kaddr_t end=paddr+length;
	extern ptableindex *page_first;
	extern int pcounter;
	int onep;

		if(isatty(fileno(stderr))) onep=pcounter/100;
		else onep = 0;

		buf= (unsigned char *)alloc_block(K_PAGESZ, K_TEMP);

		/* click down the start address to the nearest page boundary 
		   and bump up the end to the nearest page boundary */
		paddr &= ~(K_PAGESZ-1);
		end = (end + K_PAGESZ -1) & ~(K_PAGESZ-1);

		for(i=0, pp=page_first;i<pcounter;i++, pp++) {

		kaddr_t kaddr=(((kaddr_t)pp->dir.addr_hi)<<32)+pp->dir.addr_lo;

			if(kaddr >= paddr && kaddr < end) {

				kl_get_block(kaddr | K_K0BASE, K_PAGESZ, buf, "search");

				if(!KL_ERROR) {

					curadr=kaddr | K_K0BASE;
					found=pattern_match(pattern, mask, buf, K_PAGESZ, nbytes, align);

				}

			}
			if(onep && (found || !(i%onep)))
				fprintf(stderr, "%d%%\r", i/onep);
		}

		kl_free_block((k_ptr_t)buf);		

	} else {

		/* get a buffer and make sure it's alligned */
		buf = (unsigned char *)alloc_block(((K_PAGESZ * PAGECHUNK) + MAX_SEARCH_SIZE + sizeof(long long)), K_TEMP);

		while (1) {

			if((is_kseg0(addr) && !VALID_PFN(Pnum(k0_to_phys(addr)))) ||
				   	(is_kseg1(addr) && !VALID_PFN(Pnum(k1_to_phys(addr))))) {
				Itemp = Pnum((is_kseg0(addr) ? 
						k0_to_phys(addr) : k1_to_phys(addr)));

				if(maxclick && (Itemp > maxclick)) {
					/* Outta' here. Fallen off the end of valid 
				 	* memory.
				 	*/
					break;
				}

				if((Itemp=next_valid_pfn(Itemp,maxclick)) < Itemp1) {
					/* No more valid pfn's.
				 	*/
					break;
				}
				addr = KL_PHYS_TO_K0(Ctob(Itemp));
				continue;
			}
			/* If the size of the pattern we are looking for is
		 	* greater than one word, then grab additional words of
		 	* memory to prevent an overrun of the buffer (the buffer
		 	* has been extended by MAX_SEARCH_SIZE words to allow for
		 	* this).
		 	*/
	
			s = size < chunkbsize ? size : chunkbsize;
	
	
			/* this loops gets executed more then once only when reaching the end of
		   	valid chunk of memory */
			while(1) {
	
				kl_get_block(addr, (s + (nbytes - 4)), buf, "search");
				if(KL_ERROR) {
	
					if(s<=K_PAGESZ) {
	
						if (DEBUG(DC_GLOBAL, 1)) {
							fprintf(ofp, "search: error reading memory at 0x%llx\n", addr);
							}

						/* Just assume, that this block did not make it into
		 				* the coredump. The approach is to be 
		 				* thick-skinned about our failure here.
		 				*/
						if(size<K_PAGESZ) size=0;
						else size-=K_PAGESZ;
						goto next_block;
	
					} else {
	
						s = s / 2;
	
					}
	
				} else break;
	
			}
			size -= s;
		
			if (DEBUG(DC_MEM, 2)) {
				fprintf(ofp, "s=%u, addr=0x%llx\n", s, addr);
			}
	
			curadr=addr;
			pattern_match(pattern, mask, buf, s, nbytes, align, match_found);

next_block:

			if (size) {
	
				addr += s;
				fprintf(stderr, "%d %%\r", (int)((((unsigned long long)length-size)*100)/length));
			} 
			else {
			break;
			}
		}
		kl_free_block((k_ptr_t)buf);
	}
	fprintf(ofp, "\n");
	return(0);
}

/*
 * pattern_match()
 */
int
pattern_match(unsigned char *pattern, unsigned char *mask,
	unsigned char *buf, int size, int nbytes, int align)
{
	int i=0, j=0, found=0;
#define TOCHAR(p)	(*((unsigned char*)(p)))
#define TOHALF(p)	(*((unsigned short*)(p)))
#define TOWORD(p)	(*((unsigned int*)(p)))
#define TODWORD(p)	(*((unsigned long long*)(p)))


	while(j<size) {

		i=0;
		switch(align)
		{
			case 8:
				for(;i+8<=nbytes;i+=8) {

					if((( TODWORD(pattern+i) & TODWORD(mask+i)) != ( TODWORD(buf+j+i) & TODWORD(mask+i)))) {
	
						goto next;
	
					}
				}
			
			case 4:  
				for(;i+4<=nbytes;i+=4) {
	
					if(((TOWORD(pattern+i) & TOWORD(mask+i)) != (TOWORD(buf+j+i) & TOWORD(mask+i)))) {
	
						goto next;
	
					}
				}
			case 2: 
				for(;i+2<=nbytes;i+=2) {
	
					if(((TOHALF(pattern+i) & TOHALF(mask+i)) != (TOHALF(buf+j+i) & TOHALF(mask+i)))) {
	
						goto next;
	
					}
				}
			default:
			for (;i<nbytes;i++) {

				if (((buf[j+i] & mask[i]) != (pattern[i] & mask[i]))) {

					goto next;

				}
			}
			match_found(j);
			found=1;
		}
next:
		j+=align;
	}
	return found;
}

/*
 * search_cmd() -- Run the 'search' command to look through memory.
 */
int
search_cmd(command_t cmd)
{
	int i, j, mode, nchars, nbytes;
	kaddr_t start_addr, length = 0;
	unsigned char *pattern;
	unsigned *l;
	char *s, w[9], *ptr;

	/* Truncate any search pattern larger than MAX_SEARCH_SIZE (this
	 * should never happen as the maximum command paramater length is
	 * 256 characters -- the same as MAX_SEARCH_SIZE).
	 */
	nchars = strlen(cmd.args[0]);
	if (nchars > MAX_SEARCH_SIZE) {
		nchars = MAX_SEARCH_SIZE;
	}

	/* Check to see if this is an ASCII string search. If it is, set the
	 * C_STRING and C_BYTE flags (and turn off all other flags). Otherwise,
	 * Make sure the pattern value contains only hex digits and that the
	 * number of hex digits is correct. If none of the flags (C_BYTE,
	 * C_HWORD, C_WORD, or C_DWORD) is set, then make C_WORD the default
	 * flag value.
	 */
	if (cmd.args[0][0] == '\"') {

		if (DEBUG(DC_GLOBAL, 1)) {
			fprintf(cmd.ofp, "search_cmd: input string = %s\n", cmd.args[0]);
		}

		cmd.flags &= ~(C_HWORD|C_WORD| C_DWORD);
		cmd.flags |= (C_STRING | C_BYTE);

		/* Strip off the leading and tailing double quotes (") 
		 */
		bcopy (&cmd.args[0][1], cmd.args[0], nchars - 1);
		nchars--;
		if (cmd.args[0][nchars - 1] == '\"') {
			nchars--;
		}
		cmd.args[0][nchars] = 0;
		nbytes = nchars;
	}
	else {
		if (!(cmd.flags & C_BYTE) && !(cmd.flags & C_HWORD) && 
					!(cmd.flags & C_DWORD)) {
			cmd.flags |= C_WORD;
		}

		/* Check to make sure that multiple flag values are not set. If
		 * they are, go with the flag representing the smallest memory
		 * boundary size (e.g., go with C_BYTE if both C_BYTE and 
		 * C_HWORD are set).
		 */
		if (cmd.flags & C_BYTE) {
			cmd.flags &= ~(C_HWORD|C_WORD|C_DWORD);
		}
		else if (cmd.flags & C_HWORD) {
			cmd.flags &= ~(C_WORD|C_DWORD);
		}
		else if (cmd.flags & C_WORD) {
			cmd.flags &= ~(C_DWORD);
		}

		/* Check to see if the hex value provided in pattern has a
		 * 0x prefix. If it does, then strip it off (it messes up
		 * pattern and nbytes count).
		 */
		if (nchars > 2 && (!strncmp(cmd.args[0], "0X", 2) || 
				   !strncmp(cmd.args[0], "0x", 2))) {
				nchars -= 2;
				bcopy (&cmd.args[0][2], cmd.args[0], nchars);
				cmd.args[0][nchars] = 0;
		}

		/* 
		 * Trim off any extra hex digits. Also make sure that there are
		 * at least enough hex digits for the size of the search unit 
		 * (byte, hword, dword, etc.).
		 */
		if (cmd.flags & C_BYTE) {
			if (nchars < 2) {
				fprintf(cmd.ofp, "%s: pattern must consist of at least 2 hex "
					"digits for a byte search\n", cmd.args[0]);
				return(1);
			}
			nchars -= (nchars % 2);
		}
		else if (cmd.flags & C_HWORD) {
			if (nchars < 4) {
				fprintf(cmd.ofp, "%s: pattern must consist of at least 4 hex "
					"digits for a halfword search\n", cmd.args[0]);
				return(1);
			}
			nchars -= (nchars % 4);
		} 
		else if (cmd.flags & C_WORD) {
			if (nchars < 8) {
				fprintf(cmd.ofp, "%s: pattern must consist of at least 8 hex "
					"digits for a word search\n", cmd.args[0]);
				return(1);
			}
			nchars -= (nchars % 8);
		}
		else if (cmd.flags & C_DWORD) {
			if (nchars < 16) {
				fprintf(cmd.ofp, "%s: pattern must consist of at least 16 hex "
					"digits for a doubleword search\n", cmd.args[0]);
				return(1);
			}
			nchars -= (nchars % 16);
		} 
		nbytes = nchars / 2;
	}

	pattern = (unsigned char *)alloc_block(MAX_SEARCH_SIZE, K_TEMP);

	/* Copy the ASCII pattern in command argument into pattern array
	 * (as ASCII text or numeric values).
	 */
	if (cmd.flags & C_STRING) {
		strncpy((char*)pattern, cmd.args[0], nbytes);
	} 
	else {

		/* At this time, there should be nothing but hex digits in 
		 * the command line argument (and no spaces). Next, walk the 
		 * command line argument and convert each 2 hex digits into 
		 * an unsigned (byte) value. Place this value at the appropriate 
		 * location in the pattern array. 
		 */ 
		s = cmd.args[0];
		l = (unsigned*)pattern;
		for (i = 0; i < nbytes; i++) {
			strncpy(w, s, 2);
			w[2] = 0;
			pattern[i] = strtoull(w, &ptr, 16);
			if (ptr != &w[2]) {
				fprintf(cmd.ofp, 
					"%s: pattern contains one or more "
					"invalid hex digits\n", 
					cmd.args[0]);
				kl_free_block((k_ptr_t)pattern);
				return(1);
			}
			s += 2;
		}
	}

	/* If no mask was specified, fill nbytes of mask array with ones.
	 */
	if (!(cmd.flags & C_MASK)) {
		s = (char*)mask;
		for (i = 0; i < nbytes; i++) {
			s[i] = 0xff;
		}
	}

	/* Set memory location for start of search and search length 
	 */ 
	if (cmd.nargs == 1) {
		start_addr = (K_K0BASE|K_RAM_OFFSET);
	} 
	else {
		kl_get_value(cmd.args[1], &mode, 0, &start_addr);
		if (KL_ERROR) {
			kl_print_error();
			return(1);
		}

		/* a PFN was entered --  convert to physical 
		 */
		if (mode == 1) { 
			start_addr = Ctob(start_addr);
		}
		if (cmd.nargs == 3) {
			kl_get_value(cmd.args[2], NULL, 0, &length);
			if (KL_ERROR) {
				kl_print_error();
				return(1);
			}
		}
	}

	if (IS_PHYSICAL(start_addr)) {
		if (length <= 0 || length > 
			((K_PHYSMEM - Btoc(start_addr)) * K_PAGESZ)) {
			length = ((K_PHYSMEM - Btoc(start_addr)) * 
				  K_PAGESZ);
		}
	} 
	else if (is_kseg0(start_addr)) {
	if (length <= 0 || length > (K_K0SIZE - (start_addr - K_K0BASE))) {
			length =  (K_K0SIZE - (start_addr - K_K0BASE));
		}
	} 
	else if (is_kseg1(start_addr)) {
		if (length <= 0 || length >= (K_K1SIZE - (start_addr - K_K1BASE))) {
			length = (K_K1SIZE - (start_addr - K_K1BASE));
		}
	} 
        else if(is_kseg2(start_addr)) {

                if (length <= 0 || length >= (K_K2SIZE - (start_addr - K_K2BASE))) {

                        length = (K_K2SIZE - (start_addr - K_K2BASE));
                }
        }
        else if(IS_KERNELSTACK(start_addr)) {

        	int maxl=K_EXTUSIZE?K_KERNELSTACK-K_KEXTSTACK:K_KERNSTACK-K_KERNELSTACK;

                if(length <= 0 || length>maxl) length=maxl;

        } 

        if(length <= 0) {

                fprintf(cmd.ofp, "Please specifiy a length fo the search.");

        } else {

                search_memory(pattern, (unsigned char *)mask, nbytes, start_addr,
                        length, cmd.flags, cmd.ofp);
        }
	kl_free_block((k_ptr_t)pattern);
	return(0);
}

#define _SEARCH_USAGE \
  "[-B] [-H] [-W] [-D] [-w outfile] [-m mask]\n\t\tpattern [address] [length]"

/*
 * search_usage() -- Print the usage string for the 'search' command.
 */
void
search_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SEARCH_USAGE);
}

/*
 * search_help() -- Print the help information for the 'search' command.
 */
void
search_help(command_t cmd)
{
	CMD_HELP(cmd, _SEARCH_USAGE,
		"Locate contiguous bytes of memory that match the values contained "
		"in pattern, beginning at address for length bytes.  Pattern "
		"consists of a string of, from one to 256 hexadecimal digits (with "
		"no embedded spaces). For full word searches (the default), the "
		"first word of mask is anded (&) with each word of memory and the "
		"result compared against the first word in pattern (which is also "
		"anded with the first word of mask). If there is a match, subsequent "
		"words in memory are compared with their respective words in pattern "
		"(if there are any) to see if a match exists for the entire pattern. "
		"If the -D option is issued, the search is conducted on double word "
		"boundaries. If the -H option is issued, the search is conducted on "
		"halfword boundaries. If the -B option is issued, the search will be "
		"performed without regard to double word, word or halfword boundaries. "
		"If a mask is not specified, mask defaults to all ones for the size "
		"of pattern. For all but string searches, the number of hex digits " 
		"specified for pattern cannot be less than will fit into the memory "
		"boundary size specified. For example, patterns for halfword boundary "
		"searches, must contain (at least) eight hex digits (two per byte). "
		"Also, any extra digits beyond the specified boundary will be "
		"trimmed off from the right side of the input line.\n\n"

		"In addition to finding matches for hex values in memory, it is "
		"possible to search for strings of characters as well. Just begin "
		"and end the character search pattern with double quotes (\"). The "
		"ASCII value for each character in the string will form the "
		"corresponding byte values in the search pattern.\n\n"

		"The address can be specified as either a virtual address (K0, K1, K2, "
		"etc.), a physical address, or as a PFN (directly following a pound "
		"'#' sign). If no address is specified (or if the one specified "
		"does not map to a valid physical memory address), address defaults to "
		"the K0 address mapped to the start of physical memory. An optional "
		"length parameter specifies the number of bytes to search. If length "
		"is not specified, it will be set equal to the size of physical "
		"memory minus the starting physical address. Note that length can be "
		"specified ONLY when a address has been specified.");
}

/*
 * search_parse() -- Parse the command line arguments for 'search'.
 */
int
search_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_BYTE|C_HWORD|C_WORD|C_DWORD|C_MASK);
}
