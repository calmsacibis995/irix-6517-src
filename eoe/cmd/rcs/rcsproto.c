#include "rcspriv.h"
#include <sys/types.h>		/* size_t, time_t, mode_t  */
#include <stdio.h>		/* BUFSIZ */
#include "rcsapi.h"		/* RCSCONN, RCSTMP */

static int getpacket(RCSSTREAM *fd, void **data, size_t nbytes);

#define MAXPKT 32767

/* Sendfile is used on the server side, as a front end to rcsReadOutData
 * (to tranport a file from the server to the client)
 */
	int
sendfile(fd, handle)
	int fd;		/* output file descriptor */
	RCSTMP handle;	/* temp file handle */
{
	/* Given an I/O stream on a file descriptor and a handle,
	 * write the file to the client stream.
	 *
	 * Zero on success, negative error code on failure.
	 */

	int rc1, rc2;

	/* The context for sendpacket() is a file descriptor */
	 rc1 = rcsReadOutData(sendpacket, (RCSSTREAM *)fd, NULL, handle);

	/* rcsReadOutData merely writes the stream; it does not indicate EOF */
	rc2 = sendpacket((RCSSTREAM *)fd, NULL, 0);

	return rc1 >= 0 ? rc2 : rc1;
}

/* Sendstream is used on the client side, as a front end to a user-provided
 * routine to read a stream (it transports a stream from the client to
 * a server file)
 */
	int
sendstream(readMethod, inputStream, conn, mtime, mode, fname, flags)
	int readMethod(RCSSTREAM *inputStream, void **ptr, size_t nbytes);
	RCSSTREAM *inputStream;
	RCSCONN conn;
	time_t mtime;
	mode_t mode;
	const char *fname;
	int flags;
{
	/* Ship a file to the other end of the wire (RCS_FTP_PUT message).
	 *
	 * Returns handle (as int) to tmp file on success, negative error codes.
	 *
	 * readMethod	- routine for reading input stream
	 * inputStream	- context passed to readMethod()
	 * conn		- representation of connection (currently socket fd)
	 * mtime	- modification time of input stream
	 * mode		- mode of input stream (notably r/x bits)
	 * fname	- full path to target (recipient) file (or NULL)
	 * flags	- FILE_TMP - file is temporary (upload)
	 *		- ???
	 */
	
	struct sendFileHdr hdr;
	unsigned char ch;
	unsigned short fnamelen;
	void *ptr;
	int bytes_written;
	int bytes_read;
	int rc;

	/* Indicate message type */
	ch = RCS_FTP_PUT;
	if (write(conn, &ch, 1) < 1) return ERR_NETWORK_DOWN;

	/* Fill data in header */
	hdr.mtime = htonl(mtime);
	hdr.mode  = htonl(mode);
	hdr.flags = htonl(flags);
	hdr.fnamelen = fname ? htonl(fnamelen = (strlen(fname)+1)) : 0;
	if (write(conn, &hdr, sizeof(hdr)) < sizeof(hdr))
		return ERR_NETWORK_DOWN;

	/* Send file name, if any */
	if (fname && write(conn, fname, fnamelen) < 0) return ERR_NETWORK_DOWN;

	/* Send packets as needed */
	while ((bytes_read = readMethod(inputStream, &ptr, MAXPKT)) > 0) {
	    do {
		bytes_written = sendpacket((RCSSTREAM *)conn, ptr, bytes_read);
		if (bytes_written <= 0) {
		    close((int)conn);
		    return ERR_NETWORK_DOWN;
		}
		bytes_read -= bytes_written;
		ptr = (char *)ptr + bytes_written;
	    } while(bytes_read > 0);
	}

	/* Send EOF */
	sendpacket((RCSSTREAM *)conn, NULL, 0);

	/* Get completion acknowledgement from other side (which may
	 * have consumed bytes without having any place to put them,
	 * so we may get an error code).
	 */
	rc = (int)getrc(conn, 0);

	if (bytes_read < 0) {	/* Failed to read entire stream */
	    /* Improved error recovery:
	     * if (rc >= 0) rcsCloseTmp(conn, (RCSTMP)rc);
	     */
	    return ERR_READ_STREAM;
	}
	return rc;
}


	int
