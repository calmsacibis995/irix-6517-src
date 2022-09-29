int valid_parameters( struct parameters *paramp );
void error( pid_t pid, char *func, char *syscall, char *msg );
enum access_method str_to_access( char *str );
int get_parameters( char *paramstr, struct parameters *paramp );
