// H.Shirouzu 2018/02/22
//
// Calclate image rotation speed (normal/cache-line-conscious/avx2/avx2-c-l-c)
//
// Usage: fast_rotate [bulksize for normal [bulksize for AVX]]
//
//  1  2  3  4  5  6  7  8         25 17  9  1
//  9 10 11 12 13 14 15 16   -->   26 18 10  2
// 17 18 19 20 21 22 23 24         27 19 11  3
// 25 26 27 28 29 30 31 32         28 20 12  4
//                                 29 21 13  5
//                                 30 22 14  6
//                                 31 23 15  7
//                                 32 24 16  8
//
// Compile:
//  g++ (& clang):
//    define USE_AVX: g++ -O2 -mavx2
//    undef  USE_AVX: g++ -O2
//
//  VC++:
//    need to disable pre-compled header option
//
//  define:
//   USE_AVX2    enable AVX2 (using _mm256_i32gather_epi32)
//   DAT_VERIFY: dump all data (32 * 16 mini-image)
//
//
// Result: (i5-8600K(CoffeeLake) in Win10)
//
//  normal bulksize=128  avx bulksize=128
//     :
//  normal     (1): tick= 164 msec
//  normal_avx2(1): tick= 178 msec
//  cachel     (1): tick=  23 msec
//  cachel_avx2(1): tick=  20 msec
//

#ifdef _WIN32
#include "windows.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <immintrin.h>

#define USE_AVX2
//#define DAT_VERIFY

#ifdef DAT_VERIFY
#define MX (32)
#define MY (16)
#else

#if 1
// original 8K: 7680*4320*4/1024/1024 = 126.56MB
#define MX (1920*4)  // 8K image(=7680)
#define MY (1080*4)  // 8K image(=4320)
#else
// near 8K(below):  
// 7776*4384*4/1024/1024 = 126.36MB
#define MX (32*241)  // near 8K image(=7776)
#define MY (32*127)  // near 8k image(=4384)
#endif

#endif

int BULKSIZE     = 128;
int BULKSIZE_AVX = 128;

typedef unsigned int u_int;  // as pixel data type

void init_img(u_int src[][MX])
{
	for (int y = 0; y < MY; y++) {
		for (int x = 0; x < MX; x++) {
			src[y][x] = y * MX + x;
		}
	}
}

// Rotate X -> Y
void rotate_img(u_int src[][MX], u_int dst[][MY])
{
	u_int	*src_p = &src[0][0];

	for (int y = 0; y < MY; y++) {
		for (int x = 0; x < MX; x++) {
			dst[x][MY - 1 - y] = *src_p++;
		}
	}
}

// Rotate with Cache Line conscious
void rotate_img_cachel(u_int src[][MX], u_int dst[][MY])
{
	int	bulknum = BULKSIZE / sizeof(u_int);

	for (int x = 0; x < MY; x += bulknum) {
		for (int y = 0; y < MX; y++) {
			for (int i=0; i < bulknum; i++) {
				dst[y][x+i] = src[MY-x-i-1][y];
			}
		}
	}
}


#ifdef  USE_AVX2
#define AVX_NUM	(sizeof(__m256i)/sizeof(u_int))
// Rotate with AVX
void rotate_img_avx(u_int src[][MX], u_int dst[][MY])
{
	__m256i	ptn = _mm256_set_epi32(MX*0, MX*1, MX*2, MX*3, MX*4, MX*5, MX*6, MX*7);

	for (int y = 0; y < MX; y++) {
		for (int x = 0; x < MY; x += AVX_NUM) {
			*(__m256i *)&dst[y][x] =
				_mm256_i32gather_epi32((int *)&src[MY-x-AVX_NUM][y], ptn, 4);
		}
	}
}

