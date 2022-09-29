#ident "$Header: "

/* Function prototypes from libutil/kl_print.c
 */

void print_bit_value(
	k_ptr_t		/* pointer to value */, 
	int			/* byte size */, 
	int 		/* bit_size */, 
	int 		/* bit offset */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_char(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_uchar(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_int2(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_uint2(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_int4(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_uint4(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_float4(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_int8(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_uint8(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_float8(
	k_ptr_t		/* pointer to value */, 
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);

void print_base(
	k_ptr_t		/* pointer to value */, 
	int			/* size */,
	int			/* encoding */,
	int			/* flag (K_OCTAL, K_HEX, or K_DECIMAL) */, 
	FILE *		/* output file pointer */);
