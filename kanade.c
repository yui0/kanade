//---------------------------------------------------------
//	Synth Vocal
//
//		©2013-2014 Yuichiro Nakada
//---------------------------------------------------------

// gcc -Os -o kanade kanade.c wave.c -lm
// http://moge32.blogspot.jp/2012/08/3c.html
// http://hand-onlooker.blogspot.jp/2012/05/3.html
// http://k-hiura.cocolog-nifty.com/blog/2011/08/post-4c83.html
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "wave.h"

Sound *vo[5];

#define SMPL		44100	// サンプリング周波数
//#define SMPL		22050	// for M16b22k_MikuVoice.wav

//#define S_FREQ		280	// 発声基本周波数 280Hz
#define S_FREQ		230	// 発声基本周波数 280Hz

// ゆらぎ
#define VIBRATO		2	// 4Hz
//#define VIBRATO		400	// 400Hz (ロボット)
#define VIBRATO_VOL	0.5	// 50%

typedef enum {
	VOWEL_A,
	VOWEL_I,
	VOWEL_U,
	VOWEL_E,
	VOWEL_O,
	VOWEL_J,	// 半母音
	VOWEL_W,	// 半母音
	VOWEL_NON	// 無音
} VowelType;

typedef enum {
	_NON,
	_K,
	_KH,
	_KK,
	_S,
	_SH,
	_T,
	_CH,
	_TS,
	_H,
	_HH,
	_P,
	_N,
	_M,
	_R,
	_RJ,
	_G,
	_Z,
	_DG,
	_D,
	_B,
	_Y,
	_W
} ConsonantType;

#define MAX(a,b)    (((a)<(b))?(b):(a))
#define MIN(a,b)    (((a)<(b))?(a):(b))


struct NoteFrame {
	//voice;
	unsigned long length;
	unsigned char base_pitch;
	short base_velocity;
//	std::vector< std::pair<long,short> > velocity_points;
//	std::pair<short,short> *margin;
//	std::pair<short,short> *padding;
	short *prec;
	short *ovrl;
	short *cons;
//	bool is_vcv;
};

typedef struct _TalkData
{
	double freq;
	double vol;
	unsigned long length;
	int vowel;
	int dvowel;			//２重母音
	int consonant;
	int flag;
} TalkData;

#define FORMANT_LENGTH (5)
typedef struct _VocalizeData
{
	double cFor[FORMANT_LENGTH];	// フォルマント(子音)
	double cVol;			// 子音の音量
	unsigned long cLen[3];		// 子音の入り方：フェードイン・長さ・フェードアウト
	unsigned long vLen[3];		// 母音の入り方：空白・フェードイン・フォルマント移動帯
	double vFor[FORMANT_LENGTH];	// フォルマント(母音)
	double vVol;			// 母音の音量
	double freq;			// 基本周波数 !!いみなし!!!
	unsigned long length;		// 全声部の長さ
	int flag;			// 連結するかどうか
} VocalizeData;

