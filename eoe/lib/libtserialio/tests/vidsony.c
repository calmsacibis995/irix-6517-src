/*
   vidsony.c - video and sony deck control
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sys/uio.h>

#include <dmedia/vl.h>

#include <dmedia/audio.h>

#include <dmedia/dmedia.h>
#include <sys/dmcommon.h>
#include <dmedia/dm_params.h>
#include <dmedia/dm_timecode.h>
#include <dmedia/dm_image.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_imageconvert.h>
#include <dmedia/dm_audio.h>
#include <dmedia/dm_vitc.h>
#include <dmedia/dm_ltc.h>
#include <dmedia/vl.h>
#include <dmedia/vl_mvp.h>

#include <math.h>

#include <assert.h>
#include <errno.h>
extern int errno;

#include "../inc/tserialio.h"
TSstatus tsSetDebug(TSconfig config, int debug);

#include <stdarg.h>

/* Utility Routines ---------------------------------------------------- */

stamp_t getustfrommscandpair(stamp_t msc, USTMSCpair pair, double ust_per_msc)
{
  return pair.ust + (msc - pair.msc)*ust_per_msc;
}

#define max(a,b) ( ((a)>(b)) ? (a) : (b))
#define min(a,b) ( ((a)<(b)) ? (a) : (b))

void error_exit(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stdout, format, ap );
  va_end(ap);

  fprintf( stdout, "\n" );
  exit(2);
}

void error(char *format, ...)
{
  va_list ap;

  fprintf( stdout, "---- ");

  va_start(ap, format);
  vfprintf( stdout, format, ap );
  va_end(ap);

  fprintf( stdout, "\n" );
}

void perror2(char *format, ...)
{
  va_list ap;

  fprintf( stdout, "---- ");

  va_start(ap, format);
  vfprintf( stdout, format, ap );
  va_end(ap);

  fprintf( stdout, ": %s\n", strerror(oserror()) );
}

void perror_exit(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stdout, format, ap );
  va_end(ap);

  fprintf(stdout, ": ");
  perror2("");

  fprintf( stdout, "\n" );
  exit(2);
}

/*
** VNC -- "Video NULL Check".  Wraps a VL call that returns NULL
** on error.
*/

