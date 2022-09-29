#define INET6
#ifdef INET6
/*
 * Part of IPv6 API.  Functions to manipulate hop-by-hop and destination
 * options.
 */

#ifdef __STDC__
        #pragma weak inet6_option_alloc = _inet6_option_alloc
        #pragma weak inet6_option_append = _inet6_option_append
	#pragma weak inet6_option_find = _inet6_option_find
	#pragma weak inet6_option_init = _inet6_option_init
	#pragma weak inet6_option_next = _inet6_option_next
	#pragma weak inet6_option_space = _inet6_option_space
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <sys/debug.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>

int
inet6_option_space(int nbytes)
{
	return (CMSG_SPACE(nbytes));
}

int
inet6_option_init(void *bp, struct cmsghdr **cmsgp, int type)
{
	if (bp == NULL)
		return (-1);
	if (type != IPV6_HOPOPTS && type != IPV6_DSTOPTS)
		return (-1);
	*cmsgp = (struct cmsghdr *)bp;
	(*cmsgp)->cmsg_level = IPPROTO_IPV6;
	(*cmsgp)->cmsg_type = type;
	(*cmsgp)->cmsg_len = CMSG_LEN(0);/* changed when options are inserted */
	return (0);
}

/*
 * Add a pad option of length 'bytes' starting at datap and return the
 * new position of datap.
 */
static uint8_t *
add_pad_opt(uint8_t *datap, uint8_t bytes)
{
	ASSERT(bytes < 8);
	if (bytes == 0) {	/* check for zero just in case */
		return (datap);
	} else if (bytes == 1) {
		/* pad1 option */
		*datap = 0;
	} else {
		/* padN option */
		datap[0] = 1;  /* type (padN) */
		datap[1] = bytes - 2; /* len */
		/* the rest is required to be zero */
		bzero(&datap[2], bytes - 2);
	}
	return (datap + bytes);
}

/*
 * Common code for inet6_option_append() and inet6_option_alloc().
 */
static uint8_t *
append_common(struct cmsghdr *cmsg, int datalen, const uint8_t *typep,
  int multx, int plusy)
{
	uint8_t *datap, *extlen;
	uint8_t *newp, *return_datap;
	long bytes;

	/*
	 * datalen does not include the type or len field so add it in
	 * now.
	 */
	datalen += 2;

	if (cmsg == NULL)
		return (NULL);
	if (multx != 1 && multx != 2 && multx != 4 && multx != 8)
		return (NULL);
	if (plusy < 0 || plusy > 7)
		return (NULL);
	/* XXX spec doesn't require this but it seems logical */
	if (plusy >= multx)
		return (NULL);

	datap = (uint8_t *)cmsg +_ALIGN((cmsg)->cmsg_len);  /* go to end */
	if (cmsg->cmsg_len == CMSG_LEN(0)) {
		/*
		 * First time called.  We need to reserve space for the
		 * one byte next header field and the one byte header
		 * extension length.
		 */
		datap += 2;
	}
	
	/*
	 * See how much padding, if any, we need before the option.
	 * XXX Since we align to an 8 byte boundary at the end of each
	 * option we only need this for the first option.
	 */
	if (((ulong_t)datap & (multx - 1)) != plusy) {
		newp = (uint8_t *)((((ulong_t)datap + (multx - 1)) &
		  ~(multx - 1)) + plusy);
		bytes = (long)(newp - datap);
		if (bytes != 0) {
			/* need to insert a pad option. */
			datap = add_pad_opt(datap, (uint8_t)bytes);
		}
	}

	/* datap now points at the right place. */

	if (typep)
		bcopy(typep, datap, datalen);  /* inet6_option_append case */

	return_datap = datap;
	datap += datalen;

	/*
	 * The total size of the extension header must be a multiple of 8
	 * bytes.  Thus, we need to check to see if a pad option should
	 * be added to the end to round up the size.
	 * XXX Note, another option could be appended after this one so we
	 * may not really need the pad but we don't know if this is the last
	 * option or not so we add the pad.  If we really want to be careful
	 * about space we could see if there is a pad option in front of
	 * the option we are about to add and possibly reduce it or eliminate
	 * it.  We would do this up above where we called add_pad_opt() the
	 * first time.  If there was a pad option at the end of the extension
	 * header we would see if we could reduce the size of it to get the
	 * alignment we want instead of calling add_pad_opt().
	 */
	if ((ulong_t)datap & 0x7) {
		/* not ending on an 8 byte boundary so add a pad option. */
		datap = add_pad_opt(datap,
		  (uint8_t)(8L - ((long)datap & 0x7L)));
	}

	/*
	 * Last thing.  We need to update the length field in the cmsghdr
	 * and the length field of the extension header.
	 */

	cmsg->cmsg_len = CMSG_LEN(datap - CMSG_DATA(cmsg));

	/*
	 * Get a pointer to the byte that contains the extension header length.
	 * The first 2 bytes are next header and len so assign pointer to
	 * point at the second length.  Extension header length is
	 * measured in 8 byte units not counting first 8 bytes.
	 */
	extlen = CMSG_DATA(cmsg) + 1;
	*extlen = datalen / 8 - 1;

	return (return_datap);
}