// 上を *1.2065
// 母音のフォルマント[Hz×5, vol]
double vowel[7][6]=
{
	// a/0
	{965, 1266, 3316, 4220, 5426, 0.8},
	// i/1
	{422, 2532, 3617, 4220, 5426, 0.9},
	// u/2
	{482, 1628, 2712, 4220, 5426, 0.9},
	// e/3
	{723, 2351, 3316, 4220, 5426, 0.7},
	// o/4
	{693, 904, 3617, 4220, 5426, 0.8},
	// j/5
	{361, 2292, 3617, 4220, 5426, 1.0},
	// w/6
	{482, 965, 3617, 4220, 5426, 0.6},
};
// 280Hz
/*double vowel[][6]=
{
	// a/0
	{1055, 1390, 0, 0, 0, 0.8},
	// i/1
	{295, 2640, 0, 0, 0, 0.9},
	// u/2
	{370, 1040, 0, 0, 0, 0.9},
	// e/3
	{700, 2430, 0, 0, 0, 0.7},
	// o/4
	{720, 835, 0, 0, 0, 0.8},
	// y1/5
	{295, 2640, 0, 0, 0, 0.8},
	// y2/6
	{700, 2430, 0, 0, 0, 0.8},
	// r1/7
	{370, 1040, 0, 0, 0, 0.8},
	// r2/8
	{565, 1310, 0, 0, 0, 1.0},
	// w1/9
	{315, 650, 0, 0, 0, 0.8},
	// w2/10
	{580, 760, 0, 0, 0, 0.8},
	// j/5
	{1080, 1240, 0, 0, 0, 1.0},
};*/
// https://www.akashi.ac.jp/lib/siryou/k49/49_006.pdf
// (1)閉鎖音	/k/,/t/,/g/,/d/,/b/,/p/
// (2)摩擦音	/s/,/h/,/z/
// (3)破擦音	英語では/dӡ/,/t∫/（日本語では発声される事はまれである）
// (4)鼻音	/m/,/n/
// (5)わたり音	/w/,/y/
// (6)流音	/r/
// 子音のフォルマント + α
VocalizeData consonant[]=
{
	// 00 [子音なし]
	{{0},0.0,{0},{0,0,500},{0},1,1,0,1},
	// k/01 [き]
	{{800,1100,1400,3500,4500},0.8,{200,0,2200},{2200,200,0},{0},1,1,0,1},
	// kh/02 [け]
	{{800,3200,3700,3500,4500},0.8,{0,0,2200},{1600,800,0},{0},1,1,0,1},
	// k,/03 [かくこ]
//	{{400,2300,4600,3500,4500},0.8,{0,0,2400},{1800,600,0},{0},1,1,0,1},
	{{1080,1960,2760,4160,},0.8,{0,0,2400},{1800,600,0},{0},1,1,0,1},
	// s/04
//	{{250,400,4500,3500,4500},0.5,{500,0,5500},{4000,2000,0},{0},1,1,0,1},
	{{250,400,2240,3200,4500},0.4,{500,0,5500},{4000,2000,0},{0},1,1,0,1},
	// sh/05 [しゃ・し・しゅ・しぇ・しょ]/「し」以外 dvowel=5
	{{500,2100,4300,3500,4500},0.6,{500,0,5500},{4000,2000,0},{0},1,1,0,1},
	// t/06
//	{{500,2100,4300,3500,4500},1.0,{0,200,400},{500,400,0},{0},1,1,0,1},
	{{500,1240,2000,2640,3360},1.0,{0,200,400},{500,0,0},{0},1,1,0,1},
	// ch/07 [ちゃ・ち・ちゅ・ちぇ・ちょ]/「ち」以外 dvowel=5
	{{3900,4900,5400,3500,4500},0.6,{0,0,2000},{1500,500,0},{0},1,1,0,1},
	// ts/08 [つぁ・つぃ・つ・つぇ・つぉ]/「つ」以外 dvowel=6
	{{1500,4000,4700,3500,4500},0.6,{0,100,1000},{900,500,0},{0},1,1,0,1},
	// h/09 [はふへお]
	{{250,10,1400,3500,4500},0.6,{500,1000,3000},{3500,500,0},{0},1,1,0,1},
	// h,/10 [ひ]
	{{250,400,4500,3500,4500},0.5,{1000,800,3000},{3500,500,0},{0},1,1,0,1},
	// p/11
	{{241,2369,3604,4556,5473},0.4,{300,0,1000},{1000,1000,0},{0},1,1,0,1},
	// n/12 「ま」に聞こえる
	{{0},0,{0},{0,500,0},{500,1359,4151,4031,4825},0.25,1,3000,0},
	// n/12 移動フォルマントによる鼻音。nとmの差は微妙
//	{{0},0,{0},{0,500,0},{280,920,3760,3900,4900},0.25,1,3500,0},
	// m/13
	{{0},0,{0},{0,500,500},{250,1200,3294,4308,4500},0.4,1,2000,0},
	// r/14 [らるれろ]
	{{0},0,{0},{0,1500,0},{375,2069,3745,3500,4500},0.5,1,2400,1},
	// rj/15 [り]
	{{0},0,{0},{0,1500,0},{248,2358,3847,3500,4500},0.5,1,2400,1},
	// g/16
	{{0},0,{0},{0,1500,1500},{400,800,2200,3500,4500},1.0,1,3000,1},
	// z/17
	{{338,2518,4761,3500,4500},0.8,{500,0,2000},{1000,1000,0},{324,1396,2445,3997,4915},0.8,1,3000,1},
	// dg/18 [じゃ・じ・じゅ・じぇ・じょ]/「じ」以外 dvowel=5
	{{150,3600,4500,3500,4500},0.6,{500,0,2000},{1000,1000,0},{400,2000,2800,3500,4500},0.6,1,2500,1},
	// d/19
	{{304,1353,2080,2923,4654},0.6,{300,0,2000},{800,800,0},{480,2106,3361,3880,5443},1.0,1,2500,1},
	// b/20
	{{200,250,400,3500,4500},0.3,{500,0,1000},{1000,1000,0},{500,1300,2500,3500,4500},0.8,1,2000,1},
	// y/11
	{{0},0,{0},{0,300,0},{200,2100,3100,3500,4500},0.6,1,3000,1},
	// w/13
	{{0},0,{0},{0,1500,1500},{400,800,2200,3500,4500},1.0,1,3000,1},
//	// n/16
//	{{0},0,{0},{500,1000,0},{250,1400,2500,3500,4500},0.3,1,3000,1},
//	// m/12
//	{{0},0,{0},{300,900,300},{200,250,300, 3500, 4500},0.4,1,1500,1},
};


