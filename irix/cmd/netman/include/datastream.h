#ifndef DATASTREAM_H
#define DATASTREAM_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Byte-ordered data streams.
 * Integer data may be represented using big or little endian byte order,
 * without alignment or quantization.  Floating point data is IEEE.
 */

struct protofield;
struct expr;

enum dsdirection { DS_DECODE, DS_ENCODE };
enum dsbyteorder { DS_BIG_ENDIAN, DS_LITTLE_ENDIAN };
enum dsextendhow { DS_SIGN_EXTEND, DS_ZERO_EXTEND };
enum dsseekbasis { DS_ABSOLUTE, DS_RELATIVE };

typedef struct datastream {
	struct dsops		*ds_ops;	/* see below */
	enum dsdirection	ds_direction;	/* whether to get or to put */
	int			ds_size;	/* initial byte count */
	int			ds_count;	/* bytes remaining */
	int			ds_bitoff;	/* bit offset in next byte */
	unsigned char		*ds_next;	/* next byte to get/put */
} DataStream;

struct dsops {
	enum dsbyteorder dso_byteorder;
	int	(*dso_bits)(DataStream*, long*, unsigned int, enum dsextendhow);
	int	(*dso_short)(DataStream *, short *);
	int	(*dso_long)(DataStream *, long *);
	int	(*dso_float)(DataStream *, float *);
	int	(*dso_double)(DataStream *, double *);
};

#define	DefineDataStreamOperations(name,tag,order) \
int	makeident2(tag,_bits)(DataStream*,long*,unsigned int,enum dsextendhow);\
int	makeident2(tag,_short)(DataStream *, short *); \
int	makeident2(tag,_long)(DataStream *, long *); \
int	makeident2(tag,_float)(DataStream *, float *); \
int	makeident2(tag,_double)(DataStream *, double *); \
struct dsops name = { \
	order, \
	makeident2(tag,_bits), \
	makeident2(tag,_short), \
	makeident2(tag,_long), \
	makeident2(tag,_float), \
	makeident2(tag,_double) \
}

/*
 * Macros to call the virtual operations.
 */
#define	ds_bits(ds,lp,len,how)	((*(ds)->ds_ops->dso_bits)(ds,lp,len,how))
#define	ds_short(ds, sp)	((*(ds)->ds_ops->dso_short)(ds, sp))
#define	ds_long(ds, lp)		((*(ds)->ds_ops->dso_long)(ds, lp))
#define	ds_float(ds, fp)	((*(ds)->ds_ops->dso_float)(ds, fp))
#define	ds_double(ds, dp)	((*(ds)->ds_ops->dso_double)(ds, dp))

/*
 * Macros for other integral types.
 */
#define	ds_char(ds, cp)		ds_byte(ds, cp)
#define	ds_u_char(ds, cp)	ds_byte(ds, (char *) cp)
#define	ds_u_short(ds, sp)	ds_short(ds, (short *)(sp))
#define	ds_u_long(ds, lp)	ds_long(ds, (long *)(lp))

/*
 * Tell and bitwise tell macros.
 */
#define	DS_TELL(ds)	((ds)->ds_size - (ds)->ds_count)
#define	DS_TELLBIT(ds)	(DS_TELL(ds) * BITSPERBYTE + (ds)->ds_bitoff)

DataStream	*datastream(char *, int, enum dsdirection, enum dsbyteorder);
void		ds_destroy(DataStream *);
void		ds_init(DataStream *, unsigned char *, int, enum dsdirection,
			enum dsbyteorder);
enum dsbyteorder ds_setbyteorder(DataStream *, enum dsbyteorder);
int		ds_seek(DataStream *, int, enum dsseekbasis);
int		ds_seekbit(DataStream *, int, enum dsseekbasis);
int		ds_tell(DataStream *);
int		ds_tellbit(DataStream *);
int		ds_byte(DataStream *, char *);
int		ds_bytes(DataStream *, void *, unsigned int);
int		ds_int(DataStream *, long *, int, enum dsextendhow);
int		ds_field(DataStream *, struct protofield *, int, struct expr *);
char		*ds_inline(DataStream *, unsigned int, int);

#endif
