#include "fpga.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define __DEBUG__

#define ACCEL_BASE_ADDR (0xa1000000ULL)

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

#define GEMM_TOT_BYTES (0x20000000ULL)
#define MAP_MASK (GEMM_TOT_BYTES-1)

/* global memmap for fpga accesses */
static volatile int memfd = 0;
static volatile uint32_t* base = NULL;
static volatile bool init_success = false;

int fpga_init(void)
{
#ifdef __DEBUG__
    printf("fpga_init\n");
    fflush(stdout);
#endif

    //Open memory as a file
    memfd = open("/dev/mem", O_RDWR|O_SYNC);
    if(!memfd) {
        printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
        fflush(stdout);
        return -1;
    }	

#ifdef __DEBUG__
    printf("fpga_init memfd'd\n");
    fflush(stdout);
#endif

    //Map the physical base address to local pointer (in virtual address space)
    base = (uint32_t *)mmap(NULL, GEMM_TOT_BYTES, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, ACCEL_BASE_ADDR & ~MAP_MASK);	
    if(base == NULL) {
        printf("mapping failed\n");
        fflush(stdout);
        return -2;
    }

#ifdef __DEBUG__
    printf("fpga_init mmap'd\n");
    fflush(stdout);
#endif

    if (base[MAGIC] != 0xdeadbeefULL) {
        printf("accelerator memory corrupted\n");
        fflush(stdout);
        return -3;
    }

    base[RESET] = 0x01;
    init_success = true;
#ifdef __DEBUG__
    printf("fpga_init success\n");
    fflush(stdout);
#endif
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
        fx_t *A, int lda,
        fx_t *B, int ldb,
        fx_t *C, int ldc)
{
    if (!init_success) return;
    while (!fpga_ready());

    base[M_SIZE] = m;
    base[N_SIZE] = n;
    base[K_SIZE] = k;
    base[A_STEP] = lda;
    base[B_STEP] = ldb;
    base[C_STEP] = ldc;

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring arrays\nProgress: ");
    fflush(stdout);
#endif

    size_t arr_base = GEMM_REG_NUM;
    size_t s = m*k;
    base[A_BASE] = arr_base;
    void* arr = &base[arr_base];
    memcpy(arr, A, s*sizeof(fx_t));
    arr_base += s;

    s = k*n;
    base[B_BASE] = arr_base;
    arr = &base[arr_base];
    memcpy(arr, B, s*sizeof(fx_t));
    arr_base += s;

    s = m*n;
    base[C_BASE] = arr_base;
    arr = &base[arr_base];
    memcpy(arr, C, s*sizeof(fx_t));

#ifdef __DEBUG__
    printf("\ndone\n");
    fflush(stdout);
#endif

    base[DATA_RDY] = 0x01;
}

void fpga_read(int m, int n, int k, fx_t* C)
{
    if (!init_success) return;
#ifdef __DEBUG__
    printf("\nfpga_read: wait for ready\n");
    fflush(stdout);
#endif
    while (!fpga_ready());

#ifdef __DEBUG__
    printf("\nfpga_read: transferring arrays\n");
    fflush(stdout);
#endif

    size_t s = m*n;
    void* arr = &base[base[C_BASE]];
    memcpy(C, arr, s*sizeof(fx_t));

#ifdef __DEBUG__
    printf("\nfpga_read: done\n");
    fflush(stdout);
#endif
}

void fpga_free(void)
{
    if (!init_success) return;
#ifdef __DEBUG__
    printf("\nfpga_free: free\n");
    fflush(stdout);
#endif
    init_success = false;
    munmap((void *) base, GEMM_TOT_BYTES);
    close(memfd);
#ifdef __DEBUG__
    printf("\nfpga_free: done\n");
    fflush(stdout);
#endif
}


