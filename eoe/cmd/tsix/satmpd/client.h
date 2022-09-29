#ifndef CLIENT_HEADER
#define CLIENT_HEADER

#ident "$Revision: 1.1 $"

struct sockaddr_in;

int  configure_client (const char *);
void deconfigure_client (void);

int receive_init_request (init_request *, init_reply *);
int receive_init_reply (init_reply *);

int receive_get_attr_reply (get_attr_reply *);
int xlate_get_attr_reply (get_attr_request *, get_attr_reply *);
int receive_get_tok_reply (get_tok_request *, get_tok_reply *);
int receive_flush_remote_cache (u_int);

int create_init_request (init_request *);
int create_loopback_reply (init_reply *);

char *attr_to_domain (u_int, u_short);

int get_client_generation (u_int, u_int *);
int get_client_server (u_int, struct sockaddr_in *);

#endif /* CLIENT_HEADER */
