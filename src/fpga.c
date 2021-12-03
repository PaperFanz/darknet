#include "fpga.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>

#define __DEBUG__

#define ACCEL_BASE_ADDR (0xa1000000ULL) // to change
#define A_BASE_ADDR (0xa1000000ULL) // to change
#define B_BASE_ADDR (0xa1000000ULL) // to change
#define C_BASE_ADDR (0xa1000000ULL) // to change
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

//#define GEMM_TOT_BYTES  (0x20000000ULL)
//#define MAP_SIZE        (0x1000000ULL)
#define ACCELMAP_SIZE        (0x1000ULL)
#define ACCELMAP_MASK        (ACCELMAP_SIZE-1)
#define ACCELMAP_BLK_WORDS   (ACCELMAP_SIZE>>2)
#define AMAP_SIZE        (0x1000ULL)
#define AMAP_MASK        (AMAP_SIZE-1)
#define AMAP_BLK_WORDS   (AMAP_SIZE>>2)
#define BMAP_SIZE        (0x1000ULL)
#define BMAP_MASK        (BMAP_SIZE-1)
#define BMAP_BLK_WORDS   (BMAP_SIZE>>2)
#define CMAP_SIZE        (0x1000ULL)
#define CMAP_MASK        (CMAP_SIZE-1)
#define CMAP_BLK_WORDS   (CMAP_SIZE>>2)

/* global memmap for fpga accesses */
volatile int memfd = 0;
volatile uint32_t* accelptr = NULL;
volatile uint32_t* aptr = NULL;
volatile uint32_t* bptr = NULL;
volatile uint32_t* cptr = NULL;
volatile bool init_success = false;

/* interrupt stuff */
#define READ_CMD  (0x0 << 31)
#define WRITE_CMD (0x1 << 31)
volatile int det_int = 0;

// signal handler for receiving events from hardware driver
void sighandler(int signo)
{
  if(signo==SIGIO)
    {
      det_int++;
      printf("\nInterrupt detected\n");
    }
  
  return;
}

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
    accelptr = (uint32_t *)mmap(NULL, ACCELMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (ACCEL_BASE_ADDR) & ~ACCELMAP_MASK);	
    if(accelptr == MAP_FAILED) {
        printf("accel mapping failed\n");
        fflush(stdout);
        return -2;
    }
    aptr = (uint32_t *)mmap(NULL, AMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (A_BASE_ADDR) & ~AMAP_MASK);	
    if(aptr == MAP_FAILED) {
        printf("a mapping failed\n");
        fflush(stdout);
        return -2;
    }
    bptr = (uint32_t *)mmap(NULL, BMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (B_BASE_ADDR) & ~BMAP_MASK);	
    if(bptr == MAP_FAILED) {
        printf("b mapping failed\n");
        fflush(stdout);
        return -2;
    }
    cptr = (uint32_t *)mmap(NULL, CMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, memfd, (C_BASE_ADDR) & ~CMAP_MASK);	
    if(cptr == MAP_FAILED) {
        printf("c mapping failed\n");
        fflush(stdout);
        return -2;
    }

#ifdef __DEBUG__
    printf("fpga_init mmap'd\n");
    fflush(stdout);
#endif
    
//    if (aptr[MAGIC] != 0xdeadbeefULL) {
//        printf("accelerator memory corrupted\n");
//        fflush(stdout);
//        return -3;
//    }

    // Setup interrupt
    unsigned long volatile gie, iie;
    struct sigaction action;
    int fd;
    // install signal handler
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGIO);

    action.sa_handler = sighandler;
    action.sa_flags=0;

    sigaction(SIGIO, &action, NULL);

    // open hardware device (driver)
    fd=open("/dev/fpga", O_RDWR);
    if(fd < 0)
    {

        printf("Unable to open /dev/fpga.  Ensure it exists!\n");
        return -1;
    }
    fcntl(fd, F_SETOWN, getpid());
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_ASYNC);

    // enable FPGA interrupts (global and IP)
    ioctl(fd, READ_CMD + 0x1, &gie);
    gie = gie | 0x00000001;
    ioctl(fd, WRITE_CMD + 0x1, &gie);

    iie = 0x1;
    ioctl(fd, WRITE_CMD + 0x2, &iie);

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
        return det_int;
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
    accelptr[RESET] = 0x01;
    while (!fpga_ready());

    int i;

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring arrays\n");
    fflush(stdout);
#endif

    int32_t asize = k;
    int32_t bsize = k;

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring A\n");
    fflush(stdout);
#endif
    memcpy(aptr, A, asize*sizeof(fx_t));

#ifdef __DEBUG__
    printf("\nfpga_gemm: transferring B\n");
    fflush(stdout);
#endif
    memcpy(bptr, B, bsize*sizeof(fx_t));
    
#ifdef __DEBUG__
    printf("\ndone\n");
    fflush(stdout);
#endif

    accelptr[DATA_RDY] = 0x01;
}

void fpga_read(int m, int n, int k, fx_t* C)
{
    if (!init_success) return;
#ifdef __DEBUG__
    printf("\nfpga_read: wait for ready\n");
    fflush(stdout);
#endif
    while (!fpga_ready());

    int32_t csize = 1;
#ifdef __DEBUG__
    printf("\nfpga_read: C\n");
    fflush(stdout);
#endif
    memcpy(C, cptr, csize*sizeof(fx_t));
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
    munmap((void *) accelptr, ACCELMAP_SIZE);
    munmap((void *) aptr, AMAP_SIZE);
    munmap((void *) bptr, BMAP_SIZE);
    munmap((void *) cptr, CMAP_SIZE);
    close(memfd);
#ifdef __DEBUG__
    printf("\nfpga_free: done\n");
    fflush(stdout);
#endif
}


