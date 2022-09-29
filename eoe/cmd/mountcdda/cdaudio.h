#ifndef __CDAUDIO_H_
#define __CDAUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * cdaudio.h
 *
 * Functions for playing audio CD's on a CD ROM drive
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

/*
 * Possible states for the cd player.  These are for the state field
 * of a CDSTATUS structure.
 */

#define CD_ERROR    0
#define CD_NODISC   1
#define CD_READY    2
#define CD_PLAYING  3
#define CD_PAUSED   4
#define CD_STILL    5
#define CD_CDROM    6

/*
 * Block size information for digital audio data
 */
#define CDDA_DATASIZE		2352
#define CDDA_SUBCODESIZE	(sizeof(struct subcodeQ))
#define CDDA_BLOCKSIZE		(sizeof(struct cdframe))
#define CDDA_NUMSAMPLES		(CDDA_DATASIZE/2)

/*
 * Handle for playing audio on a CD-ROM drive
 */
typedef struct cdplayer CDPLAYER;

/*
 * Buffer to receive current status information
 */
typedef struct {
	int     state;
	int     track;
	int     min;            /* relative to beginning of track */
	int     sec;
	int     frame;
	int     abs_min;        /* relative to beginning of disc */
	int     abs_sec;
	int     abs_frame;
	int     total_min;      /* total on disc */
	int     total_sec;
	int     total_frame;
	int     first;          /* first legal track number */
	int     last;           /* last legal track number */
	int	scsi_audio;	/* true if drive can transfer audio via SCSI */
	int	cur_block;	/* block no. for next audio data read */
	int	polyfilla[3];	/* future expansion */
} CDSTATUS;

/*
 * Buffer to receive information about a particular track
 */
typedef struct {
	int     start_min;
	int     start_sec;
	int     start_frame;
	int     total_min;
	int     total_sec;
	int     total_frame;
} CDTRACKINFO;

/*
 * Buffer for getting/setting the volume
 */
typedef struct {
	unsigned char	chan0;
	unsigned char	chan1;
	unsigned char	chan2;
	unsigned char	chan3;
} CDVOLUME;

/*************************************************************************
 *
 * Defines and structures for CD digital audio data
 *
 *************************************************************************/

/*
 * Defines for control field
 */
#define CDQ_PREEMP_MASK		0xd
#define CDQ_COPY_MASK		0xb
#define	CDQ_DDATA_MASK		0xd
#define CDQ_BROADCAST_MASK	0x8
#define CDQ_PREEMPHASIS		0x1
#define CDQ_COPY_PERMITTED	0x2		
#define CDQ_DIGITAL_DATA	0x4
#define CDQ_BROADCAST_USE	0x8

/*
 * Defines for the type field
 */
#define CDQ_MODE1		0x1
#define CDQ_MODE2		0x2
#define CDQ_MODE3		0x3

/*
 * Useful sub-structures
 */
struct cdpackedbcd { unchar dhi:4, dlo:4; };

struct cdtimecode {
    unchar mhi:4, mlo:4;
    unchar shi:4, slo:4;
    unchar fhi:4, flo:4;
};

struct cdident {
    unchar country[2];
    unchar owner[3];
    unchar year[2];
    unchar serial[5];
};

/*
 * Structure of CD subcode Q
 */
typedef struct subcodeQ {
    unchar control;
    unchar type;
    union {
        struct {
	    struct cdpackedbcd track;
	    struct cdpackedbcd index;	/* aka point during track 0 */
	    struct cdtimecode ptime;
            struct cdtimecode atime;
            unchar fill[6];
        } mode1;
        struct {
            unchar catalognumber[13];
            struct cdpackedbcd aframe;
        } mode2;
        struct {
	    struct cdident ident;
            struct cdpackedbcd aframe;
            unchar fill;
        } mode3;
    } data;
} CDSUBCODEQ;

/*
 * Structure of the digital audio (DA) data as delivered by the drive
 * In CD parlance this is a one subcode frame
 */
typedef struct cdframe {
    char audio[CDDA_DATASIZE];
    struct subcodeQ subcode;
} CDFRAME;

/********************************************************************/

/*
 * Compatibility with old names
 */
#define cd_open 		CDopen
#define cd_play 		CDplay
#define cd_play_track		CDplaytrack
#define cd_play_track_abs	CDplaytrackabs
#define cd_play_abs		CDplayabs
#define cd_readda		CDreadda
#define cd_seek			CDseek
#define cd_seek_track		CDseektrack
#define cd_stop			CDstop
#define cd_eject		CDeject
#define cd_close		CDclose
#define cd_get_status		CDgetstatus
#define cd_toggle_pause		CDtogglepause
#define cd_get_track_info	CDgettrackinfo
#define cd_get_volume		CDgetvolume
#define cd_set_volume		CDsetvolume

