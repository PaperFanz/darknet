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
    RESET,
    DEBUG,
    MAGIC,
    GEMM_REG_NUM
} gemm_reg_t;

#define GEMM_TOT_BYTES (0x2000000ULL)

/* float conversion macros */
#define FP2FX(fp) (fx_t)((fp)*((fx_t)1 << FXFP_SCALE))
#define FX2FP(fx) ((float)(fx)/((fx_t)1 << FXFP_SCALE))

/* global memmap for fpga accesses */
static volatile int memfd = 0;
static volatile uint32_t* base = NULL;
static volatile bool init_success = false;

int fpga_init(void)
{
    if (init_success) return 0;
    printf("fpga_init\n");
    fflush(stdout);

    //Open memory as a file
    memfd = open("/dev/mem", O_RDWR|O_SYNC);
    if(!memfd) {
        printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
        fflush(stdout);
        return -1;
    }	

    //Map the physical base address to local pointer (in virtual address space)
    base = (uint32_t *)mmap(NULL, GEMM_TOT_BYTES, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, ACCEL_BASE_ADDR);	
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

    base[RESET] = 0x01;
    init_success = true;
    printf("fpga_init success\n");
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

    printf("\nfpga_gemm: transferring arrays\nProgress: ");
    fflush(stdout);

    const uint32_t one_16th = (m*k + n*k + m*n) >> 4;
    uint32_t progress = 0;
    uint32_t cnt = 0;

    int arr_base = GEMM_REG_NUM;
    int i, s = m*k;
    base[A_BASE] = arr_base;
    volatile uint32_t* arr = &base[arr_base];
    for (i = 0; i < s; ++i) {
        arr[i] = FP2FX(A[i]);
        if ((++progress % one_16th) == 0) {
            printf("\nfpga_gemm: %d/16\n", ++cnt);
            fflush(stdout);
        }
    }
    arr_base += s;

    s = k*n;
    base[B_BASE] = arr_base;
    arr = &base[arr_base];
    for (i = 0; i < s; ++i) {
        arr[i] = FP2FX(B[i]);
        if ((++progress % one_16th) == 0) {
            printf("\nfpga_gemm: %d/16\n", ++cnt);
            fflush(stdout);
        }
    }
    arr_base += s;

    s = m*n;
    base[C_BASE] = arr_base;
    arr = &base[arr_base];
    for (i = 0; i < s; ++i) {
        arr[i] = FP2FX(C[i]);
        if ((++progress % one_16th) == 0) {
            printf("\nfpga_gemm: %d/16\n", ++cnt);
            fflush(stdout);
        }
    }

    printf("\ndone\n");
    fflush(stdout);

    base[DATA_RDY] = 0x01;
}

void fpga_read(int m, int n, int k, float* C)
{
    if (!init_success) return;
    printf("\nfpga_read: wait for ready\n");
    fflush(stdout);
    while (!fpga_ready());

    printf("\nfpga_read: transferring arrays\nProgress: ");
    fflush(stdout);

    int i, s = m*n;
    uint32_t* arr = &base[base[C_BASE]];
    for (i = 0; i < s; ++i) {
        C[i] = FX2FP(arr[i]);
    }

    printf("\nfpga_read: done\n");
    fflush(stdout);
}

void fpga_free(void)
{
    if (!init_success) return;
    printf("\nfpga_free: free\n");
    fflush(stdout);
    init_success = false;
    munmap((void *) base, GEMM_TOT_BYTES);
    close(memfd);
    printf("\nfpga_free: done\n");
    fflush(stdout);
}


