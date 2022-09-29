#ifndef SERVER_HEADER
#define SERVER_HEADER

#ident "$Revision: 1.1 $"

extern u_int server_generation;

int  configure_server (const char *);
void deconfigure_server (void);

char *attribute_to_text (u_short);
u_short attribute_from_text (const char *);

int receive_get_attr_request (get_attr_request *, get_attr_reply *);
int receive_get_tok_request (get_tok_request *, get_tok_reply *);
int receive_get_lrtok_request (get_lrtok_request *, get_lrtok_reply *);

int attr_to_token (const char *, u_short, void *, u_int *);

#endif /* SERVER_HEADER */
