// H.Shirouzu 2018/02/21

#include <stdio.h>
#include <time.h>

typedef unsigned int u_int;

#define MX			(1920 * 8)
#define MY			(1080 * 8)
//#define LINESIZE	64
#define LINESIZE	(64 * 8) // bulk 8 cache lines
#define LNUM		(LINESIZE / sizeof(u_int))

void rotate_img(u_int src[][MX], u_int dst[][MY])	// X -> Y
{
	u_int	*src_p = &src[0][0];

	for (int y=0; y < MY; y++) {
		for (int x=0; x < MX; x++) {
			dst[x][MY-1-y] = *src_p++;
		}
	}
}

void rotate_img_rev(u_int src[][MX], u_int dst[][MY])	// Y -> X
{
	for (int x=0; x < MX; x++) {
		for (int y=0; y < MY; y++) {
			dst[x][MY-1-y] = src[y][x];
		}
	}
}

void rotate_img_cachel(u_int src[][MX], u_int dst[][MY])
{
	for (int x=0; x < MX; x+=LNUM) {
		for (int y=0; y < MY; y++) {
			u_int	*src_p = &src[y][x];
			for (int i=0; i < LNUM; i++) {
				dst[x+i][MY-1-y] = *src_p++;
			}
		}
	}
}

void init_img(u_int src[MY][MX])
{
	for (int y=0; y < MY; y++) {
		for (int x=0; x < MX; x++) {
			src[y][x] = y * MX + x;
		}
	}
}

void calc_rotate(const char *name, int idx, void (*rfunc)(u_int src[][MX], u_int dst[][MY]))
{
	static u_int	src[MY][MX];
	static u_int	dst[MX][MY];

	timespec	t1, t2;
	init_img(src);

	clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
	rfunc(src, dst);
	clock_gettime(CLOCK_MONOTONIC_RAW, &t2);

	printf("%s(%d): tick=%5.0f msec\n", name, idx,
		((double)t2.tv_nsec - t1.tv_nsec) / (1000*1000) + (t2.tv_sec - t1.tv_sec) * 1000);
}


int main()
{
	for (int i=0; i < 3; i++) {
		calc_rotate("normal  ", i, rotate_img);
		calc_rotate("norm_rev", i, rotate_img_rev);
		calc_rotate("cache_l ", i, rotate_img_cachel);
	}
}


