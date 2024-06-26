#include <iostream>	
#include <libavformat/avformat.h>
#include <signal.h>
#include <atomic>
#include <SDL2/SDL.h>
#include "ReberaV4l2.h"
#include "ReberaEncode.h"
#include "dfs.h"
#include "socket.h"
#include "payload.h"
#include "util.h"
#include "select.h"
#include "ReberaSendState.h"


std::atomic_bool _bStopFlag{ false };
static void sigint_handler(int _nIntCode) {
	_bStopFlag.store(true);
}

int main(char argc, char** argv) {
	Rebera::v4l2capture video("/dev/video0");
	ReberaEncoder encoder(video.GetWidth(), video.GetHeight());
	encoder.attach_camera(&video);
	encoder.reduce_fr_by(1);

	SenderState sender(argv[1], argv[2], NULL);
	sender.set_ipr(encoder.get_ipr());


	video.SetPicBuff(encoder.get_Ybuff(), encoder.get_Cbbuff(), encoder.get_Crbuff());
	video.StartCapturing();

	/* we catch events (arrival of acks, feedback packets, or new frames) using select */
	Select &sel = Select::get_instance();
	sel.add_fd( sender.socket.get_sock() );
	sel.add_fd( sender.acksocket.get_sock() );
	sel.add_fd( video.GetFd());

	signal( SIGINT, sigint_handler );
	while (!_bStopFlag.load()){
		SDL_Event evt;
		while( SDL_PollEvent( &evt )){
			switch( evt.type ) {
				case SDL_QUIT :
					_bStopFlag.store(true);
					goto EXIT;
				default :
					break;
			}
		}
		if (!_bStopFlag.load()) {
			sel.select(-1);
		}
		if (sel.read(sender.socket.get_sock())) {
			//检查当前端口是否有收到数据
			Socket::Packet incoming( sender.socket.recv() );
			Packet::FbHeader* h = (Packet::FbHeader*)incoming.payload.data();
			if ( h->measuredtime > 100 || ( h->measuredbyte == 0 && h->measuredtime == 0 ) ){
				sender.set_fb_time( GetTimeNow() );
				sender.set_B( static_cast<double>(h->measuredbyte) ); // in bytes
				sender.set_T( static_cast<double>(h->measuredtime) ); // in microseconds
			}

			if ((uint32_t)(h->bytes_rcvd) > sender.get_bytes_recv()) {
				sender.set_bytes_recv( (uint32_t)(h->bytes_rcvd) );
			}
			sender.set_bytes_lost( (uint32_t)(h->bytes_lost) );
		}else if ( sel.read( sender.acksocket.get_sock() ) ) /* incoming ack awaiting */{
			//ACK端口是否有数据，注意，这里代码似乎不完整，原作者未完成
			Socket::Packet incoming( sender.acksocket.recv() );
		}
		else /* new raw picture awaiting */{
			//send pic
			/* get the next eligible raw picture and copy to encoder's pic_in */
			if ( encoder.get_frame() ){
				if ( !encoder.get_frame_index() ) // IDR frame: time to make a prediction
				{
					sender.predict_next();
					encoder.change_bitrate( 0.008*sender.get_budget() / encoder.get_ipr() );
				}

				/* encode the picture */
				int i_frame_size = encoder.compress_frame();
				
				int i_total_size = sender.total_length( i_frame_size );

				if ( !encoder.get_frame_index() )
					sender.init_rate_adapter( sender.get_budget() < i_total_size && sender.get_inqueue() == 0 ? i_total_size : sender.get_budget() );
					
				if ( sender.rate_adapter.permits( i_total_size, encoder.get_frame_index() ) )
				{	
					std::string frame( (const char*)encoder.get_nal(), (size_t)i_frame_size );
					
					sender.send_frame( frame.data(), i_frame_size, encoder.i_enc_frame );
				}
				encoder.i_enc_frame++;
			}
		}

	}
EXIT:
	return 0;
}