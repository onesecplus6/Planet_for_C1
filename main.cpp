#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <omp.h>
#include <string.h>
#include <winuser.h>
#include <algorithm>
//#include <stdio.h>
//#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <direct.h>
#include "stl.h"
#include "my_image.h"

//#define Pi 3.14159265358979323846264338328

using namespace std;
// 配置
const int TARGET_W = 1440;
const int TARGET_H = 2560;
const UINT_PTR TIMER_ID = 1;
//x-屏幕正对方向 y-屏幕水平向右方向 z-屏幕竖直向上方向 
// 全局变量
HWND g_hwnd = NULL;
POINT mouse_start;
HBITMAP g_hDIBBitmap = NULL;
void* g_pDIBPixelData = nullptr; // 24位数据指针
vector<string> skybox_image;
vector<string> planet_image;
int skybox_num = 0, planet_num = 0, windowX = 0, windowY = 0;
vec3d screen=vec3d(576,-720,1280), basic_screen_vec = vec3d(750,-720,1280);;//默认眼睛离屏幕14400像素 最大深度576像素 
float eye_ori, eye_bias, screen_vec, screen_vec_ori, eye_ori_planet, planet_center, planet_size, base_brightness=40;
double screen_vec_max, screen_vec_min;
image planet, skybox, reflect, old_bg, old_planet, skybox_faces[6], planet_faces[6], reflect_faces[6], reflect_render;
vec3d bg[1440*2560*3], sightline[1440*2560*3], fg[1440*2560*3], sightline_p[1440*2560*3], sightline_r[1440*2560*3], reflect_vec[1440*2560*3];
double stan=0.1011,pianyi=0,jiange=19.6138/3,jianges,jiangel,jgnow;//"lineNumber":19.6138,"obliquity":0.1011,
float xiangwei[1440*2560*3];
int ylzx=202, ylzy=2000, ylzp, tmpy, self_rotation=0, linebytes, rota_bias;
image test_img, loading;
POINT mousenow,planetp[1440*2560*3];
char appdata_path[512],file_path[512],bg_path[512];
bool update_skybox=false, got_para,ylz=false, update_planet=false;
bool mask_skybox[1440*2560*3], mask_planet[1440*2560*3];
stlfile sight, sight_p;//f0v0 newx f0v1 newy f0v2 newz
struct Sphere{
	vec3d center;
	float radius;
	Sphere(vec3d c = vec3d(0,0,0), float r = 1) : center(c), radius(r) {}
	
	// 光线球体求交：返回相交距离 t。如果不相交返回 -1
	float intersect_front(const vec3d& orig, const vec3d& dir) const {//orig 光线原点  dir 光线方向 
		vec3d oc = orig - center;// 光线原点到球心 
		vec3d tmp=oc*dir;
		float dist = tmp.dot(tmp)/(dir.dot(dir));
		if(dist>radius * radius)return -1.0f;
		float a = dir.dot(dir);
		if(a<1e-6)return -1.0f;
		float b = 2.0f * oc.dot(dir);
		float c = oc.dot(oc) - radius * radius;
		float discriminant = b * b - 4 * a * c;
		if (discriminant < 0) return -fabsf(b/a);
		// 返回最近的交点
		return (-b - sqrt(discriminant)) / (2.0f * a);
	}
	float intersect_behind(const vec3d& orig, const vec3d& dir) const {//orig 光线原点  dir 光线方向 
		vec3d oc = orig - center;// 光线原点到球心 
		float a = dir.dot(dir);
		if(a<1e-6)return -1.0f;
		float b = 2.0f * oc.dot(dir);
		float c = oc.dot(oc) - radius * radius;
		float discriminant = b * b - 4 * a * c;
		if (discriminant < 0) return -fabsf(b/a);
		// 返回最近的交点
		return (-b + sqrt(discriminant)) / (2.0f * a);
	}
};
Sphere skybox_sphere,planet_sphere;
// ---查找特定分辨率的显示器---
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	MONITORINFO mi = { sizeof(MONITORINFO) };
	if (GetMonitorInfo(hMonitor, &mi)) {
		int width = mi.rcMonitor.right - mi.rcMonitor.left;
		int height = mi.rcMonitor.bottom - mi.rcMonitor.top;
		// 如果匹配 1440x2560
		if (width == TARGET_W && height == TARGET_H) {
			POINT* p = (POINT*)dwData;
			p->x = mi.rcMonitor.left;
			p->y = mi.rcMonitor.top;
			return FALSE; // 找到即停止
		}
	}
	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
float fast_atan2(float y, float x) {
    float PI = 3.1415926535f;
    float HALF_PI = 1.5707963267f;

    if (x == 0.0f) {
        if (y > 0.0f) return HALF_PI;
        if (y == 0.0f) return 0.0f;
        return -HALF_PI;
    }

    // 计算 z = y/x
    float z = y / x;
    float atan;

    // 逼近公式使用范围是 |z| < 1
    if (std::abs(z) < 1.0f) {
        // 利用公式: atan(z) ≈ z / (1 + 0.28087 * z^2)
        atan = z / (1.0f + 0.28087f * z * z);
        
        if (x < 0.0f) {
            if (y < 0.0f) return atan - PI;
            return atan + PI;
        }
    } else {
        // 对于 |z| > 1，利用公式: atan(z) = PI/2 - atan(1/z)
        atan = HALF_PI - (z / (z * z + 0.28087f));
        
        if (y < 0.0f) return atan - PI;
    }
    return atan;
}
// 假设每张 Face 的大小是 FaceSize * FaceSize
inline void SampleCubeMap(vec3d v, int FaceSize, int& faceIdx, int& u, int& vv) {
	float absX = fabsf(v.x), absY = fabsf(v.y), absZ = fabsf(v.z);
	float maxAxis, uc, vc;
	if (absX >= absY && absX >= absZ) {
		maxAxis = absX;
		if(v.x > 0){
			faceIdx = 0;
			uc = -v.y;
			vc = -v.z;
		}else{
			faceIdx = 1;
			uc = v.y;
			vc = -v.z;
		}
	} else if (absY >= absX && absY >= absZ) {
		maxAxis = absY;
		if(v.y > 0){
			faceIdx = 2;
			uc = v.x;
			vc = -v.z;
		}else{
			faceIdx = 3;
			uc = -v.x;
			vc = -v.z;
		}
	} else {
		maxAxis = absZ;
		if(v.z > 0){
			faceIdx = 4;
			uc = v.y;
			vc = -v.x;
		}else{
			faceIdx = 5;
			uc = v.y;
			vc = v.x;
		}
	}
	// 归一化到 [0, FaceSize-1]
	// 性能优化：用乘法代替除以 2.0f
	float invMax = 0.5f / maxAxis;
	u = (int)((uc * invMax + 0.5f) * (FaceSize - 1));
	vv = (int)((vc * invMax + 0.5f) * (FaceSize - 1));
}
RGBPixel SamplePanorama(image pano, float u, float v) {
	if(u<0)u=0;
	float x = u * pano.wid;
	float y = v * pano.hei;
	int x0 = ((int)x)%pano.wid;
	int y0 = min(max((int)y,0), pano.hei-1);
	int x1 = (x0 + 1)%pano.wid;
	int y1 = min(y0 + 1, pano.hei-1);

	float dx = x - floor(x0);
	float dy = max(y - y0,0.0f);

	RGBPixel res;
	for (int i = 0; i < 3; ++i) { // 遍历 RGB
		float p00 = pano.p[(y0 * pano.wid + x0) * 3 + i];
		float p10 = pano.p[(y0 * pano.wid + x1) * 3 + i];
		float p01 = pano.p[(y1 * pano.wid + x0) * 3 + i];
		float p11 = pano.p[(y1 * pano.wid + x1) * 3 + i];

		float top = p00 + dx * (p10 - p00);
		float bottom = p01 + dx * (p11 - p01);
		float finalVal = top + dy * (bottom - top);
		switch(i){
			case 0: res.R = (unsigned char)finalVal; break;
			case 1: res.G = (unsigned char)finalVal; break;
			case 2: res.B = (unsigned char)finalVal; break;
		}
	}
	return res;
}
void ConvertPanoToCubemap(image pano, image* faces, bool reverse){//reverse ? skybox : planet
	//int faceSize = faces[f].wid;
	omp_set_num_threads(12);
	#pragma omp parallel for shared(reverse) schedule(dynamic)
	for (int f = 0; f < 6; ++f) {
		for (int j = 0; j < faces[f].wid; ++j) {
			for (int i = 0; i < faces[f].wid; ++i) {
				// 将像素坐标映射到 [-1, 1]
				float a = 2.0f * i / (faces[f].wid - 1) - 1.0f;
				float b = 2.0f * j / (faces[f].wid - 1) - 1.0f;
				float x, y, z;
				// 根据你的坐标系定义映射 (X前, Y右, Z上)
				// 注意：j从0到faceSize通常对应屏幕从上往下，所以z方向需要取反
				switch (f) {
					case 0: x =  1.0f; y = -a;	z = -b;	break; // 前 (+X)
					case 1: x = -1.0f; y =  a;	z = -b;	break; // 后 (-X)
					case 2: x =  a;	y =  1.0f; z = -b;	break; // 右 (+Y)
					case 3: x = -a;	y = -1.0f; z = -b;	break; // 左 (-Y)
					case 4: x = -b;	y =  a;	z =  1.0f; break; // 上 (+Z)
					case 5: x =  b;	y =  a;	z = -1.0f; break; // 下 (-Z)
				}
				// 计算单位向量的方向角
				float r = sqrt(x * x + y * y + z * z);
				//float theta = atan2(-y, -x);		 // 经度 [-PI, PI]
				float theta = fast_atan2(-y, -x);		 // 经度 [-PI, PI]
				float phi = acos(z / r);		   // 纬度 [0, PI]
				// 映射回全景图的 UV 坐标 (0.0 - 1.0)
				float u = (theta + Pi) / (2.0f * Pi);
				float v = phi / Pi;
				if (reverse)u=1.0f-u;
				// 采样并写入内存
				RGBPixel c;
				c = SamplePanorama(pano, u, v);
				int dstIdx = (j * faces[f].wid + i) * 3;
				faces[f].p[dstIdx + 0] = c.R;
				faces[f].p[dstIdx + 1] = c.G;
				faces[f].p[dstIdx + 2] = c.B;
			}
		}
	}
	
}
void makecubemap(){
	ConvertPanoToCubemap(skybox, skybox_faces, true);
	ConvertPanoToCubemap(planet, planet_faces, false);
}
void LoadFilesFromSubfolder(const std::string& baseDir, const std::string& subFolder, std::vector<std::string>& targetVector) {
	targetVector.clear();
	string searchPath = baseDir + "\\" + subFolder + "\\*";
	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
	if (hFind == INVALID_HANDLE_VALUE)return;
	do {
		// 排除目录本身和子文件夹
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			std::string fileName = findData.cFileName;
			// 检查后缀名
			size_t dotPos = fileName.find_last_of('.');
			if (dotPos != std::string::npos) {
				std::string ext = fileName.substr(dotPos);
				// 统一转为小写进行比较，兼容 .JPG 和 .jpg
				if (_stricmp(ext.c_str(), ".jpg") == 0 || 
					_stricmp(ext.c_str(), ".jpeg") == 0 || 
					_stricmp(ext.c_str(), ".png") == 0 || 
					_stricmp(ext.c_str(), ".bmp") == 0) {// 存储完整路径：程序目录\子目录\文件名
					targetVector.push_back(baseDir + "\\" + subFolder + "\\" + fileName);
				}
			}
		}
	} while (FindNextFile(hFind, &findData));
	FindClose(hFind);
}
void get_images() {
	// 1. 获取程序自身所在的完整路径
	char exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH);
	// 2. 去掉文件名，只保留目录部分
	/*
	PathRemoveFileSpec(exePath); 
	*/
	int i;
	for(i=strlen(exePath);exePath[i]!='\\'&&i>0;i--);
	exePath[i]='\0';
	std::string baseDir = exePath;
	
	// 3. 分别读取两个文件夹
	LoadFilesFromSubfolder(baseDir, "skybox", skybox_image);
	LoadFilesFromSubfolder(baseDir, "planet", planet_image);
	
	// 调试信息（可选）：输出读取到的数量
	//char debugMsg[256];
	//sprintf(debugMsg, "Skybox: %zu, Planet: %zu", skybox.size(), planet.size());
	//MessageBox(NULL, debugMsg, "Info", MB_OK);
}
void render_bg_image();

