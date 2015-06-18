/*
 * wrapper of ffmpeg to be used in labview.
 * this module is not thread-safe
 * author: wsy
 */
#include <math.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

/*
 * use global variables because it is hard to pass
 * arguments to another function in labview.
 * these variables should be initiated in ff_init(),
 * and freed in ff_close()
 */
static AVCodec *codec;
static AVCodecContext *context;
static FILE* f;
static AVFrame *frame;
static uint64_t frame_no;

int ff_init(const char *filename, 
		    int codec_id, 
			int width, 
			int height, 
			int bit_rate,
			int fps,
			int pix_fmt)
{
	avcodec_register_all();
	codec = avcodec_find_encoder(codec_id);
	if(codec == NULL){
		fprintf(stderr, "Codec not found\n");
		return -1;
	}
	context = avcodec_alloc_context3(codec);
	if(context == NULL){
		fprintf(stderr, "Context allocation failed\n");
		return -1;
	}
	context -> bit_rate  = bit_rate;
	context -> width     = width;
	context -> height    = height;
	context -> time_base = (AVRational){1, fps};
	context -> pix_fmt   = pix_fmt;
	if(avcodec_open2(context, codec, NULL) < 0){
		fprintf(stderr, "Could not open codec\n");
		return -1;
	}
	f = fopen(filename, "wb");
	if(f == NULL){
		fprintf(stderr, "Could not open file %s\n", filename);
		return -1;
	}
	frame = av_frame_alloc();
	if(frame == NULL){
		fprintf(stderr, "Could not alloc video frame\n");
		return -1;
	}
	frame -> format = pix_fmt;
	frame -> width  = width;
	frame -> height = height;
	frame_no = 0;
	/*image_alloc not needed, use source data */
	if(av_image_fill_linesizes(frame->linesize, pix_fmt, width) < 0){
		fprintf(stderr,"Could not get line sizes\n");
		return -1;
	}
	/*
	int ret;
	ret = av_image_alloc(frame -> data,
			             frame -> linesize,
						 context -> width,
						 context -> height,
						 context -> pix_fmt,
						 1);
	if(ret < 0){
		fprintf(stderr, "Could not alloc raw image buffer\n");
		return -6;
	}
	*/
	return 0;
}

int ff_wframe(void *frame_raw)
{
	AVPacket pkt;
	int ret, got_output;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	//memcpy(frame -> data[0], frame_raw, (context->width)*(context->height));
	//frame->data[0] = frame_raw;
	ret = av_image_fill_pointers(frame->data, context->pix_fmt, context->height,
				              frame_raw, frame->linesize);
	if(ret < 0){
		fprintf(stderr, "Could not fill pointers\n");
		return -1;
	}
	frame -> pts  = frame_no;
	frame_no++;
	ret = avcodec_encode_video2(context, &pkt, frame, &got_output);
	if(ret < 0){
		fprintf(stderr, "Error encoding frame %lu\n", frame_no);
		return -1;
	}
	if(!got_output){
		fprintf(stderr,"No output generated\n");
		return -2;
	}
	ret = fwrite(pkt.data, 1, pkt.size, f);
	if(ret < pkt.size){
		fprintf(stderr,"pkt write faild\n");
		return -1;
	}
	av_free_packet(&pkt);
	return 0;
}

int ff_close()
{
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	int ret, got_output = 1;
	while(got_output){
		ret = avcodec_encode_video2(context, &pkt, NULL, &got_output);
		if(ret < 0){
			fprintf(stderr, "Error encoding frame\n");
			return -1;
		}
		if(got_output){
			ret = fwrite(pkt.data, 1, pkt.size, f);
			if(ret < pkt.size){
				fprintf(stderr,"pkt write faild\n");
				return -1;
			}
			av_free_packet(&pkt);
		}
	}
	if(fclose(f) != 0){
		fprintf(stderr, "Could not close file\n");
		return -1;
	}
	if(avcodec_close(context) < 0){
		fprintf(stderr, "Could not close context\n");
		return -1;
	}
	av_free(context);
	//av_freep(frame->data);
	av_frame_free(&frame);
	return 0;
}

