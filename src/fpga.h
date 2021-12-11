#ifndef __FPGA_H__
#define __FPGA_H__

#include <stdint.h>
#include <stdbool.h>

#define __16__

/* make it a little easier to play with different int sizes and scales */
typedef int32_t fx_c_t;
#ifdef __16__
typedef int16_t fx_t;
#define FXFP_SCALE 9
#else
typedef int32_t fx_t;
#define FXFP_SCALE 17
#endif
#define MAX_M 1024
#define MAX_K 4608
#define MAX_N 173056

#define __BLOCK_16x8__
int fpga_init(void);

bool fpga_ready(void);

void fpga_gemm(int m, int n, int k,
        fx_t *A, int lda,
        fx_t *B, int ldb,
        fx_t *C, int ldc);

void fpga_gemm_block(int m, int n, int k, int s, fx_t *A, fx_t *B, fx_t *C);

void fpga_read(int m, int n, int k, fx_t *C);

void fpga_read_block(int m, int n, fx_c_t *C);

void fpga_gemm_ablock(int k, int s, fx_t *A);

void fpga_gemm_bblock(int k, int s, fx_t *B);

void fpga_gemm_cblock(int m, int n, fx_c_t *C);

void fpga_gemm_start(int m, int n, int k, int s);

void fpga_free(void);

#endif /* __FPGA_H__ */

