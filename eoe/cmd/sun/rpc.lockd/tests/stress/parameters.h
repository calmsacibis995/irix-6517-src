#define SET_DEFAULTS( op )  *(op) = OptDefaults

#define FILENAME		0
#define DIRNAME			1
#define PROCESSES		2
#define FILESIZE		3
#define ACCESS			4
#define PAUSETIME		5
#define RECSIZE			6
#define REPEAT			7
#define MONTIME			8
#define LOCKTIMEO		9

enum access_method { RANDOM, SEQUENTIAL, BADMETHOD };

struct range {
	long	r_low;
	long	r_high;
};

struct parameters {
	char				*p_file;
	char				*p_dir;
	off_t				p_filesize;
	long				p_processes;
	enum access_method	p_access;
	long				p_pausetime;
	struct range		p_recsize;
	long				p_repeat;
	long				p_montime;
	long				p_locktimeout;
	int					p_signum;
	long				p_killtime;
};

extern struct parameters OptDefaults;
