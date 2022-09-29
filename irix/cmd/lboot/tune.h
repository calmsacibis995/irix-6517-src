
/* Parameter table - store mtune and stune */
struct tune {
	char		*tname;		/* name of tunable parameter */
	long long	def;		/* default value */
	long long	tmin;		/* minimum value */
	long long	tmax;		/* maximum value */
	long long	svalue;		/* value from stune */
	short		conf;		/* was it specified in stune? */
	char		type;		/* 32 or 64 bits */
	int		group;		/* group index */
	struct tune	*next;		/* next tune struct in tune_info list */
};

#define TUNE_TYPE_32	1
#define TUNE_TYPE_64	2

/* group table */
struct tunemodule {
	char			*t_mname;
	char			*t_gname;
	unsigned char 		t_flags;
	char			*t_sanity;
	struct tunemodule 	*t_next;
};

/* System structure for tunable parameters */
struct stune {
	char		s_name[1024];	/* name of tunable parameter */
	long long	s_value;	/* number specified */
	char		s_type;		/* 32 or 64 bits */
};


extern struct tune *tune_info;
extern struct tunemodule *group_info;

