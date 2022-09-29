/*
 * XDR example
 *
 *  Create a pipe.  Bind an xdrrec stream to each end of the pipe.
 *  Fork.  Child: write a dynamically generated, user-defined
 *  structure to the pipe. Parent: read the pipe, generate the structure,
 *  print its contents.
 */

#include <assert.h>
#include <rpc/rpc.h>

int
readproc(void *closure, void *vbuffer, u_int nbytes)
{
    int fd = * (int *) closure;
    char *buffer = (char *) vbuffer;
    int nb;

    nb = read(fd, buffer, nbytes);
    if (nb == 0)
	nb = -1;
    return nb;
}

int
writeproc(void *closure, void *vbuffer, u_int nbytes)
{
    int fd = * (int *) closure;
    char *buffer = (char *) vbuffer;

    return write(fd, buffer, nbytes);
}

bool_t
xdr_fibonacci(XDR *xdrs, unsigned int n)
{
    int i, a, b;

    for (i = 0, a = b = 1; i < n; i++)
    {   a += b;
	b = a - b;
    }
    assert(xdrs->x_op == XDR_ENCODE);
    return xdr_int(xdrs, &b);
}

bool_t
xdr_generate(XDR *xdrs,
	     unsigned int *np,
	     bool_t (*generator)(XDR *, unsigned int))
{
    unsigned int i;

    if (xdr_u_int(xdrs, np))
	for (i = 0; i < *np; i++)
	    if (!(*generator)(xdrs, i))
		return FALSE;
    return TRUE;
}

void
produce(XDR *xdrs, unsigned int n)
{
    /* XDR record initialization */

    xdrs->x_op = XDR_ENCODE;

    /* Generate and transmit first n Fibonacci numbers. */

    (void) xdr_generate(xdrs, &n, xdr_fibonacci);

    /* XDR record termination */

    xdrrec_endofrecord(xdrs, TRUE);
}

bool_t
consume(XDR *xdrs)
{
    unsigned int n;
    int *numbers;
    int i;

    /* XDR record initialization */

    xdrs->x_op = XDR_DECODE;
    if (!xdrrec_skiprecord(xdrs))
	return FALSE;

    /* Receive array of numbers. */

    numbers = NULL;
    if (xdr_array(xdrs, (caddr_t *) &numbers, &n, -1, sizeof *numbers, xdr_int))
    {
	/* Print the received numbers. */

	for (i = 0; i < n; i++)
	    printf("%d ", numbers[i]);
	printf("\n");

	/* Throw away the numbers. */

	xdrs->x_op = XDR_FREE;
	(void) xdr_array(xdrs, (caddr_t *) &numbers, &n, n, sizeof *numbers, xdr_int);
	return TRUE;
    }
    return FALSE;
}

void
producer(int fd)
{
    XDR xstream;

    /* XDR initialization */

    xdrrec_create(&xstream, 0, 0, &fd, readproc, writeproc);
    
    produce(&xstream, 7);
    produce(&xstream, 4);
    produce(&xstream, 20);

    /* XDR termination */

    xdr_destroy(&xstream);
}

void
consumer(int fd)
{
    XDR xstream;

    /* XDR initialization */

    xdrrec_create(&xstream, 0, 0, &fd, readproc, writeproc);

    while (consume(&xstream))
	;

    /* XDR termination */

    xdr_destroy(&xstream);
}

int
main()
{
    int organ[2];
    int pid;

    if (pipe(organ) < 0)
	perror("pipe"), exit(1);

    pid = fork();
    if (pid < 0)
	perror("fork"), exit(1);

    if (pid == 0)
    {   (void) close(organ[0]);
	producer(organ[1]);
	exit(0);
    }
    else
    {   (void) close(organ[1]);
	consumer(organ[0]);
	(void) wait(0);
    }
    return 0;
}
