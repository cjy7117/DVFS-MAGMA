/*
    -- MAGMA (version 1.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2012

       @generated c Wed Nov 14 22:54:26 2012

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

// Flops formula
#define PRECISION_c
#define CHECK_ERROR
#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS(n) ( 6. * FMULS_GEHRD(n) + 2. * FADDS_GEHRD(n))
#else
#define FLOPS(n) (      FMULS_GEHRD(n) +      FADDS_GEHRD(n))
#endif

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cgehrd2
*/
int main( int argc, char** argv)
{
    TESTING_CUDA_INIT();

    magma_timestr_t       start, end;
    float           eps, flops, gpu_perf, cpu_perf;
    cuFloatComplex *h_A, *h_R, *h_Q, *h_work, *tau, *twork, *dT;
    float          *rwork;
    float           result[2] = {0., 0.};

    /* Matrix size */
    magma_int_t N=0, n2, lda, nb, lwork, ltwork, once = 0;
    magma_int_t size[10] = {1024,2048,3072,4032,5184,6016,7040,8064,9088,10112};

    magma_int_t i, info, checkres;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    if (argc != 1){
        for(i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0) {
                N = atoi(argv[++i]);
                once = true;
            }
        }
        if ( N > 0 )
            printf("  testing_cgehrd -N %d\n\n", (int) N);
        else
        {
            printf("\nUsage: \n");
            printf("  testing_cgehrd -N %d\n\n", 1024);
            exit(1);
        }
    }
    else {
        printf("\nUsage: \n");
        printf("  testing_cgehrd -N %d\n\n", 1024);
        N = size[9];
    }

    checkres = getenv("MAGMA_TESTINGS_CHECK") != NULL;

    eps   = lapackf77_slamch( "E" );
    lda   = N;
    n2    = N*lda;
    nb    = magma_get_cgehrd_nb(N);
    /* We suppose the magma nb is bigger than lapack nb */
    lwork = N*nb;
    
    TESTING_MALLOC   ( h_A   , cuFloatComplex, n2    );
    TESTING_MALLOC   ( tau   , cuFloatComplex, N     );
    TESTING_HOSTALLOC( h_R   , cuFloatComplex, n2    );
    TESTING_HOSTALLOC( h_work, cuFloatComplex, lwork );
    TESTING_DEVALLOC ( dT    , cuFloatComplex, nb*N  );

    /* To avoid uninitialized variable warning */
    h_Q   = NULL;
    twork = NULL;
    rwork = NULL; 

    if ( checkres ) {
        ltwork = 2*(N*N);
        TESTING_HOSTALLOC( h_Q,   cuFloatComplex, lda*N  );
        TESTING_MALLOC(    twork, cuFloatComplex, ltwork );
#if defined(PRECISION_z) || defined(PRECISION_c) 
        TESTING_MALLOC(    rwork, float,          N      );
#endif
    }

    printf("  N    CPU GFlop/s    GPU GFlop/s   |A-QHQ'|/N|A|  |I-QQ'|/N \n");
    printf("=============================================================\n");
    for(i=0; i<10; i++){
        if ( !once ) {
            N = size[i];
        }
        lda = N;
        n2  = lda*N;
        flops = FLOPS( (float)N ) / 1e6;

        /* Initialize the matrices */
        lapackf77_clarnv( &ione, ISEED, &n2, h_A );
        lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );

        /* ====================================================================
           Performs operation using MAGMA
           =================================================================== */
        start = get_current_time();
        magma_cgehrd ( N, ione, N, h_R, lda, tau, h_work, lwork, dT, &info);
        end = get_current_time();
        if ( info < 0 )
            printf("Argument %d of magma_cgehrd had an illegal value\n", (int) -info);

        gpu_perf = flops / GetTimerValue(start,end);

        /* =====================================================================
           Check the factorization
           =================================================================== */
        if ( checkres ) {

            lapackf77_clacpy(MagmaUpperLowerStr, &N, &N, h_R, &lda, h_Q, &lda);
            { 
                int i, j;
                for(j=0; j<N-1; j++)
                    for(i=j+2; i<lda; i++)
                        h_R[i+j*lda] = MAGMA_C_ZERO;
            }

            nb = magma_get_cgehrd_nb(N);
            magma_cunghr(N, ione, N, h_Q, lda, tau, dT, nb, &info);
#if defined(PRECISION_z) || defined(PRECISION_c) 
            lapackf77_chst01(&N, &ione, &N, 
                             h_A, &lda, h_R, &lda, 
                             h_Q, &lda, twork, &ltwork, rwork, result);
#else
            lapackf77_chst01(&N, &ione, &N, 
                             h_A, &lda, h_R, &lda, 
                             h_Q, &lda, twork, &ltwork, result);
#endif
        }

        /* =====================================================================
           Performs operation using LAPACK
           =================================================================== */
        start = get_current_time();
        lapackf77_cgehrd(&N, &ione, &N, h_R, &lda, tau, h_work, &lwork, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of lapack_cgehrd had an illegal value.\n", (int) -info);

        cpu_perf = flops / GetTimerValue(start,end);

        /* =====================================================================
           Print performance and error.
           =================================================================== */
        if ( checkres ) {
            printf("%5d    %6.2f         %6.2f      %e %e\n",
                   (int) N, cpu_perf, gpu_perf,
                   result[0]*eps, result[1]*eps );
        } else {
            printf("%5d    %6.2f         %6.2f\n",
                   (int) N, cpu_perf, gpu_perf );
        }

        if ( once )
            break;
    }

    /* Memory clean up */
    TESTING_FREE    ( h_A  );
    TESTING_FREE    ( tau  );
    TESTING_HOSTFREE( h_work);
    TESTING_HOSTFREE( h_R  );
    TESTING_DEVFREE ( dT   );

    if ( checkres ) {
        TESTING_HOSTFREE( h_Q );
        TESTING_FREE( twork );
#if defined(PRECISION_z) || defined(PRECISION_c) 
        TESTING_FREE( rwork );
#endif
    }

    /* Shutdown */
    TESTING_CUDA_FINALIZE();
    return EXIT_SUCCESS;
}