//--------------------------------------------------------
//	各種波形生成エンジン
//---------------------------------------------------------

// 擬似的なノイズ出力
double GaussNoise()
{
	return ((double)rand() / (double)(RAND_MAX) - 0.5);
}

// Rosenberg波の生成
double GenRosenberg(double freq)
{
	static double t, a;
	double tau  = 0.90;  /* 声門開大期 */
	double tau2 = 0.95;  /* 声門閉小期 */
	double sample = 0.0;

	static int n;
	double v = VIBRATO_VOL * sin(n *VIBRATO *M_PI *2 /SMPL) /2.0;
	n++;

	t = a;
	t += (double)freq / (double)SMPL;
	t -= floor(t);
	a = t;
	t += v;

	if (t <= tau) {
		sample = 3.0*pow(t/tau,2.0)-2.0*pow(t/tau,3.0);
	} else if (t < tau+tau2) {
		sample = 1.0-pow((t-tau)/tau2,2.0);
	}
	return 2.0*(sample-0.5);
}

// レゾナンスフィルタ(BPF)の設定
void IIR_SettingReso(double f, double Q, double param[])
{
	double omega = 2.0 * M_PI * f / (double)SMPL;
	double alpha = sin(omega) / (2.0 * Q);
	double a0    = 1.0 + alpha;

	param[0] = alpha  / a0;
	param[1] = 0.0   / a0;
	param[2] = -alpha  / a0;
	param[3] = -2.0 * cos(omega) / a0;
	param[4] = (1.0 - alpha) / a0;
}

// フィルタの適用
double IIR_ApplyFilter(double base, double param[], double delay[])
{
	double sample = 0.0;

	sample += param[0] * base;
	sample += param[1] * delay[0];
	sample += param[2] * delay[1];
	sample -= param[3] * delay[2];
	sample -= param[4] * delay[3];

	delay[1] = delay[0]; delay[0] = base;
	delay[3] = delay[2]; delay[2] = sample;

	return sample;
}

