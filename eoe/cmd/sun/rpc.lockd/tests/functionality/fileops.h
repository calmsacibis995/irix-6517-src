#define OPEN_FLAGS      O_RDWR
#define CREATE_FLAGS    (OPEN_FLAGS | O_CREAT | O_TRUNC)
#define DEFAULT_MODE    \
	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define CREATE_MODE     DEFAULT_MODE

int open_file_init(void);
int open_file(pathstr, int, int);
int close_file(pathstr);
bool_t locks_held(pathstr);
int verify_lock(struct verifyargs *);
void closeall(void);
