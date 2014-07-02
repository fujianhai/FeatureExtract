#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cmath>
#include <iostream>
#include <fstream>
#include <cstdio>
using namespace std;

#define   PI     3.1415926
#define   TPI    2*PI
#define   PREEMCOEF 0.9375
#define    N     256     //输入样本总数
#define    M     8       //DFT运算层数  

typedef struct _ExtractedFeature
{
	    float MFCC[12];
		int zcr;
		double En;
		double EnProduct;
}GetFeature;

typedef struct _TWavHeader  //wav格式音频的文件头
{ 
        int rId;    //标志符（RIFF）
        int rLen;   //数据大小,包括数据头的大小和音频文件的大小
        int wId;    //格式类型（"WAVE"）
        int fId;    //"fmt"

        int fLen;   //Sizeof(WAVEFORMATEX)

        short wFormatTag;       //编码格式，包括WAVE_FORMAT_PCM，WAVEFORMAT_ADPCM等
        short nChannels;        //声道数，单声道为1，双声道为2
        int nSamplesPerSec;   //采样频率
        int nAvgBytesPerSec;  //每秒的数据量
        short nBlockAlign;      //块对齐
        short wBitsPerSample;   //WAVE文件的采样大小
        int dId;              //"data"
        int wSampleLength;    //音频数据的大小
}TWavHeader; 

void  PreEmphasise (double *s, double k);
void  GenHamWindow (int frameSize);
void  Ham (double *s,int frameSize);
double MyFFT(double *s,double *out); //返回能量值En
void  TrigFilter(double* in,double* out);
void  Bank(double *in,double *out);//获取12个MFCC系数
int   GetZCR(double * in,int len); //获取过零率

int    ipframesize=256; //定义帧的长度
double  hamWin[512];
double  Value[N];//FFT算法输出的幅度谱
double En=0;

int main()
{
 TWavHeader waveheader;
 FILE *sourcefile;
 ofstream outfile("1.txt");
 sourcefile=fopen("1.wav","rb");
 fread(&waveheader,sizeof(struct _TWavHeader),1,sourcefile);
 cout<<waveheader.nChannels<<endl;
 cout<<waveheader.wBitsPerSample<<endl;
 cout<<waveheader.nSamplesPerSec<<endl;
 cout<<sizeof(struct _TWavHeader)<<endl;
 
 short buffer[256];
 double singledata[256];
 double FFTCoff[256];
 double TrigCoff[24]={0};
 double BankCoff[12]={0};
 int   zcr=0;
 double energe=0;
 double enproduct=0;
 int j;
 int flag=1;
 while(flag==1 && fread(buffer,sizeof(short),ipframesize,sourcefile)==ipframesize)//等待数据读完一共读256个字节。文件指针移动，到了数据区。
 {   flag=0;
	 for(int i=0;i<ipframesize;i++) 
	 {
		 //singledata[i]=(3.0518/100000)*buffer[i];
		 singledata[i]=buffer[i];
		 printf("Data Index %d:%lf\r\n",i,singledata[i]);
	 }


PreEmphasise (singledata, PREEMCOEF);
Ham (singledata, ipframesize);//加汉明窗
energe=MyFFT(singledata,FFTCoff);
TrigFilter(FFTCoff,TrigCoff);
Bank(TrigCoff,BankCoff);
zcr=GetZCR(singledata,ipframesize);   
enproduct=zcr*energe;
fseek(sourcefile,-256,SEEK_CUR);//文件指针回溯

GetFeature Features;

for(j=0;j<12;j++)
{
	Features.MFCC[j]=BankCoff[j];
}
Features.En=energe;
Features.zcr=zcr;
Features.EnProduct=enproduct;

for(j=0;j<12;j++)
printf("bankcoff %f \r\n ",Features.MFCC[j]);
printf("\r\n");
printf("zcr %d\r\n",zcr);
printf("Energe %f\r\n",energe);
printf("EnProduct %f \r\n",enproduct);
printf("-------------------------------\r\n");
}
}


void PreEmphasise (double *s, double k)
{
   int i;
   float preE;//加重系数
   
   preE = k;
   for (i=ipframesize;i>=1;i--)
      s[i] -= s[i-1]*preE;
}

/* GenHamWindow: generate precomputed Hamming window function */
 void GenHamWindow (int frameSize)//汉明窗函数。系数取0.46。
{
   int i;
   float a;
   a = TPI / (frameSize - 1);
   for (i=1;i<=frameSize;i++)
      hamWin[i] = 0.54 - 0.46 * cos(a*(i-1));
}

//Ham: Apply Hamming Window to Speech frame s 
void Ham (double *s,int frameSize)
{
	
   int i;
   GenHamWindow(frameSize);
   s[0]=0.8000;//S[0]赋一个初值
   for (i=1;i<=frameSize;i++)
      s[i] *= hamWin[i];
}

