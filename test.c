#include <stdio.h>
#include <stdlib.h>
#include "ffvi.h"
#define W 0x100
#define H 0x100
#define N 0x10
#define SIZE W*H
int main(int argc, char **argv)
{
	int ret;
	int i, j;
	unsigned char img[SIZE];
	//FILE *f;
	//printf("codec_id: %d\npix_fmt: %d\n",AV_CODEC_ID_RAWVIDEO,AV_PIX_FMT_GRAY8);
	ret = ff_init("test.avi", AV_CODEC_ID_RAWVIDEO,
					  W, H, 0, 30, AV_PIX_FMT_GRAY8);
	if(ret < 0){
		printf("ff_init failed, code: %d\n", ret);
		exit(EXIT_FAILURE);
	}
	for(i = 0; i < N; i++){
		for(j = 0; j < SIZE; j++)
			img[j] = (unsigned char)j;
		ret = ff_wframe(img);
		//fwrite(img, 1, SIZE, f);
		if(ret < 0){
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
