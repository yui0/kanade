// http://gijishinpo.asablo.jp/blog/2009/07/21/4448757
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "wave.h"

#define BUF_LEN		320
#define LPC_ORDER	14

int main(void)
{
	MONO_PCM pcm0, pcm1, pcm2;
	int buf_cnt=0;
	int n,m,k;
	int i,j;
	int start;
	double in_buf[BUF_LEN];
	double m_buf;
	double out_buf[LPC_ORDER+1];
	double r[LPC_ORDER];
	double a[LPC_ORDER];
	double b[LPC_ORDER];
	double alfa;
	double km,alfam,t;
	double rnd;

	mono_wave_read(&pcm0, "speech.wav");

	pcm2.fs = pcm0.fs; /* 標本化周波数 */
	pcm2.bits = pcm0.bits; /* 量子化精度 */
	pcm2.length = pcm0.length; /* 音データの長さ */
	pcm2.s = calloc(pcm2.length, sizeof(double)); /* メモリの確保 */

	for(i=0; i=BUF_LEN)buf_cnt = 0;
		//機械音生成
		if(n%64==0){
			m_buf=1;
		}else{
			m_buf=0;
		}

		if(n%BUF_LEN==0 && n>0)
		{
			//LPC計算
			//自己相関関数( <- 8/5修正）
			for(i=0; i= BUF_LEN)
		{
			out_buf[0] = -(a[1] * out_buf[1]
					+ a[2] * out_buf[2]
					+ a[3] * out_buf[3]
					+ a[4] * out_buf[4]
					+ a[5] * out_buf[5]
					+ a[6] * out_buf[6]
					+ a[7] * out_buf[7]
					+ a[8] * out_buf[8]
					+ a[9] * out_buf[9]
					+ a[10] * out_buf[10]
					+ a[11] * out_buf[11]
					+ a[12] * out_buf[12]
					+ a[13] * out_buf[13]
					+ a[14] * out_buf[14])
					+m_buf;
			//出力の配列をプッシュする
			for(i=LPC_ORDER; i>0; i--){
				out_buf[i] = out_buf[i-1];
			}
			pcm2.s[n] = out_buf[0];
		}else{
			pcm2.s[n] = in_buf[0];
		}
	}
	mono_wave_write(&pcm2, "result.wav");
	free(pcm0.s);
	free(pcm2.s);

	return 0;
}