// Noise / Sin波 / Pulse波の生成
double GenWave(double freq)
{
	/*{
		// Noise
		static int n;
		double f1 = GaussNoise();
		int m = n % 20000;
		n++;
		if (m > 10000) m = 20000 - m;
		if (m < 3000) {
			return (f1 * (m / 3000.0));
		} else {
			return f1;
		}
	}*/
#if 1
	{
		// Sin波
		static int n;
		int i;
		double vol[] = { 1.0, 0.75, 0.75, 0.5, 0.5, 0.25, 0.25, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05 };
		double a = 0, r = 0, rr;

		// ゆらぎ
		double v = VIBRATO_VOL * sin(n *VIBRATO *M_PI *2 /SMPL);
//		printf("%.2f ", v);

//		double time = (double)n / SMPL;
//		double freq = 200 + 100 * sin(2 * M_PI * time/1.5);
//		double freq = 160 + 50 * sin(2 * M_PI * time * 1.7)
//			+ 40 + 70 * sin(2 * M_PI * time / 23 +  2 * cos(time));
//		double freq = 200 + 25 * sin(2 * M_PI * time * 1.7)
//			+ 40 + 35 * sin(2 * M_PI * time / 23 +  2 * cos(time));
//		printf("%.1f ", freq);

		// 重ね合わせ
		for (i=0; i<7; i++) {
//			a += vol[i] * sin((n *(i *freq) /SMPL +v) * M_PI *2);
			a += vol[i] * sin((n *((i+1) *freq) /SMPL +v) * M_PI *2);
		}
//		for (i=0; i<8/*14*/; i++) {
//			a += vol[i] * sin((n *((i*2+1) *freq) /SMPL +v) * M_PI *2);
//		}
//		printf("%.2f ", a);

		// 雑音
		/*rr = GaussNoise();
		if ((rr < 0) && (r < 0)) rr = -rr;
		else if ((rr > 0) && (r > 0)) rr = -rr;
		r = rr;
		a += rr *0.4;*/

		n++;
		return a;
	}
#else
	{
		return GenRosenberg(freq);
	}
#endif
	/*{
		// Pulse波
		double d = t % (2*M_PI);
		if (d > 50*2*M_PI) return -0.9;
		return 0.9;
	}*/
}
double _GenWave(/*double freq*/double *FL)
{
	static int n;
	static double phase;
	int i;

	double time = (double)n / SMPL;
	int vow1 = /*floor*/(int)(time / 1.5 * 5) % 5;
	int vow2 = (vow1 + 1) % 5;
	//printf("%f %d %d ", time, vow1, vow2);

	double amp = (.65 - .15 * cos(time * M_PI / 1.5 * 5));
//	double freq = 160 + 50 * sin(2 * M_PI * time * 1.7)
//		+ 40 + 70 * sin(2 * M_PI * time / 23 +  2 * cos(time));
//	double freq = 280;
//		double freq = 200 + 12 * sin(2 * M_PI * time * 1.7)
//			+ 40 + 18 * sin(2 * M_PI * time / 23 +  2 * cos(time));
		double freq = 200 + 100 * sin(2 * M_PI * time/1.5);
	double w = 1 / (1 + exp(-30 * (5 * time / 1.5 - floor(5 * time / 1.5) - .5)));
//	printf("%.2f %.1f %.2f\n", amp, freq, w);

	// フォルマント強調
	static double tap[10];
	double s = .4 * sin(phase - floor(phase))
		* ((1 - w) * vowel[vow1][5] + w * vowel[vow2][5]) * amp;
//printf("%f %f ", phase, s);
	for (i=0; i<5; i++) {
//		double ff = (1 - w) * vowel[vow1][i] + w * vowel[vow2][i];
		double ff = FL[i];
		double r  = exp(-M_PI * 50. * (1. + ff * ff * 1e-6 / 6.) / SMPL);
		double a1 = 2. * r * cos(2.0 * M_PI * ff / SMPL);
		s = (1. - a1 + r * r) * s + (a1 * tap[i]) - (r * r * tap[i + 5]);
		tap[i + 5] = tap[i]; tap[i] = s;
	}

	phase += freq / SMPL;
	n++;

//	printf("%f ", s);
	return s;
}