// SH 基函数系数常数
const float SH_C0 = 0.282095f; 
const float SH_C1 = 0.488603f; 
const float SH_C2 = 1.092548f; 
const float SH_C3 = 0.315392f; 
const float SH_C4 = 0.546274f; 
// 漫反射卷积核权重 (Lambertian weights)
const float A0 = Pi;
const float A1 = (2.0f * Pi) / 3.0f;
const float A2 = Pi / 4.0f;
// 辅助函数：根据方向向量计算 9 个 SH 基函数的值
void GetSHBasis(vec3d n, float* Y) {
	Y[0] = SH_C0;
	Y[1] = SH_C1 * n.y;
	Y[2] = SH_C1 * n.z;
	Y[3] = SH_C1 * n.x;
	Y[4] = SH_C2 * n.x * n.y;
	Y[5] = SH_C2 * n.y * n.z;
	Y[6] = SH_C3 * (3.0f * n.z * n.z - 1.0f);
	Y[7] = SH_C2 * n.x * n.z;
	Y[8] = SH_C4 * (n.x * n.x - n.y * n.y);
}
void make_diffuse_reflect() {
	// 1. 初始化系数
	vec3d sh_coeffs[9];
	for (int i = 0; i < 9; i++) sh_coeffs[i] = vec3d(0, 0, 0);
	float totalWeight = 0.0f;
	omp_set_num_threads(12);
	// --- 第一步：投影 (增加多线程优化) ---
	// 使用线程私有变量
	#pragma omp parallel
	{
		// 每个线程创建自己的私有累加器
		vec3d private_sh[9];
		for (int i = 0; i < 9; i++) private_sh[i] = vec3d(0, 0, 0);
		float private_weight = 0.0f;

		#pragma omp for nowait // 这里的 nowait 允许线程完成后直接进入汇总阶段
		for (int y = 0; y < skybox.hei; y++) {
			float theta = (y + 0.5f) / skybox.hei * Pi;
			float weight = sin(theta);

			for (int x = 0; x < skybox.wid; x++) {
				float phi = (float)x / skybox.wid * 2.0f * Pi;

				// 保持你要求的坐标系：Z轴向上
				vec3d dir(sin(theta) * cos(phi), -sin(theta) * sin(phi), cos(theta));

				int idx = (y * skybox.wid + x) * skybox.channel;
				vec3d color(
					skybox.p[idx + 0] / 255.0f,
					skybox.p[idx + 1] / 255.0f,
					skybox.p[idx + 2] / 255.0f
				);

				float Y[9];
				GetSHBasis(dir, Y);

				for (int i = 0; i < 9; i++) {
					private_sh[i].x += color.x * Y[i] * weight;
					private_sh[i].y += color.y * Y[i] * weight;
					private_sh[i].z += color.z * Y[i] * weight;
				}
				private_weight += weight;
			}
		}

		// 汇总各线程的结果（临界区）
		#pragma omp critical
		{
			for (int i = 0; i < 9; i++) {
				sh_coeffs[i].x += private_sh[i].x;
				sh_coeffs[i].y += private_sh[i].y;
				sh_coeffs[i].z += private_sh[i].z;
			}
			totalWeight += private_weight;
		}
	}

	// 归一化并应用卷积权重
	float factor = (4.0f * Pi) / totalWeight;
	for (int i = 0; i < 9; i++) {
		float Al = (i == 0) ? A0 : (i < 4 ? A1 : A2);
		sh_coeffs[i] = sh_coeffs[i] * (factor * Al);
	}

	// --- 第二步：重建 (高度并行) ---
	reflect.free();
	reflect.wid = 1024;
	reflect.hei = 512;
	reflect.channel = 3;
	reflect.space_apply();
	omp_set_num_threads(12);
	#pragma omp parallel for schedule(dynamic) // 动态调度处理可能的不均匀负载
	for (int y = 0; y < reflect.hei; y++) {
		float theta = (y + 0.5f) / reflect.hei * Pi;
		for (int x = 0; x < reflect.wid; x++) {
			float phi = (float)x / reflect.wid * 2.0f * Pi;

			vec3d n(sin(theta) * cos(phi), -sin(theta) * sin(phi), cos(theta));

			float Y[9];
			GetSHBasis(n, Y);

			vec3d irradiance(0, 0, 0);
			for (int i = 0; i < 9; i++) {
				irradiance.x += sh_coeffs[i].x * Y[i];
				irradiance.y += sh_coeffs[i].y * Y[i];
				irradiance.z += sh_coeffs[i].z * Y[i];
			}

			int out_idx = (y * reflect.wid + x) * reflect.channel;
			// 按照你要求的映射逻辑：63 + irradiance * 192
			reflect.p[out_idx + 0] = (unsigned char)std::min(255.0f, std::max(0.0f, base_brightness + irradiance.x * (255-base_brightness)));
			reflect.p[out_idx + 1] = (unsigned char)std::min(255.0f, std::max(0.0f, base_brightness + irradiance.y * (255-base_brightness)));
			reflect.p[out_idx + 2] = (unsigned char)std::min(255.0f, std::max(0.0f, base_brightness + irradiance.z * (255-base_brightness)));
		}
	}
	ConvertPanoToCubemap(reflect, reflect_faces, true);
}

