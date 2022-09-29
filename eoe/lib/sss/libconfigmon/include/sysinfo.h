#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/include/RCS/sysinfo.h,v 1.1 1999/05/08 03:19:00 tjm Exp $"

typedef struct system_info_s {
	int             	 rec_key;
	k_uint_t        	 sys_id;
	int					 sys_type;
	char 				*serial_number;
	char				*hostname;	
	char				*ip_address;
	short				 active;
	short				 local;
	time_t				 time;
} system_info_t;

/* Function prototypes
 */
k_uint_t get_sysid(void);
int check_sysid(int);
void print_sysid(void);
system_info_t *get_sysinfo(int, int);
void free_system_info(system_info_t *);
int sysinfo_compare(system_info_t *, system_info_t *);
system_info_t *check_sysinfo(system_info_t *, int, int);
