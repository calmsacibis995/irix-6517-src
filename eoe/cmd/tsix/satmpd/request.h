#ifndef REQUEST_HEADER
#define REQUEST_HEADER

#ident "$Revision: 1.1 $"

/*
 * routines for turning network data into request buffers
 */
int init_request_buffer (const char *, size_t, init_request *);
int init_reply_buffer (const char *, size_t, init_reply *);

int get_tok_request_buffer (const char *, size_t, get_tok_request *);
int get_tok_reply_buffer (const char *, size_t, get_tok_reply *);

int get_attr_request_buffer (const char *, size_t, get_attr_request *);
int get_attr_reply_buffer (const char *, size_t, get_attr_reply *);
int get_attr_reply_failed (u_char, get_attr_request *, get_attr_reply *);

int get_lrtok_request_buffer (const char *, size_t, get_lrtok_request *);

int host_cmd_buffer (const char *, size_t, host_cmd *);
int get_attr_admin_buffer (const char *, size_t, host_cmd *,
			   get_attr_request *);
int get_tok_admin_buffer (const char *, size_t, host_cmd *, get_tok_request *);

void destroy_init_request (init_request *);
void destroy_init_reply (init_reply *);

void destroy_get_attr_request (get_attr_request *);
void destroy_get_attr_reply (get_attr_reply *);

void destroy_get_tok_request (get_tok_request *);
void destroy_get_tok_reply (get_tok_reply *);

void destroy_get_lrtok_request (get_lrtok_request *);
void destroy_get_lrtok_reply (get_lrtok_reply *);

#endif /* REQUEST_HEADER */
