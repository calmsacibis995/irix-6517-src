#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <assert.h>
#include <klib/klib.h>

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
	if (flags & K_HEX) {
		fprintf(ofp, "0x%llx", value);
	}
	else if (flags & K_OCTAL) {
		fprintf(ofp, "0%llo", value);
	}
	else {
		fprintf(ofp, "%lld", value);
	}
}

/* Routines for printing various types of base values (byte, half 
 * word, word and double word) starting at ptr. Output goes to ofp.
 *
 * Note that it is the callers responsibility to ensure that there is
 * proper alignment of ptr. If any of these functions (other than 
 * print_char() and print_uchar()) get called with an improperly
 * aligned pointer address, a core dump will occur. This has never
 * happened, but since it could...
 */

/*
 * print_char()
 */
void
print_char(k_ptr_t ptr, int flags, FILE *ofp)
{
	if (flags & K_HEX) {
		fprintf(ofp, "0x%x", *(char *)ptr);
	}
	else if (flags & K_OCTAL) {
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
	if (flags & K_HEX) {
		fprintf(ofp, "0x%x", *(char *)ptr);
	}
	else if (flags & K_OCTAL) {
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
	/* Make sure the pointer is properly aligned (or we will 
	 * dump core 
	 */
	if ((unsigned)ptr % 2) {
		fprintf(ofp, "ILLEGAL ADDRESS (%x)", ptr);
	}

	if (flags & K_HEX) {
		fprintf(ofp, "0x%hx", *(short *)ptr);
	}
	else if (flags & K_OCTAL) {
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
	/* Make sure the pointer is properly aligned (or we will 
	 * dump core 
	 */
	if ((unsigned)ptr % 2) {
		fprintf(ofp, "ILLEGAL ADDRESS (%x)", ptr);
	}

	fprintf(ofp, "%hu", *(short *)ptr);
}

/*
 * print_int4()
 */
void
print_int4(k_ptr_t ptr, int flags, FILE *ofp)
{
	/* Make sure the pointer is properly aligned (or we will 
	 * dump core 
	 */
	if ((unsigned)ptr % 4) {
		fprintf(ofp, "ILLEGAL ADDRESS (%x)", ptr);
	}

	if (flags & K_HEX) {
		fprintf(ofp, "0x%x", *(int *)ptr);
	}
	else if (flags & K_OCTAL) {
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
	/* Make sure the pointer is properly aligned (or we will 
	 * dump core 
	 */
	if ((unsigned)ptr % 4) {
		fprintf(ofp, "ILLEGAL ADDRESS (%x)", ptr);
	}

	if (flags & K_HEX) {
		fprintf(ofp, "0x%x", *(int *)ptr);
	}
	else if (flags & K_OCTAL) {
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
	/* Make sure the pointer is properly aligned (or we will 
	 * dump core 
	 */
	if ((unsigned)ptr % 4) {
		fprintf(ofp, "ILLEGAL ADDRESS (%x)", ptr);
	}

	fprintf(ofp, "%f", *(float *)ptr);
}

/*
 * print_int8()
 */
void
print_int8(k_ptr_t ptr, int flags, FILE *ofp)
{
	/* Make sure the pointer is properly aligned (or we will 
	 * dump core 
	 */
	if ((unsigned)ptr % 8) {
		fprintf(ofp, "ILLEGAL ADDRESS (%x)", ptr);
	}

	if (flags & K_HEX) {
		fprintf(ofp, "0x%llx", *(long long *)ptr);
	}
	else if (flags & K_OCTAL) {
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
	/* Make sure the pointer is properly aligned (or we will 
	 * dump core 
	 */
	if ((unsigned)ptr % 8) {
		fprintf(ofp, "ILLEGAL ADDRESS (%x)", ptr);
	}

	if (flags & K_HEX) {
		fprintf(ofp, "0x%llx", *(long long *)ptr);
	}
	else if (flags & K_OCTAL) {
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
	/* Make sure the pointer is properly aligned (or we will 
	 * dump core 
	 */
	if ((unsigned)ptr % 8) {
		fprintf(ofp, "ILLEGAL ADDRESS (%x)", ptr);
	}

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