int
inet6_option_append(struct cmsghdr *cmsg, const uint8_t *typep,
  int multx, int plusy)
{
	/*
	 * typep points at an area of memory that contains:
	 *    typep[0] = type
	 *    typep[1] = len
	 *    typep[2] = first byte of data which is len bytes long.
	 */
	/*
	 * According to the RFC, type can't be 0 or 1 since those are reserved
	 * for the Pad1 and PadN options which are inserted by the system
	 * as needed.
	 */
	if (typep == NULL || *typep < 2)
		return (NULL);
	if (append_common(cmsg, typep[1], typep, multx, plusy) == NULL)
		return (-1);
	return (0);
}

uint8_t *
inet6_option_alloc(struct cmsghdr *cmsg, int datalen,
  int multx, int plusy)
{
	return (append_common(cmsg, datalen, NULL, multx, plusy));
}

int
inet6_option_next(const struct cmsghdr *cmsg, uint8_t **tptrp)
{
	uint8_t *endp, *ptr;

	if (cmsg->cmsg_level != IPPROTO_IPV6 ||
	  ((cmsg->cmsg_type != IPV6_HOPOPTS &&
	  cmsg->cmsg_type != IPV6_DSTOPTS))) {
		*tptrp = (uint8_t *)-1L;  /* On error *tptrp must not be NULL */
		return (-1);
	}

	if (*tptrp == NULL) {
		/*
		 * This is the first time we are called so set the pointer
		 * to point at the first option.  The first 2 bytes are the
		 * next header field and length field of the extension header.
		 * The first option starts right after those two bytes.
		 */
		ptr = CMSG_DATA(cmsg) + 2;
	} else {
		/*
		 * ptr points at an area of memory that contains:
		 *    ptr[0] = type
		 *    ptr[1] = len
		 *    ptr[2] = first byte of data which is len bytes long.
		 * To get to the next option add len + 2 since len does not
		 * include the type or len field.
		 */
		ptr += ptr[1] + 2;
	}

	/*
	 * Skip over pad options.  There should only be one but don't count
	 * on it.
	 */
	endp = (uint8_t *)cmsg + _ALIGN(sizeof(struct cmsghdr)) +
	  cmsg->cmsg_len;
	while (*ptr < 2 && ptr < endp) { /* Pad option */
		if (ptr == 0)
			ptr++;  /* pad1 */
		else
			ptr += ptr[1] + 2;  /* padN (ptr[1] is len) */
	}

	if (ptr >= endp) {
		/* No more options */
		*tptrp = NULL;
		return (-1);
	}
	*tptrp = ptr;
	return (0);
}

int
inet6_option_find(const struct cmsghdr *cmsg, uint8_t **tptrp, int type)
{
	uint8_t *endp, *ptr;

	if (cmsg->cmsg_level != IPPROTO_IPV6 ||
	  ((cmsg->cmsg_type != IPV6_HOPOPTS &&
	  cmsg->cmsg_type != IPV6_DSTOPTS))) {
		*tptrp = (uint8_t *)-1L;  /* On error *tptrp must not be NULL */
		return (-1);
	}

	if (*tptrp == NULL) {
		/*
		 * This is the first time we are called so set the pointer
		 * to point at the first option.  The first 2 bytes are the
		 * next header field and length field of the extension header.
		 * The first option starts right after those two bytes.
		 */
		ptr = CMSG_DATA(cmsg) + 2;
	} else
		ptr = *tptrp;

	/*
	 * Now search for the option.
	 */
	endp = (uint8_t *)cmsg + _ALIGN(sizeof(struct cmsghdr)) +
	  cmsg->cmsg_len;
	while (*ptr != type && ptr < endp) {
		/*
		 * ptr points at an area of memory that contains:
		 *    ptr[0] = type
		 *    ptr[1] = len
		 *    ptr[2] = first byte of data which is len bytes long.
		 * To get to the next option add len + 2 since len does not
		 * include the type or len field.
		 */
		ptr += ptr[1] + 2;
	}

	if (ptr >= endp) {
		/* No more options */
		*tptrp = NULL;
		return (-1);
	}
	*tptrp = ptr;
	return (0);
}

#endif /* INET6 */
