#define LBER_BUF_SIZE	8192

#define LBER_TYPE_BOOL	0x01
#define LBER_TYPE_INT	0x02
#define LBER_TYPE_BIT	0x03
#define LBER_TYPE_OCTET	0x04
#define LBER_TYPE_NULL	0x05
#define LBER_TYPE_OID	0x06
#define LBER_TYPE_ENUM	0x0a
#define LBER_TYPE_SEQ	0x10
#define LBER_TYPE_SET	0x11
#define LBER_TYPE_PSTR	0x13
#define LBER_TYPE_TSTR	0x14
#define LBER_TYPE_ISTR	0x16
#define LBER_TYPE_TIME	0x17

#define LBER_CONST	0x20
#define LBER_APP	0x40
#define LBER_CONTEXT	0x80

#define LBER_OPT_SSL	0x01

typedef unsigned char	obj_bool;
typedef long		obj_int;
typedef unsigned char	obj_enum;
typedef time_t		obj_time;

typedef struct _obj_bit {
	int		pad;
	long		len;
	char		*data;
} obj_bit;

typedef struct _obj_string {
	long		len;
	char		*data;
} obj_string;

typedef struct _obj_oid {
	int		num;
	long		*data;
} obj_oid;

typedef struct _sockbuf {
	int		fd;
	int		opt;
	char		*ptr;
	char		*end;
	char		*buf;
	int		size;
	void		*ssl;
} sockbuf;

struct ber {
	unsigned long	tag;
	unsigned long	length;
	char		*data;
	char		*ptr;
	char		*end;
} ;

struct iovec_w {
	struct iovec	iov;
	struct iovec_w	*next;
} ;

struct obj {
	unsigned long	type;
	unsigned long	atype;
	void 		*data;
	struct obj	*next;
} ;

int ber_encode(struct iovec_w **ret, struct obj *o);