#define VNC(call) \
{ \
  if ( (call) == NULL ) \
    error_exit("VL error (%s) for call \"%s\"",  \
               vlStrError(vlErrno), #call); \
}

/*
** VC - "Video Check".  Wraps a VL call that returns <0 on error.
*/

#define VC(call) \
{ \
  if ( (call) < 0 ) \
    error_exit("VL error (%s) for call \"%s\"",  \
               vlStrError(vlErrno), #call); \
}

/*
** VSCC - "Video vlSetControl Check".  Same as VC except 
**        vlValueOutOfRange is ok.
*/
#define VSCC(call) \
{ \
  if ( (call) < 0 && vlErrno != VLValueOutOfRange ) \
    error_exit("VL error (%s) for call \"%s\"",  \
               vlStrError(vlErrno), #call); \
}

/*
** OC -- "OS Check".  Wraps a Unix call that returns negative on error.
*/
#define OC(call) \
{ \
  if ( (call) < 0 ) \
    perror_exit(#call); \
}

/*
** DC -- "DM Params Check".  Wraps a call to a dmParams function that
** returns DM_FAILURE on error.
*/
#define DC(call) \
{ \
  if ( (call) != DM_SUCCESS ) \
    error_exit("DM_FAILURE for call \"%s\"", #call); \
}

char *event_name(int reason)
{
  switch (reason)
    {
    case VLStreamBusy:
      return "VLStreamBusy";    /* Stream is locked */
    case VLStreamPreempted:
      return "VLStreamPreempted"; /* Stream was grabbed */
    case VLAdvanceMissed:
      return "VLAdvanceMissed"; /* Already reached time */
    case VLStreamAvailable:
      return "VLStreamAvailable"; /* Stream has been released */
    case VLSyncLost:
      return "VLSyncLost";      /* Sync isn't being detected */
    case VLStreamStarted:
      return "VLStreamStarted"; /* Stream started delivery */
    case VLStreamStopped:
      return "VLStreamStopped"; /* Stream stopped delivery */
    case VLSequenceLost:
      return "VLSequenceLost";  /* A Field/Frame dropped */
    case VLControlChanged:
      return "VLControlChanged"; /* A Control has changed */
    case VLTransferComplete:
      return "VLTransferComplete"; /* A Transfer has completed */
    case VLTransferFailed:
      return "VLTransferFailed"; /* A Transfer has failed */
    case VLEvenVerticalRetrace:
      return "VLEvenVerticalRetrace"; /* A Vertical Retrace event */
    case VLOddVerticalRetrace:
      return "VLOddVerticalRetrace"; /* A Vertical Retrace event */
    case VLFrameVerticalRetrace:
      return "VLFrameVerticalRetrace"; /* A Vertical Retrace event */
    case VLDeviceEvent:
      return "VLDeviceEvent";   /* A Vertical Retrace event */
    case VLDefaultSource:
      return "VLDefaultSource"; /* Default Source Changed */
    case VLControlRangeChanged:
      return "VLControlRangeChanged";
    case VLControlPreempted:
      return "VLControlPreempted";
    case VLControlAvailable:
      return "VLControlAvailable";
    case VLDefaultDrain:
      return "VLDefaultDrain";  /* Default Drain Changed */
    case VLStreamChanged:
      return "VLStreamChanged"; /* Path connectivity changed */
    case VLTransferError:
      return "VLTransferError"; /* A Single Transfer errored */
    default:
      return "???";
    }
}

char *timing_name (int timingtype)
{
  switch (timingtype) 
    {
    case VL_TIMING_525_SQ_PIX:
      return "NTSC square pix analog (525 lines) 646x486";
      
    case VL_TIMING_625_SQ_PIX:
      return "PAL square pix analog (625 lines) 768x576";
      
    case VL_TIMING_525_CCIR601:
      return "525-line CCIR601 digital component 720x486";
      
    case VL_TIMING_625_CCIR601:
      return "625-line CCIR601 digital component 720x576";
      
    case VL_TIMING_525_4FSC:
      return "525-line 4x ntsc subcarrier 768x486";
      
    case VL_TIMING_625_4FSC:
      return "625-line 4x pal subcarrier 948x576";
      
    default:
      return "???";
    }
}

char *packing_name (int packtype)
{
  switch (packtype) 
    {
    case VL_PACKING_RGB_8:
      return "RGB";

    case VL_PACKING_RGBA_8:
      return "RGBA";
	
    case VL_PACKING_RBG_323:   
      return "Starter Video RGB8";
	
    case VL_PACKING_RGB_332_P:	
      return "RGB332";
	
    case VL_PACKING_Y_8_P: 
      return "8 Bit Greyscale";

    case VL_PACKING_YVYU_422_8:
      return "YVYU_422_8";
	
    default:		
      return "???";
    }
}

char *cap_type_name (int cap_type)
{
  switch(cap_type)
    {
    case VL_CAPTURE_NONINTERLEAVED:
      return "field";
    case VL_CAPTURE_INTERLEAVED:
      return "frame";
    case VL_CAPTURE_EVEN_FIELDS:
      return "even field only";
    case VL_CAPTURE_ODD_FIELDS:
      return "odd field only";
    case VL_CAPTURE_FIELDS:
      return "haphazard field";
    default:
      return "???";
    }
}

void vl_timing_to_video_field_rate(int vl_timing, 
                                   int *rounded, double *precise)
{
  switch (vl_timing) 
    {
    case VL_TIMING_525_SQ_PIX:
    case VL_TIMING_525_CCIR601:
    case VL_TIMING_525_4FSC:
      if (rounded) *rounded = 60;
      if (precise) *precise = 60000.0 / 1001.0;
      return;
      
    case VL_TIMING_625_SQ_PIX:
    case VL_TIMING_625_CCIR601:
    case VL_TIMING_625_4FSC:
      if (rounded) *rounded = 50;
      if (precise) *precise = 50.0;
      return;
        
    default:
      fprintf(stderr, "can't deal with VL timing %d\n", vl_timing);
      return;
    }
}

int vl_timing_to_DM_timecode_tc_type(int timingtype)
{
  switch (timingtype) 
    {
    case VL_TIMING_525_SQ_PIX:
    case VL_TIMING_525_CCIR601:
    case VL_TIMING_525_4FSC:
      return DM_TC_30_ND;
      
    case VL_TIMING_625_SQ_PIX:
    case VL_TIMING_625_CCIR601:
    case VL_TIMING_625_4FSC:
      return DM_TC_25_ND;
      
    default:
      return DM_TC_BAD;
    }
}

void print_tc_type_name(int tc_type)
{
  switch (tc_type & DM_TC_FORMAT_MASK)
    {
    case DM_TC_FORMAT_NTSC: printf("NTSC "); break;
    case DM_TC_FORMAT_PAL:  printf("PAL ");  break;
    case DM_TC_FORMAT_FILM: printf("FILM "); break;
    }
  
  if ((tc_type & DM_TC_FORMAT_MASK) == DM_TC_FORMAT_NTSC)
    {
      switch (tc_type & DM_TC_DROP_MASK)
        {
        case DM_TC_NODROP:    printf("non-drop "); break;
        case DM_TC_DROPFRAME: printf("drop "); break;
        }
    }
  
  switch (tc_type & DM_TC_RATE_MASK)
    {
    case DM_TC_RATE_2997: printf("29.97 frame/sec"); break;
    case DM_TC_RATE_30:   printf("30 frame/sec"); break;
    case DM_TC_RATE_24:   printf("24 frame/sec"); break;
    case DM_TC_RATE_25:   printf("25 frame/sec"); break;
    }

  printf(" (0x%x)", tc_type);
}

int string_to_timecode_sequence(char *string, int tc_type)
{
  DMtimecode tc;
  DMtimecode midnight;
  int sequence;

  midnight.hour = 
    midnight.minute = 
      midnight.second = 
        midnight.frame = 0;
  midnight.tc_type = tc_type;

  /* get frames since midnight */

  if (DM_SUCCESS != dmTCFromString(&tc, string, tc_type) ||
      DM_SUCCESS != dmTCFramesBetween(&sequence, &midnight, &tc))
    {
      error("invalid timecode string [%s]", string); 
      return -1;
    }

  /* then convert so it's really fields since midnight */
  
  return sequence*2;
}

DMstatus timecode_sequence_to_string(char *string, int sequence, int tc_type)
{
  DMtimecode midnight;
  DMtimecode tc;

  midnight.hour = 
    midnight.minute = 
      midnight.second = 
        midnight.frame = 0;
  midnight.tc_type = tc_type;
  
  if (DM_SUCCESS != dmTCAddFrames(&tc, &midnight, sequence/2, NULL) ||
      DM_SUCCESS != dmTCToString(string, &tc))
    return DM_FAILURE;
  else
    return DM_SUCCESS;
}

/* silly timecode text burnin generation code ----------------------------- */

#include "digits.h"

typedef struct tcburnin_state
{
  int packing;
  int timing;
  int tc_type;
} tcburnin_state;

void initialize_tcburnin_state(tcburnin_state *s,
                               int tc_type,
                               int packing,
                               int timing)
{
  s->packing = packing;
  s->timing = timing;
  s->tc_type = tc_type;
}

/*
 * returns fields since midnight sequence number of VITC
 */
void burnin_tc(tcburnin_state *s,
               void *data,
               int xsize, int ysize,
               int xloc, int yloc,
               int timecode_sequence) /* fields since midnight */
{
  int dominant_field;
  char string[40];
  int i, len;

  DC( timecode_sequence_to_string(string, timecode_sequence, s->tc_type) );
  dominant_field = ((timecode_sequence % 2) == 0);

  if (dominant_field)
    strcat(string, ":0");
  else
    strcat(string, ":1");

  /* printf("[%s]\n", string); */

  /* XXX do dominant */
  /* XXX do colons */
  /* XXX perhaps handle fields and digits correctly */

  len = strlen(string);
  for(i=0; i < len; i++)
    {
      char c = string[i];
      if (c >= '0' && c <= '9')
        {
          unsigned char *d = digits[c-'0'];

          switch (s->packing)
            {
            case VL_PACKING_YVYU_422_8:
              {
                int y;
                assert(xloc % 2 == 0); /* our data starts with U */
                for(y=0; y < DIGIT_YSIZE; y++)
                  {
                    unsigned char *dst = 
                      ((unsigned char *)data) + 
                        2*(xsize*(y+yloc) + i*DIGIT_XSIZE + xloc);
                    unsigned char *src = d + 2*(DIGIT_XSIZE*y);
                    memcpy(dst, src, 2*DIGIT_XSIZE);
                  }
              }
              break;
            case VL_PACKING_RGBA_8:
            case VL_PACKING_RGB_8:
            case VL_PACKING_Y_8_P:
            default:
              error_exit("can't do burnin on data with %s packing",
                         packing_name(s->packing));
            }
        }
    }
}

/* common type for VITC, LTC and sony timecodes ------------------------- */

typedef struct timecode_msc_pair
{
  int timecode_sequence; /* count of fields since midnight */
  stamp_t msc; /* a video MSC coincident with this timecode */
} timecode_msc_pair;

/* vitc helper routines -------------------------------------------------- */

typedef struct vitc_state
{
  DMVITCdecoder vitcdecoder;
  int last_vitc_sequence;
  DMtimecode midnight;
} vitc_state;

void initialize_vitc_state(vitc_state *s,
                           int tc_type,
                           int packing,
                           int timing)
{
  DC( dmVITCDecoderCreate(&s->vitcdecoder, tc_type) );
  
  switch (packing)
    {
    case VL_PACKING_RGBA_8:
    case VL_PACKING_RGB_8:
      DC( dmVITCDecoderSetStride(s->vitcdecoder, 4 /*RGBX*/, 2 /*G*/ ) );
      break;
    case VL_PACKING_Y_8_P:
      DC( dmVITCDecoderSetStride(s->vitcdecoder, 1 /*mono*/, 0 ) );
      break;
    case VL_PACKING_YVYU_422_8:
      DC( dmVITCDecoderSetStride(s->vitcdecoder, 2 /*YUV*/, 1 /*Y*/ ) );
      break;
    default:
      error_exit("can't do VITC on data with %s packing",
                 packing_name(packing));
    }
  
  DC( dmVITCDecoderSetPixelTiming(s->vitcdecoder, timing) );
  
  s->last_vitc_sequence = -1;

  s->midnight.hour = 
    s->midnight.minute = 
      s->midnight.second = 
        s->midnight.frame = 0;
  s->midnight.tc_type = tc_type;
}

/*
 * parse VITC out of the given video data with the given video MSC.
 *
 * if the VITC is valid, then update the given timecode_msc_pair
 * for the new VITC codeword.
 */
void parse_vitc(vitc_state *s,
                void *data,
                int xsize, int ysize,
                stamp_t msc,
                timecode_msc_pair *retpair)
{
  DMVITCcode vitcCodeword;
  int sequence;
  
  /* parse vitc */
  if (DM_SUCCESS != dmVITCDecode(s->vitcdecoder, 
                                 data, xsize, ysize, &vitcCodeword))
    {
      error("vitc unreadable");
      return;
    }

  /* compute frames since midnight */
  /* this also sanity checks timecode */
  if (DM_SUCCESS != dmTCFramesBetween(&sequence, 
                                      &s->midnight, &vitcCodeword.tc))
    {
      error("vitc contains invalid timecode");
      return;
    }

  /* then convert so it really is fields since midnight */
  sequence = sequence*2 + (vitcCodeword.evenField ? 1 : 0);
  
  if ( (sequence % 2) != (msc % 2) )
    {
      error("VITC field sense swapped (vitc %d video %lld)", sequence, msc);
      return;
    }
  
  retpair->timecode_sequence = sequence;
  retpair->msc = msc;
}

/* ltc helper routines -------------------------------------------------- */

typedef struct ltc_state
{
  DMLTCdecoder ltcdecoder;
  DMtimecode midnight;
  USTMSCpair last_ltc_codeword_pair;
  stamp_t last_ltc_codeword_startmsc;
  stamp_t last_frontier_msc;
  int last_ltc_sequence;

  ALconfig config;
  int bytes_per_frame;
  ALport port;
  char *intrbuf;
  double ust_per_msc;

  /* these describe the currently buffered data */
  int nmscs;
  char *bufptr;
  stamp_t startmsc;
  USTMSCpair pair;
  
} ltc_state;

void initialize_ltc_state(ltc_state *s,
                          int tc_type)
{
  int rate;
  int bytes_per_sample;
  float qseconds;
  
  /* first open audio path */
  
  s->config = ALnewconfig();  
  ALsetsampfmt(s->config, AL_SAMPFMT_TWOSCOMP);
  ALsetwidth(s->config, AL_SAMPLE_16);
  bytes_per_sample = 2;
  ALsetchannels(s->config, 1);
  s->bytes_per_frame = 2 * 1;
  
  /* get rate */
  {
    long pvbuf[2];
    pvbuf[0] = AL_INPUT_RATE;
    ALgetparams(AL_DEFAULT_DEVICE, pvbuf, 2);
    rate = pvbuf[1];
  }
  
  /* qseconds second queue size */
  
  qseconds = 1.0;
  
  ALsetqueuesize(s->config, 
                 qseconds * ALgetchannels(s->config) * rate);
  
  s->intrbuf = malloc(ALgetqueuesize(s->config) * bytes_per_sample);
  s->ust_per_msc = 1.E9 / (double)rate;
  
  printf("Audio sampfmt: TWOSCOMP\n");
  printf("Audio width: %d bits\n", ALgetwidth(s->config) * 8);
  printf("Audio channels: %d\n", ALgetchannels(s->config));
  printf("Audio rate: %d Hz\n", rate);
  printf("Audio queuesize: %d samples\n", ALgetqueuesize(s->config));
  
  if (NULL == (s->port = ALopenport("vidsony", "r", s->config)))
    error_exit("AL port open failed");
  
  /* set up ltc decoder -- DMparams suck! */

  DC( dmLTCDecoderCreate(&s->ltcdecoder, tc_type) );
  {
    int width = ALgetwidth(s->config);
    int chans = ALgetchannels(s->config);
    DMparams *audioparams;
    
    DC( dmParamsCreate(&audioparams) );
    DC( dmParamsSetInt(audioparams, DM_AUDIO_CHANNELS, chans) );
    DC( dmParamsSetEnum(audioparams, DM_AUDIO_FORMAT, 
                        DM_AUDIO_TWOS_COMPLEMENT ) );
    switch (width)
      {
      case AL_SAMPLE_8:
        DC( dmParamsSetInt(audioparams, DM_AUDIO_WIDTH, 
                           DM_AUDIO_WIDTH_8) );
        break;
      case AL_SAMPLE_16:
        DC( dmParamsSetInt(audioparams, DM_AUDIO_WIDTH, 
                           DM_AUDIO_WIDTH_16) );
        break;
      case AL_SAMPLE_24:
        DC( dmParamsSetInt(audioparams, DM_AUDIO_WIDTH, 
                           DM_AUDIO_WIDTH_24) );
        break;
      }
    
    /* XXX assumes first channel (0) for now, should be key option */
    
    dmLTCDecoderSetParams(s->ltcdecoder, audioparams, 0);
    
    dmParamsDestroy(audioparams);
  }

  /* clear out rb */
  ALreadsamps(s->port, s->intrbuf, ALgetfilled(s->port));

  s->midnight.hour = 
    s->midnight.minute = 
      s->midnight.second = 
        s->midnight.frame = 0;
  s->midnight.tc_type = tc_type;

  s->last_ltc_codeword_startmsc = -1;
  s->last_frontier_msc = -1;
  s->last_ltc_sequence = -1;

  s->nmscs = 0; /* no audio intially buffered up */
}

/*
 * see read_one_ltc_codeword for info
 */
void read_audio(ltc_state *s)
{
  int n;
  assert(s->nmscs == 0);

  /* ---- get new audio data */

  n = ALgetfilled(s->port);
  s->nmscs = n / ALgetchannels(s->config);

  if (!n)
    return;
  
  ALreadsamps(s->port, s->intrbuf, n);
  s->bufptr = s->intrbuf;
  
  /* ---- get chunk timing */
  {
    stamp_t frontier_msc;
    
    if (-1 == ALgetframetime(s->port, 
                             (unsigned long long *)(&s->pair.msc),
                             (unsigned long long *)(&s->pair.ust)))
      error_exit("ALgetframetime failed.  I wonder why.");
    
    if (-1 == ALgetframenumber(s->port, 
                               (unsigned long long *)(&frontier_msc)))
      error_exit("ALgetframenumber failed.  I wonder why.");
    
    if (s->last_frontier_msc != -1 &&
        frontier_msc != s->last_frontier_msc + s->nmscs)
      {
        error("audio frontier msc indicates jump by %d mscs",
              frontier_msc - s->last_frontier_msc + s->nmscs);
      }
    
    s->startmsc = s->last_frontier_msc;
    
    if (s->startmsc < 0)
      s->startmsc = frontier_msc - s->nmscs;
    
    s->last_frontier_msc = frontier_msc;
  }
}

/* 
 * read at most one LTC codeword from audio.
 *
 * to use this routine, first call read_audio().  this reads a chunk
 * of audio.  then call this routine a bunch of times until it returns 0.
 * do this once per trip around your select loop.
 *
 * each time the routine returns (regardless of what it returns),
 * it may have parsed a new LTC codeword.  if so, it will update
 * *retpair with the new pairing of timecode sequence (in fields since
 * midnight) and video msc.  the function uses the given video 
 * ust_per_msc and pair to determine the new timecode_msc_pair.
 * otherwise, the routine does not modify *retpair.
 */
int read_one_ltc_codeword(ltc_state *s,
                          USTMSCpair p, double ust_per_msc,
                          timecode_msc_pair *retpair)
{
  DMLTCcode codeword;
  int nmscs_before, nmscs_eaten;
  DMstatus ret;
  stamp_t codeword_startmsc;
  USTMSCpair codeword_pair;
  int sequence;
  stamp_t cwust;

  if (s->nmscs == 0)
    {
      return 0;
    }

  nmscs_before = s->nmscs;
  
  /* dmLTCDecode increments pointer and decrements nmscs */
  ret = dmLTCDecode(s->ltcdecoder, (void **)&s->bufptr, &s->nmscs, &codeword);

  nmscs_eaten = nmscs_before - s->nmscs;
  assert(nmscs_eaten <= nmscs_before && nmscs_eaten >= 0);
  s->startmsc += nmscs_eaten;
  
  if (DM_SUCCESS != ret) /* consumed all nmscs, found no LTC codeword */
    {
      assert(nmscs_eaten == nmscs_before);
      assert(s->nmscs == 0);
      return 0;
    }
  
  /* recall the timing info for start of this codeword */

  codeword_startmsc = s->last_ltc_codeword_startmsc;
  codeword_pair = s->last_ltc_codeword_pair;
  
  /* record current timing info for use as start of next codeword */

  s->last_ltc_codeword_startmsc = s->startmsc;
  s->last_ltc_codeword_pair = s->pair;
  
  if (codeword_startmsc == -1) /* if this is first codeword */
    {
      /* don't tell user about it, since we don't have accurate startust */
      return (s->nmscs > 0);
    }

  /*
   * we got a codeword end AND we know its startust.
   * get frames since midnight for this codeword.
   * this also sanity checks timecode.
   */
  if (DM_SUCCESS != dmTCFramesBetween(&sequence, 
                                      &s->midnight, 
                                      &codeword.tc))
    {
      /* blow it off, timecode is invalid */
      error("LTC audio contains invalid timecode");
      return (s->nmscs > 0);
    }
  
  /* convert to fields since midnight */
  sequence *= 2;
  
  /* check LTC sequence */
  if (s->last_ltc_sequence != -1 &&
      sequence != s->last_ltc_sequence + 2)
    {
      /* use it anyway -- it's just as likely to be valid as invalid */
      error("LTC out of sequence (%d->%d)", s->last_ltc_sequence, sequence);
    }
  s->last_ltc_sequence = sequence;

  /* get ust for this codeword */

  cwust = getustfrommscandpair(codeword_startmsc, codeword_pair, 
                               s->ust_per_msc);

  /* 
   * update the timecode_msc_pair.
   * choose msc of field whose ust is nearest codeword ust.
   */
  retpair->timecode_sequence = sequence;
  retpair->msc = p.msc + rint( (cwust-p.ust)/ust_per_msc );

  /* sanity check: should be dominant field */
  if ((retpair->msc % 2) == 1)
    error("LTC lines up with non-dominant video MSC %lld", retpair->msc);

  return (s->nmscs > 0);
}

/* sony 9-pin RS-422 deck control routines ------------------------------ */

void send0(TSport wp, stamp_t stamp,
           unsigned char cmd1, unsigned char cmd2)
{
  unsigned char c;
  
  tsWrite(wp, &cmd1,  &stamp, 1);
  tsWrite(wp, &cmd2,  &stamp, 1);
  
  c = (unsigned char)(cmd1+cmd2); /* checksum */
  tsWrite(wp, &c, &stamp, 1);
}

void send1(TSport wp, stamp_t stamp,
           unsigned char cmd1, unsigned char cmd2,
           unsigned char data1)
{
  unsigned char c;
  
  tsWrite(wp, &cmd1,  &stamp, 1);
  tsWrite(wp, &cmd2,  &stamp, 1);
  tsWrite(wp, &data1, &stamp, 1);
  
  c = (unsigned char)(cmd1+cmd2+data1); /* checksum */
  tsWrite(wp, &c, &stamp, 1);
}

void send2(TSport wp, stamp_t stamp,
           unsigned char cmd1, unsigned char cmd2,
           unsigned char data1, unsigned char data2)
{
  unsigned char c;
  
  tsWrite(wp, &cmd1,  &stamp, 1);
  tsWrite(wp, &cmd2,  &stamp, 1);
  tsWrite(wp, &data1, &stamp, 1);
  tsWrite(wp, &data2, &stamp, 1);
  
  c = (unsigned char)(cmd1+cmd2+data1+data2); /* checksum */
  tsWrite(wp, &c, &stamp, 1);
}

#define packem(hi, lo)  (((unsigned char)(hi) << 4) | ((unsigned char)(lo)))

void sendtc(TSport wp, stamp_t stamp,
           unsigned char cmd1, unsigned char cmd2,
           int h, int m, int s, int f)
{
  unsigned char hh = packem(h/10, h%10);
  unsigned char mm = packem(m/10, m%10);
  unsigned char ss = packem(s/10, s%10);
  unsigned char ff = packem(f/10, f%10);
  unsigned char c;
  
  tsWrite(wp, &cmd1,  &stamp, 1);
  tsWrite(wp, &cmd2,  &stamp, 1);
  tsWrite(wp, &ff, &stamp, 1);
  tsWrite(wp, &ss, &stamp, 1);
  tsWrite(wp, &mm, &stamp, 1);
  tsWrite(wp, &hh, &stamp, 1);
  
  c = (unsigned char)(cmd1+cmd2+ff+ss+mm+hh); /* checksum */
  tsWrite(wp, &c, &stamp, 1);
}

void print_tc(unsigned char tc[4])
{
  printf("%02xh:%02xm:%02xs:%02xf%s", 
         tc[3], tc[2], tc[1]&0x7f, tc[0], (tc[1]&0x80)?"*":"");
}

void get_tc(unsigned char tc[4], DMtimecode *otc, int *is_dom_field)
{
  otc->hour =   (tc[3]&0x0f) + 10*((tc[3]&0x30)>>4);
  otc->minute = (tc[2]&0x0f) + 10*((tc[2]&0x70)>>4);
  otc->second = (tc[1]&0x0f) + 10*((tc[1]&0x70)>>4);
  otc->frame =  (tc[0]&0x0f) + 10*((tc[0]&0x30)>>4);
  if (is_dom_field)
    *is_dom_field = ((tc[1]&0x80)==0);
  /* printf("data h%02x m%02x s%02x f%02x\n", tc[3], tc[2], tc[1], tc[0]); */
}

void print_ub(unsigned char ub[4])
{
  printf("ub%02x:%02x:%02x:%02x", ub[3], ub[2], ub[1], ub[0]);
}

void printcmd(unsigned short cmd, char *data, int datalen)
{
  switch (cmd)
    {
    case 0x1001: 
      printf("ACK\n");
      break;
    case 0x1112:
      printf("NACK %02x\n", data[0]);
      exit(2);
      break;

    case 0x700d: 
      printf("no time to report, sorry.");
      printf("\n");  
      break;

#undef PRINTCODES

    case 0x7400: 
#ifdef PRINTCODES      
      printf("TIMER-1 tc: "); print_tc(data); 
      printf("\n");  
#endif
      break;
    case 0x7404: 
#ifdef PRINTCODES      
      printf("LTC tc: "); print_tc(data); 
      printf("\n");  
#endif
      break;
    case 0x7804: 
#ifdef PRINTCODES      
      printf("LTC tc: "); print_tc(data); 
      printf(" LTC ub: "); print_ub(data+4); 
      printf("\n");  
#endif
      break;
    case 0x7405: 
#ifdef PRINTCODES      
      printf("LTC ub: "); print_ub(data); 
      printf("\n");  
#endif
      break;
    case 0x7406: 
#ifdef PRINTCODES      
      printf("VITC tc: "); print_tc(data); 
      printf("\n");  
#endif
      break;
    case 0x7806: 
#ifdef PRINTCODES      
      printf("VITC tc: "); print_tc(data); 
      printf(" VITC ub: "); print_ub(data+4); 
      printf("\n");  
#endif
      break;
    case 0x7407: 
#ifdef PRINTCODES      
      printf("VITC ub: "); print_ub(data); 
      printf("\n");  
#endif
      break;
    case 0x7414: 
#ifdef PRINTCODES      
      printf("LTC (interp) tc: "); print_tc(data); 
      printf("\n");  
#endif
      break;
    case 0x7814: 
#ifdef PRINTCODES      
      printf("LTC (interp) tc: "); print_tc(data); 
      printf(" LTC (interp) ub: "); print_ub(data+4); 
      printf("\n");  
#endif
      break;
    case 0x7416: 
#ifdef PRINTCODES      
      printf("VITC (hold) tc: "); print_tc(data); 
      printf("\n");  
#endif
      break;
    case 0x7816: 
#ifdef PRINTCODES      
      printf("VITC (hold) tc: "); print_tc(data); 
      printf(" VITC (hold) ub: "); print_ub(data+4); 
      printf("\n");  
#endif
      break;

    default:
      {
        int i;
        printf("Command %04x: ", cmd);
        for(i=0; i < datalen; i++)
          printf("%02x ", data[i]);
        printf("\n");
      }
      break;
    }
}

/*
 * call this when not using a sony9pin_state.
 * blocks until response arrives.  assumes first character
 * is the beginning of a response.  returns TRUE for all 
 * responses except error responses (NAK).
 */
int wait_for_one_response(TSport rp)
{
  stamp_t stamp;
  unsigned char b[2+16+1];
  unsigned short cmd;
  unsigned char csum;
  int len, i;

  tsRead(rp, &b[0], &stamp, 1); /* read cmd1 */
  tsRead(rp, &b[1], &stamp, 1); /* read cmd2 */
  
  cmd = (b[0]<<8)|b[1];
  len = b[0]&0x0f;

  for(i=0; i < len; i++)
    tsRead(rp, &b[2+i], &stamp, 1); /* read data */
  
  tsRead(rp, &b[2+len], &stamp, 1); /* read checksum */

  csum = 0;
  for(i=0; i < 2 + len; i++) /* compute checksum */
    csum += b[i];
  
  if (csum != b[2+len])
    error_exit("bad checksum on sony command 0x%04x\n", cmd);
  
  printcmd(cmd, b, len);
  
  switch (cmd)
    {
    case 0x1112: /* NAK */
      return FALSE;
    }
  
  return TRUE;
}

/*
 * 'capacity' in bytes must be enough to transmit 'ahead' worth of
 * where am I and edit in/out commands
 *
 * software will attempt to trasmit where am I and edit commands
 * 'within_field' after the start of the field (after the ust of
 * the corresponding video item).  For most commands, the maximum
 * allowable by the deck is 6ms, and there is a +-1ms jitter in
 * the SGI's serial scheduling.
 */
#define SONY9PIN_AHEAD 1000000000LL      /* 1 s */
#define SONY9PIN_WITHIN_FIELD 2000000LL
#define SONY9PIN_CAPACITY 600

typedef struct sony9pin_state
{
  stamp_t next_sony_out_ust;
  stamp_t next_sony_out_msc;
  stamp_t last_command_ust; /* debugging */

  unsigned char rcvbuf[20];
  int nrcv;
  stamp_t sony_in_ust;
  int last_timecode_sequence;
  int tc_type;
  DMtimecode midnight;

  int nfilled;
} sony9pin_state;

void initialize_sony9pin_state(sony9pin_state *s,
                               int tc_type)
{
  s->next_sony_out_ust = -1;
  s->next_sony_out_msc = -1;

  s->last_command_ust = -1; /* debugging */

  s->sony_in_ust = -1;
  s->nrcv = 0;
  s->last_timecode_sequence = -1;
  s->tc_type = tc_type;
  s->midnight.hour = 
    s->midnight.minute = 
      s->midnight.second = 
        s->midnight.frame = 0;
  s->midnight.tc_type = s->tc_type;

  s->nfilled = 0;
}

void snap_ust_to_next_msc_boundary(stamp_t ust, 
                                   USTMSCpair p, double ust_per_msc,
                                   stamp_t *retust, stamp_t *retmsc)
{
  int away = (int)(1 + floor( (ust-p.ust)/ust_per_msc ));
  *retmsc = p.msc + away;
  *retust = p.ust + away * ust_per_msc;
}

/* 
 * given a UST/MSC pair from video, which tells us where the
 * video vertical syncs fall, queue up some more serial commands
 * for transmission to the deck. the idea is that we always want 
 * to keep SONY9PIN_AHEAD nanoseconds worth of commands queued up.
 *
 * this function sends exactly one command per field time.
 * if we reach an edit in or edit out point, send the appropriate
 * command.  otherwise send a current time sense command.  
 * edit in and out should occur at the video msc given.
 */
void send_serial_commands(sony9pin_state *s, TSport wp, 
                          USTMSCpair p, double ust_per_msc,
                          int edit_in_msc, int edit_out_msc)
{
  stamp_t next_ust = s->next_sony_out_ust;
  stamp_t next_msc = s->next_sony_out_msc;
  stamp_t now;
  
  dmGetUST((unsigned long long *)(&now));

  if (next_ust == -1) /* first time */
    {
      /* our first command should go out SONY9PIN_AHEAD nanos from now.
       * this gives us a good safety margin to assure that all output
       * commands are timely.
       */
      snap_ust_to_next_msc_boundary(now + SONY9PIN_AHEAD, p, ust_per_msc,
                                    &next_ust, &next_msc);
    }

  while (1) 
    {
      stamp_t command_ust;
      
      /* invariant: 'next_ust' contains the ust of the field in which we
       * should next send a current time sense command.  'next_msc' contains
       * the corresponding video msc.
       */
      if (next_ust >= now+SONY9PIN_AHEAD) /* we've buffered enough commands */
        break;
      
      /* compute place within field to transmit current time sense command */
      
      command_ust = next_ust + SONY9PIN_WITHIN_FIELD; 
      
      /* sanity check command spacing */
      
      if (s->last_command_ust != -1 &&
          llabs((command_ust-s->last_command_ust)-ust_per_msc) > 2000000LL)
        error("output delta was %lfms and not %lfms", 
              (command_ust-s->last_command_ust)/1.E6,
              ust_per_msc/1.E6);
      s->last_command_ust = command_ust;

      /* send command */

      if (edit_in_msc >= 0 && next_msc == edit_in_msc)
        {
          /* write edit in command */
          printf("edit in\n");
          send0(wp, command_ust, 0x20, 0x65);
        }
      else if (edit_out_msc >= 0 && next_msc == edit_out_msc)
        {
          /* write edit out command */
          printf("edit out\n");
          send0(wp, command_ust, 0x20, 0x64);
        }
      else
        {
          /* write a current time sense command */
          send1(wp, command_ust,
                0x61, 0x0c, /* current time sense */
                0x02); /* request VITC only */
        }
      
      /* advance to the next field */
      snap_ust_to_next_msc_boundary(next_ust + ust_per_msc/2, p, ust_per_msc,
                                    &next_ust, &next_msc);
    }
  
  s->next_sony_out_ust = next_ust;
  s->next_sony_out_msc = next_msc;
}

/*
 * see read_one_sony_response for info
 */
void read_sony(sony9pin_state *s, TSport rp)
{
  assert(s->nfilled == 0);
  s->nfilled = tsGetFilledBytes(rp);
}

#undef SHOWPROTOCOL

/* 
 * read at most one sony response from the specified port
 *
 * to use this routine, first call read_sony().  this reads a chunk
 * of serial.  then call this routine a bunch of times until it returns 0.
 * do this once per trip around your select loop.
 *
 * each time the routine returns (regardless of what it returns),
 * it may have parsed a new sony response.  if so, and if the response
 * was a valid response to a current time sense command, it will update
 * *retpair with the new pairing of timecode sequence (in fields since
 * midnight) and video msc.  the function uses the given video 
 * ust_per_msc and pair to determine the new timecode_msc_pair.
 * otherwise, the routine does not modify *retpair.
 */
int read_one_sony_response(sony9pin_state *s, TSport rp, 
                           USTMSCpair p, double ust_per_msc,
                           timecode_msc_pair *retpair)
{
  int len, n2grab, is_dom_field;
  DMtimecode sony_timecode;
  int sequence;

  /* response format is:
   *    (CMD1<<4)|LEN CMD2 DATA1 ... DATA16 CHECKSUM
   * 
   * CMD1 and CMD2 make up command.
   * LEN is 0x0 to 0xf indicating how many bytes of DATA
   * DATA is LEN bytes of data
   * CHECKSUM is simple running sum of all bytes from CMD1 on.
   *
   * when this routine is called, we could already be in the middle
   * of a response (s->nrcv > 0) or we could be waiting for a new
   * response (s->nrcv == 0).
   *
   * this routine empties the TSport.  as it completes each response,
   * it updates *retpair with an up-to-date timecode_sequence/msc pair.
   */
  if (s->nfilled == 0) /* is there 1 byte? */
    return 0;

  if (s->nrcv == 0) /* are we reading first byte of a response? */
    {
      stamp_t stamp;
      tsRead(rp, &s->rcvbuf[0], &stamp, 1);
#ifdef SHOWPROTOCOL
      printf("got 0x%02x at %lfms\n", s->rcvbuf[0], stamp/1.E6);
#endif
      s->nfilled -= 1;
      if (s->sony_in_ust != -1 && 
          llabs((stamp-s->sony_in_ust)-ust_per_msc) > 2000000LL)
        error("sony in ust jumped %lfms and not %lfms",
              (stamp-s->sony_in_ust)/1.E6, ust_per_msc/1.E6);
      s->sony_in_ust = stamp;
      s->nrcv = 1;
    }

  if (s->nfilled == 0) /* is there still 1 byte? */
    return 0;
  
  /* read subsequent bytes of a response -- as much as possible */
  
  len = s->rcvbuf[0]&0x0f;
  n2grab = min(s->nfilled, (2+len+1)-s->nrcv);

#ifdef SHOWPROTOCOL
  {
    int k;
    stamp_t stamp;
    for(k=0; k < n2grab; k++)
      {
        tsRead(rp, &s->rcvbuf[s->nrcv+k], &stamp, 1);
        printf("got 0x%02x at %lfms\n", s->rcvbuf[s->nrcv+k],stamp/1.E6);
      }
  }
#else
  tsRead(rp, &s->rcvbuf[s->nrcv], NULL, n2grab);
#endif
  s->nfilled -= n2grab;
  s->nrcv += n2grab;
  assert(s->nrcv <= 2 + len + 1);

  /* did we get a complete response? */

  if (s->nrcv < 2 + len + 1)
    {
      assert(s->nfilled == 0); /* nope, and we used up all serial bytes */
      return 0;
    }
  assert(s->nrcv == 2 + len + 1);

  /* got a complete response -- parse it */

  s->nrcv = 0;

  /* get a DMtimecode from the serial bytes */
  /* also determine is_dom_field */
  {
    unsigned short cmd = (s->rcvbuf[0]<<8)|s->rcvbuf[1];
    unsigned char *data = &s->rcvbuf[2];
    unsigned char csum=0;
    int i;
    for(i=0; i < 2 + len; i++) /* sum up CMD and DATA */
      csum += s->rcvbuf[i];
    if (csum != s->rcvbuf[2 + len])
      error_exit("bad checksum on sony command 0x%04x\n", cmd);
    printcmd(cmd, data, len);
    sony_timecode.tc_type = DM_TC_BAD;
    switch (cmd)
      {
      case 0x7404:        /* ltc tc */
      case 0x7804:        /* ltc tcub */
      case 0x7406:        /* vitc tc */
      case 0x7806:        /* vitc tcub */
      case 0x7414:        /* ltc interp tc */
      case 0x7814:        /* ltc interp tcub */
      case 0x7416:        /* vitc hold tc <-- XXX omit ?? */
      case 0x7816:        /* vitc hold tcub <-- XXX omit ?? */
        get_tc(data, &sony_timecode, &is_dom_field);
        sony_timecode.tc_type = s->tc_type;
        break;
      case 0x1001:         /* ACK */
        break;
      default:
        error_exit("unexpected command 0x%04d", cmd);
      }
  }

  /* did this response contain timecode? */

  if (sony_timecode.tc_type == DM_TC_BAD)
    return (s->nfilled > 0); /* nope.  return */
  
  /* got a timecode response.
   * compute frames since midnight.
   * sanity-check timecode.
   */
  if (DM_SUCCESS != dmTCFramesBetween(&sequence, &s->midnight, &sony_timecode))
    {
      error("sony timecode invalid");
      return (s->nfilled > 0);
    }
  
  /* then convert so it really is fields since midnight */
  
  sequence = sequence*2 + (is_dom_field ? 0 : 1);

  /* check sequence */
  
  if (s->last_timecode_sequence != -1 &&
      sequence != s->last_timecode_sequence+1)
    {
      /* use it anyway -- it's just as likely to be valid as invalid */
      error("sony timecode jumped %d fields",
            sequence - s->last_timecode_sequence);
    }
  s->last_timecode_sequence = sequence;
  
  /* 
   * update the timecode_msc_pair.
   * choose msc of field whose ust is just before response 
   */
  retpair->timecode_sequence = sequence;
  retpair->msc = p.msc + floor( (s->sony_in_ust-p.ust)/ust_per_msc );
  return (s->nfilled > 0);
}

/* main ------------------------------------------------------------ */

stamp_t last_report = LONGLONG_MIN;
stamp_t last_sony_midnight_msc = LONGLONG_MIN;
stamp_t last_ltc_midnight_msc = LONGLONG_MIN;
stamp_t last_vitc_midnight_msc = LONGLONG_MIN;

#define TIMEOUT

void check_tpairs(timecode_msc_pair sony_tpair,
                  timecode_msc_pair vitc_tpair,
                  timecode_msc_pair ltc_tpair)
{
#ifdef TIMEOUT
  stamp_t now;
#endif

  int sonygood = (sony_tpair.timecode_sequence >= 0);
  int ltcgood = (ltc_tpair.timecode_sequence >= 0);
  int vitcgood = (vitc_tpair.timecode_sequence >= 0);

  stamp_t sony_midnight_msc = (sonygood ? 
     (sony_tpair.msc - sony_tpair.timecode_sequence) : LONGLONG_MIN);
  stamp_t ltc_midnight_msc = (ltcgood ? 
     (ltc_tpair.msc - ltc_tpair.timecode_sequence) : LONGLONG_MIN);
  stamp_t vitc_midnight_msc = (vitcgood ? 
     (vitc_tpair.msc - vitc_tpair.timecode_sequence) : LONGLONG_MIN);

  int show=0;
#ifdef TIMEOUT
  int timeout=0;
#endif

  if (sony_midnight_msc != last_sony_midnight_msc) 
    {
      show=1;
      last_sony_midnight_msc = sony_midnight_msc;
    }
  if (ltc_midnight_msc != last_ltc_midnight_msc) 
    {
      show=1;
      last_ltc_midnight_msc = ltc_midnight_msc;
    }
  if (vitc_midnight_msc != last_vitc_midnight_msc) 
    {
      show=1;
      last_vitc_midnight_msc = vitc_midnight_msc;
    }

#ifdef TIMEOUT
  dmGetUST((unsigned long long *)(&now));
  if (now >= last_report + 5000000000LL)
    {
      show = 1;
      timeout = 1;
      last_report = now;
    }
#endif
  
  if (show)
    {
      stamp_t min = LONGLONG_MAX;
      if (sony_midnight_msc != LONGLONG_MIN && sony_midnight_msc < min) 
        min = sony_midnight_msc;
      if (ltc_midnight_msc != LONGLONG_MIN && ltc_midnight_msc < min) 
        min = ltc_midnight_msc;
      if (vitc_midnight_msc != LONGLONG_MIN && vitc_midnight_msc < min) 
        min = vitc_midnight_msc;
       
      printf("sony ");
      if (sony_midnight_msc == LONGLONG_MIN) 
        printf("<nogood> ");
      else
        { 
          assert(min != LONGLONG_MAX); 
          printf("%lld ", sony_midnight_msc - min);
        }

      printf("ltc ");
      if (ltc_midnight_msc == LONGLONG_MIN) 
        printf("<nogood> ");
      else
        { 
          assert(min != LONGLONG_MAX); 
          printf("%lld ", ltc_midnight_msc - min);
        }

      printf("vitc ");
      if (vitc_midnight_msc == LONGLONG_MIN) 
        printf("<nogood> ");
      else
        { 
          assert(min != LONGLONG_MAX); 
          printf("%lld ", vitc_midnight_msc - min);
        }

#ifdef TIMEOUT
      if (!timeout)
        printf("CHANGE");
#endif

      printf("\n");
    }
}

#define VM 0 /* vid to mem */
#define MV 1 /* mem to vid */

int main(int argc, char **argv)
{
  int c, rc;
  int job = VM;

  /* video */
  VLServer svr;
  VLPath path;
  VLNode drn, src, mem, vid;
  VLControlValue val;
  VLBuffer buffer;
  int xsize, ysize;
  double video_ust_per_msc;
  int video_event_fd;
  int video_buffer_fd;
  stamp_t expected_fmsc;
  int xfrsize;
  int tc_type;
  int timing;
  int packing;
  vitc_state vs;
  tcburnin_state tbs;
  timecode_msc_pair vitc_tpair;

  /* serial */
  TSport rp, wp;
  char devname[20];
  int portnum = 2;
  sony9pin_state s9s;
  timecode_msc_pair sony_tpair;

  /* ltc */
  ltc_state ls;
  timecode_msc_pair ltc_tpair;

  int edit_in_sequence = -1;
  int edit_out_sequence = -1;
  char *edit_in_sequence_string = NULL;
  char *edit_out_sequence_string = NULL;
  char *cue_up_string = NULL;

  while ((c = getopt(argc, argv, "p:d:r:io1:2:c:")) != EOF) 
    {
      switch(c) 
	{
        case 'p':
          portnum = atoi(optarg);
          break;
        case 'i':
          job = VM;
          break;
        case 'o':
          job = MV;
          break;
        case '1':
          edit_in_sequence_string = strdup(optarg);
          break;
        case '2':
          edit_out_sequence_string = strdup(optarg);
          break;
        case 'c':
          cue_up_string = strdup(optarg);
          break;
        }
    }

  /* ----- open serial port */

  printf("opening serial port %d\n", portnum);
  
  sprintf(devname,"/dev/ttyts%d", portnum);
  
  {
    TSconfig config = tsNewConfig();
    tsSetPortName(config, devname);
    tsSetQueueSize(config, SONY9PIN_CAPACITY);
    tsSetCflag(config, CS8|PARENB|PARODD);
    tsSetOspeed(config, 38400);
    tsSetProtocol(config, TS_PROTOCOL_RS232);

    tsSetDebug(config, 0);
  
    tsSetDirection(config, TS_DIRECTION_RECEIVE);
    
    if (TS_SUCCESS != (rc=tsOpenPort(config, &rp)))
      error_exit("rx open bailed with %d", rc);
    
    tsSetDirection(config, TS_DIRECTION_TRANSMIT);
    
    if (TS_SUCCESS != (rc=tsOpenPort(config, &wp)))
      error_exit("tx open bailed with %d", rc);
    
    tsFreeConfig(config);
  }
  
  /* ----- open video port */

  if (job == VM)
    {
      /*
       * Open video input.
       */
      VNC(svr = vlOpenVideo(""));
      VC(src = vlGetNode(svr, VL_SRC, VL_VIDEO, VL_ANY));
      VC(drn = vlGetNode(svr, VL_DRN, VL_MEM, VL_ANY));
      VC(path = vlCreatePath(svr, VL_ANY, src, drn));
      VC(vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE,VL_SHARE));
      vid = src;
      mem = drn;
    }
  else /* job == MV */
    {
      /*
       * Open video output.
       */
      VNC(svr = vlOpenVideo(""));
      VC(src = vlGetNode(svr, VL_SRC, VL_MEM, VL_ANY));
      VC(drn = vlGetNode(svr, VL_DRN, VL_VIDEO, VL_ANY));
      VC(path = vlCreatePath(svr, VL_ANY, src, drn));
      VC(vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE,VL_SHARE));
      mem = src;
      vid = drn;
    }

  /* get field rate and video_ust_per_msc */
  {
    double fields_per_sec;
    VC(vlGetControl(svr, path, vid, VL_TIMING, &val));
    timing = val.intVal;
    printf("timing is %s\n", timing_name(timing));
    vl_timing_to_video_field_rate(timing, NULL, &fields_per_sec);
    
    /* since we choose VL_CAPTURE_NONINTERLEAVED, each MSC is a field */
    video_ust_per_msc = 1.E9 / fields_per_sec;
    
    /* get timecode type */
    tc_type = vl_timing_to_DM_timecode_tc_type(timing);
    printf("tc_type is "); print_tc_type_name(tc_type); printf("\n");
  }

  /* each buffer will contain one field. */

  val.intVal = VL_CAPTURE_NONINTERLEAVED;
  printf("one %s per buffer\n", cap_type_name(val.intVal));
  VSCC(vlSetControl(svr, path, mem, VL_CAP_TYPE, &val));

  /* set colorspace */

  packing = VL_PACKING_YVYU_422_8;
  val.intVal = packing;
  vlSetControl (svr, path, mem, VL_PACKING, &val);
  printf("packing is %s\n", packing_name(packing));

  /* set negative offset so we capture VITC */

  VC(vlGetControl(svr, path, mem, VL_OFFSET, &val));
  /* XXX need to figure out device-dependencies here */
  val.xyVal.y = -11;
  VSCC(vlSetControl(svr, path, mem, VL_OFFSET, &val));

  /* get size */
  
  VC(vlGetControl(svr, path, mem, VL_SIZE, &val));
  printf("video field size is %dx%d\n", val.xyVal.x, val.xyVal.y);
  xsize = val.xyVal.x;
  ysize = val.xyVal.y;

  xfrsize = vlGetTransferSize(svr, path);

#define BUFSIZE 10

  buffer = vlCreateBuffer(svr, path, mem, BUFSIZE);
  vlRegisterBuffer (svr, path, mem, buffer);
  vlBeginTransfer (svr, path, 0, NULL);

  video_event_fd = vlGetFD(svr);
  video_buffer_fd = vlBufferGetFd(buffer);

  /* ----- deal with edit points */

  if (job == MV) /* mem to vid */
    {
      if (edit_in_sequence_string)
        {
          edit_in_sequence = 
            string_to_timecode_sequence(edit_in_sequence_string, tc_type);
          printf("edit in at [%s] (%d)\n", 
                 edit_in_sequence_string, edit_in_sequence);
        }
      
      if (edit_out_sequence_string)
        {
          edit_out_sequence = 
            string_to_timecode_sequence(edit_out_sequence_string, tc_type);
          printf("edit out at [%s] (%d)\n", 
                 edit_out_sequence_string, edit_out_sequence);
        }

      if (edit_in_sequence > 0 ||
          edit_out_sequence > 0)
        {
          printf("perfoming insert edit of all but timecode\n");
          send2(wp, 0, 0x42, 0x30, 0x53, 0x0f);
          if (!wait_for_one_response(rp))
            error_exit("bad response");
        }
    }
      
  /* ----- cue up tape */

  if (cue_up_string)
    {
      DMtimecode tc;
      if (DM_SUCCESS != dmTCFromString(&tc, cue_up_string, tc_type))
        error("invalid timecode string [%s]", cue_up_string); 
      else
        {
          printf("queuing up to [%s] (%d)\n", 
                 cue_up_string, 
                 string_to_timecode_sequence(cue_up_string, tc_type));
          sendtc(wp, 0, 0x24, 0x31,
                 tc.hour, tc.minute, tc.second, tc.frame);
          if (!wait_for_one_response(rp))
            error_exit("bad response");
          printf("press a key when ready...\n");
          getchar();
          send0(wp, 0, 0x20, 0x01);
          if (!wait_for_one_response(rp))
            error_exit("bad response");
        }
    }

  /* ----- go! */

  expected_fmsc = -1;
  initialize_vitc_state(&vs, tc_type, packing, timing);
  initialize_tcburnin_state(&tbs, tc_type, packing, timing);
  vitc_tpair.timecode_sequence = -1;
  vitc_tpair.msc = -1;

  initialize_sony9pin_state(&s9s, tc_type);
  sony_tpair.timecode_sequence = -1;
  sony_tpair.msc = -1;

  initialize_ltc_state(&ls, tc_type);
  ltc_tpair.timecode_sequence = -1;
  ltc_tpair.msc = -1;

  while (1)
    {
      fd_set readset, writeset;
      int n_xfered;
      stamp_t fmsc;
      USTMSCpair p;

      /* get a video UST/MSC pair so we know where vertical syncs are */

      VC( vlGetUSTMSCPair(svr, path, vid, VL_ANY, mem, &p) );

      /* send some more serial commands out the serial port */
      {
        stamp_t edit_in_msc = -1;
        stamp_t edit_out_msc = -1;
        if (sony_tpair.timecode_sequence >= 0) /* if sony pair is valid */
          {
            stamp_t d = sony_tpair.msc - sony_tpair.timecode_sequence;
            if (edit_in_sequence >= 0) /* if we want to edit in */
              edit_in_msc = edit_in_sequence + d;
            if (edit_out_sequence >= 0) /* if we want to edit out */
              edit_out_msc = edit_out_sequence + d;
          }
        send_serial_commands(&s9s, wp, 
                             p, video_ust_per_msc,
                             edit_in_msc, edit_out_msc);
      }
      
      /* read the latest LTC codewords and update ltc_tpair */
      read_audio(&ls);
      while (read_one_ltc_codeword(&ls, p, video_ust_per_msc, &ltc_tpair))
        check_tpairs(sony_tpair, vitc_tpair, ltc_tpair);
              
      /* read the latest sony deck control responses, update sony_tpair */
      read_sony(&s9s, rp);
      while (read_one_sony_response(&s9s, rp, p,video_ust_per_msc,&sony_tpair))
        check_tpairs(sony_tpair, vitc_tpair, ltc_tpair);
      
      /* process up to BUFSIZE video fields */
      
      for(n_xfered=0; n_xfered < BUFSIZE; n_xfered++)
        {
          VLInfoPtr info;
          void *data;
          
          if (job == VM)
            {
              info = vlGetNextValid(svr, buffer);
              if (info && expected_fmsc > 0) expected_fmsc += 1;
            }
          else /* job == MV */
            info = vlGetNextFree(svr, buffer, xfrsize);
          
          if (!info) /* no more data/space this time around the loop */
            break;
          
          /* get a frontier msc -- we use this to relate MSC to our data */
          
          VC( fmsc = vlGetFrontierMSC(svr, path, mem) );
          
          /* use the video data */
          
          VNC( data = vlGetActiveRegion(svr, buffer, info) );

          if (job == VM)
            {
              /* printf("got field msc %lld\n", vitc_tpair.msc); */
              
              /* parse VITC, updating vitc_tpair if VITC is valid */
              
              parse_vitc(&vs, data, xsize, ysize,
                         fmsc-1, /* msc of item we just took out */
                         &vitc_tpair);
              
              check_tpairs(sony_tpair, vitc_tpair, ltc_tpair);
            }
          else /* job == MV */   
            {
              /* printf("got field msc %lld\n", fmsc); */
              
              if (sony_tpair.timecode_sequence >= 0)
                {
                  /* fmsc is the msc of the field we're writing */
                  /* deck control tells us which tc this should be */
                  int sequence = fmsc +
                    sony_tpair.timecode_sequence - sony_tpair.msc;
                  
                  /* burn that tc into image */

                  burnin_tc(&tbs, data, xsize, ysize, 100,100, sequence);
                }
            }
          
          /* now we're done with the data */
          
          if (job == VM)
            vlPutFree(svr, buffer);
          else /* job == MV */
            {
              vlPutValid(svr, buffer);
              if (expected_fmsc > 0) expected_fmsc += 1;
            }
        }

      /* check for video underflow/overflow */
      
      VC( fmsc = vlGetFrontierMSC(svr, path, mem) );
      if (expected_fmsc != -1 &&
          fmsc != expected_fmsc)
        {
          error("we dropped/padded %lld fields",
                fmsc-expected_fmsc);
          
          if (sony_tpair.timecode_sequence >= 0)
            {
              char string[40];
              int sequence = fmsc +
                sony_tpair.timecode_sequence - sony_tpair.msc;
              timecode_sequence_to_string(string, sequence, tc_type);
              error("drop/pad happened around timecode [%s]", string);
            }
        }
      expected_fmsc = fmsc;

      /* block until next interesting event */

      FD_ZERO(&readset);
      FD_ZERO(&writeset);
      FD_SET(video_event_fd, &readset);
      if (job == VM)
        FD_SET(video_buffer_fd, &readset);
      else
        FD_SET(video_buffer_fd, &writeset);
      OC(select(getdtablehi(), &readset, &writeset, 0, NULL));
      
      /* process video events */
      
      while (vlPending(svr))
        {
          VLEvent ev;
          VC(vlNextEvent(svr, &ev));
          
          switch (ev.reason)
            {
            case VLTransferComplete:
              /* ignore these events.  we get our data another way. */
              break;
              
            default:
              printf("video got %s event (%d)\n",
                     event_name(ev.reason), ev.reason);
              if (ev.reason == VLTransferFailed)
                error_exit("video transfer failed.\n");
              break;
            }
        }
    }
  return 0;
}
