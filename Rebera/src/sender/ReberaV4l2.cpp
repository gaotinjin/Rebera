#include "ReberaV4l2.h"
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>



using Rebera::v4l2capture;

void v4l2capture::ErrorExit(const char *s) {
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

int v4l2capture::Xioctl( int fh, int request, void* arg ) {
/*
POSIX specification defines that when signal (such as Ctrl+C) is caught, the blocking function returns EINTR error.
*/
	int r;
	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

return r;
}

v4l2capture::v4l2capture(const char* _strDevName)
    : m_strDevName(_strDevName), m_nFd( -1 ), m_pBuffers( NULL ), m_bBufferNums( 0 ){
	OpenVideoDevice();
	InitVideoDevice();
	InitSDL2();
}

v4l2capture::~v4l2capture( void ) {
	//~ if ( m_pY ) delete m_pY;
	UnInitDevice();
	DeviceClose();
}

int v4l2capture::ReadFrame( void ) {
/*
 * To query the current image format applications set the type field of a struct v4l2_format to V4L2_BUF_TYPE_VIDEO_CAPTURE or V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE and call the VIDIOC_G_FMT ioctl with a pointer to this structure.
 * Applications call the VIDIOC_DQBUF ioctl to dequeue a filled (capturing) or displayed (output) buffer from the driver's outgoing queue. They just set the type, memory and reserved fields of a struct v4l2_buffer as above, when VIDIOC_DQBUF is called with a pointer to this structure the driver fills the remaining fields or returns an error code.
 * To enqueue a memory mapped buffer, applications set the memory field to V4L2_MEMORY_MMAP.
 * By default VIDIOC_DQBUF blocks when no buffer is in the outgoing queue. When the O_NONBLOCK flag was given to the open() function, VIDIOC_DQBUF returns immediately with an EAGAIN error code when no buffer is available.
*/
	struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if ( -1 == Xioctl(m_nFd, VIDIOC_DQBUF, &buf) ) {
        switch (errno) {
            case EAGAIN:
                return -1;
            case EIO:
            default:
                ErrorExit("VIDIOC_DQBUF");
        }
    }

    ProcessImage( m_pBuffers[buf.index].start, buf.bytesused );

    if (-1 == Xioctl(m_nFd, VIDIOC_QBUF, &buf)) {
        ErrorExit("VIDIOC_QBUF");
	}

return 0;
}

void v4l2capture::StopCapture( void ) {
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if ( -1 == Xioctl(m_nFd, VIDIOC_STREAMOFF, &type) )
        ErrorExit("VIDIOC_STREAMOFF");
}

void v4l2capture::StartCapturing( void ) {

    for ( unsigned i = 0; i < m_bBufferNums; ++i ){
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (-1 == Xioctl(m_nFd, VIDIOC_QBUF, &buf))
            ErrorExit("VIDIOC_QBUF");
    }
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == Xioctl(m_nFd, VIDIOC_STREAMON, &type)) {
        ErrorExit("VIDIOC_STREAMON");
    }
}

void v4l2capture::OpenVideoDevice( void ) {
    struct stat _nState; // #include <sys/stat.h>

	// stat - get file status: int stat(const char* path, struct stat* buf);
    if (-1 == stat(m_strDevName, &_nState)) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n", m_strDevName, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(_nState.st_mode)) { //Test for a character special file, non-zero if yes
        fprintf(stderr, "%s is no device\n", m_strDevName);
        exit(EXIT_FAILURE);
    }

    m_nFd = open(m_strDevName, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (m_nFd == -1) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n", m_strDevName, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void v4l2capture::MmapInit( void ) {
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = 32;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == Xioctl(m_nFd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support memory mapping\n", m_strDevName);
            exit(EXIT_FAILURE);
        }
        else {
            ErrorExit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n", m_strDevName);
        exit(EXIT_FAILURE);
    }

    m_pBuffers = (buffer*)calloc(req.count, sizeof(*m_pBuffers));

    if (!m_pBuffers) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (m_bBufferNums = 0; m_bBufferNums < req.count; ++m_bBufferNums) {

        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = m_bBufferNums;

        if (-1 == Xioctl(m_nFd, VIDIOC_QUERYBUF, &buf)) {
            ErrorExit("VIDIOC_QUERYBUF");
        }

        m_pBuffers[m_bBufferNums].length = buf.length;
        m_pBuffers[m_bBufferNums].start =
        mmap(NULL /* start anywhere */,
             buf.length,
             PROT_READ | PROT_WRITE /* required */,
             MAP_SHARED /* recommended */,
             m_nFd,
             buf.m.offset);

        if (MAP_FAILED == m_pBuffers[m_bBufferNums].start) {
            ErrorExit("mmap");
        }
    }
}

