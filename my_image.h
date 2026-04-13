#include "Windows.h"  
#include "stdio.h"  
#include "string"  
#include "cstring"  
#include "stack"  
#include "malloc.h"  
#include "math.h"  
#include "cstdlib"  
#include "iostream"  
#include "bmp.h"  

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

using namespace std;

typedef struct image{
	int wid,hei,channel;
	unsigned char *p;
	bool read(const char bmpname[]){
		p = stbi_load(bmpname, &wid, &hei, &channel, 0);
		if (!p) return false;
		return true;
	}
	bool read_3bytes(const char bmpname[]){
		p = stbi_load(bmpname, &wid, &hei, &channel, 3);
		channel=3;
		if (!p) return false;
		return true;
	}
	bool read_4bytes(const char bmpname[]){
		p = stbi_load(bmpname, &wid, &hei, &channel, 4);
		channel=4;
		if (!p) return false;
		return true;
	}
	void save_png(char Name[]){
		stbi_write_png(Name, wid, hei, channel, p, wid * channel);
	}
	void save_jpg(char Name[],int quality){
		stbi_write_jpg(Name, wid, hei, channel, p, quality);
	}
	void save(char Name[]){
		int len = strlen(Name);
		if(Name[len-3]=='j'||Name[len-3]=='J'||Name[len-4]=='j'||Name[len-4]=='J') save_jpg(Name,90); 
		else save_png(Name);
	}
	void clear(){memset(p, 0, wid*hei * channel);}
	void space_apply(){p=new unsigned char[wid*hei*channel];clear();}
	void resize(int w,int h){
		if(wid==w&&hei==h)return;
		unsigned char *newp = new unsigned char[w*h*channel];
		if(channel==4)stbir_resize_uint8_srgb(p, wid, hei, 0, newp, w, h, 0, STBIR_RGBA);
		else stbir_resize_uint8_srgb(p, wid, hei, 0, newp, w, h, 0, STBIR_RGB);
		delete[] p;
		p=newp;
		wid=w;
		hei=h;
	}
	void free(){
		delete[] p;
	}
	void clone(bmp24 a){
		wid=a.wid;
		hei=a.hei;
		channel=3;
		space_apply();
		for(int i=0;i<a.hei;i++)
		{
			for(int j=0;j<a.wid;j++)
			{
				int inloc = i*a.wid+j, outloc=((a.hei-1-i)*a.wid+j)*channel;
				p[outloc] = a.p[inloc].R;
				p[outloc+1] = a.p[inloc].G;
				p[outloc+2] = a.p[inloc].B;
			}
		}
	}
	void clone(image a){
		wid=a.wid;
		hei=a.hei;
		channel=a.channel;
		space_apply();
		memcpy(p,a.p,wid*hei*channel);
	}
	void put(bmp24 &a){
		a.wid=wid;
		a.hei=hei;
		a.space_apply();
		if(channel==4){
			for(int i=0;i<a.hei;i++)
			{
				for(int j=0;j<a.wid;j++)
				{
					int outloc = i*a.wid+j, inloc=((a.hei-1-i)*a.wid+j)*channel;
					a.p[outloc].R = p[inloc] * p[inloc+3] / 255;
					a.p[outloc].G = p[inloc+1] * p[inloc+3] / 255;
					a.p[outloc].B = p[inloc+2] * p[inloc+3] / 255;
				}
			}
		}
		else if(channel==3){
			for(int i=0;i<a.hei;i++)
			{
				for(int j=0;j<a.wid;j++)
				{
					int outloc = i*a.wid+j, inloc=((a.hei-1-i)*a.wid+j)*channel;
					a.p[outloc].R = p[inloc];
					a.p[outloc].G = p[inloc+1];
					a.p[outloc].B = p[inloc+2];
				}
			}
		}
	}
	void draw_line(int x1,int y1,int x2,int y2,RGBPixel color){
		int t; 
		int xerr=0,yerr=0,delta_x,delta_y,distance; 
		int incx,incy,uRow,uCol; 
		delta_x=x2-x1;
		delta_y=y2-y1; 
		uRow=x1; 
		uCol=y1; 
		if(delta_x>0)incx=1;
		else if(delta_x==0)incx=0;
		else {incx=-1;delta_x=-delta_x;} 
		if(delta_y>0)incy=1; 
		else if(delta_y==0)incy=0;
		else{incy=-1;delta_y=-delta_y;} 
		if(delta_x>delta_y)distance=delta_x;
		else distance=delta_y; 
		for(t=0;t<=distance+1;t++ )
		{
			if(uRow>=0&&uRow<wid&&uCol>=0&&uCol<hei)
			{
				p[(uRow+uCol*wid)*3]=color.R;
				p[(uRow+uCol*wid)*3+1]=color.G;
				p[(uRow+uCol*wid)*3+2]=color.B;
			}
			xerr+=delta_x ; 
			yerr+=delta_y ; 
			if(xerr>distance) 
			{ 
				xerr-=distance; 
				uRow+=incx; 
			} 
			if(yerr>distance) 
			{ 
				yerr-=distance; 
				uCol+=incy; 
			} 
		}  
	}
}image;