// 補間
double _slide(unsigned long a, unsigned long b, double x1, double x2)
{
	double w;
	if ((b==0) || (a > b)) {
		return x2;
	} else {
		w = ((double)a)/((double)b);
	}
	//w = (w<0.5)? w*w*2 : -2 * w * (w - 2) -1;
	//w = sin(M_PI * (w -0.5)) * 0.5 + 0.5;
	return (1 - w) * x1 + w * x2;
}
void Synth(VocalizeData *v, TalkData *t/*次の発音*/, short *p)
{
	int i, j;
	double in, out;
	double param[5][5] = {0}, delay[5][4] = {0};

	// 波形の生成
	for (i=0; i<v->length; i++) {
		out = 0.0;

		if (i > v->vLen[0]) {
			// 母音
#if 1
//			if (v->flag < 0)
//			{
				// 機械音声
				if (i < v->vLen[0] + v->vLen[1]) {
					// フェードイン
					in = GenWave(v->freq) * _slide(i - v->vLen[0], (v->vLen[0] + v->vLen[1]), 0, v->vVol);
				} else if (i > v->vLen[0] + v->vLen[1] + v->vLen[2]) {
					// フォルマント移動帯
					double freq = _slide(i - (v->vLen[0] + v->vLen[1] + v->vLen[2]), v->length - (v->vLen[0] + v->vLen[1] + v->vLen[2]), v->freq, t->freq);
					double vol  = _slide(i - (v->vLen[0] + v->vLen[1] + v->vLen[2]), v->length - (v->vLen[0] + v->vLen[1] + v->vLen[2]), v->vVol, t->vol);
					//printf("%d ", (int)freq);
					in = GenWave(/*v->*/freq) * vol;
				} else {
					in = GenWave(v->freq) * v->vVol;
				}

				// F1-F5を強調
				for (j=0; j<5; j++) {
					IIR_SettingReso(v->vFor[j], 20.0, param[j]);
					out += IIR_ApplyFilter(in, param[j], delay[j]);
				}

				out = _GenWave(v->vFor);
//			} else {
#else
				// 人間の声を使用
				// for M16b22k_MikuVoice.wav
//				out = (vo->monaural16[(vo->datanum/135)*v->flag + i])/65536.0;
				// for UTAU
				out = (vo[v->flag<0 ? -v->flag : v->flag]->monaural16[i-v->vLen[0]])/65536.0;//*3.0;
//				printf("%d ", (short)out);
//				printf("%f ", out);

				// 有声? 子音作成
				if (v->flag < 0) {
					in = 0.0;

					// F1-F5を強調
					for (j=0; j<5; j++) {
						IIR_SettingReso(v->vFor[j], 20.0, param[j]);
//						out += IIR_ApplyFilter(in, param[j], delay[j]);
						in += IIR_ApplyFilter(out, param[j], delay[j]);
					}
					out = in;
				}

				if (i < v->vLen[0] + v->vLen[1]) {
					// フェードイン
					in = out * _slide(i - v->vLen[0], (v->vLen[0] + v->vLen[1]), 0, v->vVol);
				} else if (i > v->vLen[0] + v->vLen[1] + v->vLen[2]) {
					// フォルマント移動帯
					in = out * _slide(i - (v->vLen[0] + v->vLen[1] + v->vLen[2]), v->length - (v->vLen[0] + v->vLen[1] + v->vLen[2]), v->vVol, (t->vol));
				} else {
					in = out * v->vVol;
				}
				out = in;
//			}
#endif
		}

		if (i < v->cLen[0] + v->cLen[1] + v->cLen[2]) {
			// 子音
			if (i < v->cLen[0]) {
				// フェードイン
				in = GaussNoise() * _slide(i, v->cLen[0], 0, v->cVol);
			} else if (i > v->cLen[0] + v->cLen[1]) {
				// フェードアウト
				in = GaussNoise() * _slide(i - (v->cLen[0] + v->cLen[1]), v->length - (v->cLen[0] + v->cLen[1]), v->cVol, 0);
			} else {
				in = GaussNoise() * v->cVol;
			}

			// F1-F5を強調
			for (j=0; j<5; j++) {
				IIR_SettingReso(v->cFor[j], 20.0, param[j]);
				out += IIR_ApplyFilter(in, param[j], delay[j]);
			}
		}

//		*p++ = (32768.0 * MIN(MAX(out, -1.0), 1.0)) + 32768;
		*p++ = (32768.0 * MIN(MAX(out, -1.0), 1.0));
		//printf("%d ", (short)*(p-1));
	}
}
void Vocalize(char *name, TalkData *t)
{
	int i;
	VocalizeData v;

	int size = SMPL * 30;	// 10s
	Sound *snd;
	snd = createSound(1/*ch*/, SMPL/*Hz*/, 16/*bit*/, size*16/8);
	short *p = snd->monaural16;

	while (t->freq) {
		v = consonant[t->consonant];
		if (t->consonant > 11) {
			// 母音と同じように生成
			// 「ん」「な行」「ま行」は同じ
			// n,m,r,rj,g,z,dg,d,b
			v.cVol *= t->vol * v.vVol;
			v.vVol *= t->vol;
			v.freq *= t->freq;
//			v.flag = 1;	// 継続
			v.flag = - t->vowel;

			if (t->vowel==VOWEL_NON) {
				// 「ん」
				v.length = t->length * 2.0/3;
				v.flag = - VOWEL_O;
			/*} else if (t->consonant==_N) {
				for (i=0; i<5; i++) v.vFor[i] = vowel[t->vowel][i];*/
			}

			//printf("  freq:%.1f vol:[%.2f, %.2f] clen:[%d-%d-%d] len:%d\n", v.freq, v.cVol, v.vVol, v.cLen[0], v.cLen[1], v.cLen[2], v.length);
			printf("  freq:%.1f vol:[%.2f, %.2f] clen:[%d-%d-%d] vlen:[%d-%d-%d] len:%d\n", v.freq, v.cVol, v.vVol, v.cLen[0], v.cLen[1], v.cLen[2], v.vLen[0], v.vLen[1], v.vLen[2], v.length);
			Synth(&v, t+1, p);
			p += v.length;
			t->length -= v.length;
			v = consonant[0];
		}

		if (t->vowel==VOWEL_NON) {
			// 無音
			printf("break len:%d\n", t->length);
			memset(p, 0, t->length*2);
			p += t->length;
			t++;
		}

		if (t->dvowel > 0) {
			// 2重母音
			for (i=0; i<5; i++) v.vFor[i] = vowel[t->dvowel][i];
			v.vVol *= t->vol * vowel[t->vowel][5];
			v.cVol *= v.vVol;
			v.length = 6000;
			v.freq *= t->freq;
//			v.flag *= t->flag;

			v.flag = t->vowel;

			printf("  freq:%f vol:[%f, %f] clen:[%d-%d-%d] len:%d\n", v.freq, v.cVol, v.vVol, v.cLen[0], v.cLen[1], v.cLen[2], v.length);
			Synth(&v, t+1, p);
			p += v.length;
			t->length -= v.length;
			v = consonant[0];
		}

		for (i=0; i<5; i++) v.vFor[i] = vowel[t->vowel][i];
		v.vVol *= t->vol * vowel[t->vowel][5];
		v.cVol *= v.vVol;
//		v.cVol *= t->vol;
		v.freq *= t->freq;
//		v.flag *= t->flag;
		v.length = t->length;
//		v.vLen[2] = (t->length - (1500 + v.vLen[0] + v.vLen[1])) * 0.5;
		v.vLen[2] = (t->length - (1500 + v.vLen[0] + v.vLen[1])) * 0.95;

#if SMPL == 44100
		// SMPL = 44100
		v.cLen[0] *= 2;
		v.cLen[1] *= 2;
		v.cLen[2] *= 2;
		v.vLen[0] *= 2;
		v.vLen[1] *= 2;
		v.vLen[2] *= 2;
		//v.length *= 2;
#endif
		printf("freq:%.1f vol:[%.2f, %.2f] clen:[%d-%d-%d] vlen:[%d-%d-%d] len:%d\n", v.freq, v.cVol, v.vVol, v.cLen[0], v.cLen[1], v.cLen[2], v.vLen[0], v.vLen[1], v.vLen[2], v.length);

		v.flag = t->vowel;
		Synth(&v, ++t, p);
		p += v.length;
	}

	snd->datanum = p - snd->monaural16;
	writeWave(name, snd);
	freeSound(snd);
}