// ---取图片 ---
void LoadSkyboxImage(int index) {
	if (skybox_image.empty()) return;
	skybox.free();
	if(!skybox.read_3bytes(skybox_image[index].c_str())){
		skybox.wid=1;
		skybox.hei=1;
		skybox.channel=3;
		skybox.space_apply();
	}
	//ConvertPanoToCubemap(skybox, skybox_faces, true);
	make_diffuse_reflect(); 
}
void LoadPlanetImage(int index) {
	if (planet_image.empty()) return;
	planet.free();
	if(!planet.read_3bytes(planet_image[index].c_str())){
		planet.wid=1;
		planet.hei=1;
		planet.channel=3;
		planet.space_apply();
		planet.p[0]=255;
		planet.p[1]=255;
		planet.p[2]=255;
	}
	//ConvertPanoToCubemap(planet, planet_faces, false);
	linebytes = planet.wid*3;
	rota_bias = max(planet.wid/2000,1);
}
void MakeSightLine(){
	omp_set_num_threads(12);
	#pragma omp parallel for shared(sightline,screen,eye_ori,skybox_sphere) schedule(dynamic)
	for(int i=0;i<2560;i++){
		for(int j=0;j<1440;j++){
			for(int k=0;k<3;k++){
				vec3d eye_start, tmp;
				float ll;
				eye_start = vec3d(eye_ori,(xiangwei[(i*1440+j)*3+k]-0.5)*eye_bias*2,0);
				tmp = vec3d(screen.x,screen.y*(720-j-k/3)/720,screen.z*(1279.5-i)/1279.5) - eye_start;
				ll=skybox_sphere.intersect_behind(eye_start,tmp);
				if(ll<1)mask_skybox[(i*1440+j)*3+k]=false;
				else {
					mask_skybox[(i*1440+j)*3+k]=true;
					//sightline[(i*1440+j)*3+k] = vec3d(screen.x-eye_ori,screen.y*(720-j-k/3)/720,screen.z*(1280-j)/1280);
					sightline[(i*1440+j)*3+k] = eye_start + tmp * ll;
				}
				eye_start.x = eye_ori_planet;
				tmp = vec3d(screen.x,screen.y*(720-j-k/3)/720,screen.z*(1279.5-i)/1279.5) - eye_start;
				ll=planet_sphere.intersect_front(eye_start,tmp);
				if(ll<0)mask_planet[(i*1440+j)*3+k]=false;
				else{
					mask_planet[(i*1440+j)*3+k]=true;
					sightline_p[(i*1440+j)*3+k] = eye_start + tmp * ll - planet_sphere.center;
					sightline_r[(i*1440+j)*3+k] = tmp - 2 * (tmp.dot(sightline_p[(i*1440+j)*3+k])/(sightline_p[(i*1440+j)*3+k].dot(sightline_p[(i*1440+j)*3+k])))*sightline_p[(i*1440+j)*3+k];
					ll=skybox_sphere.intersect_behind(sightline_p[(i*1440+j)*3+k],sightline_r[(i*1440+j)*3+k]);
					sightline_r[(i*1440+j)*3+k] = sightline_p[(i*1440+j)*3+k] + ll * sightline_r[(i*1440+j)*3+k]; 
				}
			}
		}
	}
}
void render_bg_vec(){
	//MakeSightLine();
	omp_set_num_threads(12);
	#pragma omp parallel for shared(bg,sightline,sight) schedule(dynamic)
	for(int i=0;i<2560;i++){
		for(int j=0;j<1440;j++){
			for(int k=0;k<3;k++){
				if(mask_skybox[(i*1440+j)*3+k]){
					vec3d tmp=sightline[(i*1440+j)*3+k];
					bg[(i*1440+j)*3+k].x = tmp.x * sight.f[0].vertex[0].x + tmp.y * sight.f[0].vertex[1].x + tmp.z * sight.f[0].vertex[2].x;
					bg[(i*1440+j)*3+k].y = tmp.x * sight.f[0].vertex[0].y + tmp.y * sight.f[0].vertex[1].y + tmp.z * sight.f[0].vertex[2].y;
					bg[(i*1440+j)*3+k].z = tmp.x * sight.f[0].vertex[0].z + tmp.y * sight.f[0].vertex[1].z + tmp.z * sight.f[0].vertex[2].z;
					tmp=sightline_r[(i*1440+j)*3+k];
					reflect_vec[(i*1440+j)*3+k].x = tmp.x * sight.f[0].vertex[0].x + tmp.y * sight.f[0].vertex[1].x + tmp.z * sight.f[0].vertex[2].x;
					reflect_vec[(i*1440+j)*3+k].y = tmp.x * sight.f[0].vertex[0].y + tmp.y * sight.f[0].vertex[1].y + tmp.z * sight.f[0].vertex[2].y;
					reflect_vec[(i*1440+j)*3+k].z = tmp.x * sight.f[0].vertex[0].z + tmp.y * sight.f[0].vertex[1].z + tmp.z * sight.f[0].vertex[2].z;
				}
			}
		}
	}
}
void render_fg_vec(){
	//MakeSightLine();
	omp_set_num_threads(12);
	#pragma omp parallel for shared(bg,sightline,sight) schedule(dynamic)
	for(int i=0;i<2560;i++){
		for(int j=0;j<1440;j++){
			for(int k=0;k<3;k++){
				int loc = (i*1440+j)*3+k;
				if(mask_planet[loc]){
					vec3d tmp=sightline_p[loc];
					fg[loc].x = tmp.x * sight_p.f[0].vertex[0].x + tmp.y * sight_p.f[0].vertex[1].x + tmp.z * sight_p.f[0].vertex[2].x;
					fg[loc].y = tmp.x * sight_p.f[0].vertex[0].y + tmp.y * sight_p.f[0].vertex[1].y + tmp.z * sight_p.f[0].vertex[2].y;
					fg[loc].z = tmp.x * sight_p.f[0].vertex[0].z + tmp.y * sight_p.f[0].vertex[1].z + tmp.z * sight_p.f[0].vertex[2].z;
					planetp[loc].x = ((int)((1 + (fast_atan2(-fg[loc].y,-fg[loc].x) + Pi) / (2.0f * Pi)) * planet.wid) % planet.wid)*3+k;
					planetp[loc].y = max(min(planet.hei-1,(int)(fast_atan2(sqrt(fg[loc].y*fg[loc].y+fg[loc].x*fg[loc].x),fg[loc].z)/Pi*planet.hei)),0)*linebytes;
				}
			}
		}
	}
}
void render_bg_image(){
	omp_set_num_threads(12);
	#pragma omp parallel for shared(bg,old_bg,skybox) schedule(dynamic, 1024)
	for (int i = 0; i < 3686400; ++i) {
		for(int k=0; k<3; k++){
			//立方体采样 (核心加速点：无三角函数)
			if(mask_skybox[i*3+k]){
				int fIdx, u, v;
				//像素拷贝
				if(mask_planet[i*3+k]){
					SampleCubeMap(reflect_vec[i*3+k], reflect_faces[0].wid , fIdx, u, v);
					reflect_render.p[i * 3 + 2 - k] = reflect_faces[fIdx].p[(v * reflect_faces[fIdx].wid + u) * 3 + k];//调换RGB为BGR 
				}
				else {
					/*
					SampleCubeMap(bg[i*3+k], skybox_faces[0].wid , fIdx, u, v);
					old_bg.p[i * 3 + 2 - k] = skybox_faces[fIdx].p[(v * skybox_faces[fIdx].wid + u) * 3 + k];//调换RGB为BGR 
					*/
					u = ((int)((1 + (fast_atan2(-bg[i*3+k].y,-bg[i*3+k].x) + Pi) / (2.0f * Pi)) * skybox.wid) % skybox.wid)*3+k;
					v = max(min(skybox.hei-1,(int)(fast_atan2(sqrt(bg[i*3+k].y*bg[i*3+k].y+bg[i*3+k].x*bg[i*3+k].x),bg[i*3+k].z)/Pi*skybox.hei)),0)*skybox.wid*3;
					old_bg.p[i * 3 + 2 - k] = skybox.p[v+u];
				}
			}
			else old_bg.p[i * 3 + 2 - k] = 0;
		}
	}
	/*
	#pragma omp parallel for shared(bg,old_bg,skybox) schedule(dynamic)
	for(int i=0;i<2560;i++){
		for(int j=0;j<1440;j++){
			for(int k=0;k<3;k++){
				int p_x=(int)((Pi-atan2(-bg[(i*1440+j)*3+k].y,-bg[(i*1440+j)*3+k].x))/(2*Pi)*skybox.wid);
				int p_y=(int)((atan2(sqrt(bg[(i*1440+j)*3+k].x*bg[(i*1440+j)*3+k].x+bg[(i*1440+j)*3+k].y*bg[(i*1440+j)*3+k].y),bg[(i*1440+j)*3+k].z))/Pi*skybox.hei);
				if(p_x<0)p_x+=skybox.wid;
				else if(p_x>=skybox.wid)p_x-=skybox.wid;
				if(p_y<0)p_y=0;
				else if(p_y>=skybox.hei)p_y=skybox.hei-1;
				old_bg.p[(i*old_bg.wid+j)*3+2-k]=skybox.p[(p_y*skybox.wid+p_x)*3+k];//调换RGB为BGR 
			}
		}
	}
	*/
}
void render_fg_image(){
	omp_set_num_threads(12);
	#pragma omp parallel for shared(bg,old_bg,skybox) schedule(dynamic, 1024)
	for (int i = 0; i < 3686400; ++i) {
		for(int k=0; k<3; k++){
			//立方体采样：无三角函数
			if(mask_planet[i*3+k]){
				/*
				int fIdx, u, v;
				SampleCubeMap(fg[i*3+k], planet_faces[0].wid , fIdx, u, v);
				//像素拷贝
				old_bg.p[i * 3 + 2 - k] = planet_faces[fIdx].p[(v * planet_faces[fIdx].wid + u) * 3 + k] * reflect_render.p[i * 3 + 2 - k] / 255;//调换RGB为BGR 
				*/
				planetp[i*3+k].x = (planetp[i*3+k].x + linebytes + self_rotation*3*rota_bias) % linebytes;
				old_bg.p[i * 3 + 2 - k] = planet.p[planetp[i*3+k].y + planetp[i*3+k].x] * reflect_render.p[i * 3 + 2 - k] / 255;
			}
		}
	}
}
void ptpt(){
	FILE *fop=fopen("test.txt","w");
	for(int i=0;i<2560;i++){
		for(int j=720;j<721;j++){
			for(int k=0;k<1;k++){
				int p_x=(int)((Pi-atan2(-bg[(i*1440+j)*3+k].y,-bg[(i*1440+j)*3+k].x))/(2*Pi)*skybox.wid);
				int p_y=(int)((atan2(sqrt(bg[(i*1440+j)*3+k].x*bg[(i*1440+j)*3+k].x+bg[(i*1440+j)*3+k].y*bg[(i*1440+j)*3+k].y),bg[(i*1440+j)*3+k].z))/Pi*skybox.hei);
				if(p_x<0)p_x+=skybox.wid;
				else if(p_x>=skybox.wid)p_x-=skybox.wid;
				if(p_y<0)p_y=0;
				else if(p_y>=skybox.hei)p_y=skybox.hei-1;
				fprintf(fop,"%4d %4d %d (%f,%f,%f) (%d,%d)\n",i,j,k,sightline[(i*1440+j)*3+k].x,sightline[(i*1440+j)*3+k].y,sightline[(i*1440+j)*3+k].z,p_x,p_y);
			}
		}
	}
	fclose(fop);
	old_bg.save("render.jpg");
}
// ---更新像素---
void UpdateBitmapPixels() {
	if (!g_pDIBPixelData) return;
	// 强制转换为 unsigned char 指针
	unsigned char* pixels = (unsigned char*)g_pDIBPixelData;
	
	if(self_rotation!=0&&!update_planet){
		//update_planet=true;
		sight_p.rotate(vec3d(0,0,1),2*rota_bias*self_rotation*Pi/planet.wid);
		render_fg_image();
	}
	
	if(update_skybox){
		render_bg_vec();
		render_fg_vec();
		render_bg_image();
		render_fg_image();
		update_skybox=false;
	}
	else if(update_planet){
		render_fg_vec();
		render_fg_image();
		update_planet=false;
	}

	// 这里是 3 通道处理逻辑 (BGR 顺序)
	memcpy(pixels,old_bg.p,1440*2560*3);
	/*
	for (int i = 0; i < TARGET_W * TARGET_H * 3; i += 1) {
		pixels[i] = old_bg.p[i]; // B通道
	}
	*/
}
void measure_para(){
	unsigned char* pixels = (unsigned char*)g_pDIBPixelData;
	if(ylz){
		if(ylzp==0){
			test_img.clear();
			test_img.draw_line(0,2559,ylzx,2559-ylzy,RGBPixel(0,255,0));
			memcpy(pixels,test_img.p,1440*2560*3);
		}
		else if(ylzp==1){
			memcpy(pixels,test_img.p,1440*2560*3);
			GetCursorPos(&mousenow);
			tmpy = mousenow.y - windowY;
			int tmpys=max(tmpy-4,0),tmpyl=min(tmpy+5,2560);
			//tmpy=tmpy*lsbmp.wid;
			tmpys=tmpys*test_img.wid;
			tmpyl=tmpyl*test_img.wid;
			for(int j=tmpys;j<tmpyl;j++)
			{
				pixels[j*3+0] = 255 - pixels[j*3+0];
				pixels[j*3+1] = 255 - pixels[j*3+1];
				pixels[j*3+2] = 255 - pixels[j*3+2];
			}
		}
	}
	else{
		memcpy(pixels,test_img.p,1440*2560*3);
		GetCursorPos(&mousenow);
		tmpy = mousenow.y - windowY;
		if(tmpy<280)tmpy=280;
		if(tmpy>=2280)tmpy=2279;
		for(int i=0;i<75;i++)
		{
			int starty=tmpy+i;
			if(starty>=2280)starty-=2000;
			for(int j=0;j<680;j++)if(pixels[(starty*test_img.wid+j)*3+1]<151-2*i)pixels[(starty*test_img.wid+j)*3+1]=150-2*i;
			for(int j=760;j<1440;j++)if(pixels[(starty*test_img.wid+j)*3+1]<151-2*i)pixels[(starty*test_img.wid+j)*3+1]=150-2*i;
			starty=tmpy-i;
			if(starty<280)starty+=2000;
			for(int j=0;j<680;j++)if(pixels[(starty*test_img.wid+j)*3+1]<151-2*i)pixels[(starty*test_img.wid+j)*3+1]=150-2*i;
			for(int j=760;j<1440;j++)if(pixels[(starty*test_img.wid+j)*3+1]<151-2*i)pixels[(starty*test_img.wid+j)*3+1]=150-2*i;
		}
		tmpy = 2559 - tmpy;
	}
	// 这里是 3 通道处理逻辑 (BGR 顺序)
	//memcpy(pixels,old_bg.p,1440*2560*3);
	
}
class NamedPipeClient {
private:
	HANDLE hPipe;
	std::string pipeName;
public:
	NamedPipeClient(const std::string& name) : pipeName(name), hPipe(INVALID_HANDLE_VALUE) {}
	~NamedPipeClient(){disconnect();}
	bool connect() {
		hPipe = CreateFileA(
			pipeName.c_str(),		   // 管道名称
			GENERIC_READ | GENERIC_WRITE, // 读写权限
			FILE_SHARE_READ | FILE_SHARE_WRITE, // 共享模式
			NULL,					   // 默认安全属性
			OPEN_EXISTING,			  // 打开已存在的管道
			FILE_ATTRIBUTE_NORMAL,	  // 文件属性
			NULL						// 无模板文件
		);
		
		if (hPipe == INVALID_HANDLE_VALUE)return false;
		
		return true;
	}
	void disconnect() {
		if (hPipe != INVALID_HANDLE_VALUE) {
			CloseHandle(hPipe);
			hPipe = INVALID_HANDLE_VALUE;
		}
	}
	bool sendCommand(const std::string& command, std::string& response) {
		if (hPipe == INVALID_HANDLE_VALUE) {
			//std::cerr << "未连接到管道" << std::endl;
			return false;
		}
		DWORD bytesWritten;
		BOOL writeResult = WriteFile(
			hPipe,					  // 管道句柄
			command.c_str(),			// 要发送的数据
			static_cast<DWORD>(command.length()), // 数据长度
			&bytesWritten,			  // 实际写入的字节数
			NULL						// 非重叠I/O
		);
		if (!writeResult) {
			//std::cerr << "发送命令失败，错误代码: " << GetLastError() << std::endl;
			return false;
		}
		char buffer[2048];
		DWORD bytesRead;
		BOOL readResult = ReadFile(
			hPipe,					  // 管道句柄
			buffer,					 // 接收缓冲区
			sizeof(buffer) - 1,		 // 缓冲区大小
			&bytesRead,				 // 实际读取的字节数
			NULL						// 非重叠I/O
		);
		if (readResult && bytesRead > 0) {
			buffer[bytesRead] = '\0';
			response = std::string(buffer, bytesRead);
			return true;
		} else {
			DWORD error = GetLastError();
			//std::cerr << "读取响应失败，错误代码: " << error << std::endl;
			return false;
		}
	}
};

