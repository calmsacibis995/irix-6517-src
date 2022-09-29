#include <sys/types.h>
#include <stdio.h>

#include "cmd.h"
#include "oobmsg.h"
#include "error.h"

/*
 * cmd_ping
 *
 *   Sends a NOP message to the MMSC and waits for a response.
 *   A NOP message is used because it consists of a string of
 *   mostly innoctuous non-printing characters.
 */

int cmd_ping(mmsc_t *m)
{
    return mmsc_transact(m, OOB_NOP, 0, 0, 0, 0, 0);
}

/*
 * cmd_init
 *
 *   Sends our protocol version to MMSC.  Receives the MMSC's
 *   protocol version.  Decides which protocol to use as the
 *   minimum of these two (the MMSC does likewise).
 */

int cmd_init(mmsc_t *m)
{
    int			r;
    u_char		proto_out[1], proto_in[1];
    int			proto_len;

    proto_out[0] = MMSC_PROTOCOL_VERSION;

    if ((r = mmsc_transact(m,
			   MMSC_INIT, proto_out, 1,
			   proto_in, 1, &proto_len)) < 0)
	warning("Could not initialize MMSC: %s\n", mmsc_errmsg(r));
    else {
	r = (int) proto_in[0];

	if (r > MMSC_PROTOCOL_VERSION)
	    r = MMSC_PROTOCOL_VERSION;
    }

    return r;
}

int cmd_graph_start(mmsc_t *m, int bars, int depth)
{
    int			r;
    u_char		data[2];

    data[0] = (u_char) bars;
    data[1] = (u_char) depth;

    if ((r = mmsc_transact(m,
			   GRAPH_START, data, 2,
			   0, 0, 0)) < 0)
	warning("Could not start graph: %s\n", mmsc_errmsg(r));

    return r;
}

int cmd_graph_end(mmsc_t *m)
{
    int			r;

    if ((r = mmsc_transact(m,
			   GRAPH_END, 0, 0,
			   0, 0, 0)) < 0)
	warning("Could not end graph: %s\n", mmsc_errmsg(r));

    return r;
}

int cmd_graph_label(mmsc_t *m, int graph_label_op, char *s)
{
    int			r;

    if ((r = mmsc_transact(m,
			   graph_label_op, (u_char *) s, strlen(s) + 1,
			   0, 0, 0)) < 0)
	warning("Could not set graph label %s: %s\n", s, mmsc_errmsg(r));

    return r;
}

int cmd_graph_data(mmsc_t *m, u_char *data, int data_size, int bars)
{
    int			r;

    /*
     * Note: due to a hack in load.c:load_graph(), we can use the two
     * bytes before the load buffer.  Otherwise, we'd have to copy
     * it all into a new buffer just to add the two bytes in front.
     */

    data[-2] = 0;		/* Start bar */
    data[-1] = bars;		/* Bar count */

    if ((r = mmsc_transact(m,
			   GRAPH_DATA, data - 2, data_size + 2,
			   0, 0, 0)) < 0)
	warning("Could not send graph data: %s\n", mmsc_errmsg(r));

    return r;
}
