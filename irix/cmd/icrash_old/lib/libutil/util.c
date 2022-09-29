#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/util.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#define _KERNEL  1
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <termio.h>
#include <errno.h>
#include <nlist.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>

#include <sys/graph.h>
#include <sys/iograph.h>
#include <sys/invent.h>
#include <sys/hwgraph.h>
#include <sys/dump.h>

#include "icrash.h"
#include "trace.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

#define ARGLEN 40                      /* max length of argument             */

/* 
 * get_value() -- Translate numeric input strings 
 * 
 *   A generic routine for translating an input string (param) in a
 *   number of dfferent ways. If the input string is an equation
 *   (contains the characters '+', '-', '/', and '*'), then perform
 *   the math evaluation and return one of the following modes (if
 *   mode is passed):
 *
 *   0 -- if the resulting value is <= elements, if elements (number
 *        of elements in a table) is passed.  
 *
 *   1 -- if the first character in param is a pund sign ('#').
 *
 *   3 -- the numeric result of an equation.
 *
 *   If the input string is NOT an equation, mode (if passed) will be
 *   set in one of the following ways (depending on the contents of
 *   param and elements).
 *
 *   o When the first character of param is a pound sign ('#'), mode
 *     is set equal to one and the trailing numeric value (assumed to
 *     be decimal) is returned.
 *
 *   o When the first two characters in param are "0x" or "0X," or
 *     when when param contains one of the characers "abcdef," or when
 *     the length of the input value is eight characters. mode is set
 *     equal to two and the numeric value contained in param is
 *     translated as hexadecimal and returned.
 *
 *   o The value contained in param is translated as decimal and mode
 *     is set equal to zero. The resulting value is then tested to see
 *     if it exceeds elements (if passed). If it does, then value is
 *     translated as hexadecimal and mode is set equal to two.
 *
 *   Note that mode is only set when a pointer is passed in the mode
 *   paramater.
 *
 *   Also note that when elements is set equal to zero, any non-hex
 *   (as determined above) value not starting with a pound sign will
 *   be translated as hexadecimal (mode will be set equal to two) --
 *   IF the length of the string of characters is less than 16
 *   (kaddr_t).
 *
 */
int
get_value(char *param, int *mode, int elements, kaddr_t *value)
{
	char *loc, *c;

	if (DEBUG(DC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "get_value: param=\"%s\", mode=0x%x, elements=%d, "
			"value=0x%x\n", param, mode, elements, value);
	}

	kl_reset_error();

	/* Check to see if we are going to need to do any math 
	 */
	if (strpbrk(param, "+-/*")) {
		if (!strncmp(param, "#", 1)) {
			*value = do_math(&param[1]);
			if (mode) {
				*mode = 1;
			}
		}
		else {
			*value = do_math(param);
			if (mode) {
				if (elements && (*value <= elements)) {
					*mode = 0;
				}
				else {
					*mode = 3;
				}
			}
		}
	}
	else {
		if (!strncmp(param, "#", 1)) {
			*value = strtoull(&param[1], &loc, 10);
			if (*loc) {
				KL_SET_ERROR_CVAL(KLE_INVALID_VALUE, param);
				return (1);
			}
			if (mode) {
				*mode = 1;
			}
		} 
		else if (!strncmp(param, "0x", 2) || 
				 !strncmp(param, "0X", 2) || strpbrk(param, "abcdef")) {
			*value = strtoull(param, &loc, 16);
			if (*loc) {
				KL_SET_ERROR_CVAL(KLE_INVALID_VALUE, param);
				return (1);
			}
			if (mode) {
				*mode = 2; /* HEX VALUE */
			}
		} 
		else if (elements || (strlen(param) < 16) || (strlen(param) > 16)) {
			*value = strtoull(param, &loc, 10);
			if (*loc) {
				KL_SET_ERROR_CVAL(KLE_INVALID_VALUE, param);
				return (1);
			}
			if (elements && (*value >= elements)) {
				*value = GET_HEX_VALUE(param);
				if (mode) {
					*mode = 2; /* HEX VALUE */
				}
			}
			else if (mode) {
				*mode = 0;
			} 
		}
		else {
			*value = strtoull(param, &loc, 16);
			if (*loc) {
				KL_SET_ERROR_CVAL(KLE_INVALID_VALUE, param);
				return (1);
			}
			if (mode) {
				*mode = 2; /* ASSUME HEX VALUE */
			}
		}
	}
	return (0);
}

