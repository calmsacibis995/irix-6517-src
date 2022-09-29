/*
 * dptex.c
 * Usage: dptex file
 * This program accepts one argument which is a file name. "file" is
 * the source, containing image data destined for texture memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <dpipe.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/keysym.h>
#include <sys/re4_tex.h>
#include <math.h>

#define XSIZE 1280
#define YSIZE 1024

static void openwindow(int, int);
static void initialize_graphics();
static void reshape(int, int);
static void mtest(GLfloat, GLfloat, GLfloat, GLfloat, GLfloat);

/* X window structures */
Display *dpy;
Window	window;
XEvent	xevent;
char hello[] = "\0";

short size, sx1, sy1, sx2, sy2;
GLfloat near=0.25;
GLfloat far=100000.0;
GLfloat toplod = 0;
GLfloat bottomlod = 0;
GLdouble topz;
GLdouble botz;

main(int argc, char **argv) {
    char *source;            /* file name */
    char *target;            /* file name */
    int src, tgt;            /* source and destination file descriptors */
    int pfd;                 /* pipe file descriptor */
    GLint exit_app=0;
    struct stat buf;
    int		    trans_z, trans_xy, rot, do_redraw;
    GLfloat	    tx, ty, tz, rx, rz;
    static int	    prev_x, prev_y;
    char buffer[5];
    int		    bufsize = 5;
    KeySym	    key;
    XComposeStatus  compose;
    int		    root_x, root_y, cur_x, cur_y;
    unsigned int    pointer_mask;
    dpipe_lib_hdl_t src_h, tgt_h;  /* handlers for src and dest */
    struct dpipelist iov;
    dpipe_file_ctx_t fiov;
    __int64_t xfrid;


    if (argc != 2) {
	printf("Usage: dptex source\n");
	exit(1);
	}

    source = argv[1];

    if ((src = open(source, O_RDONLY)) < 0) {
	perror("source can't open");
	exit(1);
	}

    initialize_graphics();
    glGetTexParameteriv(GL_TEXTURE_2D, __GL_MGRAS_PIPEND_FD, &tgt);
    if(tgt <= 0) {
	fprintf(stderr, "target bad: %ld\n", (int)tgt);
	exit(1);
	}

    /* create pipe */
    if ((pfd = dpipeCreate(src, tgt)) < 0) {
	perror ("pipe create failure");
	exit(1);
	}

    if (fstat(src, &buf) < 0) {
	perror ("fstat of source fail");
	exit(1);
	}

    src_h = dpipe_file_get_hdl(src);
    if (src_h == NULL) {
	perror("source handler can't be allocated");
	exit(1);
	}
	
    iov.offset = 0;
    iov.size = buf.st_size;
    fiov.iovcnt = 1;
    fiov.iov = &iov;
    if (dpipe_file_set_ctx(src_h, fiov) < 0) {
		perror ("dpipe_file_set_ctx(source) fail");
		exit(1);
	}

    glGetTexParameteriv(GL_TEXTURE_2D, __GL_MGRAS_PIPEND_FD_H, (int*)&tgt_h);
    if (tgt_h == NULL) {
	perror("dest handler can't be allocated");
	exit(1);
	}

    glTexSubImage2DEXT(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    if (xfrid = dpipeTransfer(pfd, src_h, tgt_h) < 0) {
	perror ("dpipe transfer fail");
	exit(1);
	}

    dpipeFlush(pfd);
    dpipeDestroy(pfd);
    close(src);
    close(tgt);

#ifdef DOUBLECHECK
    {GLvoid *image;
    FILE *fp;
    image = malloc(256*256*4);
    fp = fopen("dpdata.bin", "rb");
    fread(image, 4, 256*256, fp);
    fclose(fp);
    glTexSubImage2DEXT(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, image);}
#endif

    topz = pow(2.0, toplod);
    botz = pow(2.0, bottomlod);
    if(topz > far) far = topz;
    if(botz > far) far = botz;
    if(topz < near) near = topz;
    if(botz < near) near = botz;
    far *= 1.25; /* To avoid clipping */
    near *= 0.75; /* To avoid clipping */
    trans_z = trans_xy = rot = 0;
    tx = ty = rz = rx = 0.0;
    tz = -1200.0;
    prev_x = prev_y = 0;

    glClearColor(0.3, 0.3, 0.3, 1.0);
    
    mtest(tx, ty, tz, rx, rz);

    do {
    	XNextEvent(dpy, &xevent);
	    
	switch (xevent.type) {
	    case ConfigureNotify:
		/* reshape(XSIZE, YSIZE); */
		break;
		    
	    case ButtonPress:
		switch(xevent.xbutton.button) {
		    case Button1: trans_z = 1; break;
		    case Button2: rot = 1; break;
		    case Button3: trans_xy = 1; break;
		    default: break;
		    }
		break;
		    
	    case ButtonRelease: break;
		    
	    case KeyPress:
		XLookupString(&xevent.xkey, buffer, bufsize, &key, &compose);
		if(key == XK_Escape) {exit_app = 1; break;}
		break;
		    
	    default: break;
	    }

	if(trans_z || rot || trans_xy) {
	    cur_x = prev_x = xevent.xbutton.x;
	    cur_y = prev_y = YSIZE - xevent.xbutton.y + 1;
	    do {
		while(XPending(dpy)) {
	    	    XNextEvent(dpy, &xevent);
		    if(xevent.type == MotionNotify) {
		    	cur_x = xevent.xmotion.x;
		    	cur_y = YSIZE - xevent.xmotion.y + 1;
			}
		    else if(xevent.type == ButtonRelease)
		      switch(xevent.xbutton.button) {
		    	case Button1: trans_z = 0; break;
		    	case Button2: rot = 0; break;
		    	case Button3: trans_xy = 0; break;
		    	default: break;
		    	}
		    }
		if(cur_x != prev_x || cur_y != prev_y) {
		    if(trans_z) tz += -(far - near) * (cur_y - prev_y)/YSIZE/50.0;
	    	    if(rot) {
		    	rz += ((2.0 * (cur_x - prev_x))/XSIZE - 1.0) * 360.0;
		    	rx += ((2.0 * (cur_y - prev_y))/YSIZE - 1.0) * 360.0;
		    	}
	    	    if(trans_xy) {
		    	tx += (cur_x - prev_x)/8.0;
		    	ty += (cur_y - prev_y)/8.0;
	    	    	}
	    	    prev_x = cur_x;
	    	    prev_y = cur_y;
    	    	    mtest(tx, ty, tz, rx, rz);
		    }
		}
	    while(xevent.type != ButtonRelease);
	    }
    	}
    while(!exit_app);
}

static void
mtest(
    GLfloat trx,
    GLfloat try,
    GLfloat trz,
    GLfloat rx,
    GLfloat rz
    ) {
    GLdouble txframe = 4.0 * topz;
    GLdouble tyframe = 4.0 * topz;
    GLdouble bxframe = 4.0 * botz;
    GLdouble byframe = 4.0 * botz;
    GLdouble txedge = txframe/256;
    GLdouble tyedge = tyframe/256;
    GLdouble bxedge = bxframe/256;
    GLdouble byedge = byframe/256;
    GLfloat midz = (topz+botz)*0.5;
    GLfloat zslp, ty, tz, by, bz, y, z;

    size = 256;
    sx2 = 128;
    sy2 = 128;
    zslp = (size>>1) ? (topz-midz)/(size>>1) : (topz-midz);
    ty = sy2+tyframe;
    tz = ty * zslp + midz;
    by = -sy2-byframe;
    bz = by * zslp + midz;

    glPushMatrix();
    glTranslatef(trx, try, trz);
    glRotatef(rz, 0.0, 0.0, 1.0);
    glRotatef(rx, 1.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(1.0, 1.0, 1.0, 1.0);

    glBegin(GL_QUADS);
    	glTexCoord2d(1+txedge, 1+tyedge); glVertex3d(sx2+txframe, ty, -tz);
    	glTexCoord2d(-txedge, 1+tyedge); glVertex3d(-sx2-txframe, ty, -tz);
    	glTexCoord2d(-bxedge, -byedge); glVertex3d(-sx2-bxframe, by, -bz);
    	glTexCoord2d(1+bxedge, -byedge); glVertex3d(sx2+bxframe, by, -bz);
    glEnd();

    glPopMatrix();
    glXSwapBuffers(dpy, window);
}

static GLvoid
initialize_graphics(
    ) {
    GLfloat bcol[4];    /* border color RGBA  (lum in B spot) */

    openwindow(XSIZE, YSIZE);
    reshape(XSIZE, YSIZE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    bcol[0] = bcol[1] = bcol[2] = bcol[3] = 0.0;
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bcol);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8_EXT, 256, 256, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glEnable(GL_TEXTURE_2D);
}

static void
reshape(int w, int h)
{
    GLfloat fxsize = 0.4*near;
    GLfloat fysize = fxsize * YSIZE/XSIZE;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-fxsize, fxsize, -fysize, fysize, near, far);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, XSIZE, YSIZE);
}