void v4l2capture::InitVideoDevice( void ) {
    struct v4l2_capability 	_stgCap;

    if ( -1 == Xioctl(m_nFd, VIDIOC_QUERYCAP, &_stgCap) ) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is no V4L2 device\n", m_strDevName);
            exit(EXIT_FAILURE);
        }else {
            ErrorExit("VIDIOC_QUERYCAP");
        }
    }

    fprintf(stderr, 
            "  Driver: \"%s\"\n"
            "  Device: \"%s\"\n"
            "  Bus: \"%s\"\n"
            "  Version: %u.%u.%u\n"
            "  Capabilities: 0x%08x\n",
            _stgCap.driver,
            _stgCap.card,
            _stgCap.bus_info,
            (_stgCap.version>>16) & 0xff, (_stgCap.version>>8) & 0xff, _stgCap.version & 0xFF,
            _stgCap.capabilities);

    if ( !(_stgCap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ) {
        fprintf(stderr, "%s is no video capture device\n", m_strDevName);
        exit(EXIT_FAILURE);
    }

	if ( !(_stgCap.capabilities & V4L2_CAP_STREAMING) ) {
		fprintf(stderr, "%s does not support streaming i/o\n", m_strDevName);
		exit(EXIT_FAILURE);
	}

	struct v4l2_fmtdesc fmtdesc;
    CLEAR(fmtdesc);
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    char fourcc[5] = {0};
    char c, e;
    fprintf(stderr,"> Supported formats:\n");
    while (0 == Xioctl(m_nFd, VIDIOC_ENUM_FMT, &fmtdesc)) {
        strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
        c = fmtdesc.flags & 1 ? 'C' : ' ';
        e = fmtdesc.flags & 2 ? 'E' : ' ';
        fprintf(stderr, "  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
        fmtdesc.index++;
    }

	struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
    if (-1 == Xioctl(m_nFd, VIDIOC_S_FMT, &fmt)) {
		ErrorExit("VIDIOC_S_FMT");
    }

	strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
    fprintf(stderr, "> Selected Camera Mode:\n"
            "  Width: %d\n"
            "  Height: %d\n"
            "  PixFmt: %s\n"
            "  Field: %d\n",
            fmt.fmt.pix.width,
            fmt.fmt.pix.height,
            fourcc,
            fmt.fmt.pix.field);

	m_nWidth  = fmt.fmt.pix.width;
	m_nHeight = fmt.fmt.pix.height;
	m_iPixels = m_nWidth*m_nHeight;
    /******************/
    //调试部分代码，平时注释
    //m_pY = new uint8_t[(m_iPixels*3)>>1];
	//m_pCb = m_pY + m_iPixels;
	//m_pCr = m_pCb + (m_iPixels>>2);
    /******************/

	struct v4l2_frmivalenum fr;
    CLEAR(fr);

    fr.pixel_format = fmt.fmt.pix.pixelformat;
    fr.width = fmt.fmt.pix.width;
    fr.height = fmt.fmt.pix.height;
    fr.index = 0;

    if (-1 == Xioctl(m_nFd, VIDIOC_ENUM_FRAMEINTERVALS, &fr)) {
        ErrorExit("VIDIOC_ENUM_FRAMEINTERVALS");
    }

    if (fr.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		struct v4l2_streamparm 	strmp;
		CLEAR(strmp);
		strmp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		strmp.parm.capture.timeperframe.numerator = fr.discrete.numerator;
		strmp.parm.capture.timeperframe.denominator = fr.discrete.denominator;
        if (-1 == Xioctl(m_nFd, VIDIOC_S_PARM, &strmp)) {
			ErrorExit("VIDIOC_S_PARM");
        }
		fprintf(stderr, "> Frame rate: %f fps\n", (float)(strmp.parm.capture.timeperframe.denominator)/strmp.parm.capture.timeperframe.numerator);
    }
    MmapInit();
}

void v4l2capture::DeviceClose(void) {
    if ( -1 == close(m_nFd) )
        ErrorExit("close");

    m_nFd = -1;
}

void v4l2capture::UnInitDevice( void ) {
    unsigned int _nIndex;

    for (_nIndex = 0; _nIndex < m_bBufferNums; ++_nIndex) {
        if (-1 == munmap(m_pBuffers[_nIndex].start, m_pBuffers[_nIndex].length)) {
            ErrorExit("munmap");
        }
    }
    free(m_pBuffers);
}

void v4l2capture::ProcessImage( const void *p, unsigned int size )
{
	unsigned int n = 0;
	uint8_t* pCurr = (uint8_t*)p;
	int halfwidth = m_nWidth>>1;

	for ( int y = 0; y < m_nHeight; y += 2, pCurr += m_nWidth*4) {
		uint8_t* pHO2 = pCurr;
		for ( int x = 0; x < halfwidth; pHO2 += 4, x++, n++) {
			uint8_t* pdummy = m_pY + n*4 - x*2;
			pdummy[0] = pHO2[0];
			pdummy[1] = pHO2[2];
			pdummy[m_nWidth] = pHO2[m_nWidth*2];
			pdummy[m_nWidth+1] = pHO2[m_nWidth*2+2];
			m_pCb[n] = (uint8_t) ((pHO2[1] + pHO2[2*m_nWidth+1] + 1)>>1);
			m_pCr[n] = (uint8_t) ((pHO2[3] + pHO2[2*m_nWidth+3] + 1)>>1);
		}
	}
	//DisplayRaw();
}

void v4l2capture::InitSDL2( void ) {
	if ( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
		fprintf( stderr, "Cannot initialize SDL: %s\n", SDL_GetError() );
		exit( EXIT_SUCCESS );
	}
	m_pWindow = SDL_CreateWindow( "Rebera Sender - NYU Video Lab", 300, 300, m_nWidth, m_nHeight, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE );
	m_pRender = SDL_CreateRenderer( m_pWindow, -1, SDL_RENDERER_ACCELERATED );
	SDL_GetRendererInfo( m_pRender, &m_iInfo );
	fprintf( stderr, "> Using %s rendering\n\n", m_iInfo.name );
	m_pTexture = SDL_CreateTexture(m_pRender, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, m_nWidth, m_nHeight);
}
void v4l2capture::SetPicBuff(uint8_t* _Ybuf_, uint8_t* _Cbbuf_, uint8_t* _Crbuf_ ) {
    m_pY = _Ybuf_;
    m_pCb = _Cbbuf_;
    m_pCr = _Crbuf_;
}
void v4l2capture::DisplayRaw(void) {
    SDL_UpdateTexture(m_pTexture, NULL, m_pY, m_nWidth);
	SDL_RenderClear(m_pRender);
	SDL_RenderCopy(m_pRender, m_pTexture, NULL, NULL);
	SDL_RenderPresent(m_pRender);
}