sendpacket(outputStream, data, nbytes)
	RCSSTREAM *outputStream;	/* this stream's ctx is the fd */
	const void *data;		/* data to send in a single pkt */
	size_t nbytes;			/* size of packet */
{
	/* Sends a packet, and returns the number of bytes written.
	 * Returns negative number(s) on error(s).
	 *
	 * Called with zero bytes means send EOF.
	 * Called with negative bytes means error; terminate connection.
	 */
	unsigned short num;	/* htons of nbytes */
	size_t orig_nbytes;	/* original value of nbytes */
	size_t bytes_wrote;	/* num of bytes written in last write */

	/* File packet:
	 * Bytes 0-1:	number of bytes of data in this packet (htons order)
	 *		(zero means no more data in this file)
	 * Bytes 2-N:	data (N-2 == number of bytes of data)
	 *
	 * This is needed because we may not know the size of the stream
	 * up front, so we can only indicate the number of bytes we are
	 * writing SO FAR.  Since the receiver cannot otherwise tell where
	 * the file ends, it must rely upon the byte count.  So, we
	 * read some data, write as much as we got so far, and simply
	 * indicate whether there is more to come.
	 */

	if ((orig_nbytes = nbytes) > MAXPKT) orig_nbytes = nbytes = MAXPKT;

	/* Write the number of bytes in this packet */
	num = htons((unsigned short)nbytes);
	if (write((int)outputStream, &num, sizeof(num)) < sizeof(num)) {
	    close((int)outputStream);
	    return ERR_NETWORK_DOWN;
	}

	/* Write the block of data in pieces, if only some bytes are
	 * taken at a time.
	 */
	for (; nbytes > 0;
	      nbytes -= bytes_wrote, data = (char *)data + bytes_wrote) {
	    if ((bytes_wrote = write((int)outputStream, data, nbytes)) <= 0) {
		close((int)outputStream);
		return ERR_NETWORK_DOWN;
	    }
	}
	return orig_nbytes;
}

#define flushinput(fd) \
	do { \
	    void *ptr; \
	    while (getpacket((RCSSTREAM *)&fd, &ptr, MAXPKT) > 0); \
	} while(0)


/* Getfile is used on the server side, as a front end to rcsReadInData
 * (to tranport a file from the client to the server)
 */
	int
getfile(fd, handle)
	int fd;		/* input file descriptor */
	RCSTMP *handle;	/* temp file handle returned (if temp file) */
{
	/* Given an input stream on a file descriptor, decode file
	 * information, and create the file appropriately.
	 *
	 * For temp files, hands back an RCSTMP handle.
	 * For permanent "files", expects the static pointers
	 *
	 * Zero on success, negative error code on failure.
	 */

	struct sendFileHdr filehdr;
	char fname[MAXPATHLEN];
	int rc;
	struct rcsStreamOpts rdopts;

	/* Parse the incoming header */
	if ((rc = getfilehdr(fd, &filehdr, fname)) < 0) return rc;
 
	rdopts.mtime = filehdr.mtime;
	rdopts.mode = filehdr.mode;
	rdopts.fname = filehdr.fnamelen ? fname : NULL;

			/* only allow temp files on server */
	if (!(filehdr.flags & FILE_TMP)) {
	    flushinput(fd);
	    return ERR_INTERNAL;	/* stubs should never have sent this */
	}
	else {
	    /* The context for getpacket() is a file descriptor */
	    return rcsReadInData(getpacket, (RCSSTREAM *)&fd, NULL, &rdopts,
				 handle);
	}
}

/* Getstream is used on the client side, as a front end to a user-provided
 * routine to write a stream (it transports a file from the server to
 * the client stream)
 */
	int
getstream(fd, writeMethod, outStream)
	int fd;					/* socket */
	int (*writeMethod)(RCSSTREAM *, const void *, size_t);
	RCSSTREAM *outStream;			/* context for writeMethod */

