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
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

/*
 * use global variables because it is hard to pass
 * arguments to another function in labview.
 * these variables should be initiated in ff_init(),
 * and freed in ff_close()
 */
static AVCodecContext *codec_contextp;
//static FILE* f;
static AVFrame *framep;
static uint64_t frame_no;
static AVFormatContext *fmt_contextp;
static AVStream *streamp;
static struct SwsContext *sws_contextp;

int ff_init(const char *filename, 
		    int codec_id, 
			int width, 
			int height, 
			int bit_rate,
			int fps,
			int pix_fmt)
{
	AVCodec *codecp;
	avcodec_register_all();
	av_register_all();
	codecp = avcodec_find_encoder(codec_id);
	if(codecp == NULL){
		fprintf(stderr, "Codec not found\n");
		return -1;
	}
	/* initialize context */
	codec_contextp = avcodec_alloc_context3(codecp);
	if(codec_contextp == NULL){
		fprintf(stderr, "Context allocation failed\n");
		return -1;
	}
	codec_contextp -> bit_rate  = bit_rate;
	codec_contextp -> width     = width;
	codec_contextp -> height    = height;
	codec_contextp -> time_base = (AVRational){1, fps};
	codec_contextp -> pix_fmt   = pix_fmt;
	if(avcodec_open2(codec_contextp, codecp, NULL) < 0){
		fprintf(stderr, "Could not open codec\n");
		return -1;
	}
	/*
	f = fopen(filename, "wb");
	if(f == NULL){
		fprintf(stderr, "Could not open file %s\n", filename);
		return -1;
	}
	*/
	/* initialize frame */
	framep = av_frame_alloc();
	if(framep == NULL){
		fprintf(stderr, "Could not alloc video frame\n");
		return -1;
	}
	framep -> format = pix_fmt;
	framep-> width  = width;
	framep-> height = height;
	frame_no = 0;
	/*image_alloc not needed, use source data */
	if(av_image_fill_linesizes(framep->linesize, pix_fmt, width) < 0){
		fprintf(stderr,"Could not get line sizes\n");
		return -1;
	}

	/* initialize muxing context, fmt is guessed by filename */
	int ret;
	ret = avformat_alloc_output_context2(&fmt_contextp, NULL, NULL, filename);
	if(ret < 0){
		fprintf(stderr, "Could not find output format\n");
		return -1;
	}
	if(avio_open(&(fmt_contextp->pb), filename, AVIO_FLAG_WRITE) < 0){
		fprintf(stderr, "Could not open output file %s\n", filename);
		return -1;
	}
	streamp = avformat_new_stream(fmt_contextp, codecp);
	if(streamp == NULL){
		fprintf(stderr, "Could not open stream\n");
		return -1;
	}
	if(avcodec_copy_context(streamp->codec, codec_contextp) < 0){
		fprintf(stderr, "Could not set stream codec\n");
		return -1;
	}
	streamp->time_base = codec_contextp->time_base;

	/* write header */
	if(avformat_write_header(fmt_contextp, NULL) < 0){
		fprintf(stderr, "Could not write file header\n");
		return -1;
	}
	return 0;
}

int ff_wframe(void *frame_raw)
{
	AVPacket pkt;
	int ret, got_output;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	ret = av_image_fill_pointers(framep->data, codec_contextp->pix_fmt,
		                         codec_contextp->height, frame_raw, framep->linesize);
	if(ret < 0){
		fprintf(stderr, "Could not fill pointers\n");
		return -1;
	}
	framep -> pts  = frame_no;
	frame_no++;
	ret = avcodec_encode_video2(codec_contextp, &pkt, framep, &got_output);
	if(ret < 0){
		fprintf(stderr, "Error encoding frame %lu\n", frame_no);
		return -1;
	}
	if(!got_output){
		fprintf(stderr,"No pkt generated\n");
		av_free_packet(&pkt);
		return -2;
	}
	pkt.stream_index = streamp -> index;
	ret = av_interleaved_write_frame(fmt_contextp, &pkt);
	if(ret < 0){
		fprintf(stderr,"No frame written\n");
		av_free_packet(&pkt);
		return -2;
	}
	/*
	ret = fwrite(pkt.data, 1, pkt.size, f);
	if(ret < pkt.size){
		fprintf(stderr,"pkt write faild\n");
		return -1;
	}
	*/
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
	/* flush encoder queues */
	while(got_output){
		ret = avcodec_encode_video2(codec_contextp, &pkt, NULL, &got_output);
		pkt.stream_index = streamp->index;
		if(ret < 0){
			fprintf(stderr, "Error encoding frame\n");
			return -1;
		}
		if(got_output){
			ret = av_interleaved_write_frame(fmt_contextp, &pkt);
			if(ret < 0){
				fprintf(stderr,"pkt write faild\n");
				return -1;
			}
			/*
			ret = fwrite(pkt.data, 1, pkt.size, f);
			if(ret < pkt.size){
				fprintf(stderr,"pkt write faild\n");
				return -1;
			}
			*/
			av_free_packet(&pkt);
		}
	}
	/* flush muxer queues, continue on error */
	if(av_interleaved_write_frame(fmt_contextp, NULL) < 0){
		fprintf(stderr, "flush queue error\n");
	}
	if(av_write_trailer(fmt_contextp) < 0){
		fprintf(stderr, "Could not write trailer\n");
		return -1;
	}

	/*
	if(fclose(f) != 0){
		fprintf(stderr, "Could not close file\n");
		return -1;
	}
	*/
	if(avcodec_close(codec_contextp) < 0){
		fprintf(stderr, "Could not close context\n");
		return -1;
	}
	av_free(codec_contextp);
	//av_freep(frame->data);
	av_frame_free(&framep);

	if(avio_close(fmt_contextp->pb) < 0){
		fprintf(stderr,"Could not close file");
		return -1;
	}
	avformat_free_context(fmt_contextp);
	return 0;
}

