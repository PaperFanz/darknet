#include "fpga.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define ACCEL_BASE_ADDR (0xa0000000ULL)

typedef enum GEMM_CTRL_REG {
    M_SIZE = 0,
    N_SIZE,
    K_SIZE,
    A_BASE,
    A_STEP,
    B_BASE,
    B_STEP,
    C_BASE,
    C_STEP,
    DATA_RDY,
    DEBUG,
    MAGIC,
    GEMM_REG_NUM
} gemm_reg_t;

#define GEMM_TOT_BYTES (0x400000ULL)

/* float conversion macros */
#define FP2FX(fp) (fx_t)((fp)*((fx_t)1 << FXFP_SCALE))
#define FX2FP(fx) ((float)(fx)/((fx_t)1 << FXFP_SCALE))

/* global memmap for fpga accesses */
static uint32_t* base = NULL;
static bool init_success = false;

int fpga_init(void)
{
    //Open memory as a file
    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    if(!fd) {
        printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
        return -1;
    }	

    //Map the physical base address to local pointer (in virtual address space)
    base = (uint32_t *)mmap(NULL, GEMM_TOT_BYTES, PROT_READ|PROT_WRITE, MAP_SHARED, fd, ACCEL_BASE_ADDR);	
    if((base == MAP_FAILED)) {
        printf("mapping failed\n");
        fflush(stdout);
        return -2;
    }

    if (base[MAGIC] != 0xdeadbeefULL) {
        printf("accelerator memory corrupted\n");
        fflush(stdout);
        return -3;
    }

    init_success = true;
    printf("fpga init success\n");
    fflush(stdout);
    return 0;
}

bool fpga_ready(void)
{
    if (init_success) {
        return base[DATA_RDY] & 0x02;
    } else {
        return false;
    }
}

void fpga_gemm(int m, int n, int k,
        float *A, int lda,
        float *B, int ldb,
        float *C, int ldc)
{
    if (!init_success) return;
    while (!fpga_ready());

    base[M_SIZE] = m;
    base[N_SIZE] = n;
    base[K_SIZE] = k;
    base[A_STEP] = lda;
    base[B_STEP] = ldb;
    base[C_STEP] = ldc;

    int arr_base = GEMM_REG_NUM;
    int i, s = m*k;
    base[A_BASE] = arr_base;
    uint32_t* arr = &base[arr_base];
    for (i = 0; i < s; ++i) {
        arr[i] = FP2FX(A[i]);
    }
    arr_base += s;

    s = k*n;
    base[B_BASE] = arr_base;
    arr = &base[arr_base];
    for (i = 0; i < s; ++i) {
        arr[i] = FP2FX(B[i]);
    }
    arr_base += s;

    s = m*n;
    base[C_BASE] = arr_base;
    arr = &base[arr_base];
    for (i = 0; i < s; ++i) {
        arr[i] = FP2FX(C[i]);
    }

    base[DATA_RDY] = 0x01;
}

void fpga_read(int m, int n, int k, float* C)
{
    if (!init_success) return;
    while (!fpga_ready());

    int i, s = m*n;
    uint32_t* arr = &base[base[C_BASE]];
    for (i = 0; i < s; ++i) {
        C[i] = FX2FP(arr[i]);
    }
}

void fpga_free(void)
{
    init_success = false;
    free(base);
}