{
	/* Given an input stream on a file descriptor, and an output
	 * stream upon which to write it, do so.  Unlike getfile(),
	 * there is no filehdr info in the stream (since the client
	 * already knew the file attributes, and made the output stream
	 * already).
	 */

	void *ptr;
	int bytes_read;
	int bytes_written;
	int rc;

	/* We should have done all the error checking up front, on client
	 * side, so that any unforseen failure is an OS failure (can't
	 * open file that should be there), network failure, etc.  So,
	 * we don't get an intermediate status return code here; just
	 * read the stream immediately.
	 */

	/* Iterate on reading from stream; for each read,
	 * iterate on writing the data, until the read data are completely
	 * written.
	 */
	while ((bytes_read = getpacket((RCSSTREAM*)&fd, &ptr, MAXPKT)) > 0)
	    {
	    do {
		bytes_written = writeMethod(outStream, ptr,
					      bytes_read);
		if (bytes_written <= 0) {
		    flushinput(fd);
		    return ERR_WRITE_STREAM;
	        }
	        bytes_read -= bytes_written;
	        ptr = (char *)ptr + bytes_written;
	    } while(bytes_read > 0);
	}
	if (bytes_read < 0) return ERR_NETWORK_DOWN;
	return 0;
}
		


	int
getfilehdr(fd, filehdr, fname)
	int fd;
	struct sendFileHdr *filehdr;
	char fname[MAXPATHLEN];
{
	/* Utility routine to help getting a file.
	 * This routine reads in the header, converts to host
	 * format, and passes back the data, including the
	 * file name (if any).
	 *
	 * If there is any error (other than ERR_NETWORK_DOWN,
	 * which means nothing more can be read anyway), this
	 * will flush the entire file input.
	 */
	struct sendFileHdr hdr;
	int fnamelen;

	/* First read header in network format */
	if (read(fd, &hdr, sizeof(hdr)) < sizeof(hdr))
		return ERR_NETWORK_DOWN;

	/* Next, convert to native format */
	filehdr->mtime = ntohl(hdr.mtime);
	filehdr->mode =  ntohl(hdr.mode);
	filehdr->flags = ntohl(hdr.flags);
	fnamelen = filehdr->fnamelen = ntohs(hdr.fnamelen);

	/* If there is a file name, fetch it */
	if (fnamelen > 0) {
		if (read(fd, fname, fnamelen) < fnamelen) {
		    return ERR_NETWORK_DOWN;
		}
		if (fname[fnamelen-1]) {
		    flushinput(fd);
		    return ERR_INTERNAL;	    /* not null-terminated */
		}
	}
	return 0;
}

	static int
getpacket(fd, data, nbytes)
	RCSSTREAM *fd;		/* pointer to file descriptor */
	void **data;		/* return the address of the data */
	size_t nbytes;		/* max number of bytes to fetch */
{
	/* Returns a pointer to the next part of a file packet, along
	 * with its size in bytes.  Never returns more than the next packet.
	 *
	 * Zero indicates EOF, a negative return is an error code.
	 *
	 * This is an internal utility.  It must be called repeatedly,
	 * until it returns a non-positive value.  Otherwise, it will
	 * get out of sync with the socket stream.
	 *
	 * To allow this routine to be used with multiple sockets,
	 * add one parameter: *pktleft (the number of bytes left in
	 * the current packet).  This will allow the routine to communicate
	 * its context with the caller.  That way, it does not need to
	 * remember across calls how much remains in the current packet.
	 */

	static int pktleft = 0; /* remaining bytes in current pkt */
	static char buf[BUFSIZ * 8];

	if (!pktleft) {			/* need to read next packet */
	    unsigned short pktlen;
	    if (read(*(int *)fd, &pktlen, sizeof(pktlen)) != sizeof(pktlen))
		return ERR_NETWORK_DOWN;

	    if (!(pktleft = ntohs(pktlen))) /* empty packet means EOF */
		return 0;
	}
	
	/* Number of bytes to read cannot exceed the bytes in the
	 * packet (this routine does not cross packet boundaries)
	 */
	if (nbytes > pktleft)
	    nbytes = pktleft < sizeof(buf) ? pktleft : sizeof(buf);
	else if (nbytes > sizeof(buf))
	    nbytes = sizeof(buf);

	if ((nbytes = read(*(int *)fd, buf, nbytes)) <= 0)
	    return ERR_NETWORK_DOWN;
	pktleft -= nbytes;
	*data = buf;
	return nbytes;
}

	unsigned long