/*
 * print_bit_value()
 *
 * x = byte_size, y = bit_size, z = bit_offset
 */
void
print_bit_value(k_ptr_t ptr, int x, int y, int z, int flags, FILE *ofp)
{
	k_uint_t value;

	value = kl_get_bit_value(ptr, x, y, z);
	if (flags & C_HEX) {
		fprintf(ofp, "0x%llx", value);
	}
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%llo", value);
	}
	else {
		fprintf(ofp, "%lld", value);
	}
}

/* Routines for printing various types of base values (byte, half 
 * word, word and double word) starting at ptr. Output goes to ofp.
 */

/*
 * print_char()
 */
void
print_char(k_ptr_t ptr, int flags, FILE *ofp)
{
	if (flags & C_HEX) {
		fprintf(ofp, "0x%x", *(char *)ptr);
	}
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%o", *(char *)ptr);
	}
	else {
		if ((*(char *)ptr < 32) || (*(char *)ptr > 126)) {
			fprintf(ofp, "\'\\%03o\'", *(char *)ptr);
		}
		else {
			fprintf(ofp, "\'%c\'", *(char *)ptr);
		}
	}
}

/*
 * print_uchar()
 */
void
print_uchar(k_ptr_t ptr, int flags, FILE *ofp)
{
	if (flags & C_HEX) {
		fprintf(ofp, "0x%x", *(char *)ptr);
	}
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%o", *(char *)ptr);
	}
	else {
		fprintf(ofp, "%u", *(char *)ptr);
	}
}

/*
 * print_int2()
 */
void
print_int2(k_ptr_t ptr, int flags, FILE *ofp)
{
	if (flags & C_HEX) {
		fprintf(ofp, "0x%hx", *(short *)ptr);
	}
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%ho", *(short *)ptr);
	}
	else {
		fprintf(ofp, "%hd", *(short *)ptr);
	}
}

/*
 * print_uint2()
 */
void
print_uint2(k_ptr_t ptr, int flags, FILE *ofp)
{
	fprintf(ofp, "%hu", *(short *)ptr);
}

/*
 * print_int4()
 */
void
print_int4(k_ptr_t ptr, int flags, FILE *ofp)
{
	if (flags & C_HEX) {
		fprintf(ofp, "0x%x", *(int *)ptr);
	}
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%o", *(int *)ptr);
	}
	else {
		fprintf(ofp, "%d", *(int *)ptr);
	}
}

/*
 * print_uint4()
 */
void
print_uint4(k_ptr_t ptr, int flags, FILE *ofp)
{
	if (flags & C_HEX) {
		fprintf(ofp, "0x%x", *(int *)ptr);
	}
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%o", *(int *)ptr);
	}
	else {
		fprintf(ofp, "%u", *(uint *)ptr);
	}
}

/*
 * print_float4()
 */
void
print_float4(k_ptr_t ptr, int flags, FILE *ofp)
{
	fprintf(ofp, "%f", *(float *)ptr);
}

/*
 * print_int8()
 */
void
print_int8(k_ptr_t ptr, int flags, FILE *ofp)
{
	if (flags & C_HEX) {
		fprintf(ofp, "0x%llx", *(long long *)ptr);
	}
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%llo", *(long long *)ptr);
	}
	else {
		fprintf(ofp, "%lld", *(long long *)ptr);
	}
}

/*
 * print_uint8()
 */
void
print_uint8(k_ptr_t ptr, int flags, FILE *ofp)
{
	if (flags & C_HEX) {
		fprintf(ofp, "0x%llx", *(long long *)ptr);
	}
	else if (flags & C_OCTAL) {
		fprintf(ofp, "0%llo", *(long long *)ptr);
	}
	else {
		fprintf(ofp, "%llu", *(long long *)ptr);
	}
}

/*
 * print_float8()
 */
void
print_float8(k_ptr_t ptr, int flags, FILE *ofp)
{
	fprintf(ofp, "%llf", *(double *)ptr);
}

/*
 * print_base()
 */
