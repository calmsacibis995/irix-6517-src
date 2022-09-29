#ifndef _mmsc_h
#define _mmsc_h

#define MMSC_CMD_MAX		1536
#define MMSC_ESCAPE		0xa0
#define MMSC_TIMEOUT		5	/* Seconds			     */

#define MMSC_PROTOCOL_VERSION	1

#define MMSC_ERROR_NONE		0	/* No error                          */
#define MMSC_ERROR_RESPLEN     -1	/* MMSC response too long            */
#define MMSC_ERROR_TIMEOUT     -2	/* Timed out waiting for MMSC resp.  */
#define MMSC_ERROR_CHECKSUM    -3	/* MMSC response checksum error      */
#define MMSC_ERROR_GRAB	       -4	/* Could not grab MMSC device        */
#define MMSC_ERROR_RELEASE     -5	/* Could not release MMSC device     */
#define MMSC_ERROR_MMSC	       -6	/* MMSC generated error (see syslog) */

typedef struct mmsc_s mmsc_t;

mmsc_t	       *mmsc_open(char *name, int baud, int console);
int		mmsc_baud_get(mmsc_t *m);
int		mmsc_close(mmsc_t *m);
void		mmsc_warn(mmsc_t *m);

int		mmsc_write(mmsc_t *m,
			   u_char *u_data, int data_len);
int		mmsc_read(mmsc_t *m,
			  u_char *data, int data_max, int *data_len,
			  double expire);

int		mmsc_flush(mmsc_t *m);
int		mmsc_putc(mmsc_t *m, int c);
int		mmsc_puts(mmsc_t *m, char *s, int len);
int		mmsc_getc(mmsc_t *m, int block);

int		mmsc_command(mmsc_t *m, int opcode,
			     u_char *u_data, int data_len);
int		mmsc_response(mmsc_t *m, int *status,
			      u_char *data, int data_max, int *data_len,
			      double expire);

int		mmsc_transact(mmsc_t *m,
			      int opcode,
			      u_char *cmd_data,
			      int cmd_data_len,
			      u_char *resp_data,
			      int resp_data_max,
			      int *resp_data_len);

char	       *mmsc_errmsg(int code);

#endif /* _mmsc_h */