#if 0
// 母音のみ合成「あいうえお」
void GenVowel()
{
	static int n;
	static double phase;
	int i;

	double time = (double)n / SMPL;
	int vow1 = /*floor*/(int)(time / 1.5 * 5) % 5;
	int vow2 = (vow1 + 1) % 5;

	double amp = (.65 - .15 * cos(time * M_PI / 1.5 * 5));
	double freq = 160 + 50 * sin(2 * M_PI * time * 1.7)
		+ 40 + 70 * sin(2 * M_PI * time / 23 +  2 * cos(time));
	double w = 1 / (1 + exp(-30 * (5 * time / 1.5 - floor(5 * time / 1.5) - .5)));

	// フォルマント強調
	static double tap[10];
	double s = .4 * sin(phase - floor(phase))
		* ((1 - w) * vowel[vow1][5] + w * vowel[vow2][5]) * amp;
	for (i=0; i<5; i++) {
		double ff = (1 - w) * vowel[vow1][i] + w * vowel[vow2][i];
		double r  = exp(-M_PI * 50. * (1. + ff * ff * 1e-6 / 6.) / SMPL);
		double a1 = 2. * r * cos(2.0 * M_PI * ff / SMPL);
		s = (1. - a1 + r * r) * s + (a1 * tap[i]) - (r * r * tap[i + 5]);
		tap[i + 5] = tap[i]; tap[i] = s;
	}

	phase += freq / SMPL;
	n++;

	return s;
}
#endif
#if 0
// 母音のみ合成「あいうえお」
void Speech(char *name)
{
	int i, j, size;
	double freq, vtlen, in, out;
	double param[5][5] = {0}, delay[5][4] = {0};

	// 共鳴周波数の変動値（お兄さん仕様）
	double formant[5][5] = {
		/* F1,  F2,  F3,  F4,  F5 */
		{1.60, 0.70, 1.10, 1.00, 1.00},
		{0.70, 1.40, 1.20, 1.00, 1.00},
		{0.80, 0.90, 0.90, 1.00, 1.00},
		{1.20, 1.30, 1.10, 1.00, 1.00},
		{1.15, 0.50, 1.20, 1.00, 1.00}
	};

	// 設定
	size  = SMPL * 1;	// 1秒間
	freq  = 220.0;		// 基本周波数
//	vtlen = 15.0;		// 声道の長さ(成人男性で16.9cm、成人女性で14.1cm)
	vtlen = 14.1;		// 声道の長さ(成人男性で16.9cm、成人女性で14.1cm)

	// 共鳴周波数を計算
	for (i=0; i<5; i++) {
		printf("ふぉるまんと[%d]\n", i);
		for (j=0; j<5; j++) {
			formant[i][j] *= (double)((34000.0*(double)(2*j+1))/(4.0*vtlen));
//			double a = (double)((34000.0*(double)(2*j+1))/(4.0*vtlen));
//			formant[i][j] = formant[i][j] * a;
			printf(" %f", formant[i][j]);
		}
		printf("\n");
	}

	Sound *snd;
	snd = createSound(1/*ch*/, SMPL/*Hz*/, 16/*bit*/, size*16/8);

	// 波形の生成
	for (i=0; i<size; i++) {
		out = 0.0;
		in = GenRosenberg(freq);

		// F1-F5を強調
		for (j=0; j<5; j++) {
			IIR_SettingReso(formant[5*i/size][j], 20.0, param[j]);
			out += IIR_ApplyFilter(in, param[j], delay[j]);
		}

		//buf[i] = (char)(128.0 * MIN(MAX(out, -1.0), 1.0)) + 128;
		//snd->monaural8[i] = (char)(128.0 * MIN(MAX(out, -1.0), 1.0)) + 128;
		snd->monaural16[i] = (32768.0 * MIN(MAX(out, -1.0), 1.0)) + 32768;
	}

	writeWave(name, snd);
	freeSound(snd);
}
#endif

