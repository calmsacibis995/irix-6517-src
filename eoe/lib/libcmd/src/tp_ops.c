/*
 * tp_ops.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.5 $"


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mkdev.h>
#include <string.h>

#include <sys/stropts.h>

#include <sys/time.h>
#include <sys/termios.h>
#include <sys/stream.h>

#ifdef NOTDEF
#include <sys/tp.h>
#define	TP_ERRORMSGSZ		256
#define	TPROOTPATH		"/dev/tp/"


extern	int	errno;



static	int	Tp_errno;
static	char	Tp_errormsg[TP_ERRORMSGSZ];
static	char	Tp_tmpbuf[TP_ERRORMSGSZ];
static	struct	tp_info Tp_info;
static	struct	sak sakundef = {
	saktypeUNDEF,
	(ulong)0,
	(enum saklinecond)0,
	(enum saksecondary)0
};
static	struct	termios nulltermios;

/* strioctl for TP driver ioctls
*/
static	struct strioctl TP_CONNECTstrioctl = {
	TP_CONNECT,			/* ic_cmd	command */
	INFTIM,				/* ic_timout	timeout value */
	sizeof(struct tp_info),		/* ic_len	data length */
	(char *)&Tp_info		/* ic_dp	pointer to data */
};
static	struct strioctl TP_DEFSAKstrioctl = {
	TP_DEFSAK,
	INFTIM,
	sizeof(struct tp_info),
	(char *)&Tp_info
};
static	struct strioctl TP_DATACONNECTstrioctl = {
	TP_DATACONNECT,
	INFTIM,
	sizeof(struct tp_info),
	(char *)&Tp_info
};
static	struct strioctl TP_DATADISCONNECTstrioctl = {
	TP_DATADISCONNECT,
	INFTIM,
	sizeof(struct tp_info),
	(char *)&Tp_info
};
static	struct strioctl TP_CONSCONNECTstrioctl = {
	TP_CONSCONNECT,
	INFTIM,
	sizeof(struct tp_info),
	(char *)&Tp_info
};
static	struct strioctl TP_CONSDISCONNECTstrioctl = {
	TP_CONSDISCONNECT,
	INFTIM,
	sizeof(struct tp_info),
	(char *)&Tp_info
};
static	struct strioctl TP_CONSSETstrioctl = {
	TP_CONSSET,
	INFTIM,
	sizeof(struct tp_info),
	(char *)&Tp_info
};
static	struct strioctl TP_GETINFstrioctl = {
	TP_GETINF,
	INFTIM,
	sizeof(struct tp_info),
	(char *)&Tp_info
};


	int	tp_open();
	int	tp_devopen();
	int	tp_devclose();
	int	tp_defsak();
	int	tp_dataconnect();
	int	tp_datadisconnect();
	int	tp_consconnect();
	int	tp_fconsconnect();
	int	tp_consdisconnect();
	int	tp_fconsdisconnect();
	int	tp_consset();
	int	tp_getinf();
	int	tp_fgetinf();
	int	tp_chanopen();
	int	tp_makedevice();
	void	tp_geterror();





#define	TPC_ADMDEV_DSF		"/dev/tp/adm"
#define	TPC_DATACLONE_DSF	"/dev/tp/data"
#define	TPC_CTRLCLONE_DSF	"/dev/tp/ctrl"
#define	TPC_CONSDEV_DSF		"/dev/tp/cons"

