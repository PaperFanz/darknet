#ifndef __FPGA_H__
#define __FPGA_H__

#include <stdint.h>
#include <stdbool.h>

/* make it a little easier to play with different int sizes and scales */
typedef int32_t fx_t;
#define FXFP_SCALE 17
#define MAX_M 1024
#define MAX_K 4608
#define MAX_N 173056

int fpga_init(void);

bool fpga_ready(void);

void fpga_gemm(int m, int n, int k,
        fx_t *A, int lda,
        fx_t *B, int ldb,
        fx_t *C, int ldc);

void fpga_read(int m, int n, int k, fx_t *C);

void fpga_free(void);

#endif /* __FPGA_H__ */