static int configAttr[] = {
    GLX_RENDER_TYPE_SGIX, GLX_RGBA_BIT_SGIX,
    GLX_DRAWABLE_TYPE_SGIX, GLX_WINDOW_BIT_SGIX,
    GLX_DOUBLEBUFFER, True,
    GLX_RED_SIZE, 5,
    GLX_GREEN_SIZE, 5,
    GLX_BLUE_SIZE, 5,
    GLX_ALPHA_SIZE, 1,
    None
};


static void
openwindow(int x, int y) {
    int n, scrn;
    XVisualInfo	*vis;
    GLXContext ctx;
    XSetWindowAttributes wa;
    XSizeHints sh;
    GLXFBConfigSGIX *configs, config;
    Window root;
    XEvent xevent;
    
    if((dpy = XOpenDisplay(NULL)) == NULL) {
	fprintf(stderr, "can't open display\n");
	exit(EXIT_FAILURE);
    	}

    scrn = DefaultScreen(dpy);
    root = RootWindow(dpy, scrn);

    if((configs = glXChooseFBConfigSGIX(dpy, scrn, configAttr, &n)) == NULL) {
	fprintf(stderr, "couldn't choose FBConfig\n");
	exit(EXIT_FAILURE);
    	}
    config = configs[0];

    if((vis = glXGetVisualFromFBConfigSGIX(dpy, config)) == NULL) {
	fprintf(stderr, "couldn't get visual from FBConfig\n");
	exit(EXIT_FAILURE);
    	}

    ctx = glXCreateContextWithConfigSGIX(dpy, config, GLX_RGBA_TYPE_SGIX, None, True);
    if(ctx == NULL) {
	fprintf(stderr, "couldn't create context\n");
	exit(EXIT_FAILURE);
    	}

    wa.background_pixel = None;
    wa.border_pixel = None;
    wa.colormap = XCreateColormap(dpy, root, vis->visual, AllocNone);
    wa.event_mask = StructureNotifyMask | KeyPressMask | ButtonPressMask |
		ButtonReleaseMask | Button1MotionMask | Button2MotionMask |
		Button3MotionMask | ExposureMask;

    window = XCreateWindow(dpy, root, 50, 50, x, y,
                      0, vis->depth, InputOutput, vis->visual,
                      CWBorderPixel | CWEventMask | CWColormap, &wa);

    sh.flags = USPosition | USSize;
    XmbSetWMProperties(dpy, window, &hello[0],0,0,0,&sh, 0, 0);
    XMapWindow(dpy, window);
    do XNextEvent(dpy, &xevent);
    while(!(xevent.type == MapNotify && xevent.xmapping.window == window));
    
    if(!glXMakeCurrent(dpy, window, ctx)) {
	fprintf(stderr, "glXMakeCurrent failed\n");
	exit(EXIT_FAILURE);
    	}
}