/* tp_open(devname, openflags)
**
** -connects a newly allocated data channel to a TP device that has real device
**  devname multiplexed underneath.  This is done in the following manner:
**
** 	-opens the adm channel
**	-opens data channel clone to get a new data channel
**	-attempts to connect data channel by calling tp_dataconnect()
**	IF connecting data channel to TP device for devname via adm channel
**	 fails
**		IF reason for failure is ENXIO (devname is not multiplexed
**						under a TP device)
**			-setup a TP device (via tp_devopen())
**			IF connecting data channel to TP device for devname via
**			 adm channel fails
**				-return failure
**			ELSE
**				-return data channel's file descriptor
**		ELSE
**			-return failure
**	ELSE
**		-return data channel's file descriptor
**
**
** -returns -1 upon failure
**	-Tp_error is set to errno of failing system call
**	-Tp_errormsg is filled in with an appropriate message
** -returns fd number of ctrl channel when successful
*/
int
tp_open(devname, openflags)
	char	*devname;	/* path of real device that is muxed or is to be
				** muxed under a TP device
				*/
	int	openflags;	/* same flags used by open(2) and defined in
				** <fnctl.h>
				*/
{

	int		afd;	/* fd for adm channel */
	int		dfd;	/* fd for data channel */
	int		cfd;	/* fd for ctrl channel */
	struct sak	sak;
	struct tp_info	tpinf;
	struct termios	termios;
	int		tmpfd;
	int		ret;



	/* -initially open and close the real/physical tty device.  This
	**  will raise outgoing DTR on the real/physical device (if
	**  the real/physical tty device is already linked underneath
	**  a TP device), which is necessary to be able to use the
	**  real/physical tty device.
	*/
	tmpfd = open(devname, O_RDWR|O_NONBLOCK|O_NOCTTY);
	(void)close(tmpfd);

	/* -open adm channel
	** -open data channel
	*/
	if ((afd = tp_chanopen(TPC_ADM, O_WRONLY|O_NONBLOCK)) == -1){
		(void)sprintf(Tp_tmpbuf,"tp_open:%s", Tp_errormsg);
		(void)strcpy(Tp_errormsg, Tp_tmpbuf);
		return (-1);
	}
	if ((dfd = tp_chanopen(TPC_DATA, openflags)) == -1){
		(void)sprintf(Tp_tmpbuf,"tp_open:%s", Tp_errormsg);
		(void)strcpy(Tp_errormsg, Tp_tmpbuf);
		(void)close(afd);
		return (-1);
	}


	/* -setup tp_info sturcture for tp_dataconnect() function
	**	-set TPINF_FAILUNLINK_IFDATA flag so TP device can not
	**	 be dismantled while data channel is connected (via
	**	 the ctrl channel unless the data channel is explicitly
	**	 disconnected
	** -attempt to connect data channel via adm channel
	** IF it fails because a TP device for rdev does not exist
	**	-setup a TP device for devname
	**	-attempt to connect data channel via adm channel
	** -make file system name for data channel
	*/
	TP_LOADINF(tpinf, (dev_t)-1, (dev_t)-1, 0, 0, (dev_t)-1, (dev_t)-1,
	 (dev_t)-1, 0, 0, 0, 0, sakundef, TPINF_FAILUNLINK_IFDATA, nulltermios,
	 nulltermios);
	if (tp_dataconnect(devname, afd, dfd, &tpinf) == -1){
		if (Tp_errno != ENXIO){
			Tp_errno = errno;
			(void)sprintf(Tp_tmpbuf,
			 "tp_open:%s",Tp_errormsg);
			(void)strcpy(Tp_errormsg, Tp_tmpbuf);
			(void)close(afd);
			(void)close(dfd);
			return (-1);
		}

		sak.sak_type = saktypeNONE;
		TP_LOADINF(tpinf, (dev_t)-1, (dev_t)-1, 0, 0, (dev_t)-1,
		 (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sak, TPINF_FAILUNLINK_IFDATA,
		 nulltermios, nulltermios);
		if ((cfd = tp_devopen(devname, &tpinf)) == -1){
			(void)sprintf(Tp_tmpbuf, "tp_open:%s", Tp_errormsg);
			(void)strcpy(Tp_errormsg, Tp_tmpbuf);
			(void)close(afd);
			(void)close(dfd);
			return (-1);
		}
		if ((ret = ioctl(cfd, TCGETS, &termios)) != 0 ){
			Tp_errno = errno;
			(void)sprintf(Tp_errormsg,
			 "tp_open:ioctl(cfd, TCGETS) failed");
		}else if ((ret = ioctl(cfd, TCSETS, &termios)) != 0 ){
			Tp_errno = errno;
			(void)sprintf(Tp_errormsg,
			 "tp_open:ioctl(cfd, TCSETS) failed");
		}
		if (ret != 0) {
			(void)close(afd);
			(void)close(dfd);
			(void)close(cfd);
			return (-1);
		}
		/* -TP device for devname is set up, try to connect data
		**  channel again
		*/
		TP_LOADINF(tpinf, (dev_t)-1, (dev_t)-1, 0, 0, (dev_t)-1,
		 (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sakundef,
		 TPINF_FAILUNLINK_IFDATA, nulltermios, nulltermios);
		if (tp_dataconnect(devname, afd, dfd, &tpinf) == -1){
			if (Tp_errno != ENXIO){
				Tp_errno = errno;
				(void)sprintf(Tp_tmpbuf,
				 "tp_open:%s",Tp_errormsg);
				(void)strcpy(Tp_errormsg, Tp_tmpbuf);
				(void)close(afd);
				(void)close(dfd);
				(void)close(cfd);
				return (-1);
			}
		}
	}
	if (tp_makedevice(dfd, 0, (char *)NULL) == -1){
		(void)sprintf(Tp_tmpbuf, "tp_open:%s", Tp_errormsg);
		(void)strcpy(Tp_errormsg, Tp_tmpbuf);
		(void)close(afd);
		(void)close(dfd);
		(void)close(cfd);
		return (-1);
	}

	/* This is here until the issue of who(this routine, the
	** calling application, or autopush facility) pushed stream modules.
	*/
	if (ioctl(dfd, I_PUSH, "ldterm") == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "push \"ldterm\" on %s failed", devname);
		(void)close(afd);
		(void)close(dfd);
		(void)close(cfd);
		return (-1);
	}

	(void)close(cfd);
	(void)close(afd);
	return (dfd);
}



/* tp_devopen(devname, tpinfp)
**
** -Sets up a TP device in the following manner:
**	-sets up the Tp_info structure for a TP_CONNECT ioctl (tpinf_sak
**	 and tpinf_flags are extracted from tpinfp)
**		-opens real device  devname
**		-stats real device for Major Minor device number rdev
**	-opens ctrl channel clones to get a ctrl channel
**	-connects ctrl channel to a TP device for rdev
**	-loads default SAK
**	-links tty device under TP device's ctrl channel
**
**
** -returns fd to ctrl channel of TP device if successful
**	-also the following tpinf fields values are set
**		-tpinf_rdev	Maj/Min device number of devname
**		-tpinf_cdev	Maj/Min device number of ctrl channel
**		-tpinf_ddev	(dev_t) -1
**		-tpinf_muxid	multiplexor id for TP device
**		-tpinf_connid	connection id of ctrl channel
**		-tpinf_sak	UNCHANGED (is an input parameter)
**		-tpinf_flags	UNCHANGED (is an input parameter)
**			-valid flags:
**				-tpinfCONSOLE: real device to which the
**				 TP_CONNECT is connecting to becomes the console
**				 device
**		-tpinf_mask	termios mask based on tpinf_sak
**		-tpinf_valid	valid termios settings, for all settings
**				specified by tpinf_mask, to insure integrity
**				of SAK detection
** -returns -1 on failure
**	-Tp_errno is to errno of failing system call
**	-Tp_errormsg is filled in with an apropriate message
**	-tpinfp is UNCHANGED
*/


int
tp_devopen(devname, tpinfp)
	char		*devname;	/* open fd of device to be muxed */
	struct tp_info	*tpinfp;	/* -tpinf_sak contains sak defintion
					** -tpinf_flags contains tp device
					**  specific flags
					** -do not care about values of other
					**  fields
					*/
{

	dev_t		rdev;	/* Major Minor device number for real device */
	dev_t		rdevfsdev;/* Major Minor device number of File System
				** on which real device resides
				*/
	ino_t		rdevino;/* Inode number of real device's DSF */
	mode_t		rdevmode;/*Modes of DSF */
	dev_t		cdev;	/* Major Minor device number for ctrl channel */
	dev_t		ddev;	/* Major Minor device number for data channel */
	dev_t		dev;	/* Major Minor device number of channel issuing
				** the ioctl
				*/
	int		muxid;
	int		cconnid;/* Connection id of ctrl channel */
	int		dconnid;/* Connection id of data channel */
	int		connid;	/* Connection id of channel issuing ioctl */
	struct sak	sak;
	int		flags;
	struct termios	mask;
	struct termios	valid;


	int		rfd;
	int		ctrlfd;
	struct stat	tpstat;
	struct tp_info	tpinf;


	TP_UNLOADINF(*tpinfp, rdev, rdevfsdev, rdevino, rdevmode, cdev, ddev,
	 dev, muxid, cconnid, dconnid, connid, sak, flags, mask, valid);
	ddev = (dev_t)-1;
	dconnid = 0;


	/* -open real device 
	**  this also raises out going DTR on real device
	** -get Major Minor device number via stat
	*/

	if ((rfd = open(devname, O_RDWR|O_NONBLOCK|O_NOCTTY)) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_devopen: open failed on %s ",devname);
		return (-1);
	}

	if(fstat(rfd, &tpstat) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_devopen: stat failed on %s ",devname);
		(void)close(rfd);
		return(-1);
	}
	rdev = tpstat.st_rdev;
	rdevfsdev = tpstat.st_dev;
	rdevino = tpstat.st_ino;
	rdevmode = tpstat.st_mode;

	/* -open ctrl channel
	** -setup argument (Tp_info) to TP_CONNECT ioctl
	** -connect to TP device for rdev
	** -get LINK status from Tp_info
	**	-muxid != 0 indicates rdev is already LINKed under a
	**	 TP device 
	**	-connid is a unique id assigned to this connection
	*/
	if ((ctrlfd = open(TPC_CTRLCLONE_DSF, O_RDWR|O_NOCTTY)) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "opentp: ctrl channel clone open failed on %s",
		 TPC_CTRLCLONE_DSF);
		(void)close(rfd);
		return(-1);
	}

	TP_LOADINF(Tp_info, rdev, rdevfsdev, rdevino, rdevmode, (dev_t)-1,
	 (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sakundef, flags, nulltermios,
	 nulltermios);
	if (ioctl(ctrlfd, I_STR, &TP_CONNECTstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "opentp: TP_CONNECT failed for %s ", devname);
		(void)close(rfd);
		(void)close(ctrlfd);
		return(-1);
	}

	cdev = Tp_info.tpinf_cdev;
	dev = Tp_info.tpinf_dev;
	muxid = Tp_info.tpinf_muxid;
	cconnid = Tp_info.tpinf_cconnid;
	connid = Tp_info.tpinf_connid;




	/* -load default SAK on TP device via ctrl channel
	** -NOTE: eventually check sak info returned from CONNECT
	**	IF same as sakp passed down to tp_devopen() no need to
	**	   send TP_DEFSAK
	*/
	TP_LOADINF(tpinf, (dev_t)-1, (dev_t)-1, 0, 0, (dev_t)-1, (dev_t)-1,
	 (dev_t)-1, 0, 0, 0, 0, sak, 0, nulltermios, nulltermios);
	if (tp_defsak(ctrlfd, &tpinf) == -1){
		(void)sprintf(Tp_tmpbuf, "tp_devopen:%s on %s",
		 Tp_errormsg, devname);
		(void)strcpy(Tp_errormsg, Tp_tmpbuf);
		(void)close(rfd);
		(void)close(ctrlfd);
		return(-1);
	}
	mask = tpinf.tpinf_mask;
	valid = tpinf.tpinf_valid;



	/* -link real device under TP device's ctrl channel if it is
	**  not already LINKed
	*/
	if (muxid == 0){
		/* -pop off STREAMS modules that may have been pushed
		**  by autopush facility
		*/
		while (ioctl(rfd,I_POP,0) != -1);

		if ((muxid = ioctl(ctrlfd, I_PLINK, rfd)) == -1){
			Tp_errno = errno;
			(void)sprintf(Tp_errormsg,
			 "opentp: I_PLINK failed for %s ", devname);
			(void)close(rfd);
			(void)close(ctrlfd);
			return(-1);
		}
	}


	(void)close(rfd);
	TP_LOADINF(*tpinfp, rdev, rdevfsdev, rdevino, rdevmode, cdev, ddev, dev, muxid, cconnid, dconnid, connid, sak, flags, mask, valid);
	return (ctrlfd);
}




/* tp_devclose(ctrlfd, muxid)
**
** -Dismantles a TP device in the following manner:
**	-unlinks tty device under TP device's ctrl channel
**	-closes the ctrl channel which dismantles the TP device
**
** -returns 0 if TP mux is dismantled successfully
** -returns -1 on failure
**	-Tp_errno is to errno of failing system call
**	-Tp_errormsg is filled in with an apropriate message
**
**	-Some reasons for failures include:
**		-a data channel is connected to the TP device and the
**		 TPINF_FAILUNLINK_IFDATA flag was set when the data
**		 channel was connected
**		-invalid muxid
*/
int
tp_devclose(ctrlfd, muxid)
	int	ctrlfd;		/* ctrl channel fd */
	int	muxid;		/* TP multiplexor Link id */
{

	if (ioctl(ctrlfd, I_PUNLINK, muxid) < 0){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg, "tp_devclose: I_PUNLINK failed");
		return (-1);
	}
	(void)close(ctrlfd);
	return(0);
}