// 音声データ読み込み
void LoadVocal(char *path)
{
	int i, x;
	char buff[256];

	sprintf(buff, "%s/あ.wav", path);
	if (!(vo[0] = readWave(buff))) exit(1);
	printf("%s [%dHz/%dbit] len:%d\n", buff, vo[0]->samplingrate, vo[0]->bit_per_sample, vo[0]->datanum);
	sprintf(buff, "%s/い.wav", path);
	if (!(vo[1] = readWave(buff))) exit(1);
	sprintf(buff, "%s/う.wav", path);
	if (!(vo[2] = readWave(buff))) exit(1);
	sprintf(buff, "%s/え.wav", path);
	if (!(vo[3] = readWave(buff))) exit(1);
	sprintf(buff, "%s/お.wav", path);
	if (!(vo[4] = readWave(buff))) exit(1);

	// 音量調整+無音区間をカット
	int max = 0;
	double a = 1;
	for (i=0; i<5; i++) {
		for (x=0; x<vo[i]->datanum; x++) {
			if (vo[i]->monaural16[x] > max) max = vo[i]->monaural16[x];
		}
		a = 32000.0 / max;
		for (x=0; x<vo[i]->datanum; x++) {
			if (vo[i]->monaural16[x]*a > 1000 || vo[i]->monaural16[x]*a < -1000) {
				//memcpy(vo[i]->monaural16, &vo[i]->monaural16[x-10], vo[i]->datanum-x);
//				a = 1;
//				max = x - 10;
//				if (max<0) max = 0;

				for (x=0; x<vo[i]->datanum-max; x++) vo[i]->monaural16[x] = vo[i]->monaural16[x+max] * a;
				break;
			}
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <config dir> <outputfile>\n", argv[0]);
		exit(1);
	}

	// 音声データ読み込み
	LoadVocal(argv[1]);

#if 0
	Speech(argv[1]);
#else
	/*TalkData speech[] = {
		{240, 18000, 8000, VOWEL_A, 0, 1},
		{250, 26000, 8000, VOWEL_E, 0, 1},
		{260, 22000, 8000, VOWEL_I, 0, 1},
		{240, 20000, 8000, VOWEL_U, 0, 1},
		{250, 18000, 8000, VOWEL_E, 0, 1},
		{240, 26000, 8000, VOWEL_O, 0, 1},
		{260, 22000, 8000, VOWEL_A, 0, 1},
		{220, 20000, 8000, VOWEL_O, 0, 1},
		{0},
	};*/
	TalkData speech[] = {
#if 0
		// あいうえお
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, 0, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, 0, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, 0, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, 0, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, 0, 1},

		// かきくけこ
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _KK, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _K, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _KK, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _KH, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _KK, 1},

		// さしすせそ
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _S, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _SH, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _S, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _S, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _S, 1},

		// たちつてと
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _T, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _CH, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _TS, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _T, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _T, 1},

		// なにぬねの
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _N, 1},

		// はひふへほ
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _H, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _HH, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _H, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _H, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _H, 1},

		// まみむめも
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _M, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _M, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _M, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _M, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _M, 1},

		// やゆよ
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _Y, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _Y, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _Y, 1},

		// らりるれろ
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _R, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _RJ, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _R, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _R, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _R, 1},

		// わをん
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _W, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _W, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _N, 1},
#endif
//		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _KK, 1},	// か
//		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _S, 1},	// さ
//		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _T, 1},	// た
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _N, 1},	// な
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _N, 1},
//		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _H, 1},	// は
//		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _M, 1},	// ま
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _Y, 1},	// や

		// 無音
		{230, 1.0, SMPL*0.4, VOWEL_NON, 0, 0, 1},

		/*{230, 0.8, SMPL*0.2, VOWEL_O, 0, 0, 1},
		{240, 1.1, SMPL*0.3, VOWEL_A, 0, _H, 1},
		{245, 1.0, SMPL*0.3, VOWEL_O, 0, _Y, 1},
		{250, 0.9, SMPL*0.4, VOWEL_U, 0, 0, 1},*/

		// こんにちは。
