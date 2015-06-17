#include <stdio.h>
#include <stdlib.h>
#include "ffvi.h"
#define W 0x10
#define H 0x10
#define N 2
int main(int argc, char **argv)
{
	int ret;
	int i, j;
	unsigned char img[W*H];
	FILE *f;
	f = fopen("test0.bin", "wb");
	ret = ff_init("test.bin", AV_CODEC_ID_RAWVIDEO,
					  W, H, 0, 30, AV_PIX_FMT_GRAY8);
	if(ret < 0){
		printf("ff_init failed, code: %d\n", ret);
		exit(EXIT_FAILURE);
	}
	for(i = 0; i < N; i++){
		for(j = 0; j < W*H; j++)
			img[j] = (unsigned char)j;
		ret = ff_wframe(img);
		fwrite(img, 1, W*H, f);
		if(ret < 0){
			printf("ff_wframe failed, code: %d\n", ret);
			exit(EXIT_FAILURE);
		}
	}
	ret = ff_close();
	fclose(f);
	if(ret < 0){
		printf("ff_close failed, code: %d\n", ret);
		exit(EXIT_FAILURE);
	}
	return 0;
}