/* tp_defsak(ctrlfd, tpinfp)
**
** -loads default sak, contained in tpinfp, on TP device via ctrl channel,
**  ctrlfd
**
** -returns 0 if successful
**	-also the following tpinf fields values are set
**		-tpinf_rdev	UNCHANGED
**		-tpinf_cdev	UNCHANGED
**		-tpinf_ddev	UNCHANGED
**		-tpinf_muxid	UNCHANGED
**		-tpinf_connid	UNCHANGED
**		-tpinf_sak	UNCHANGED (is an input parameter)
**		-tpinf_flags	UNCHANGED
**		-tpinf_mask	termios mask based on tpinf_sak
**		-tpinf_valid	valid termios settings, for all settings
**				specified by tpinf_mask, to insure integrity
**				of SAK detection
** -returns -1 on failure
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinf UNCHANGED
*/
int
tp_defsak(ctrlfd, tpinfp)
	int		ctrlfd;
	struct tp_info	*tpinfp;
{

	/* -setup Tp_info for TP_DEFSAK ioctl
	*/
	TP_LOADINF(Tp_info, (dev_t)-1, (dev_t)-1, 0, 0, (dev_t)-1, (dev_t)-1,
	 (dev_t)-1, 0, 0, 0, 0, tpinfp->tpinf_sak, 0, nulltermios, nulltermios);
	if (ioctl(ctrlfd, I_STR, &TP_DEFSAKstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,"tp_defsak: TP_DEFSAK failed");
		return (-1);
	}
	tpinfp->tpinf_mask = Tp_info.tpinf_mask;
	tpinfp->tpinf_valid = Tp_info.tpinf_valid;
	return (0);
}





