#include <math.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
int ff_init(const char *filename, 
		    int codec_id, 
			int width, 
			int height, 
			int bit_rate,
			int fps,
			int pix_fmt_in,
			int q[2],
			float crf);
int ff_wframe(void *frame_raw);
int ff_close();
