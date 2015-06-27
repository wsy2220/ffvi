#include <stdio.h>
#include <stdlib.h>
#include "ffvi.h"
#define W 0x400
#define H 0x400
#define N 0x100
#define SIZE W*H*3
int main(int argc, char **argv)
{
	int ret;
	int i, j ;
	unsigned char img[SIZE];
	int q[2]= {-1,-1};
	//FILE *f;
	//printf("codec_id: %d\npix_fmt: %d\n",AV_CODEC_ID_RAWVIDEO,AV_PIX_FMT_GRAY8);
	ret = ff_init("test.mp4", AV_CODEC_ID_H264,
					  W, H, 8000, 30, AV_PIX_FMT_GRAY8, q, (float)1.0, "medium");
	if(ret < 0){
		printf("ff_init failed, code: %d\n", ret);
		exit(EXIT_FAILURE);
	}
	for(i = 0; i < N; i++){
		for(j = 0; j < SIZE-2; j+=3)
			img[j] = (unsigned char)(j+i);
		for(j = 0; j < SIZE-1; j+=3)
			img[j+1] = (unsigned char)(j - i);
		for(j = 0; j < SIZE; j+=3)
			img[j+2] = (unsigned char)(i);

		ret = ff_wframe(img);
		//fwrite(img, 1, SIZE, f);
		if(ret < 0 && ret != -2){
			printf("ff_wframe failed, code: %d\n", ret);
			exit(EXIT_FAILURE);
		}
	}
	ret = ff_close();
	//fclose(f);
	if(ret < 0 && ret != -2){
		printf("ff_close failed, code: %d\n", ret);
		exit(EXIT_FAILURE);
	}
	return 0;
}