/* tp_dataconnect(realdevice, chanfd, datafd, tpinfp)
**
** -connect a TP data channel (datafd) to a TP device for which (realdevice)
**  is multiplexed underneath.
**  	-the connection is made either
**		-via the TP device's associated ctrl channel (chanfd) 
**			or
**		-via the ADM channel (chanfd)
**
**	-realdevice  pathname of tty device that is (should be) multiplexed
**	 under a TP device
**
**	-chanfd is either
**		-open fd to the ADM device
**		-open fd to the ctrl channel that is connected to a TP device
**		 that has the realdevice multiplexed underneath
**
**	-datafd  open fd to a data channel that is
**		-not connected to a TP device
**		-was not previously connected and subsequently disconnected from
**		 a TP device (i.e. the data channel was opened via the data
**		 channel clone device and has not been connected to a TP device)
**
**	-tpinfp
**		-tpinf_flags: (an input parameter)
**		 The following are the list of valid flags for the
**		 TP_DATACONNECT	ioctl (see <sys/tp.h> for detailed description)
**			-TPINF_DISCONNDATA_ONHANGUP 
**			-TPINF_FAILUNLINK_IFDATA
**
**	-connid	-return parameter; it is assigned a unique id to the connection
**		 of the data channel.  The connid is used by tp_datadisconnect
**		 to "help" verify the data channel was connected by the process
**		 that knows the connid
**
** -returns -1 is a failure occurs
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
** -returns 0 if successful
**	-also the following tpinfp fields values are set
**		-tpinf_rdev	Maj/Min device number of devname
**		-tpinf_cdev	(dev_t)-1
**		-tpinf_ddev	Maj/Min device number of data device
**		-tpinf_muxid	UNCHANGED
**		-tpinf_connid	connection id of data channel
**		-tpinf_sak	UNCHANGED (is an input parameter)
**		-tpinf_flags	UNCHANGED (is an input parameter)
**		-tpinf_mask	UNCHANGED
**		-tpinf_valid	UNCHANGED
*/