void
print_base(k_ptr_t ptr, int size, int encoding, int flags, FILE *ofp)
{
	switch (size) {

		case 1:
			if (encoding == DW_ATE_unsigned_char) {
				print_uchar(ptr, flags, ofp);
			}
			else {
				print_char(ptr, flags, ofp);
			}
			break;

		case 2:
			if (encoding == DW_ATE_unsigned) {
				print_uint2(ptr, flags, ofp);
			}
			else {
				print_int2(ptr, flags, ofp);
			}
			break;

		case 4:
			if (encoding == DW_ATE_unsigned) {
				print_uint4(ptr, flags, ofp);
			}
			else if (encoding == DW_ATE_float) {
				print_float4(ptr, flags, ofp);
			}
			else {
				print_int4(ptr, flags, ofp);
			}
			break;

		case 8:
			if (encoding == DW_ATE_unsigned) {
				print_uint8(ptr, flags, ofp);
			}
			else if (encoding == DW_ATE_float) {
				print_float8(ptr, flags, ofp);
			}
			else {
				print_int8(ptr, flags, ofp);
			}
			break;

		default:
			break;
	}
}

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

/*
 * fatal()
 */
void 
fatal(char *x)
{
	if (x != (char *)NULL) {
		fprintf(KL_ERRORFP, "%s: %s", program, x);
	}
	exit(1);
}

/*
 * Definitions for the do_math() routine.
 */
#define M_ADD      '+'
#define M_SUBTRACT '-'
#define M_MULTIPLY '*'
#define M_DIVIDE   '/'

/*
 * do_math() -- Calculate some math values based on a string argument
 *              passed into the function.  For example, if you use:
 * 
 *              0xffffc000*2+6/5-3*19-8
 * 
 *              And you will get the value 0xffff7fc0 back.  I could
 *              probably optimize this a bit more, but right now, it
 *              works, which is good enough for me.
 */
