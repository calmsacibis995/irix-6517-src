int server_fcntl( fcntlargs *fap, CLIENT *clnt );
int server_flock( flockargs *fap, CLIENT *clnt );
int server_lockf( lockfargs *lap, CLIENT *clnt );
int set_server_opts( servopts *sop, CLIENT *clnt );
bool_t server_verify( verifyargs *vap, CLIENT *clnt );
bool_t server_held( pathstr *file_name, CLIENT *clnt );
int reset_server( CLIENT *clnt );