int
tp_dataconnect(realdevice, chanfd, datafd, tpinfp)
	char		*realdevice;
	int		chanfd;
	int		datafd;
	struct tp_info	*tpinfp;
{
	dev_t		rdev;		/* Major/Minor device number of the
					** real device
					*/
	dev_t		rdevfsdev;	/* Major/Minor device of File System
					** on which DSF resides.
					*/
	ino_t		rdevino;	/* Inode number of DSF */
	mode_t		rdevmode;	/* File Attributes of DSF */
	dev_t		ddev;		/* dev_t of the data channel */
	struct stat	tpstat;		/* save area for stat(2) */


	/* -get dev_t for real device and data channel
	** -fill out Tp_info for TP_DATACONNECT ioctl
	** -connect data channel to TP device
	*/

	if (stat(realdevice, &tpstat) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,"tp_dataconnect: stat failed on %s ", realdevice);
		return (-1);
	}
	rdev = tpstat.st_rdev;
	rdevfsdev = tpstat.st_dev;
	rdevino = tpstat.st_ino;
	rdevmode = tpstat.st_mode;

	if (fstat(datafd, &tpstat) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,"tp_dataconnect: stat failed on datachannel ");
		return (-1);
	}
	ddev = tpstat.st_rdev;

	TP_LOADINF(Tp_info, rdev, rdevfsdev, rdevino, rdevmode, (dev_t)-1, ddev,
	 (dev_t)-1, 0, 0, 0, 0, sakundef, tpinfp->tpinf_flags, nulltermios,
	 nulltermios);

	if (ioctl(chanfd, I_STR, &TP_DATACONNECTstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,"tp_dataconnect: TP_DATACONNECT ioctl failed for %s ",realdevice);
		return (-1);
	}

	tpinfp->tpinf_rdev = rdev;
	tpinfp->tpinf_cdev = (dev_t)-1;
	tpinfp->tpinf_ddev = ddev;
	tpinfp->tpinf_connid = Tp_info.tpinf_connid;

	return (0);
}



/* tp_datadisconnect(ctrlfd, tpinfp)
**
** -disconnect a TP data channel from the TP device associated with the ctrl
**  channel (ctrlfd)
**
**	-tpinfp
**		-tpinf_connid: (an input paratmer)
** 		 connection id assigned to the data channel when it was
**		 connected
**
** -assuming that a data channel is connected
** IF the connid matches the connection id of the data channel that
**  is associated with the TP device that is associated with the ctrl channel
**  (ctrlfd)
**	-the data channel is disconnected
** ELSE IF the connid == 0
**	-the data channel is disconnected
** ELSE
**	-the data channel is NOT disconnected
**
** -returns 0 if the data channel is disconnected successfully
**	-tpinfp is UNCHANGED
** -returns -1 on failure
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
*/

