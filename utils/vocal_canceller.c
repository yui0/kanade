//---------------------------------------------------------
//	Wave Lab
//
//		©2013-2014 Yuichiro Nakada
//---------------------------------------------------------

// gcc -Os -o vocal_canceller vocal_canceller.c wave.c
#include <stdio.h>
#include <stdlib.h>
#include "wave.h"

// http://www.atelier-blue.com/memo/memo2006-4-22.htm
void vocal_canceller(Sound *snd/*, Sound *snd2*/)
{
	int i;
	for (i=0; i<snd->datanum; i++) {
		// モノラル化
		int mono = ((((int)snd->stereo16[i].l) +((int)snd->stereo16[i].r))/2);
//		snd->stereo16[i].l = mono;
//		snd->stereo16[i].r = mono;

		// 真ん中の音を消す
		int t = ((int)snd->stereo16[i].l) -((int)snd->stereo16[i].r);
		if (t>32767) t = 32767;
		else if (t<-32768) t = -32768;
//		snd->stereo16[i].l = snd->stereo16[i].r = t;

		t = mono -t;	// ボーカルのみ
//		snd->stereo16[i].l = t;
//		snd->stereo16[i].r = t;

		snd->stereo16[i].l -= t;
		snd->stereo16[i].r -= t;

//		snd->stereo16[i].l = snd->stereo16[i].r = snd->stereo16[i].l-t;
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <inputfile> <outputfile>\n", argv[0]);
		exit(1);
	}

	Sound *snd, *snd2;

	if (!(snd = readWave(argv[1]))) exit(1);
	//snd2 = createSound(snd->channelnum, snd->samplingrate, snd->bit_per_sample, snd->datanum*snd->channelnum*snd->bit_per_sample/8);

	vocal_canceller(snd/*, snd2*/);

	writeWave(argv[2], snd);
	freeSound(snd);

	return 0;
}
