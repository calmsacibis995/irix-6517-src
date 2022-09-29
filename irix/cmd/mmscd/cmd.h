#ifndef _cmd_h
#define _cmd_h

#include "mmsc.h"

int		cmd_ping(mmsc_t *m);
int		cmd_init(mmsc_t *m);
int		cmd_graph_start(mmsc_t *m, int bars, int depth);
int		cmd_graph_end(mmsc_t *m);
int		cmd_graph_label(mmsc_t *m, int graph_label_op, char *s);
int		cmd_graph_data(mmsc_t *m,
			       u_char *data, int data_size, int bars);

#endif /* _cmd_h */
