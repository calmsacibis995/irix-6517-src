#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_search.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * search_memory()
 */
int
search_memory(unsigned char *pattern, unsigned char *mask, int nbytes,
	kaddr_t start_addr, k_uint_t length, int flags, FILE *ofp)
{
	int vaddr_mode;
	unsigned i, k, s;
	kaddr_t j, addr, size; 
	unsigned char *buf;
	kaddr_t KPmaxclick,KPptr0,KPptr1;
	__uint32_t maxclick;
	int Itemp,Itemp1;
	
	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "pattern=0x%x, mask=0x%x, nbytes=%d, "
			"start_addr=0x%llx length=%lld\n", 
			pattern, mask, nbytes, start_addr, length);
	}

	vaddr_mode = kl_get_addr_mode(K, start_addr);
	if (KL_ERROR) {
		kl_print_error(K);
		return(1);
	}

	fprintf(ofp, "\n");
	if (IS_PHYSICAL(K, start_addr)) {
		if (IS_SMALLMEM(K, start_addr)) {
			fprintf(ofp, "  START = %08llx\n", (start_addr|K_K0BASE(K)));
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

	if (length < IO_NBPC) {
		fprintf(ofp, "\n");
	} 
	else {
		fprintf(ofp, "(%lld%s page%s)\n", 
			length/K_PAGESZ(K), (length%K_PAGESZ(K)) ? "+" : "", 
			(((length/K_PAGESZ(K)) == 1) && 
			!(length%K_PAGESZ(K))) ? "" : "s");
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

	buf = (unsigned char *)alloc_block((IO_NBPC + MAX_SEARCH_SIZE), 
					   B_TEMP);

	size = length;
	addr = start_addr;
	Itemp1 = Pnum(K,
		     (is_kseg0(K,addr) ?
		      k0_to_phys(K,addr) : 
		      k1_to_phys(K,addr)));
	K_SYM_ADDR(K)((void*)NULL, "maxclick", &KPmaxclick);
	kl_get_block(K,KPmaxclick,4,&maxclick, "maxclick");
			
	while (1) 
	{
		if((is_kseg0(K,addr) && 
		    !VALID_PFN(Pnum(K,k0_to_phys(K,addr)))) ||
		   (is_kseg1(K,addr) && 
		    !VALID_PFN(Pnum(K,k1_to_phys(K,addr)))))
		{
			Itemp = Pnum(K,
				     (is_kseg0(K,addr) ?
				      k0_to_phys(K,addr) : 
				      k1_to_phys(K,addr)));
			if(maxclick && (Itemp > maxclick))
			{
				/*
				 * Outta' here. Fallen off the end of valid 
				 * memory.
				 */
				break;
			}

			if((Itemp=next_valid_pfn(Itemp,maxclick)) < Itemp1)
			{
				/*
				 * No more valid pfn's.
				 */
				break;
			}
			addr = KL_PHYS_TO_K0(Ctob(K,Itemp));
			continue;
		}
		if (size > IO_NBPC) {
			s = IO_NBPC;
			size -= IO_NBPC;
		} 
		else {
			s = size;
			size = 0;
		}
	
		if (DEBUG(DC_MEM, 2)) 
		{
			fprintf(ofp, "s=%u, addr=0x%llx\n", s, addr);
		}

		j = addr;

		/* If the size of the pattern we are looking for is
		 * greater than one word, then grab additional words of
		 * memory to prevent an overrun of the buffer (the buffer
		 * has been extended by MAX_SEARCH_SIZE words to allow for
		 * this).
		 */
		if (!kl_get_block(K, addr, (s + (nbytes - 4)), buf, "search")) 
		{
			if (DEBUG(DC_GLOBAL, 1)) 
			{
				fprintf(ofp, 
					"search: error reading memory at "
					"0x%llx\n", addr);
			}
			/*
			 * Just assume, that this block did not make it into
			 * the coredump. The approach is to be 
			 * thick-skinned about our failure here.
			 */
			goto next_block;
		}

		i = 0;
		while (i < s) 
		{
#if 0
			/* 
			 * If we roll beyond the end of the K0 boundary, turn 
			 * starting address into a physical address and 
			 * continue.
			 *
			 * Should we do this.. I think it is pointless. It is
			 * outta' here.
			 */
			if (((vaddr_mode == 0) && 
			     (addr > (K_K0BASE(K) + K_K0SIZE(K)))) ||
			    ((vaddr_mode == 1) && 
			     (addr > (K_K1BASE(K) + K_K1SIZE(K))))) 
			{
				addr = K_K0BASE(K);
				vaddr_mode = 3; /* switch to physical mode */
			}
#endif
			if (DEBUG(DC_MEM, 3)) {
				fprintf(ofp, "search_memory: &buf[%d]=0x%x "
					"(0x%llx)\n", 
					i, &buf[i], j + i);
			}

			/* Check to see if there is a pattern match starting at
			 * buf[i].
			 */
			if (pattern_match(pattern, mask, &buf[i], nbytes)) 
			{
				fprintf(ofp, "MATCH: found at ");
				if (is_kseg2(K, j + i) || is_kseg1(K, j + i)) 
				{
					fprintf(ofp, "0x%llx\n", j + i);
				} 
				else if (IS_SMALLMEM(K, j + i)) 
				{
					if (IS_PHYSICAL(K, j + i)) 
					{
						fprintf(ofp, "0x%llx\n", 
							(j + i) | K_K0BASE(K));
					} 
					else {
						fprintf(ofp, "0x%llx\n", 
							(j + i));
					}
				} 
				else {
					fprintf(ofp, "physical address "
						"0x%llx\n", (j + i));
				}
				fprintf(ofp, "  ");

				if (flags & C_STRING) 
				{
					fprintf(ofp, "\"");
					for (k = 0; k < nbytes; k++) 
					{
						fprintf(ofp, "%c", buf[i+k]);
						if (!((k + 1) % 70)) {
							fprintf(ofp, "\n  ");
						}
					}
					fprintf(ofp, "\"");
				} 
				else 
				{
					for (k = 0; k < nbytes; k++) 
					{
						fprintf(ofp, "%02x", buf[i+k]);
						if (!((k + 1) % 24)) 
						{
							fprintf(ofp, "\n  ");
						} 
						else if (!((k + 1) % 4)) 
						{
							fprintf(ofp, " ");
						}
					}
				}
				fprintf(ofp, "\n");
			}

			/* 
			 * Bump the count to the start of the next search 
			 * position 
			 */
			if (flags & C_BYTE) 
			{
				i++;
			} 
			else if (flags & C_HWORD) 
			{
				i += 2;
			} 
			else if (flags & C_DWORD) 
			{
				i += 8;
			} 
			else {
				i += 4;
			}
		}
next_block:
		if (size) {
			if (j != addr) {
					addr = j;
			}
			addr += IO_NBPC;
		} 
		else {
			break;
		}
	}
	fprintf(ofp, "\n");
	free_block((k_ptr_t)buf);
	return(0);
}

/*
 * pattern_match()
 */
int
pattern_match(unsigned char *pattern, unsigned char *mask,
	unsigned char *buf, int nbytes)
{
	int i;

	if ((buf[0] & mask[0]) == (pattern[0] & mask[0])) {
		for (i = 1; i < nbytes; i++) {
			if (!((buf[i] & mask[i]) == (pattern[i] & mask[i]))) {
				return(0);
			}
		}
		return(1);
	} 
	else {
		return(0);
	}
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
			fprintf(cmd.ofp, "search_cmd: input string = %s\n",
				cmd.args[0]);
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
	else 
	{
		if (!(cmd.flags & C_BYTE) && !(cmd.flags & C_HWORD) && 
		    !(cmd.flags & C_DWORD)) 
		{
			cmd.flags |= C_WORD;
		}

		/* Check to make sure that multiple flag values are not set. If
		 * they are, go with the flag representing the smallest memory
		 * boundary size (e.g., go with C_BYTE if both C_BYTE and 
		 * C_HWORD are set).
		 */
		if (cmd.flags & C_BYTE) 
		{
			cmd.flags &= ~(C_HWORD|C_WORD|C_DWORD);
		}
		else if (cmd.flags & C_HWORD) 
		{
			cmd.flags &= ~(C_WORD|C_DWORD);
		}
		else if (cmd.flags & C_WORD) 
		{
			cmd.flags &= ~(C_DWORD);
		}

		/* Check to see if the hex value provided in pattern has a
		 * 0x prefix. If it does, then strip it off (it messes up
		 * pattern and nbytes count).
		 */
		if (nchars > 2 && (!strncmp(cmd.args[0], "0X", 2) || 
				   !strncmp(cmd.args[0], "0x", 2))) 
		{
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

	pattern = (unsigned char *)alloc_block(MAX_SEARCH_SIZE, B_TEMP);

	/* Copy the ASCII pattern in command argument into pattern array
	 * (as ASCII text or numeric values).
	 */
	if (cmd.flags & C_STRING) 
	{
		strncpy((char*)pattern, cmd.args[0], nbytes);
	} 
	else 
	{

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
				free_block((k_ptr_t)pattern);
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
		start_addr = (K_K0BASE(K)|K_RAM_OFFSET(K));
		length = (K_PHYSMEM(K) * K_PAGESZ(K));
	} 
	else {
		get_value(cmd.args[1], &mode, 0, &start_addr);
		if (KL_ERROR) {
			kl_print_error(K);
			return(1);
		}

		/* a PFN was entered --  convert to physical 
		 */
		if (mode == 1) { 
			start_addr = Ctob(K, start_addr);
		}
		if (cmd.nargs == 3) {
			get_value(cmd.args[2], NULL, 0, &length);
			if (KL_ERROR) {
				kl_print_error(K);
				return(1);
			}
		}
	}

	if (IS_PHYSICAL(K, start_addr)) 
	{
		if (length <= 0 || length > 
		    ((K_PHYSMEM(K) - Btoc(K, start_addr)) * K_PAGESZ(K))) 
		{
			length = ((K_PHYSMEM(K) - Btoc(K, start_addr)) * 
				  K_PAGESZ(K));
		}
	} 
	else if (is_kseg0(K, start_addr)) 
	{
		if (length <= 0 || 
		    length > (K_K0SIZE(K) - (start_addr - K_K0BASE(K)))) 
		{
			length =  (K_K0SIZE(K) - (start_addr - K_K0BASE(K)));
		}
	} 
	else if (is_kseg1(K, start_addr)) 
	{
		if (length <= 0 ||
		    length >= (K_K1SIZE(K) - (start_addr - K_K1BASE(K)))) 
		{
			length = (K_K1SIZE(K) - (start_addr - K_K1BASE(K)));
		}
	} 
	else 
	{ /* K2SEG */
		if (length <= 0 || 
		    length >= ((K_SYSSEGSZ(K) * K_PAGESZ(K)) - 
			       (start_addr - K_K2BASE(K)))) 
		{
			length = (K_SYSSEGSZ(K) * K_PAGESZ(K)) - 
				(start_addr - K_K2BASE(K));
		}
	}
	search_memory(pattern, (unsigned char *)mask, nbytes, start_addr,
		length, cmd.flags, cmd.ofp);

	free_block((k_ptr_t)pattern);
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