/********************************************************************
 *
 * CD-ROM Drive Functions.
 *
 * The various *play* functions operate the CD-ROM drive as a player
 * delivering audio from the audio jacks on the drive.  They do not
 * transfer any digital audio data into the host computer.  
 *
 * CDstop and CDtogglepause are only for use when operating the drive
 * as a player.
 *
 ********************************************************************/

/*
 * Open a devscsi device corresponding to a CD-ROM drive for audio.
 * Specifying NULL for devscsi causes the hardware inventory
 * to be consulted for a CD-ROM drive.
 *
 * direction specifies way the device is to be used "r", "w", or "rw".
 * It is for future support of writable CD's and is currently ignored.
 */
CDPLAYER* CDopen( char const *devscsi, char const *direction );

/*
 * Play the disc, starting at start and continuing through the rest
 * of the tracks.  If play is 0, start in pause mode.
 */
int CDplay(CDPLAYER *cd, int start, int play );

/*
 * Play one track; play ends when track is over
 */
int CDplaytrack( CDPLAYER *cd, int track, int play );

/*
 * Play one track, starting at min:sec:frame within that track
 */
int CDplaytrackabs( CDPLAYER* cd, int track,
 int min, int sec, int frame, int play );

/*
 * Play starting at min:sec:frame, continuing throught the rest of
 * the disc
 */
int CDplayabs( CDPLAYER *cd, int min, int sec, int frame, int play );

/*
 * Read digital audio data
 */
int CDreadda( CDPLAYER *cd, CDFRAME *buf, int num_frames );

/*
 * Return the best size for the num_frames argument of CDreadda.
 * This size will maintain continous data flow from the CD.
 */
int CDbestreadsize( CDPLAYER *cd );

/*
 * Position drive at minute, second, frame for reading digital audio
 */
unsigned long CDseek( CDPLAYER *cd, int min, int sec, int frame );

/*
 * Position drive at given logical block number.
 */
unsigned long CDseekblock( CDPLAYER *cd, unsigned long block);

/*
 * Position drive at start of track t.
 */
int CDseektrack( CDPLAYER *cd, int t );

/*
 * Stop play (or pause)
 */
int CDstop( CDPLAYER *cd );

/*
 * Eject the caddy
 */
int CDeject( CDPLAYER *cd );

/*
 * Enable the eject button
 */
void CDallowremoval( CDPLAYER *cd );

/*
 * Disable the eject button
 */
void CDpreventremoval( CDPLAYER *cd );

/*
 * close the devscsi device
 */
int CDclose( CDPLAYER *cd );

int CDupdatestatus( CDPLAYER *Cd );

/*
 * Get current status
 */
int CDgetstatus( CDPLAYER *cd, CDSTATUS *status );

/*
 * if playing, pause it; if paused, play it.  Otherwise do nothing
 */
int CDtogglepause( CDPLAYER *cd );

/*
 * Get information about a track
 */
int CDgettrackinfo( CDPLAYER *cd, int track, CDTRACKINFO *info );

/*
 * Set/Get volume
 */
int CDsetvolume( CDPLAYER *cd, CDVOLUME *vol );
int CDgetvolume( CDPLAYER *cd, CDVOLUME *vol );

/*
 * Convert an ascii string to timecode
 * return 0 if timecode invalid
 */
int CDatotime(struct cdtimecode *tc, const char *loc);

/*
 * Convert an ascii string to msf
 * return 0 if timecode invalid
 */
int CDatomsf(const char *loc, int *m, int *s, int *f);

/*
 * Convert a frame number to struct timecode
 */

void CDframetotc(unsigned long frame, struct cdtimecode *tc);

/*
 * Convert a frame number to mins/secs/frames
 */

void CDframetomsf(unsigned long frame, int *m, int *s, int *f);

/*
 * Convert a struct time code to a displayable ASCII string
 */
void CDtimetoa( char *s, struct cdtimecode *tp );

/*
 * Convert an array of 6-bit values to an ASCII string
 * This function returns the number of characters converted
 */
int CDsbtoa( char *s, const unchar *sixbit, int count );

/*
 * Convert (minutes, seconds, frame) value to logical block number
 * on given device.  This function is device specific.  You should
 * use CDmsftoframe instead.
 */
unsigned long CDmsftoblock( CDPLAYER *cd, int m, int s, int f );

/*
 * Convert a logical block number to a (minutes, seconds, frame) value.
 */
void CDblocktomsf( CDPLAYER *cd, unsigned long frame, int *m, int *s,  int*f );

/*
 * Convert (minutes, seconds, frame) value to CD frame number.
 * This is useful when you need to compare two values.
 */
unsigned long CDmsftoframe( int m, int s, int f );

/*
 * Convert struct timecode value to CD frame number.
 * This is useful when you need to compare two timecode values.
 */
unsigned long CDtctoframe( struct cdtimecode* tc );

void CDtestready(CDPLAYER*cd, int* changed );

#ifdef __cplusplus
}
#endif

#endif /* !__CDAUDIO_H_ */