getrc(conn, typeknown)
	RCSCONN conn;
	int typeknown;	/* true if we have already decoded type of message */
{
	/* wait for, and receive, a return code from the other side.
	 * A return code may be zero (success),
	 * negative (an error),
	 * or positive (a single value returned; success is implied)
	 *
	 * This routine is called either in anticipation of a return
	 * code message, or after having determined that the incoming
	 * message is indeed a return code.  The difference is that
	 * in the latter case, the first byte of the message has already
	 * been read; it indicated that the message was a return code.
	 */
	    
	long unsigned ul;
	unsigned char ch;
	unsigned long rc;

	if (!typeknown) {  /* Read, verify the type of the message */

	    if (read(conn, &ch, 1) < 1) return ERR_NETWORK_DOWN;

	    /* if the type is wrong, it is actually ERR_INTERNAL,
	     * but we pass back ERR_NETWORK_DOWN, because once we
	     * are out of sync, the application might as well terminate
	     * the session.
	     */
	    if (ch != RCS_RETURN_CODE) return ERR_NETWORK_DOWN;
	}

	if (read(conn, &ul, sizeof(ul)) < sizeof(ul)) return ERR_NETWORK_DOWN;
	switch(rc = ntohl(ul)) {

	case ERR_FATAL:
	case ERR_NETWORK_DOWN:
		/* The server is in a corrupt state, and cannot process
		 * any more requests.
		 */
	    rcsCloseConnection(conn);
	    /* fall through vvv */

	default:
	    return rc;
	}
}


	int
rcsGetShortString(fd, buf)
	int fd;
	char *buf;
{
	/* Gets a "short" string (one of 255 or fewer bytes, as told
	 * by the first char of the stream.  The length of the string
	 * includes the terminating '\0', so that "" has a length of one,
	 * while a NULL string (no value, not "") has a length of zero.
	 *
	 * Returns the length of the string received. negative code on error.
	 */
	unsigned char ch;
	if (read(fd, &ch, 1) <= 0) return ERR_NETWORK_DOWN;

	/* Allow the transmission of null strings, via length zero
	 * (length of 1 is "").
	 */
	if (ch > 0) {if (read(fd, buf, ch) != ch) return ERR_NETWORK_DOWN;}
	else buf[0] = '\0';	/* in case caller can treat "" and NULL same */
	
	return ch;
}

	int
rcsPutShortString(fd, buf)
	int fd;
	const char *buf;
{
	/* Given a null terminated string, write data in a format that
	 * getshortstring() can analyze, viz. a single byte (length),
	 * representing the length, including the terminating '\0';
	 * then the string itself (including the '\0').  If buf is
	 * NULL, then writes a length of zero.
	 *
	 * Returns: length written, or negative error if error.
	 */
	unsigned char len = buf ? strlen(buf)+1 : 0;
	if (write(fd, &len, sizeof(len)) != sizeof(len))
						 return ERR_NETWORK_DOWN;
	if (len) if (write(fd, buf, len) != len) return ERR_NETWORK_DOWN;
	return len;
}

	int
rcsPutLongString(fd, buf)
	int fd;
	const char *buf;
{
	/* Given a null terminated string, write data in a format that
	 * getlongstring() can analyze, viz. a short length (in network
	 * format), representing the length, including the terminating '\0';
	 * then the string itself (including the '\0').  If buf is NULL,
	 * then writes a length of zero.
	 *
	 * Returns: length written, or -1 if error.
	 */
	short len = buf ? strlen(buf)+1 : 0;
	short msglen;

	msglen = htons(len);
	if (write(fd, &msglen, sizeof(len)) != sizeof(len))
						 return ERR_NETWORK_DOWN;
	if (len) if (write(fd, buf, len) != len) return ERR_NETWORK_DOWN;
	return len;
}

	int
rcsSendRC(fd, rc)
	int fd;		/* socket file descriptor */
	int rc;		/* return code to be sent */
{
	/* Send a return code:
	 * Byte 0:     RCS_RETURN_CODE
	 * Bytes 1-4:  htonl format of return code
	 */
	char buf[sizeof(char) + sizeof(unsigned long)];
	unsigned long ul;
	ul = htonl((unsigned long)(long)rc);
	
	buf[0] = RCS_RETURN_CODE;
	memcpy(buf+sizeof(char), &ul, sizeof(unsigned long));
	if (write(fd, buf, sizeof(buf)) != sizeof(buf)) return ERR_NETWORK_DOWN;
	return 0;
}