unsigned char fonts[6][71*7]={//56*71
0X00,0X00,0X07,0XFF,0XE0,0X00,0X00,0X00,0X00,0X7F,0XFF,0XFC,0X00,0X00,0X00,0X01,0XFF,0XFF,0XFF,0X80,0X00,0X00,0X07,0XFF,0XFF,0XFF,0XE0,0X00,0X00,0X1F,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X3F,0XFF,0XFF,0XFF,0XF8,0X00,0X00,0X7F,0XFF,0XFF,0XFF,0XFC,0X00,0X00,0XFF,0XFF,0XFF,0XFF,0XFE,0X00,0X01,0XFF,0XFF,0XC7,0XFF,0XFF,0X00,0X03,0XFF,0XFE,0X00,0XFF,0XFF,0X80,0X03,0XFF,0XF8,0X00,0X7F,0XFF,0X80,0X07,0XFF,0XF0,0X00,0X1F,0XFF,0XC0,0X0F,0XFF,0XE0,0X00,0X1F,0XFF,0XC0,0X0F,0XFF,0XE0,0X00,0X0F,0XFF,0XE0,0X1F,0XFF,0XC0,0X00,0X07,0XFF,0XE0,0X1F,0XFF,0X80,0X00,0X07,0XFF,0XF0,0X1F,0XFF,0X80,0X00,0X03,0XFF,0XF0,0X3F,0XFF,0X80,0X00,0X03,0XFF,0XF0,0X3F,0XFF,0X00,0X00,0X01,0XFF,0XF8,0X7F,0XFF,0X00,0X00,0X01,0XFF,0XF8,0X7F,0XFE,0X00,0X00,0X01,0XFF,0XF8,0X7F,0XFE,0X00,0X00,0X01,0XFF,0XFC,0X7F,0XFE,0X00,0X00,0X00,0XFF,0XFC,0X7F,0XFE,0X00,0X00,0X00,0XFF,0XFC,0XFF,0XFE,0X00,0X00,0X00,0XFF,0XFC,0XFF,0XFE,0X00,0X00,0X00,0XFF,0XFC,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFC,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFC,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0X7F,0XFE,0XFF,0XFC,0X00,0X00,0X00,0X7F,0XFE,0XFF,0XFC,0X00,0X00,0X00,0X7F,0XFE,0XFF,0XFC,0X00,0X00,0X00,0X7F,0XFE,0XFF,0XFC,0X00,0X00,0X00,0X7F,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFE,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFC,0XFF,0XFC,0X00,0X00,0X00,0XFF,0XFC,0XFF,0XFE,0X00,0X00,0X00,0XFF,0XFC,0XFF,0XFE,0X00,0X00,0X00,0XFF,0XFC,0X7F,0XFE,0X00,0X00,0X00,0XFF,0XFC,0X7F,0XFE,0X00,0X00,0X00,0XFF,0XFC,0X7F,0XFE,0X00,0X00,0X00,0XFF,0XFC,0X7F,0XFE,0X00,0X00,0X01,0XFF,0XF8,0X7F,0XFF,0X00,0X00,0X01,0XFF,0XF8,0X3F,0XFF,0X00,0X00,0X01,0XFF,0XF8,0X3F,0XFF,0X00,0X00,0X01,0XFF,0XF8,0X1F,0XFF,0X80,0X00,0X03,0XFF,0XF0,0X1F,0XFF,0X80,0X00,0X03,0XFF,0XF0,0X1F,0XFF,0XC0,0X00,0X07,0XFF,0XE0,0X0F,0XFF,0XC0,0X00,0X0F,0XFF,0XE0,0X0F,0XFF,0XE0,0X00,0X0F,0XFF,0XC0,0X07,0XFF,0XF0,0X00,0X1F,0XFF,0XC0,0X07,0XFF,0XF8,0X00,0X3F,0XFF,0X80,0X03,0XFF,0XFC,0X00,0X7F,0XFF,0X80,0X01,0XFF,0XFF,0X01,0XFF,0XFF,0X00,0X00,0XFF,0XFF,0XFF,0XFF,0XFE,0X00,0X00,0X7F,0XFF,0XFF,0XFF,0XFC,0X00,0X00,0X3F,0XFF,0XFF,0XFF,0XF8,0X00,0X00,0X1F,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X0F,0XFF,0XFF,0XFF,0XE0,0X00,0X00,0X03,0XFF,0XFF,0XFF,0X80,0X00,0X00,0X00,0XFF,0XFF,0XFE,0X00,0X00,0X00,0X00,0X3F,0XFF,0XF8,0X00,0X00,0X00,0X00,0X00,0XFF,0X00,0X00,0X00,
0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0XE0,0X00,0X03,0XFF,0XF0,0X00,0X00,0XFE,0X00,0X03,0XFF,0XF0,0X00,0X00,0XFF,0XC0,0X03,0XFF,0XF0,0X00,0X00,0XFF,0XF0,0X03,0XFF,0XF0,0X00,0X00,0XFF,0XFE,0X03,0XFF,0XF0,0X00,0X00,0XFF,0XFF,0X83,0XFF,0XF0,0X00,0X00,0XFF,0XFF,0XC3,0XFF,0XF0,0X00,0X00,0X7F,0XFF,0XF3,0XFF,0XF0,0X00,0X00,0X1F,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X07,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X03,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X00,0X7F,0XFF,0XFF,0XF0,0X00,0X00,0X00,0X1F,0XFF,0XFF,0XF0,0X00,0X00,0X00,0X0F,0XFF,0XFF,0XF0,0X00,0X00,0X00,0X07,0XFF,0XFF,0XF0,0X00,0X00,0X00,0X03,0XFF,0XFF,0XF0,0X00,0X00,0X00,0X01,0XFF,0XFF,0XF0,0X00,0X00,0X00,0X00,0XFF,0XFF,0XF0,0X00,0X00,0X00,0X00,0X7F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X3F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X3F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X1F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X0F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X0F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X07,0XFF,0XF0,0X00,0X00,0X00,0X00,0X07,0XFF,0XF0,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XF8,0XFF,0XFE,0X00,0X00,0X00,0X00,0X00,0X7F,0XFF,0X00,0X00,0X00,0X00,0X00,0X3F,0XFF,0X80,0X00,0X00,0X00,0X00,0X3F,0XFF,0XC0,0X00,0X00,0X00,0X00,0X1F,0XFF,0XE0,0X00,0X00,0X00,0X00,0X0F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X0F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X07,0XFF,0XFC,0X00,0X00,0X00,0X00,0X03,0XFF,0XFE,0X00,0X00,0X00,0X00,0X01,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0XFF,0XFF,0X80,0X00,0X00,0X00,0X00,0X7F,0XFF,0XC0,0X00,0X00,0X00,0X00,0X3F,0XFF,0XE0,0X00,0X00,0X00,0X00,0X1F,0XFF,0XF8,0X00,0X00,0X00,0X00,0X0F,0XFF,0XFC,0X00,0X00,0X00,0X00,0X07,0XFF,0XFE,0X00,0X00,0X00,0X00,0X03,0XFF,0XFF,0X80,0X00,0X00,0X00,0X00,0XFF,0XFF,0XE0,0X00,0X00,0X00,0X00,0X7F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X3F,0XFF,0XFC,0X00,0X00,0X00,0X00,0X0F,0XFF,0XFE,0X00,0X00,0X00,0X00,0X07,0XFF,0XFF,0X80,0X00,0X00,0X00,0X03,0XFF,0XFF,0XC0,0X00,0X00,0X00,0X00,0XFF,0XFF,0XE0,0X00,0X00,0X00,0X00,0X7F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X1F,0XFF,0XF8,0X00,0X00,0X00,0X00,0X0F,0XFF,0XFC,0X00,0X00,0X00,0X00,0X07,0XFF,0XFE,0X00,0X00,0X00,0X00,0X03,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0XFF,0XFF,0X80,0X00,0X00,0X00,0X00,0X7F,0XFF,0XC0,0X00,0X00,0X00,0X00,0X3F,0XFF,0XC0,0X00,0X00,0X00,0X00,0X1F,0XFF,0XE0,0X00,0X00,0X00,0X00,0X1F,0XFF,0XE0,0X00,0X00,0X00,0X00,0X0F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X07,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF8,0X00,0X00,0X00,0X00,0X03,0XFF,0XF8,0X00,0X00,0X00,0X00,0X03,0XFF,0XF8,0X00,0X00,0X00,0X00,0X01,0XFF,0XF8,0X00,0X00,0X00,0X00,0X01,0XFF,0XF8,0XFF,0XF8,0X00,0X00,0X01,0XFF,0XF8,0XFF,0XFC,0X00,0X00,0X01,0XFF,0XF8,0XFF,0XFC,0X00,0X00,0X01,0XFF,0XF8,0X7F,0XFE,0X00,0X00,0X01,0XFF,0XF8,0X7F,0XFE,0X00,0X00,0X01,0XFF,0XF8,0X3F,0XFF,0X00,0X00,0X03,0XFF,0XF8,0X3F,0XFF,0X00,0X00,0X03,0XFF,0XF8,0X3F,0XFF,0X80,0X00,0X03,0XFF,0XF0,0X1F,0XFF,0XC0,0X00,0X07,0XFF,0XF0,0X1F,0XFF,0XE0,0X00,0X0F,0XFF,0XF0,0X0F,0XFF,0XF0,0X00,0X1F,0XFF,0XE0,0X07,0XFF,0XFC,0X00,0X3F,0XFF,0XE0,0X03,0XFF,0XFF,0X81,0XFF,0XFF,0XC0,0X03,0XFF,0XFF,0XFF,0XFF,0XFF,0X80,0X01,0XFF,0XFF,0XFF,0XFF,0XFF,0X80,0X00,0XFF,0XFF,0XFF,0XFF,0XFE,0X00,0X00,0X3F,0XFF,0XFF,0XFF,0XFC,0X00,0X00,0X1F,0XFF,0XFF,0XFF,0XF8,0X00,0X00,0X07,0XFF,0XFF,0XFF,0XE0,0X00,0X00,0X01,0XFF,0XFF,0XFF,0X80,0X00,0X00,0X00,0X1F,0XFF,0XF8,0X00,0X00,
0X00,0X00,0XFF,0XFF,0XFF,0X80,0X00,0X00,0X07,0XFF,0XFF,0XFF,0XE0,0X00,0X00,0X1F,0XFF,0XFF,0XFF,0XF8,0X00,0X00,0X7F,0XFF,0XFF,0XFF,0XFE,0X00,0X01,0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X03,0XFF,0XFF,0XFF,0XFF,0XFF,0X80,0X0F,0XFF,0XFF,0XFF,0XFF,0XFF,0XC0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XE0,0X3F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0,0X7F,0XFF,0XFE,0X07,0XFF,0XFF,0XF0,0X7F,0XFF,0XC0,0X00,0X7F,0XFF,0XF8,0X7F,0XFE,0X00,0X00,0X1F,0XFF,0XF8,0X7F,0XF8,0X00,0X00,0X07,0XFF,0XF8,0X7F,0XE0,0X00,0X00,0X03,0XFF,0XFC,0X7F,0X80,0X00,0X00,0X03,0XFF,0XFC,0X7E,0X00,0X00,0X00,0X01,0XFF,0XFC,0X7C,0X00,0X00,0X00,0X01,0XFF,0XFC,0X78,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X01,0XFF,0XFC,0X00,0X00,0X00,0X00,0X01,0XFF,0XFC,0X00,0X00,0X00,0X00,0X03,0XFF,0XF8,0X00,0X00,0X00,0X00,0X03,0XFF,0XF8,0X00,0X00,0X00,0X00,0X07,0XFF,0XF0,0X00,0X00,0X00,0X00,0X0F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X3F,0XFF,0XE0,0X00,0X00,0X00,0X01,0XFF,0XFF,0XC0,0X00,0X00,0X00,0X3F,0XFF,0XFF,0XC0,0X00,0X00,0XFF,0XFF,0XFF,0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XFE,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XF8,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XE0,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XC0,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XFC,0X00,0X00,0X00,0X00,0X1F,0XFF,0XFE,0X00,0X00,0X00,0X00,0X01,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0X7F,0XFF,0X80,0X00,0X00,0X00,0X00,0X3F,0XFF,0XC0,0X00,0X00,0X00,0X00,0X1F,0XFF,0XC0,0X00,0X00,0X00,0X00,0X0F,0XFF,0XE0,0X00,0X00,0X00,0X00,0X07,0XFF,0XE0,0X00,0X00,0X00,0X00,0X07,0XFF,0XE0,0X00,0X00,0X00,0X00,0X07,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0X00,0X00,0X00,0X00,0X03,0XFF,0XF0,0XFF,0XF8,0X00,0X00,0X03,0XFF,0XF0,0XFF,0XF8,0X00,0X00,0X03,0XFF,0XF0,0XFF,0XFC,0X00,0X00,0X03,0XFF,0XF0,0X7F,0XFC,0X00,0X00,0X07,0XFF,0XF0,0X7F,0XFE,0X00,0X00,0X07,0XFF,0XF0,0X7F,0XFE,0X00,0X00,0X07,0XFF,0XE0,0X3F,0XFF,0X00,0X00,0X0F,0XFF,0XE0,0X3F,0XFF,0X80,0X00,0X1F,0XFF,0XE0,0X1F,0XFF,0XC0,0X00,0X3F,0XFF,0XC0,0X1F,0XFF,0XF0,0X00,0X7F,0XFF,0XC0,0X0F,0XFF,0XFE,0X03,0XFF,0XFF,0X80,0X07,0XFF,0XFF,0XFF,0XFF,0XFF,0X80,0X03,0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X01,0XFF,0XFF,0XFF,0XFF,0XFE,0X00,0X00,0XFF,0XFF,0XFF,0XFF,0XFC,0X00,0X00,0X3F,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X1F,0XFF,0XFF,0XFF,0XC0,0X00,0X00,0X03,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0X7F,0XFF,0XF8,0X00,0X00,
0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0X00,0X00,0X00,0X00,0X1F,0XFF,0X00,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFC,0X00,0X00,0X1F,0XFF,0X00,0X7F,0XFE,0X00,0X00,0X1F,0XFF,0X00,0X3F,0XFE,0X00,0X00,0X1F,0XFF,0X00,0X1F,0XFF,0X00,0X00,0X1F,0XFF,0X00,0X1F,0XFF,0X80,0X00,0X1F,0XFF,0X00,0X0F,0XFF,0XC0,0X00,0X1F,0XFF,0X00,0X07,0XFF,0XC0,0X00,0X1F,0XFF,0X00,0X03,0XFF,0XE0,0X00,0X1F,0XFF,0X00,0X03,0XFF,0XF0,0X00,0X1F,0XFF,0X00,0X01,0XFF,0XF8,0X00,0X1F,0XFF,0X00,0X00,0XFF,0XF8,0X00,0X1F,0XFF,0X00,0X00,0X7F,0XFC,0X00,0X1F,0XFF,0X00,0X00,0X7F,0XFE,0X00,0X1F,0XFF,0X00,0X00,0X3F,0XFF,0X00,0X1F,0XFF,0X00,0X00,0X1F,0XFF,0X00,0X1F,0XFF,0X00,0X00,0X0F,0XFF,0X80,0X1F,0XFF,0X00,0X00,0X0F,0XFF,0XC0,0X1F,0XFF,0X00,0X00,0X07,0XFF,0XE0,0X1F,0XFF,0X00,0X00,0X03,0XFF,0XE0,0X1F,0XFF,0X00,0X00,0X01,0XFF,0XF0,0X1F,0XFF,0X00,0X00,0X01,0XFF,0XF8,0X1F,0XFF,0X00,0X00,0X00,0XFF,0XFC,0X1F,0XFF,0X00,0X00,0X00,0X7F,0XFE,0X1F,0XFF,0X00,0X00,0X00,0X3F,0XFE,0X1F,0XFF,0X00,0X00,0X00,0X3F,0XFF,0X1F,0XFF,0X00,0X00,0X00,0X1F,0XFF,0X9F,0XFF,0X00,0X00,0X00,0X0F,0XFF,0XDF,0XFF,0X00,0X00,0X00,0X07,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X07,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X03,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X01,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0X7F,0XFF,0XFF,0X00,0X00,0X00,0X00,0X3F,0XFF,0XFF,0X00,0X00,0X00,0X00,0X1F,0XFF,0XFF,0X00,0X00,0X00,0X00,0X1F,0XFF,0XFF,0X00,0X00,0X00,0X00,0X0F,0XFF,0XFF,0X00,0X00,0X00,0X00,0X07,0XFF,0XFF,0X00,0X00,0X00,0X00,0X03,0XFF,0XFF,0X00,0X00,0X00,0X00,0X01,0XFF,0XFF,0X00,0X00,0X00,0X00,0X01,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0X7F,0XFF,0X00,0X00,0X00,0X00,0X00,0X3F,0XFF,0X00,0X00,0X00,0X00,0X00,0X3F,0XFF,0X00,0X00,0X00,0X00,0X00,0X0F,0XFF,0X00,
0X00,0X00,0X0F,0XFF,0XC0,0X00,0X00,0X00,0X01,0XFF,0XFF,0XFE,0X00,0X00,0X00,0X0F,0XFF,0XFF,0XFF,0X80,0X00,0X00,0X3F,0XFF,0XFF,0XFF,0XE0,0X00,0X00,0XFF,0XFF,0XFF,0XFF,0XF8,0X00,0X01,0XFF,0XFF,0XFF,0XFF,0XFC,0X00,0X07,0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X0F,0XFF,0XFF,0XFF,0XFF,0XFF,0X80,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0X80,0X3F,0XFF,0XFF,0XFF,0XFF,0XFF,0XC0,0X7F,0XFF,0XFC,0X1F,0XFF,0XFF,0XE0,0X7F,0XFF,0X80,0X01,0XFF,0XFF,0XE0,0X7F,0XFC,0X00,0X00,0X3F,0XFF,0XF0,0X7F,0XF0,0X00,0X00,0X1F,0XFF,0XF0,0X7F,0XC0,0X00,0X00,0X0F,0XFF,0XF8,0X7F,0X00,0X00,0X00,0X07,0XFF,0XF8,0X7E,0X00,0X00,0X00,0X03,0XFF,0XF8,0X78,0X00,0X00,0X00,0X03,0XFF,0XF8,0X70,0X00,0X00,0X00,0X01,0XFF,0XF8,0X00,0X00,0X00,0X00,0X01,0XFF,0XFC,0X00,0X00,0X00,0X00,0X01,0XFF,0XFC,0X00,0X00,0X00,0X00,0X01,0XFF,0XFC,0X00,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X00,0XFF,0XFC,0X00,0X00,0X00,0X00,0X01,0XFF,0XFC,0X00,0X00,0X00,0X00,0X01,0XFF,0XFC,0X00,0X00,0X00,0X00,0X01,0XFF,0XFC,0X00,0X00,0X00,0X00,0X01,0XFF,0XF8,0X00,0X00,0X00,0X00,0X03,0XFF,0XF8,0X00,0X00,0X00,0X00,0X03,0XFF,0XF8,0X00,0X00,0X00,0X00,0X07,0XFF,0XF8,0X00,0X00,0X00,0X00,0X07,0XFF,0XF0,0X00,0X00,0X00,0X00,0X0F,0XFF,0XF0,0X00,0X00,0X00,0X00,0X3F,0XFF,0XE0,0X00,0X00,0X00,0X00,0X7F,0XFF,0XE0,0X00,0X00,0X00,0X03,0XFF,0XFF,0XC0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0X80,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X1F,0XFF,0XFF,0XFF,0XFF,0XFE,0X00,0X1F,0XFF,0XFF,0XFF,0XFF,0XFC,0X00,0X1F,0XFF,0XFF,0XFF,0XFF,0XF0,0X00,0X1F,0XFF,0XFF,0XFF,0XFF,0XC0,0X00,0X1F,0XFF,0XFF,0XFF,0XFF,0X00,0X00,0X1F,0XFF,0XFF,0XFF,0XF0,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0X80,0X00,0X00,0X00,0X00,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0,0X1F,0XFF,0XFF,0XFF,0XFF,0XFF,0XF0};