double MyFFT(double *s,double *out)
{  
	float local_en=0;//定义能量的局部变量 

    float   x_i[N]={0}; 
	float  x_r[N];
	//bulk1
    int    p=1, q, i;
    int    bit_rev[ N ];  
    float   xx_r[ N ];   
    
    //bulk2
    int     cur_layer, gr_num, k;	//cur_layer代表正要计算的当前层，gr_num代表当前层的颗粒数
    float   tmp_real, tmp_imag, temp;   // 临时变量, 记录实部
    float   tw1, tw2;// 旋转因子,tw1为旋转因子的实部cos部分, tw2为旋转因子的虚部sin部分.
    int    step;      // 步进
    int    sample_num;   // 颗粒的样本总数(各层不同, 因为各层颗粒的输入不同) 
    
    for(i=0;i<N;i++)
    x_r[i] =s[i] ;  //输入数据，此处设为256
 
    //bulk1
    bit_rev[ 0 ] = 0;
    while( p < N )
    {
       for(q=0; q<p; q++)  
       {
           bit_rev[ q ]     = bit_rev[ q ] * 2;
           bit_rev[ q + p ] = bit_rev[ q ] + 1;
       }
       p *= 2;
    }
    
    for(i=0; i<N; i++)  
		xx_r[ i ] = x_r[ i ];    
    for(i=0; i<N; i++)   
		x_r[i] = xx_r[ bit_rev[i] ];
   
     //bulk2
    /* 对层循环 */
    for(cur_layer=1; cur_layer<=M; cur_layer++)
    {      
       /* 求当前层拥有多少个颗粒(gr_num) */
       gr_num = 1;
       i = M - cur_layer;
       while(i > 0)
       {
           i--;
           gr_num *= 2;
       }
       /* 每个颗粒的输入样本数N' */
       sample_num    = (int)pow(2, cur_layer); 
       /* 步进. 步进是N'/2 */
       step       = sample_num/2;
       /*  */
       k = 0;
       /* 对颗粒进行循环 */
       for(i=0; i<gr_num; i++)
       {//  对样本点进行循环, 注意上限和步进 
            for(p=0; p<sample_num/2; p++)
           {   
              // 旋转因子, 需要优化...    
              tw1 = cos(2*PI*p/pow(2, cur_layer));
              tw2 = -sin(2*PI*p/pow(2, cur_layer));
              tmp_real = x_r[k+p];
              tmp_imag = x_i[k+p];
              temp = x_r[k+p+step];
              /* 蝶形算法 */
              x_r[k+p]   = tmp_real + ( tw1*x_r[k+p+step] - tw2*x_i[k+p+step] );
              x_i[k+p]   = tmp_imag + ( tw2*x_r[k+p+step] + tw1*x_i[k+p+step] );
              x_r[k+p+step]   = tmp_real - ( tw1* temp - tw2*x_i[k+p+step] );
              x_i[k+p+step]   = tmp_imag - ( tw2* temp + tw1*x_i[k+p+step] );
              
           }
           /* 开跳!:) */
           k += 2*step;
       }   
    }

    for(i=0;i<N;i++)           //传出输出结果
		out[i]=x_r[i]*x_r[i]+x_i[i]*x_i[i];
	for(i=0;i<N;i++)   
		local_en+=out[i]*out[i];
	return local_en;
}


void TrigFilter(double* in,double* out)//计算三角滤波系数
{   int i;
    float var;
    int channel,pot; 
    float Fres[26];//24个通
	float F[24][256]={0};
	float fh=4000; 
    float melf=2595*log(1+fh/700);
	
	for(i=0;i<26;i++)
	{
		Fres[i]=700*(exp((melf/2595)*i/25)-1);
    }

	for(channel=0;channel<24;channel++)
	{
		for(pot=0;pot<256;pot++)
		{   
			var=fh*(pot+1)/256;
			if( (Fres[channel]<=var)&&(var<=Fres[channel+1]) )
				F[channel][pot]=(var-Fres[channel])/(Fres[channel+1]-Fres[channel]);
			else if( (Fres[channel+1]<=var)&&(var<=Fres[channel+2]) )
                F[channel][pot]=(Fres[channel+2]-var)/(Fres[channel+2]-Fres[channel+1]);
            else F[channel][pot]=0;
		}
	}

	for(channel=0;channel<24;channel++)
	{  
		out[channel]=0;//先进行初始化，然后计数
		for(pot=0;pot<256;pot++)
		{   
			out[channel]+=in[pot]*F[channel][pot];
		}
  	out[channel]=log(out[channel]);
	}

}

void Bank(double *in,double *out)
{
	int i,j;
	float dctcoef[12][24];
	float win[12];
	float Wmax=0;
    for(i=0;i<12;i++)
	{
		for(j=0;j<24;j++)
		{
			dctcoef[i][j]=cos((2*j+1)*(i+1)*PI/(2*24));
		}
	}

	for(i=0;i<12;i++)
	{
		win[i]=1+6*sin((float)(PI*(i+1))/12);
		if(win[i]>Wmax)
			Wmax=win[i];
	}
    
	for(i=0;i<12;i++)
	{
		win[i]=win[i]/Wmax;//归一化
	}

	for(i=0;i<12;i++)
	{
		for(j=0;j<24;j++)
		{
			out[i]+=dctcoef[i][j]*in[j];
		}
		out[i]=out[i]*win[i];
	}
}


int GetZCR(double * in,int len)  //过零率算法应该和Matlab一样，因为是用它的数据进训练的
{
	int zcr=0;
	int i;
    float delta=0.02;
    for(i=0;i<len-1;i++){
		if( (in[i]*in[i+1]<0) && (abs(in[i]-in[i+1])>delta) )
			zcr++;
	}
	return zcr;
}
