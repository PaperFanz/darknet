#ifndef __FPGA_H__
#define __FPGA_H__

#include <stdint.h>
#include <stdbool.h>

/* make it a little easier to play with different int sizes and scales */
typedef int_fast32_t fx_t;
#define FXFP_SCALE 17

int fpga_init(void);

bool fpga_ready(void);

void fpga_gemm(int m, int n, int k,
        float *A, int lda,
        float *B, int ldb,
        float *C, int ldc);

void fpga_read(int m, int n, int k, float *C);

void fpga_free(void);

#endif /* __FPGA_H__ */

