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
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#define FLOAT_STRING_BUF 20
/*
 * use global variables because it is hard to pass
 * arguments to another function in labview.
 * these variables should be initiated in ff_init(),
 * and freed in ff_close()
 */
static AVCodecContext *codec_contextp;
//static FILE* f;
static AVFrame *framep_out;
static AVFrame *framep_in;
static uint64_t frame_no;
static AVFormatContext *fmt_contextp;
static AVStream *streamp;
static struct SwsContext *sws_contextp;
/* some muxer(e.g. mp4) use different timebase. need to scale pkt->pts before write */
static int time_scale;
static int pix_fmt_out;

//static void print_options(const void *obj);
/*
 * initialize encoding envrionment
 * codec_id: enum AVCodecID
 * bit_rate: suggested bitrate, ignored when using crf.
 * pix_fmt_in: enum AVPixelFormat
 * q: {qmin, qmax} quantizer settings, 1 <= q <= 69, lower will get better quality. ignored
 *    when set to -1, must set to {-1,-1} to use crf.
 * crf: constant quality mode, 0 < crf <= 51. lower will get better quality. negative value
 * is ignored.
 */
int ff_init(const char *filename, 
		    int codec_id, 
			int width, 
			int height, 
			int bit_rate,
			int fps,
			int pix_fmt_in,
			int q[2],
			float crf)
{
	int ret;
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
	/* keep fmt only when raw video to be encoded */
	if(codec_id == AV_CODEC_ID_RAWVIDEO)
		pix_fmt_out = pix_fmt_in;
	else
		pix_fmt_out = AV_PIX_FMT_YUV420P;

	codec_contextp -> bit_rate  = bit_rate;
	codec_contextp -> width     = width;
	codec_contextp -> height    = height;
	codec_contextp -> time_base = (AVRational){1, fps};
	codec_contextp -> pix_fmt   = pix_fmt_out;
	codec_contextp -> flags    |= CODEC_FLAG_GLOBAL_HEADER;
	codec_contextp -> qmin      = q[0];
	codec_contextp -> qmax      = q[1];
	//print_options(codec_contextp->priv_data);
	if(codec_id == AV_CODEC_ID_H264){
		codec_contextp->profile = FF_PROFILE_H264_HIGH;
		if(crf > 0){
			char crfs[FLOAT_STRING_BUF];
			snprintf(crfs, FLOAT_STRING_BUF, "%f", crf);
			av_opt_set(codec_contextp->priv_data,"crf",crfs,0);
		}
	}
	if(avcodec_open2(codec_contextp, codecp, NULL) < 0){
		fprintf(stderr, "Could not open codec\n");
		return -1;
	}
	/*initialize input frame structure, only for conversion */
	framep_in = av_frame_alloc();
	if(framep_in == NULL){
		fprintf(stderr, "Could not alloc video frame\n");
		return -1;
	}
	framep_in -> format = pix_fmt_in;
	framep_in-> width  = width;
	framep_in-> height = height;
	ret = av_image_fill_linesizes(framep_in->linesize, pix_fmt_in, width);
	if(ret < 0){
		fprintf(stderr, "Could not calculate linesize\n");
		return -1;
	}

	/* initialize output frame */
	framep_out = av_frame_alloc();
	if(framep_out == NULL){
		fprintf(stderr, "Could not alloc video frame\n");
		return -1;
	}
	framep_out -> format = pix_fmt_out;
	framep_out-> width  = width;
	framep_out-> height = height;
	frame_no = 0;
	ret = av_image_alloc(framep_out->data, framep_out->linesize, width, height, pix_fmt_out, 1);
	if(ret < 0){
		fprintf(stderr, "Could not alloc image\n");
		return -1;
	}
	/* initialize colorspace conversion context */
	sws_contextp = sws_getContext(framep_out->width, framep_out->height, pix_fmt_in,
			                      framep_out->width, framep_out->height, pix_fmt_out,
								  0, NULL, NULL, NULL);
	if(sws_contextp == NULL){
		fprintf(stderr, "Could not start conversion context");
		return -1;
	}

	/* initialize muxing context, fmt is guessed by filename */
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
	if(fmt_contextp->oformat->flags & AVFMT_GLOBALHEADER)
		streamp->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	streamp->time_base = codec_contextp->time_base;

	/* write header */
	if(avformat_write_header(fmt_contextp, NULL) < 0){
		fprintf(stderr, "Could not write file header\n");
		return -1;
	}
	time_scale = streamp->time_base.den / codec_contextp->time_base.den;
	return 0;
}

int ff_wframe(void *frame_raw)
{
	AVPacket pkt;
	int ret, got_output;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	ret = av_image_fill_pointers(framep_in->data, framep_in->format,
			                     framep_in->height, frame_raw, framep_in->linesize);
	if(ret < 0){
		fprintf(stderr, "Could not fill pointers\n");
		return -1;
	}
	ret = sws_scale(sws_contextp,(const uint8_t **)framep_in->data, framep_in->linesize, 0,
			        framep_in->height, framep_out->data, framep_out->linesize);
	if(ret < 0){
		fprintf(stderr, "Could not convert image\n");
		return -1;
	}
	framep_out -> quality = 1;
	framep_out -> pts  = frame_no * time_scale;
	frame_no++;
	ret = avcodec_encode_video2(codec_contextp, &pkt, framep_out, &got_output);
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

	if(avcodec_close(codec_contextp) < 0){
		fprintf(stderr, "Could not close context\n");
		return -1;
	}
	av_free(codec_contextp);
	//av_freep(frame->data);
	av_frame_free(&framep_out);

	if(avio_close(fmt_contextp->pb) < 0){
		fprintf(stderr,"Could not close file");
		return -1;
	}
	avformat_free_context(fmt_contextp);
	return 0;
}

/* print all private options for a given object */
/*
void print_options(const void *obj)
{
	char buf[1024];
	AVOption* avopt_p = av_opt_next(obj,NULL);
	 while(avopt_p != NULL){
		printf("%s\ntype:%d\n%s\n", avopt_p->name, avopt_p->type, avopt_p->help);
		if(avopt_p->type == AV_OPT_TYPE_INT)
			printf("value:%ld\n\n", av_get_int(obj, avopt_p->name, &avopt_p));
		else if(avopt_p->type == AV_OPT_TYPE_FLOAT ||
				avopt_p->type == AV_OPT_TYPE_DOUBLE)
			printf("value:%f\n\n", av_get_double(obj, avopt_p->name, &avopt_p));
		else if(avopt_p->type == AV_OPT_TYPE_STRING){
			printf("value:%s\n\n", av_get_string(obj, avopt_p->name, &avopt_p,buf,1024);
		}
		avopt_p = av_opt_next(obj, avopt_p);
	}
}
*/