kaddr_t
do_math(char *str)
{
	int i = 0, j;
	char *buf, *loc;
	kaddr_t value1, value2;
	struct syment *sp;

	buf = (char *)alloc_block((strlen(str) + 1), B_TEMP);
	sprintf(buf, "%s", str);
	for (i = strlen(str); i >= 0; i--) {
		if ((str[i] == M_ADD) || (str[i] == M_SUBTRACT)) {
			buf[i] = '\0';
			value1 = do_math(buf);
			value2 = do_math(&str[i+1]);
			free_block((k_ptr_t)buf);
			if (str[i] == M_SUBTRACT) {
				return value1 - value2;
			} 
			else {
				return value1 + value2;
			}
		}
	}

	for (i = strlen(str); i >= 0; i--) {
		if ((str[i] == M_MULTIPLY) || (str[i] == M_DIVIDE)) {
			buf[i] = '\0';
			value1 = do_math(buf);
			value2 = do_math(&str[i+1]);
			free_block((k_ptr_t)buf);
			if (str[i] == M_MULTIPLY) {
				return value1 * value2;
			} 
			else {
				if (value2 == 0) {
					/* handle divide by zero */
					if (DEBUG(DC_GLOBAL, 1)) {
						fprintf(KL_ERRORFP, "Divide by zero error!\n");
					}
					return 0;
				} 
				else {
					return value1 / value2;
				}
			}
		}
	}

	/*
	 * Otherwise, just process the value, and return it.
	 */
	sp = get_sym(buf, B_TEMP);
	if (KL_ERROR) {
		kl_reset_error();
		value2 = strtoul(buf, &loc, 10);
		if (((!value2) && (buf[0] != '0')) || (*loc) ||
			(!strncmp(buf, "0x", 2)) || (!strncmp(buf, "0X", 2))) {
			value1 = GET_HEX_VALUE(buf);
		} 
		else {
			value1 = GET_DEC_VALUE(buf);
		}
	}
	else {
		free_sym(sp);
		value1 = sp->n_value;
	}
	free_block((k_ptr_t)buf);
	return value1;
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
	vaddr_mode = kl_get_addr_mode(K, addr);
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
		if (PTRSZ64(K)) {
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

	buf = (unsigned char *)alloc_block(IO_NBPC, B_TEMP);

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
		if ((vaddr_mode == 0) && ((a + s) > (K_K0BASE(K) + K_K0SIZE(K)))) {
			s = (int)((K_K0BASE(K) + K_K0SIZE(K)) - a);
			size = 0;
			short_print = 0;
		}
		else if ((vaddr_mode == 1) && ((a + s) > (K_K1BASE(K) + K_K1SIZE(K)))) {
			s = (int)((K_K1BASE(K) + K_K1SIZE(K)) - a);
			size = 0;
			short_print = 1;
		}
		else if ((vaddr_mode == 2) && ((a + s) > (K_K2BASE(K) + K_K2SIZE(K)))) {
			s = (int)((K_K2BASE(K) + K_K2SIZE(K)) - a);
			size = 0;
			short_print = 2;
		}
		else if ((vaddr_mode == 3) && ((a + s) >= K_KERNSTACK(K))) {
			s = (K_KERNSTACK(K) - a);
		}

		if (!kl_get_block(K, addr, s, buf, "dump")) {
			free_block((k_ptr_t)buf);
			return(-1);
		}

		bp = buf;
		while(bp < (buf + s)) {

			if (PTRSZ64(K)) {
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
								if (PTRSZ64(K)) {
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
								if (PTRSZ64(K)) {
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
	free_block((k_ptr_t)buf);
	return(0);
}

/*
 * dump_string()
 */
kaddr_t
dump_string(kaddr_t addr, int flags, FILE *ofp)
{
	int length, done = 0;
	unsigned *buf;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(KL_ERRORFP, "dump_string: addr=0x%llx, flags=%d\n", 
			addr, flags);
	}

	kl_reset_error();

	buf = (unsigned *)alloc_block(IO_NBPC, B_TEMP);
	while (!done) {
		kl_get_block(K, addr, IO_NBPC, buf, "string");
		if (KL_ERROR) {
			free_block((k_ptr_t)buf);
			return(-1);
		}
		fprintf(ofp, "%s", (char*)buf);
		if ((length = strlen((char *)buf)) < IO_NBPC) {
			done = 1;
		} 
		else {
			addr += IO_NBPC;
		}
	}
	free_block((k_ptr_t)buf);
	return (addr + length + 1);
}

/*
 * print_string() 
 *
 *   print out a string, translating all embeded control characters 
 *   (e.g., '\n' for newline, '\t' for tab, etc.)
 */
void
print_string(char *s, FILE *ofp)
{
	char *sp, *cp;

	kl_reset_error();

	if (!(sp = s)) {
		KL_SET_ERROR(E_BAD_STRING);
		return;
	}

	while (sp) {
		if (cp = strchr(sp, '\\')) {
			switch (*(cp + 1)) {

				case 'n' :
					*cp++ = '\n';
					*cp++ = 0;
					break;

				case 't' :
					*cp++ = '\t';
					*cp++ = 0;
					break;

				default :
					if (*(cp + 1) == 0) {
						KL_SET_ERROR(E_BAD_STRING);
						return;
					}
					/* Change the '\' character to a zero and then print
					 * the string (the rest of the string will be picked
					 * up on the next pass).
					 */
					*cp++ = 0;
					break;
			}
			fprintf(ofp, "%s", sp);
			sp = cp;
		}
		else {
			fprintf(ofp, "%s", sp);
			sp = 0;
		}
	}
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
	
	if (!ACTIVE(K) && (K_DUMP_HDR(K)->dmp_version > 2)) {
		pbufsize = K_DUMP_HDR(K)->dmp_putbuf_size;
	} else {
		if (!(putbuf_cpusz = get_sym("putbuf_cpusz", B_TEMP))) {
			pbufsize = 1000;
		} 
		else if (!kl_get_block(K, putbuf_cpusz->n_value, 
												4, &psize, "putbuf_cpusz")) {
			pbufsize = K_MAXCPUS(K) * 0x800;
		} 
		else {
			pbufsize = K_MAXCPUS(K) * psize;
		}
	}

	np = alloc_block(pbufsize + 500, B_TEMP);
	pbuf = (char *)alloc_block(pbufsize, B_TEMP);
	if(!np || !pbuf || !pbufsize)
	{
		return;
	}
	kl_get_block(K, K_PUTBUF(K), pbufsize, pbuf, "putbuf");
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
	free_block((k_ptr_t)np);
	free_block((k_ptr_t)pbuf);
	return;
}

/*
 * print_utsname() -- Print out utsname structure (uname -a)
 */
void
print_utsname(FILE *ofp)
{
	fprintf(ofp, "    system name:    %s\n",
		CHAR(K, K_UTSNAME(K), "utsname", "sysname"));
	fprintf(ofp, "    release:        %s\n",
		CHAR(K, K_UTSNAME(K), "utsname", "release"));
	fprintf(ofp, "    node name:      %s\n",
		CHAR(K, K_UTSNAME(K), "utsname", "nodename"));
	fprintf(ofp, "    version:        %s\n",
		CHAR(K, K_UTSNAME(K), "utsname", "version"));
	fprintf(ofp, "    machine name:   %s\n",
		CHAR(K, K_UTSNAME(K), "utsname", "machine"));
}

/*
 * print_curtime() -- Print out the time of the system crash.
 */
void
print_curtime(FILE *ofp)
{
	time_t curtime;
	char *tbuf;
	
	tbuf = (char *)alloc_block(100, B_TEMP);
	kl_get_block(K, K_TIME(K), 4, &curtime, "time");
	cftime(tbuf, "%h  %d %T %Y", &curtime);
	fprintf(ofp, "%s\n", tbuf);
	free_block((k_ptr_t)tbuf);
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

	psp = alloc_block(PID_SLOT_SIZE(K), B_TEMP);
	pep = alloc_block(PID_ENTRY_SIZE(K), B_TEMP);

	for (i = 0; i < K_PIDTABSZ(K); i++) {

		pid_slot = K_PIDTAB(K) + (i * PID_SLOT_SIZE(K));
		while (pid_slot) {

			kl_get_struct(K, pid_slot, PID_SLOT_SIZE(K), psp, "pid_slot");
			if (KL_ERROR) {
				if (DEBUG(DC_GLOBAL, 1)) {
					KL_ERROR |= KLE_BAD_PID_SLOT;
					kl_print_debug(K, "list_pid_entries");
				}
				pid_slot = 0;
				continue;
			}

			/* Get the pointer to the pid_entry struct
			 */
			if (!(pid_entry = kl_kaddr(K, psp, "pid_slot", "ps_chain"))) {
				pid_slot = 0;
				continue;
			}

			/* Now get the pid_entry struct
			 */
			kl_get_struct(K, pid_entry, PID_ENTRY_SIZE(K), pep, "pid_entry");
			if (KL_ERROR) {
				if (DEBUG(DC_GLOBAL, 1)) {
					KL_ERROR |= KLE_BAD_PID_SLOT;
					kl_print_debug(K, "list_pid_entries");
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

			pid_slot = kl_kaddr(K, pep, "pid_entry", "pe_next");
		}
	}
	free_block(psp);
	free_block(pep);
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
	if (K_IP(K) == 27) {
		if ((nodeid > (K_NASID_BITMASK(K) + 1)) || !node_memory[nodeid]) {
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
			if (K_IP(K) == 27) {
				p = K_MEM_PER_NODE(K) / K_NBPC(K);
				addr = ((k_uint_t)node_memory[nodeid]->n_nasid << 
							K_NASID_SHIFT(K));
			}
			else {
				p = (node_memory[nodeid]->n_memsize * 0x100000) / K_NBPC(K);
				addr = K_RAM_OFFSET(K);
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
	for (i = 0; i < pages; i++, addr += K_NBPC(K)) {
		if (klib_page_in_dump(K, addr)) {
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
				fprintf(ofp, "  0x%llx", first_addr|K_K0BASE(K));
				fprintf(ofp, "-->0x%llx\n", 
						(last_addr + K_NBPC(K) - 1)|K_K0BASE(K));
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
		fprintf(ofp, "  0x%llx", first_addr|K_K0BASE(K));
		fprintf(ofp, "-->0x%llx\n", 
				(last_addr + K_NBPC(K) - 1)|K_K0BASE(K));
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
		p = (bank->b_size * 0x100000) / K_NBPC(K);
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

	if (K_IP(K) == 27) {
		int nasid;
		kaddr_t value, nodepda;

		for (i = 0; i < K_NUMNODES(K); i++) {
			/* Get the address of the nodepda_s struct for this
			 * particular node. We can get the nasid from its address.
			 * Once we have the nasid, we can set the base address
			 * for our range search.
			 */
			value = (K_NODEPDAINDR(K) + (i * K_NBPW(K)));
			kl_get_kaddr(K, value, &nodepda, "nodepda_s");
			if (KL_ERROR) {
				continue;
			}
			nasid = kl_addr_to_nasid(K, nodepda);
			addr = ((k_uint_t)nasid << K_NASID_SHIFT(K));
			p = K_MEM_PER_NODE(K) / K_NBPC(K);
			fprintf(ofp, "Memory from node %d found in vmcore image:\n\n", i);
			pages += print_dump_page_range(ofp, addr, p, 0);
		}
	}
	else {
		pages += print_dump_page_range(ofp, K_RAM_OFFSET(K), K_PHYSMEM(K), 0);
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
	if (K_IP(K) == 27) {
		for (n = 0; n < (K_NASID_BITMASK(K) + 1); n++) {
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