// Rotate with AVX/Cache Line conscious
void rotate_img_avx_cachel(u_int src[][MX], u_int dst[][MY])
{
	__m256i	ptn    = _mm256_set_epi32(MX*0, MX*1, MX*2, MX*3, MX*4, MX*5, MX*6, MX*7);
	int		bulknum = BULKSIZE_AVX / sizeof(u_int) / AVX_NUM;

	for (int x = 0; x < MY; x += AVX_NUM * bulknum) {
		for (int y = 0; y < MX; y++) {
			for (int i=0; i < bulknum; i++) {
				*(__m256i *)&dst[y][x+AVX_NUM*i] =
					_mm256_i32gather_epi32((int *)&src[MY-x-AVX_NUM*(i+1)][y], ptn, 4);
			}
		}
	}
}
#endif

// Elaps rotation
void calc_rotate(const char *name, int idx, void(*rfunc)(u_int src[][MX], u_int dst[][MY]))
{
#ifdef _WIN32
//	__declspec(align(4096)) static u_int	src[MY][MX];
//	__declspec(align(4096)) static u_int	dst[MX][MY];
	static u_int	(*src)[MX] = (u_int (*)[MX])VirtualAlloc(0, sizeof(u_int)* MX * MY, MEM_COMMIT, PAGE_READWRITE);
	static u_int	(*dst)[MY] = (u_int (*)[MY])VirtualAlloc(0, sizeof(u_int)* MX * MY, MEM_COMMIT, PAGE_READWRITE);
#else
	static u_int	src[MY][MX] __attribute__((aligned(4096)));
	static u_int	dst[MX][MY] __attribute__((aligned(4096)));
#endif

	init_img(src);
	memset(dst, 0, sizeof(dst));

#ifdef DAT_VERIFY
	static bool once = [&]() {
		printf("Orginal bmp\n");
		for (int y=0; y < MY; y++) {
			for (int x=0; x < MX; x++) {
				printf("%4d ", src[y][x]);
			}
			printf("\n");
		}
		printf("\n");
		return	true;
	}();
#endif

#ifdef _WIN32
	LARGE_INTEGER	fq, t1, t2;

	QueryPerformanceFrequency(&fq);
	QueryPerformanceCounter(&t1);
	rfunc(src, dst);
	QueryPerformanceCounter(&t2);

	printf("%s(%d): tick=%4lld msec\n", name, idx, (t2.QuadPart - t1.QuadPart)/(fq.QuadPart/1000));
#else
	timespec	t1, t2;

	clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
	rfunc(src, dst);
	clock_gettime(CLOCK_MONOTONIC_RAW, &t2);

	printf("%s(%d): tick=%4.0f msec\n", name, idx,
		((double)t2.tv_nsec - t1.tv_nsec) / (1000 * 1000) + (t2.tv_sec - t1.tv_sec) * 1000);
#endif

#ifdef DAT_VERIFY
	for (int y=0; y < MX; y++) {
		for (int x=0; x < MY; x++) {
			printf("%4d ", dst[y][x]);
		}
		printf("\n");
	}
#endif
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		BULKSIZE = atoi(argv[1]);
		if (argc > 2) {
			BULKSIZE_AVX = atoi(argv[2]);
		}
	}
	if (BULKSIZE < 4 || BULKSIZE_AVX < 32) {
		printf("fast_rotate [bulksize for normal(>=4) [bulksize for AVX(>=32)]]\n");
		return	-1;
	}
	printf("normal bulksize=%d  avx bulksize=%d\n\n", BULKSIZE, BULKSIZE_AVX);

#ifdef DAT_VERIFY
#define MAX_LOOP 1
#else
#define MAX_LOOP 2
#endif

	for (int i = 0; i < MAX_LOOP; i++) {
		calc_rotate("normal     ", i, rotate_img);
#ifdef  USE_AVX2
		calc_rotate("normal_avx2", i, rotate_img_avx);
#endif
		calc_rotate("cachel     ", i, rotate_img_cachel);
#ifdef  USE_AVX2
		calc_rotate("cachel_avx2", i, rotate_img_avx_cachel);
#endif
		printf("\n");
	}
}


