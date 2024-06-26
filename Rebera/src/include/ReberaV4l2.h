#ifndef REBERA_V4L2_H
#define REBERA_V4L2_H


#include <SDL2/SDL.h>
#include <string>
#include <unistd.h> // for usleep
#define CLEAR(x) memset(&(x), 0, sizeof(x))

namespace Rebera {

	struct buffer {
	    void*  start;
		size_t length;
	};

	class v4l2capture {
	private:
		int m_nFd;
		int m_nWidth;
		int m_nHeight;
		unsigned  m_bBufferNums;
		const char*  m_strDevName;
		struct buffer*  m_pBuffers;

		int m_iPixels;
		uint8_t* m_pY;
		uint8_t* m_pCb;
		uint8_t* m_pCr;

		SDL_Window* m_pWindow;
		SDL_Renderer* m_pRender;
		SDL_RendererInfo m_iInfo;
		SDL_Surface* m_pImage;
		SDL_Texture* m_pTexture;
		SDL_Event m_iEvent;
	public:
		v4l2capture(const char* _strDevName);
		~v4l2capture(void);
		int GetFd(void) { return m_nFd; };
		int GetWidth(void) { return m_nWidth; };
		int GetHeight(void) { return m_nHeight; };
		void StartCapturing(void);
		void StopCapture(void);
		void SetPicBuff(uint8_t*, uint8_t*, uint8_t*);
		void DisplayRaw(void);
		int Xioctl(int, int, void*);
		int ReadFrame(void);
		void ErrorExit(const char*);
		void ProcessImage(const void*, unsigned int);
		void OpenVideoDevice(void);
		void InitVideoDevice(void);
		void UnInitDevice(void);
		void DeviceClose(void);
		void MmapInit(void);
		void InitSDL2(void);
	};
}


#endif // REBERA_V4L2_H