int
tp_datadisconnect(ctrlfd, tpinfp)
	int		ctrlfd;
	struct tp_info	*tpinfp;
{

	TP_LOADINF(Tp_info, (dev_t)-1, (dev_t)-1, 0, 0, (dev_t)-1, (dev_t)-1,
	 (dev_t)-1, 0, tpinfp->tpinf_dconnid, 0, 0, sakundef, 0, nulltermios,
	 nulltermios);

	if (ioctl(ctrlfd, I_STR, &TP_DATADISCONNECTstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_datadisconnect: TP_DATADISCONNECT ioctl failed connid = %d\n",
		 tpinfp->tpinf_connid);
		return (-1);
	}

	return (0);
}

/* tp_consconnect(tpinfp)
**
** -connect the CONS channel for input.  This is done via a TP ioctl,
**  TP_CONSCONNECT down the ADM channel.
**
** -returns -1 is a failure occurs
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
**
** -retuns 0 on success.
*/
int
tp_consconnect(tpinfp)
struct tp_info	*tpinfp;
{

	int	afd;	/* file descriptor for ADM channel */


	if ((afd = tp_chanopen(TPC_ADM, O_WRONLY|O_NONBLOCK|O_NOCTTY)) == -1){
		(void)sprintf(Tp_tmpbuf, "tp_consconnect:%s:",Tp_errormsg);
		(void)strcpy(Tp_errormsg, Tp_tmpbuf);
		return (-1);
	}
	TP_LOADINF(Tp_info, (dev_t)-1, (dev_t)-1, (ino_t)0, (mode_t)0,
	 (dev_t)-1, (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sakundef, 0, nulltermios,
	 nulltermios);
	if (ioctl(afd, I_STR, &TP_CONSCONNECTstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_consconnect:TP_CONSCONNECT FAILED ");
		(void)close(afd);
		return (-1);
	}
	*tpinfp = Tp_info;
	(void)close(afd);
	return (0);
}


/* tp_fconsconnect(fd, tpinfp)
**
** -connect the CONS channel for input.  This is done via a TP ioctl,
**  TP_CONSCONNECT down the file descriptor fd.
**
** -returns -1 is a failure occurs
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
**
** -retuns 0 on success.
*/
int
tp_fconsconnect(fd, tpinfp)
int		fd;
struct tp_info	*tpinfp;
{


	TP_LOADINF(Tp_info, (dev_t)-1, (dev_t)-1, (ino_t)0, (mode_t)0,
	 (dev_t)-1, (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sakundef, 0, nulltermios,
	 nulltermios);
	if (ioctl(fd, I_STR, &TP_CONSCONNECTstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_fconsconnect:TP_CONSCONNECT FAILED ");
		return (-1);
	}
	*tpinfp = Tp_info;
	return (0);
}


/* tp_consdisconnect(tpinfp)
**
** -disconnect the CONS channel for input.  This is done via a TP ioctl,
**  TP_CONSDISCONNECT down the ADM channel.
**
** -returns -1 is a failure occurs
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
**
** -retuns 0 on success.
*/
int
tp_consdisconnect(tpinfp)
struct tp_info	*tpinfp;
{

	int	afd;	/* file descriptor for ADM channel */


	if ((afd = tp_chanopen(TPC_ADM, O_WRONLY|O_NONBLOCK|O_NOCTTY)) == -1){
		(void)sprintf(Tp_tmpbuf, "tp_consdisconnect:%s:",Tp_errormsg);
		(void)strcpy(Tp_errormsg, Tp_tmpbuf);
		return (-1);
	}
	TP_LOADINF(Tp_info, (dev_t)-1, (dev_t)-1, (ino_t)0, (mode_t)0,
	 (dev_t)-1, (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sakundef, 0, nulltermios,
	 nulltermios);
	if (ioctl(afd, I_STR, &TP_CONSDISCONNECTstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_consdisconnect:TP_CONSDISCONNECT FAILED ");
		(void)close(afd);
		return (-1);
	}
	(void)close(afd);
	*tpinfp = Tp_info;
	return (0);
}


/* tp_fconsdisconnect(fd, tpinfp)
**
** -disconnect the CONS channel for input.  This is done via a TP ioctl,
**  TP_CONSDISCONNECT down the file descriptor fd.
**
** -returns -1 is a failure occurs
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
**
** -retuns 0 on success.
*/
int
tp_fconsdisconnect(fd, tpinfp)
int		fd;
struct tp_info	*tpinfp;
{


	TP_LOADINF(Tp_info, (dev_t)-1, (dev_t)-1, (ino_t)0, (mode_t)0,
	 (dev_t)-1, (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sakundef, 0, nulltermios,
	 nulltermios);
	if (ioctl(fd, I_STR, &TP_CONSDISCONNECTstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_fconsdisconnect:TP_CONSDISCONNECT FAILED ");
		return (-1);
	}
	*tpinfp = Tp_info;
	return (0);
}

/* tp_consset(devname, tpflag, tpinfp)
**
** -switches the real/physical device that the CONS channel displays messages
**  to (and retrieves input from, if CONS channel is connected for input) to
**  devname.  This is done via a TP ioctl TP_CONSSET down the ADM channel.
**
** -devname:	The real/physical device to switch console input/output from/to.
** -tpflag:	Is input for  tpinf_flags field.  The following is a list of
**		valid flgas(s) for TP_CONSSET:
**
**			-TPINF_ONLYIFLINKED: 	only allow the console to be
**						switched if devname is linked
**						under a TP device.
**
** -returns -1 is a failure occurs
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
**
** -retuns 0 on success.
*/
int
tp_consset(devname, tpflag, tpinfp)
char		*devname;
int		tpflag;
struct tp_info	*tpinfp;
{

	int		afd;	/* file descriptor for ADM channel */
	struct stat	dnstat;


	if ((afd = tp_chanopen(TPC_ADM, O_WRONLY|O_NONBLOCK|O_NOCTTY)) == -1){
		(void)sprintf(Tp_tmpbuf, "tp_consset:%s:",Tp_errormsg);
		(void)strcpy(Tp_errormsg, Tp_tmpbuf);
		return (-1);
	}
	if (stat(devname, &dnstat) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_consset:stat failed on %s ", devname);
		(void)close(afd);
		return (-1);
	}
	TP_LOADINF(Tp_info, dnstat.st_rdev, dnstat.st_dev, dnstat.st_ino,
	 dnstat.st_mode, (dev_t)-1, (dev_t)-1, (dev_t)-1, 0, 0, 0, 0,
	 sakundef, tpflag, nulltermios, nulltermios);
	if (ioctl(afd, I_STR, &TP_CONSSETstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_consset:TP_CONSSET FAILED ");
		(void)close(afd);
		return (-1);
	}
	*tpinfp = Tp_info;
	(void)close(afd);
	return (0);
}

/* tp_getinf(devname, tpinfp)
**
** -get TP device status information for the real/physical device devname.
**  This is done via the TP_GETINF TP ioctl via the ADM channel.
**
** -returns -1 is a failure occurs
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
**
** -retuns 0 on success.
*/
int
tp_getinf(devname, tpinfp)
char		*devname;
struct tp_info	*tpinfp;
{

	int		afd;	/* file descriptor for ADM channel */
	struct stat	devnamestat;


	if ((afd = tp_chanopen(TPC_ADM, O_WRONLY|O_NONBLOCK|O_NOCTTY)) == -1){
		(void)sprintf(Tp_tmpbuf, "tp_getinf:%s:",Tp_errormsg);
		(void)strcpy(Tp_errormsg, Tp_tmpbuf);
		return (-1);
	}
	if (stat(devname, &devnamestat) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_getinf:stat failed on %s ", devname);
		(void)close(afd);
		return (-1);
	}
	TP_LOADINF(Tp_info, devnamestat.st_rdev, (dev_t)-1, (ino_t)0, (mode_t)0,
	 (dev_t)-1, (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sakundef, 0, nulltermios,
	 nulltermios);
	if (ioctl(afd, I_STR, &TP_GETINFstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_getinf:TP_GETINF FAILED ");
		(void)close(afd);
		return (-1);
	}
	*tpinfp = Tp_info;
	(void)close(afd);
	return (0);
}

/* tp_fgetinf(fd, tpinfp)
**
** -get TP device status information for the real/physical device devname.
**  This is done via the TP_GETINF TP ioctl via the file descriptor fd. 
**
** -returns -1 is a failure occurs
**	-Tp_errno is set to errno on failing system call
**	-Tp_errormsg is filled in with an appropriate message
**	-tpinfp is UNCHANGED
**
** -retuns 0 on success.
*/
int
tp_fgetinf(fd, tpinfp)
int		fd;
struct tp_info	*tpinfp;
{

	TP_LOADINF(Tp_info, (dev_t)-1, (dev_t)-1, (ino_t)0, (mode_t)0,
	 (dev_t)-1, (dev_t)-1, (dev_t)-1, 0, 0, 0, 0, sakundef, 0, nulltermios,
	 nulltermios);
	if (ioctl(fd, I_STR, &TP_GETINFstrioctl) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_fgetinf:TP_GETINF FAILED ");
		return (-1);
	}
	*tpinfp = Tp_info;
	return (0);
}

/* tp_chanopen(chantype, openflags)
**
** -opens a TP channel of type (chantype)
**
** IF chantype is TPC_DATA or TPC_CTRL
**	-tp_chanopen() opens the TP data or ctrl clone device respectively
**		-opening the data or ctrl clone device returns an unused and
**		 non-connected data or ctrl channel
**		-??? may want to insure clone devices are in DEV_PRIVATE state
** ELSE IF chantype is TPC_ADM or TPC_CONS
**	-tp_chanopen() opens the TP ADM or CONS minor device respectively
**
** -open flags are the same flags used by open(2) and defined in <fnctl.h>
**
** -returns -1 on failure
** -returns an open fd to a TP channel of type chantype
*/

int
tp_chanopen(chantype, openflags)
	int	chantype;
	int	openflags;
{

	int fd;
	char *device;


	switch (chantype){

	case TPC_DATA:
		device = TPC_DATACLONE_DSF;
		break;
	case TPC_CTRL:
		device = TPC_CTRLCLONE_DSF;
		break;
	case TPC_ADM:
		device = TPC_ADMDEV_DSF;
		break;
	case TPC_CONS:
		device = TPC_CONSDEV_DSF;
		break;
	default:
		Tp_errno = EINVAL;
		(void)sprintf(Tp_errormsg, "tp_chanopen: invalid channel type specified");
		return (-1);
		break;
	}

	if ((fd = open(device, openflags)) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg, "tpopenchan: open failed on %s ",device);
	}

	return (fd);
}




/* tp_makedevice(chanfd, bufsize, buf)
**
** -make a device special file (via mknod(2)) for the open fd (chanfd)
**
**	-chanfd  open fd to a TP channel
**	-bufsize size of buf
**	-buf     return parameter: for path name of device special file
**
** -returns -1 on failure
** -returns  0 on success
*/

int
tp_makedevice(chanfd, bufsize, buf)
	int	chanfd;
	uint	bufsize;
	char	*buf;
{

	dev_t	chandev;
	struct	stat tpstat;
	char	scratchbuf[sizeof(TPROOTPATH) + 10];	/* + 10 is to
							** accommodate size for
							** minor device and
							** NULL byte
							*/

	/* -stat chanfd
	** -create a pathname
	** -make device special file
	** -copy paht name to return paramter buf IF
	**	-buf is not NULL
	**	-bufsize >= length of device special file pathname + 1
	*/

	if (fstat(chanfd, &tpstat) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg, "tp_makedevice: fstat failed ");
		return (-1);
	}
	chandev = tpstat.st_rdev;

	(void)sprintf(scratchbuf,"%s%lu",TPROOTPATH, minor(tpstat.st_rdev));

	if (mknod(scratchbuf, S_IFCHR|S_IRUSR|S_IWUSR|S_IWGRP, tpstat.st_rdev) == -1){ 
		if (errno != EEXIST){
			Tp_errno = errno;
			(void)sprintf(Tp_errormsg,
			 "tp_makedevice: mknod failed for %s ", scratchbuf);
			return (-1);
		}
	}

	/* stat device special file again to verify it is the same st_rdev
	*/
	if (stat(scratchbuf, &tpstat) == -1){
		Tp_errno = errno;
		(void)sprintf(Tp_errormsg,
		 "tp_makedevice: stat failed on %s ", scratchbuf);
		return (-1);
	}else if ( chandev != tpstat.st_rdev){
		Tp_errno = ENXIO;
		(void)sprintf(Tp_errormsg,
		 "tp_makedevice: %s is not a Device Special File", scratchbuf);
		return (-1);
	}

	if ( (bufsize >= strlen(scratchbuf)) && (buf != (char *)NULL))
		(void)strcpy(buf, scratchbuf);
	else if (buf != (char *)NULL)
		buf[0] = '\0';

	return (0);
}





/* tp_geterror(errnop, errorbufsize, errorbuf)
**
** -return the errno and the associated error message of the last call made to
**  the TP library interface
**	-errno is in Tp_errno
**	-error msg is in Tp_errormsg
*/

void
tp_geterror(errnop, errorbufsize, errorbuf)
	int	*errnop;
	size_t	errorbufsize;
	char	*errorbuf;
{

	if (errnop != (int *)NULL)
		*errnop = Tp_errno;

	if ((errorbufsize > (size_t)0) && (errorbuf != (char *)NULL)){
		(void)strncpy(errorbuf, Tp_errormsg, errorbufsize);
		errorbuf[errorbufsize - 1] = '\0';
	}

	return;
}
#endif /* NOTDEF */


