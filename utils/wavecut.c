//---------------------------------------------------------
//	Wave Lab
//
//		©2013-2014 Yuichiro Nakada
//---------------------------------------------------------

// gcc -Os -o wavecut wavecut.c wave.c
// ./wavecut M16b22k_MikuVoice.wav 
#include <stdio.h>
#include <stdlib.h>
#include "wave.h"

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <inputfile>\n", argv[0]);
		exit(1);
	}

	Sound *snd;
	if (!(snd = readWave(argv[1]))) exit(1);

	printf("%s [%dHz/%dbit] len:%d\n", argv[1], snd->samplingrate, snd->bit_per_sample, snd->datanum);

	snd->datanum /= 135;
	printf("%s [%dHz/%dbit] len:%d\n", argv[1], snd->samplingrate, snd->bit_per_sample, snd->datanum);

	short *p = snd->monaural16;
	char buff[256];
	int i;
	for (i=0; i<135; i++) {
		sprintf(buff, "%02d.%s", i, argv[1]);
		writeWave(buff, snd);
		snd->monaural16 += snd->datanum;
	}
	snd->monaural16 = p;
	freeSound(snd);

	return 0;
}
