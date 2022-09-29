
/* Parameter table - store mtune and stune */
struct tune {
	char		*tname;		/* name of tunable parameter */
	long long	def;		/* default value */
	long long	tmin;		/* minimum value */
	long long	tmax;		/* maximum value */
	long long	svalue;		/* value from stune */
	short		conf;		/* was it specified in stune? */
	char		type;		/* 32 or 64 bits */
	struct tune	*next;		/* next tune struct in tune_info list */
};

#define TUNE_TYPE_32	1
#define TUNE_TYPE_64	2

/* group table */
struct tunemodule {
	struct tune		*t_tunep;
	int			t_count;
	char			*t_gname;
	unsigned char 		t_flags;
	struct tunemodule 	*t_next;
};

struct stune {			/* System structure for tunable parameters */
	char		s_name[1024];	/* name of tunable parameter */
	long long	s_value;	/* number specified */
	char		s_type;		/* 32 or 64 bits */
};

#define LSIZE 1024
#define BSIZE 128

#define V_NOMATCH       0
#define V_VARIABLE 	1
#define V_GROUP		2

#define READMODE  0
#define WRITEMODE 1
#define ALLMODE   2
#define HELP   3
