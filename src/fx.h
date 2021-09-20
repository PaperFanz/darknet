#ifndef FX_H
#define FX_H
#include <stddef.h>
#include <stdint.h>

/* make it a little easier to play with different int sizes and scales */
typedef int32_t fx_t;
#define FXFP_SCALE 17

static inline fx_t fx_mul(fx_t a, fx_t b)
{
    return (((int64_t)a*b)>>FXFP_SCALE);
}

static inline fx_t fx_mul_opt(fx_t a, uint8_t ashf, fx_t b)
{
    return ((a >> ashf) * (b >> (FXFP_SCALE - ashf)));
}

#define FX_MUL_OPT(a, ashf, b) (((a >> ashf) * (b >> (FXFP_SCALE - ashf))))

static inline fx_t fx_mul_dopt(fx_t a, uint8_t ashf, fx_t b, uint8_t bshf)
{
    return ((a >> ashf) * (b >> bshf)) >> (FXFP_SCALE - ashf - bshf);
}

#define FX_MUL_DOPT(a, ashf, b, bshf) (((a >> ashf) * (b >> bshf)) >> (FXFP_SCALE - ashf - bshf))

static inline fx_t roundup(float fp_number)
{
    return (fx_t)fp_number;
}

static inline fx_t fp2fx(float fp)
{
    return fp*((fx_t)1 << FXFP_SCALE);
}
#define FP2FX(fp) (fx_t)((fp)*((fx_t)1 << FXFP_SCALE))

static inline float fx2fp(fx_t fx)
{
    return (float) (fx)/((fx_t)1 << FXFP_SCALE);
}
#define FX2FP(fx) ((float)(fx)/((fx_t)1 << FXFP_SCALE))

fx_t * fp2fxarr(float* arr, size_t n)
{
    fx_t * fx = (fx_t *) xcalloc(n, sizeof(fx_t));
    size_t i;
	float temp;
    for (i = 0; i < n; ++i) {
        temp = arr[i];
		fx[i] = FP2FX(temp);
    }
	//printf("CHECK fx[0]: %d\n", fx[0]);
    return fx;
}

void fx2fparr(float* ret, fx_t* arr, size_t n)
{
    size_t i;
	fx_t temp;
    for (i = 0; i < n; ++i) {
        temp = arr[i];
		ret[i] = FX2FP(temp);
    }
	//printf("CHECK fp[0]: %g\n", ret[0]);
	free(arr);
}

void gemm_nn_fx(int M, int N, int K, fx_t ALPHA,
    fx_t *A, int lda,
    fx_t *B, int ldb,
    fx_t *C, int ldc)
{
    int i, j, k;
    for (i = 0; i < M; ++i) {
        for (k = 0; k < K; ++k) {
            PUT_IN_REGISTER fx_t A_PART = FX_MUL_OPT(ALPHA, 14, A[i * lda + k]);
            for (j = 0; j < N; ++j) {
            	C[i*ldc + j] += FX_MUL_DOPT(A_PART, 3, B[k*ldb + j], 5);
			}
        }
    }
}

#endif //FX_H
