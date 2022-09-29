/*
 * djpg2mmsc.c
 *
 * This file decompresses a JPEG image file and converts it to a
 * a Metawindows format image to be shown on the MMSC display.
 * NOTE: this prototype only works for width%8 == 0!!!
 */

#include <stdio.h>

/*
 * Include file for users of JPEG library.
 * You will need to have included system headers that define at least
 * the typedefs FILE and size_t before you can include jpeglib.h.
 * (stdio.h is sufficient on ANSI-conforming systems.)
 * You may also wish to include "jerror.h".
 */

#include "jpeglib.h"

#define CMAP_NCMP 3
#define CMAP_NCLR 240
#define NPLANES   1  /* Metawindow 16 color stripes pixel bits across planes */
#define PIXBITS   8

static unsigned char *mmsc_buf;
static int mmscbufsize;

  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
static struct jpeg_decompress_struct cinfo;

#ifdef NCLR16

split_planes(JSAMPROW buffer, int row) {

  int i, j, offset;
  unsigned char mask = 0x01;

  for (i=0; i<NPLANES; i++) {
    offset = i*cinfo.output_height*cinfo.output_width/8 + 
      row*cinfo.output_width/8;
    for (j=0; j<cinfo.output_width; j++) {
      if (j%8 == 0)
	mmsc_buf[offset] = 0x00;            /* initialize byte */
      mmsc_buf[offset] |= ((buffer[j] & mask) >> i);
      if (j%8 < 7)
	mmsc_buf[offset] <<= 1;             /* shift to prepare for next bit */
      else
	offset++;                           /* byte completed: bump offset */ 
    }
    mask <<= 1;
  }
}

#endif

output_image_struct() {

  int i;

  printf("image backimg[] = {");
  printf("0x%x,0x%x,", cinfo.output_width & 0x00ff, 
	 cinfo.output_width >> 8);
  printf("0x%x,0x%x,", cinfo.output_height & 0x00ff, 
	 cinfo.output_height >> 8);

#ifdef NCLR16
  printf("0x0,0x0,0x%x,0x0,0x1,0x0,0x4,0x0,", 
	 cinfo.output_width/8);            /* bytes per row limited to 255 */
#else
  printf("0x0,0x0,0x0,0x4,0x8,0x0,0x1,0x0,");
#endif

  for (i=0; i<mmscbufsize; i++) {
    if (i%16 == 0)
      printf("\n");
    printf("0x%x",mmsc_buf[i]);
    if (i < mmscbufsize - 1)
      printf(",");
  }
  printf("};\n");
}

output_image_palette() {

  int i, j;

  printf("palData imgPal[%d] = {\n", cinfo.actual_number_of_colors);
  for (i=0; i<cinfo.actual_number_of_colors; i++) {
    printf("{");
    for (j=0; j<CMAP_NCMP; j++) {
      printf("0x%x00 ", cinfo.colormap[j][i]);
      if (j < CMAP_NCMP-1)
	printf(", ");
      else
	printf("}");
    }
    if (i < cinfo.actual_number_of_colors - 1)
      printf(",");
    printf("\n");
  }
  printf("};\n");
}


main ()
{
  int row, i, j;
  struct jpeg_error_mgr jerr;

  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr);

  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source to be stdin (pipe in a file) */

  jpeg_stdio_src(&cinfo, stdin);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(&cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.doc for more info.
   */

  /* Step 4: set parameters for decompression */

  cinfo.quantize_colors = TRUE;         /* need 16 color map for Metawindow */
  cinfo.out_color_components = CMAP_NCMP;
  cinfo.desired_number_of_colors = CMAP_NCLR;

  /* Allocate space for a color map of maximum supported size. */
/*  cinfo.colormap = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr) &cinfo, JPOOL_IMAGE,
     (JDIMENSION) (MAXJSAMPLE+1), (JDIMENSION) 3);

  for (i=0; i<CMAP_NCMP; i++)
    for (j=0; j<CMAP_NCLR; j++)
      cinfo.colormap[i][j] = (JSAMPLE) mymap[i][j]; */

  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */

  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

/* alloc a buffer for the (plane-transposed) mmsc image */

#ifdef NCLR16
  mmscbufsize = cinfo.output_width * cinfo.output_height * NPLANES * PIXBITS/8;
#else
#define PADWIDTH 1024
  mmscbufsize = PADWIDTH * cinfo.output_height * NPLANES * PIXBITS/8;
#endif

  mmsc_buf = (unsigned char *) malloc(mmscbufsize);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  row = 0;
  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);

#ifdef NCLR16
    for (i=0; i<row_stride; i++)
      buffer[0][i] += 8;            /* leave first 8 palette colors intact */
    split_planes(buffer[0], row);
#else

    for (i=0; i< PADWIDTH; i++) {
      if (i<row_stride) {
	buffer[0][i] += 16;           /* leave first 16 palette colors intact */
	mmsc_buf[row*PADWIDTH + i] = buffer[0][i];
      }
      else
	mmsc_buf[row*PADWIDTH + i] = 0;
    }
#endif

/*    printf("\nrow %d:",row);
    for (i=0; i<row_stride; i++)
      printf(" %d", buffer[0][i]);  */

    row++;
  }

  printf("#include \"metawndo.h\"\n\n");
  output_image_palette();
  printf("\n");
  output_image_struct();

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&cinfo);

  free(mmsc_buf);

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  /* And we're done! */
  return 0;               /* keep make happy */
}