void make_test(){//test_img
	bmp24 lsbmp;
	lsbmp.wid=1440;
	lsbmp.hei=2560;
	lsbmp.space_apply();
	for(int i=0;i<2560;i++)for(int j=716;j<724;j++)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
	for(int i=280;i<2280;i++)
	{
		for(int j=680;j<690;j++)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
		for(int j=750;j<760;j++)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
	}
	for(int j=680;j<760;j++)
	{
		for(int i=270;i<280;i++)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
		for(int i=2280;i<2290;i++)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
	}
	for(int kd=0;kd<=40;kd++)
	{
		for(int i=50*kd-5+280;i<50*kd+5+280;i++)
		{
			for(int j=((kd%5)==0)?630:660;j<690;j++)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
			for(int j=((kd%5)==0)?810:780;j>=750;j--)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
		}
	}
	for(int i=280;i<2280;i++)
	{
		for(int j=690;j<750;j++)
		{
			double xw;
			
			xw=i*stan-j+1440.0;
			while(xw>=jiange)xw-=jiange;
			if(((xw/jiange*2000.0+280)>=i-20)&&((xw/jiange*2000.0+280)<i+20))lsbmp.p[i*lsbmp.wid+j].G=255;
			else lsbmp.p[i*lsbmp.wid+j].G=0;
			///xw=i/stan-j+1.0/3+1440.0;
			xw=xw+1.0/3;
			while(xw>=jiange)xw-=jiange;
			if(((xw/jiange*2000.0+280)>=i-20)&&((xw/jiange*2000.0+280)<i+20))lsbmp.p[i*lsbmp.wid+j].B=255;
			else lsbmp.p[i*lsbmp.wid+j].B=0;
			
			//xw=i/stan-j-1.0/3+1440.0;
			xw=xw+jiange-2.0/3;
			while(xw>=jiange)xw-=jiange;
			if(((xw/jiange*2000.0+280)>=i-20)&&((xw/jiange*2000.0+280)<i+20))lsbmp.p[i*lsbmp.wid+j].R=255;
			else lsbmp.p[i*lsbmp.wid+j].R=0;
		}
	}
	for(int kd=0;kd<=40;kd+=5)
	{
		int sty=50*kd-5+280-36,stx0,stx1,n0=kd/10,n1=kd%10,sp=10;
		stx0=810+sp;
		stx1=620-56-56-sp;
		for(int i=0;i<497;i++)
		{
			int st=(i/7+sty)*lsbmp.wid+(i%7)*8+stx0;
			for(int j=0;j<8;j++)if(fonts[n0][i]&(0x80>>j))lsbmp.p[st+j]=RGBPixel(255,255,255);
			st=(i/7+sty)*lsbmp.wid+(i%7)*8+stx0+sp+56;
			for(int j=0;j<8;j++)if(fonts[n1][i]&(0x80>>j))lsbmp.p[st+j]=RGBPixel(255,255,255);
			st=(i/7+sty)*lsbmp.wid+(i%7)*8+stx1;
			for(int j=0;j<8;j++)if(fonts[n0][i]&(0x80>>j))lsbmp.p[st+j]=RGBPixel(255,255,255);
			st=(i/7+sty)*lsbmp.wid+(i%7)*8+stx1+sp+56;
			for(int j=0;j<8;j++)if(fonts[n1][i]&(0x80>>j))lsbmp.p[st+j]=RGBPixel(255,255,255);
		}
		for(int i=50*kd-5+280;i<50*kd+5+280;i++)
		{
			for(int j=((kd%5)==0)?630:660;j<690;j++)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
			for(int j=((kd%5)==0)?810:780;j>=750;j--)lsbmp.p[i*lsbmp.wid+j]=RGBPixel(255,255,255);
		}
	}
	test_img.clone(lsbmp);
	delete[] lsbmp.p;
}
bool pipes(){
	string pipeName = "\\\\.\\pipe\\OpenstageAI_server_pipe";
	string pipeName2 = "\\\\.\\pipe\\Cubestage_server_pipe";
	string command = "getDeivice"; 
	string response;
	bool get_pipes=false;
	NamedPipeClient client(pipeName);
	NamedPipeClient client2(pipeName2);
	if (client.connect()) {
		if (client.sendCommand(command, response)){
			get_pipes=true;
		}
	}
	else if (client2.connect()) {
		if (client2.sendCommand(command, response)){
			get_pipes=true;
		}
	}
	if (get_pipes) 
	{
		//std::cout << response << std::endl;
		int loc=-1;
		loc = response.find("lineNumber");
		sscanf(&response[loc+12],"%lf",&jiange);
		jiange=jiange/3.0;
		loc = response.find("obliquity"); 
		sscanf(&response[loc+11],"%lf",&stan);
		return true;
	}
	else return false;
}
bool get_para(){
	FILE *fpt=fopen(file_path,"r");
	if(fpt!=NULL)
	{
		fscanf(fpt,"%lf\n%lf\n%lf\n",&stan,&jiange,&pianyi);
		fclose(fpt);
		return true;
	}
	else
		return false;
}
void save_data(){
	FILE *fpt=fopen(file_path,"w");
	fprintf(fpt,"%lf\n%lf\n%lf\n",stan,jiange,pianyi);
	fclose(fpt);
}
void reset_bias(){
	got_para=false;
	if(pipes())
	{
		ylz=false;
		make_test();
		//memcpy(old_show.p,lsbmp.p,lsbmp.wid*lsbmp.hei*3);
	}
	else
	{
		ylz=true;
		ylzx=202;
		ylzy=2000;
		ylzp=0;
		test_img.wid=1440;
		test_img.hei=2560;
		test_img.channel=3;
		test_img.space_apply();
		test_img.draw_line(0,2559,ylzx,2559-ylzy,RGBPixel(0,255,0));
		//memcpy(old_show.p,lsbmp.p,lsbmp.wid*lsbmp.hei*3);
		return;
	}
}
void make_mask(){
	for(int i=0;i<2560;i++)
	{
		for(int j=0;j<1440;j++)
		{
			double xw;
			xw=(2559-i)*stan-j+1440.0+pianyi;
			xw=fmod(xw,jiange);
			xiangwei[(i*1440+j)*3+1]=xw/jiange;//G
			xw=xw+1.0/3.0;
			xw=fmod(xw,jiange);
			xiangwei[(i*1440+j)*3]=xw/jiange;//R
			xw=xw+jiange-2.0/3.0;
			xw=fmod(xw,jiange);
			xiangwei[(i*1440+j)*3+2]=xw/jiange;//B
		}
	}
}
void bg_spaceapply(){
	old_bg.wid=1440;
	old_bg.hei=2560;
	old_bg.channel=3;
	old_bg.space_apply();
	reflect_render.wid=1440;
	reflect_render.hei=2560;
	reflect_render.channel=3;
	reflect_render.space_apply();
	sight.space_apply(1);
	sight_p.space_apply(1);
}
void reset(){
	skybox_sphere.center=vec3d(0,0,0);
	skybox_sphere.radius=1;
	//screen=vec3d(576,-720,1280);
	screen_vec = skybox_sphere.intersect_behind(vec3d(-1,0,0),basic_screen_vec);
	screen = screen_vec * basic_screen_vec + vec3d(-1,0,0);
	screen_vec_ori = screen_vec;
	screen_vec_max = 1.0/1470;
	screen_vec_min = screen_vec*0.05;
	eye_ori = screen.x - 20 * screen.y;
	eye_ori_planet = eye_ori;
	eye_bias = (eye_ori-screen.x)*0.3639702343;
	//eye_bias = 0;
	planet_center = -1;
	planet_size = 0.5;
	planet_sphere.radius = -screen.y * planet_size;
	planet_sphere.center = vec3d(screen.x+planet_center*planet_sphere.radius, 0, 0);
	
	sight.f[0].vertex[0]=vec3d(1,0,0);
	sight.f[0].vertex[1]=vec3d(0,1,0);
	sight.f[0].vertex[2]=vec3d(0,0,1);
	sight_p.f[0].vertex[0]=vec3d(1,0,0);
	sight_p.f[0].vertex[1]=vec3d(0,1,0);
	sight_p.f[0].vertex[2]=vec3d(0,0,1);
	MakeSightLine();
	render_bg_vec();
	render_fg_vec();
	render_bg_image();
	render_fg_image();
	self_rotation=0;
}
void init(){
	ylzp=2;
	char *env_path = getenv("APPDATA");
	if (env_path != NULL) {
		snprintf(appdata_path, sizeof(appdata_path), "%s\\Planet_C1", env_path);
		_mkdir(appdata_path);
		snprintf(file_path, sizeof(file_path), "%s\\data", appdata_path);
		//snprintf(bg_path, sizeof(bg_path), "%s\\Planet_C1\\bg.bmp", appdata_path);
	}
	else{
		sprintf(file_path,"data");
		//sprintf(bg_path,"bg.bmp");
	}
	
	if(get_para())
	{
		make_mask();
		got_para=true;
	}
	else
	{
		reset_bias();
	}
	get_images();
	for(int i=0;i<6;i++){
		/*
		skybox_faces[i].wid=4096;
		skybox_faces[i].hei=4096;
		skybox_faces[i].channel=3;
		skybox_faces[i].space_apply();
		planet_faces[i].wid=4096;
		planet_faces[i].hei=4096;
		planet_faces[i].channel=3;
		planet_faces[i].space_apply();
		*/
		reflect_faces[i].wid=256;
		reflect_faces[i].hei=256;
		reflect_faces[i].channel=3;
		reflect_faces[i].space_apply();
	}
	skybox.read_3bytes(skybox_image[0].c_str());
	planet.read_3bytes(planet_image[0].c_str());
	linebytes = planet.wid*3;
	reflect.wid=1;
	reflect.hei=1;
	reflect.channel=3;
	reflect.space_apply();
	make_diffuse_reflect();
	//makecubemap();
	if(got_para){
		bg_spaceapply();
		reset();
	}
	/*
	old_planet.wid=1440;
	old_planet.hei=2560;
	old_planet.channel=3;
	old_planet.space_apply();
	*/
}
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
	POINT pt = { 0, 0 };
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&pt);
	windowX = pt.x; windowY = pt.y;

	init();

	// C. 窗口注册
	WNDCLASS wc = { 0 };
	wc.hbrBackground = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW); 
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = "Planet";
	RegisterClass(&wc);

	// D. 创建无边框最上层窗口
	g_hwnd = CreateWindowEx(WS_EX_TOPMOST, "Planet", NULL, WS_POPUP, pt.x, pt.y, TARGET_W, TARGET_H, NULL, NULL, hInst, NULL);
	// E. 创建 24 位 DIB Section (3通道)
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = TARGET_W;
	bmi.bmiHeader.biHeight = -TARGET_H; // Top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;	  // 关键：3通道
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC hdc = GetDC(g_hwnd);
	g_hDIBBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &g_pDIBPixelData, NULL, 0);
	ReleaseDC(g_hwnd, hdc);

	if (!skybox_image.empty()) LoadSkyboxImage(0);
	UpdateWindow(g_hwnd);
	SetTimer(g_hwnd, TIMER_ID, 10, NULL);
	ShowWindow(g_hwnd, nShow);
	
	if(got_para)UpdateBitmapPixels();
	else measure_para();
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
void measure_length(){
	HSVPixel hsvtmp;
	RGBPixel rgbtmp;
	double jg,pp;
	// 设置线程数为12
	omp_set_num_threads(12);
	// 简单的并行化：只对最外层i循环并行
	#pragma omp parallel for private(hsvtmp, rgbtmp, jg, pp) schedule(dynamic)
	for(int i=0;i<2560;i++)
	{
		jg=(jianges*(2560-i)+jiangel*i)/2560;
		pp=(2559-i)*stan*0.6;
		hsvtmp.S=1;
		hsvtmp.V=1;
		for(int j=0;j<1440;j++)
		{
			hsvtmp.H=fmod(j+pp,jg)*360/jg;
			rgbtmp=hsv2rgb(hsvtmp);
			test_img.p[(i*test_img.wid+j)*3] = rgbtmp.B;
			test_img.p[(i*test_img.wid+j)*3+1] = rgbtmp.G;
			test_img.p[(i*test_img.wid+j)*3+2] = rgbtmp.R;
		}
	}
	
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static POINT ptMouseOffset;
	static BOOL Rdragging = FALSE, Ldragging = FALSE;
	switch (msg) {
		case WM_TIMER:{
			if (wParam == TIMER_ID) {
				if(got_para){
					UpdateBitmapPixels();
					InvalidateRect(hwnd, NULL, FALSE);
				}
				else {
					measure_para();
					InvalidateRect(hwnd, NULL, FALSE);
				}
			}
			break;
		}
		case WM_PAINT:{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			HDC hdcMem = CreateCompatibleDC(hdc);
			SelectObject(hdcMem, g_hDIBBitmap);
			BitBlt(hdc, 0, 0, TARGET_W, TARGET_H, hdcMem, 0, 0, SRCCOPY);
			DeleteDC(hdcMem);
			EndPaint(hwnd, &ps);
			break;
		}
		case WM_LBUTTONDOWN: {
			if(got_para==false)
			{
				if(ylzp==2)
				{
					pianyi=60-(tmpy-280)/50.0;
					pianyi=pianyi*(jiange/40.0);
					bg_spaceapply();
					save_data();
					make_mask();
					reset();
					got_para=true;
				}
				else if(ylzp==1)
				{
					ylz=false;
					GetCursorPos(&ptMouseOffset);
					tmpy = ptMouseOffset.y - windowY;
					jiange=(jianges*(2560-tmpy)+jiangel*tmpy)/2560;
					make_test();
					ylzp=2;
				}
			}
			else {
				Ldragging=true;
				SetCapture(hwnd);
				GetCursorPos(&ptMouseOffset);
				mouse_start.x = ptMouseOffset.x;
				mouse_start.y = ptMouseOffset.y;
			}
			//ptMouseOffset.x = ptMouseOffset.x - rect.left;
			//ptMouseOffset.y = ptMouseOffset.y - rect.top;
			break;
		}
		case WM_RBUTTONDOWN: {
			Rdragging = true;
			//update=true;
			SetCapture(hwnd);
			GetCursorPos(&ptMouseOffset);
			mouse_start.x = ptMouseOffset.x;
			mouse_start.y = ptMouseOffset.y;
		}
		case WM_MOUSEMOVE: {
			if(Rdragging){
				POINT ptCurrentMousePos;
				GetCursorPos(&ptCurrentMousePos);
				int tmp_dx = ptCurrentMousePos.x - mouse_start.x;
				int tmp_dy = ptCurrentMousePos.y - mouse_start.y;
				SetCursorPos(mouse_start.x,mouse_start.y);
				vec3d rotation_axis;
				rotation_axis = sight.f[0].vertex[1]*tmp_dy*Pi/6300 + sight.f[0].vertex[2]*tmp_dx*Pi/6300;
				sight.rotate(rotation_axis,rotation_axis.length()*screen_vec/screen_vec_ori);
				update_skybox=true;
			}
			else if(Ldragging){
				POINT ptCurrentMousePos;
				GetCursorPos(&ptCurrentMousePos);
				int tmp_dx = mouse_start.x - ptCurrentMousePos.x;
				int tmp_dy = mouse_start.y - ptCurrentMousePos.y;
				SetCursorPos(mouse_start.x,mouse_start.y);
				vec3d rotation_axis;
				rotation_axis = sight_p.f[0].vertex[1]*tmp_dy*Pi/3000 + sight_p.f[0].vertex[2]*tmp_dx*Pi/3000;
				sight_p.rotate(rotation_axis,rotation_axis.length()/planet_size);
				update_planet=true;
			}
			break;
		}
		case WM_RBUTTONUP: {
			Rdragging = false;
		}
		case WM_LBUTTONUP: {
			Ldragging = false;
		}
		case WM_KEYDOWN:{
			if (wParam == 'R') {
				reset();
				update_skybox=true;
			}
			else if (wParam == 'W') {
				planet_center = max(-1.0,planet_center-0.05);
				planet_sphere.center = vec3d(screen.x+planet_center*planet_sphere.radius, 0, 0);
				MakeSightLine();
				update_skybox = true;
			}
			else if (wParam == 'S') {
				planet_center = min(0.1,planet_center+0.05);
				planet_sphere.center = vec3d(screen.x+planet_center*planet_sphere.radius, 0, 0);
				MakeSightLine();
				update_skybox = true;
			}
			else if (wParam == 'A') {
				self_rotation=min(100,self_rotation+1);
			}
			else if (wParam == 'D') {
				self_rotation=max(-100,self_rotation-1);
			}
			else if (wParam == 'Q') {
				vec3d rotation_axis;
				rotation_axis = sight_p.f[0].vertex[0]*(-1);
				sight_p.rotate(rotation_axis,0.0872664626);
				update_planet=true;
			}
			else if (wParam == 'E') {
				vec3d rotation_axis;
				rotation_axis = sight_p.f[0].vertex[0];
				sight_p.rotate(rotation_axis,0.0872664626);
				update_planet=true;
			}
			else if (wParam == VK_RIGHT) {
				if(ylz&&ylzp==0)ylzx=min(1439,ylzx+1);
				else {
					skybox_num = (skybox_num + 1) % skybox_image.size();
					LoadSkyboxImage(skybox_num);
					update_skybox=true;
				}
			}
			else if (wParam == VK_LEFT) {
				if(ylz&&ylzp==0)ylzx=max(0,ylzx-1);
				else {
					skybox_num = (skybox_num + skybox_image.size() - 1) % skybox_image.size();
					LoadSkyboxImage(skybox_num);
					update_skybox=true;
				}
			}
			else if(wParam == VK_UP){
				if(ylz&&ylzp==0)ylzy=min(2559,ylzy+1);
				else {
					planet_num = (planet_num + planet_image.size() - 1) % planet_image.size();
					LoadPlanetImage(planet_num);
					update_planet=true;
				}
			}
			else if(wParam == VK_DOWN){
				if(ylz&&ylzp==0)ylzy=max(0,ylzy-1);
				else {
					planet_num = (planet_num + 1) % planet_image.size();
					LoadPlanetImage(planet_num);
					update_planet=true;
				}
			}
			else if(wParam == VK_RETURN){
				if(ylz&&ylzp==0)
				{
					stan=(double)ylzx/ylzy;
					ylzp=1;
					jiange=6.5047745156*sqrt(1+stan*stan);
					jianges=jiange-0.002;
					jiangel=jiange+0.002;
					measure_length();
				}
			}
			else if(wParam == 'P'){
				reset_bias();
			}
			else if (wParam == VK_ESCAPE) PostQuitMessage(0);
			break;
		}
		case WM_MOUSEWHEEL: {
			if(ylz){
				if(ylzp==1)
				{
					GetCursorPos(&mouse_start);
					tmpy = mouse_start.y - windowY;
					jgnow=(jianges*(2560-tmpy)+jiangel*tmpy)/2560;
					if(((short)HIWORD(wParam))>0)
					{
						jianges=(jianges+jgnow)/2;
						jiangel=(jiangel+jgnow)/2;
					}
					else
					{
						jianges=jianges*2-jgnow;
						jiangel=jiangel*2-jgnow;
					}
					measure_length();
				}
			}
			else if(GetKeyState(VK_CONTROL)<0){//ctrl pushed
				if(((short)HIWORD(wParam))>0)//up
				{
					screen_vec = max(screen_vec - screen_vec_min, screen_vec_min*5); 
				}
				else
				{
					screen_vec = min(screen_vec + screen_vec_min, screen_vec_max); 
				}
				if(screen_vec<screen_vec_ori){
					screen = screen_vec * basic_screen_vec + vec3d(-1,0,0);
					eye_ori = screen.x - 20 * screen.y;
					eye_ori_planet = eye_ori;
					eye_bias = (eye_ori-screen.x)*0.3639702343;
					//eye_bias = 0;
				}else{
					float bs = (1 - sqrt(max(0.0,1 - screen_vec * sqrt(720*720+1280*1280))))/(screen_vec*basic_screen_vec.x);
					screen = screen_vec * basic_screen_vec + vec3d(-1,0,0);
					eye_ori = screen.x - 20 * screen.y;
					eye_ori_planet = eye_ori;
					eye_bias = (eye_ori-screen.x)*0.3639702343;
					//eye_bias = 0;
					screen.x = (screen.x+1)*bs-1;
					eye_ori = (eye_ori+1)*bs-1;
				}
				planet_sphere.radius = -screen.y * planet_size;
				planet_sphere.center = vec3d(screen.x+planet_center*planet_sphere.radius, 0, 0);
				MakeSightLine();
				update_skybox = true;
			}
			else{
				if(((short)HIWORD(wParam))>0)//up
				{
					planet_size = min(planet_size + 0.05, 0.8); 
				}
				else
				{
					planet_size = max(planet_size - 0.05, 0.3); 
				}
				planet_sphere.radius = -screen.y * planet_size;
				planet_sphere.center = vec3d(screen.x+planet_center*planet_sphere.radius, 0, 0);
				MakeSightLine();
				update_skybox = true;
			}
			break;
		}
		case WM_DESTROY: PostQuitMessage(0); break;
		default: return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}