//		{230, 1.0, SMPL*0.3, VOWEL_O, 0, _K, 1},
		{230, 1.0, SMPL*0.3, VOWEL_O, 0, _KK, 1},
//		{230, 1.0, SMPL*0.3, VOWEL_O, 0, _N, 1},
		{230, 1.0, SMPL*0.3, VOWEL_NON, 0, _N, 1},
		{230, 1.0, SMPL*0.3, VOWEL_I, 0, _N, 1},
		{230, 1.0, SMPL*0.3, VOWEL_I, 0, _CH, 1},
		{230, 1.0, SMPL*0.3, VOWEL_A, 0, _W, 1},

		// 無音
		{230, 1.0, SMPL*0.4, VOWEL_NON, 0, 0, 1},

/*		// 私は音声合成ソフト「かえで」です。
//		{230, 1.0, SMPL*0.3, VOWEL_A, VOWEL_W, _W, 1},
		{230, 1.0, SMPL*0.3, VOWEL_A, 0, _W, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _T, 1},	// た
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, _SH, 1},
		{230, 1.0, SMPL*0.6, VOWEL_A, 0, _H, 1},

		{230, 1.0, SMPL*0.4, VOWEL_O, 0, 0, 1},
		{230, 1.0, SMPL*0.3, VOWEL_O, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _S, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, 0, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _G, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, 0, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _S, 1},
		{230, 1.0, SMPL*0.4, VOWEL_I, 0, 0, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _S, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _H, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _T, 1},
		{230, 1.0, SMPL*0.4, VOWEL_E, 0, _D, 1},
		{230, 1.0, SMPL*0.4, VOWEL_U, 0, _S, 1},

		// 無音
		{230, 1.0, SMPL*0.4, VOWEL_NON, 0, 0, 1},*/

		// よろしくお願いします。
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_O, VOWEL_J, _Y, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_O, 0, _R, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_I, 0, _S, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_U, 0, _K, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_O, 0, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_E, 0, _N, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_A, 0, _G, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_I, 0, 0, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_I, 0, _S, 1},
		{S_FREQ, 1.0, SMPL*0.3, VOWEL_A, 0, _M, 1},
		{S_FREQ, 1.0, SMPL*0.4, VOWEL_U, 0, _S, 1},

		/*{230, 1.0, SMPL*0.4, VOWEL_A, 0, 0, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _K, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _S, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _T, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _N, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _H, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _M, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _Y, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _R, 1},
		{230, 1.0, SMPL*0.4, VOWEL_A, 0, _W, 1},
		{230, 1.0, SMPL*0.4, VOWEL_O, 0, _N, 1},*/
		{0},
	};
	Vocalize(argv[2], speech);
#endif

	freeSound(vo[0]);
	freeSound(vo[1]);
	freeSound(vo[2]);
	freeSound(vo[3]);
	freeSound(vo[4]);

	return 0;
}
