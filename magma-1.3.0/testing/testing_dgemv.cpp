/*
 *  -- MAGMA (version 1.3.0) --
 *     Univ. of Tennessee, Knoxville
 *     Univ. of California, Berkeley
 *     Univ. of Colorado, Denver
 *     November 2012
 *
 *  @generated d Wed Nov 14 22:54:09 2012
 *
 **/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define PRECISION_d
#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS(m, n) ( 6. * FMULS_GEMV(m, n) + 2. * FADDS_GEMV(m, n))
#else
#define FLOPS(m, n) (      FMULS_GEMV(m, n) +      FADDS_GEMV(m, n))
#endif

int main(int argc, char **argv)
{        
    TESTING_CUDA_INIT();

    magma_timestr_t  start, end;
    double      flops, magma_perf, cuda_perf, error, work[1];
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    double c_neg_one = MAGMA_D_NEG_ONE;

    FILE        *fp ; 
    magma_int_t i, lda, Xm, Ym;
    magma_int_t M, M0 = 0;
    magma_int_t N, N0 = 0;
    magma_int_t szeA, szeX, szeY;
    magma_int_t istart = 64;
    magma_int_t iend   = 10240;
    magma_int_t incx = 1;
    magma_int_t incy = 1;
    char        trans = MagmaNoTrans;
    double alpha = MAGMA_D_MAKE(1., 0.); // MAGMA_D_MAKE(  1.5, -2.3 );
    double beta  = MAGMA_D_MAKE(0., 0.); // MAGMA_D_MAKE( -0.6,  0.8 );
    double *A, *X, *Y, *Ycublas, *Ymagma;
    double *dA, *dX, *dY;
        
    if (argc != 1){
        for(i=1; i<argc; i++){
            if ( strcmp("-n", argv[i]) == 0 ){
                N0 = atoi(argv[++i]);
            }
            else if ( strcmp("-m", argv[i]) == 0 ){
                M0 = atoi(argv[++i]);
            }
            else if (strcmp("-N", argv[i])==0){
                trans = MagmaNoTrans;
            }
            else if (strcmp("-T", argv[i])==0){
                trans = MagmaTrans;
            }
#if defined(PRECISION_z) || defined(PRECISION_c)
            else if (strcmp("-C", argv[i])==0){
                trans = MagmaTrans;
            }
#endif
        }
    }

    if ( (M0 != 0) && (N0 != 0) )
        iend = istart + 1;

    M = N = iend;
    if ( M0 != 0 ) M = M0;
    if ( N0 != 0 ) N = N0;

    if( trans == MagmaNoTrans ) {
        Xm = N;
        Ym = M;
    }  else {
        Xm = M;
        Ym = N;
    }

    lda = ((M+31)/32)*32;
    
    szeA = lda*N;
    szeX = incx*Xm;
    szeY = incy*Ym;
      
    TESTING_MALLOC( A, double, szeA );
    TESTING_MALLOC( X, double, szeX );
    TESTING_MALLOC( Y, double, szeY );
    TESTING_MALLOC( Ycublas, double, szeY );
    TESTING_MALLOC( Ymagma,  double, szeY );

    TESTING_DEVALLOC( dA, double, szeA );
    TESTING_DEVALLOC( dX, double, szeX );
    TESTING_DEVALLOC( dY, double, szeY );

    /* Initialize the matrix */
    lapackf77_dlarnv( &ione, ISEED, &szeA, A );
    lapackf77_dlarnv( &ione, ISEED, &szeX, X );
    lapackf77_dlarnv( &ione, ISEED, &szeY, Y );

    fp = fopen ("results_dgemv.txt", "w") ;
    if( fp == NULL ){ printf("Couldn't open output file\n"); exit(1);}

    printf("\nUsage: \n");
    printf("  testing_dgemv [-N|T|C] [-m %d] [-n %d]\n\n", 1024, 1024);

    printf( "   m    n   CUBLAS,Gflop/s   MAGMABLAS Gflop/s   \"error\"\n" 
            "==============================================================\n");
    fprintf(fp, "   m    n   CUBLAS,Gflop/s   MAGMABLAS Gflop/s   \"error\"\n" 
            "==============================================================\n");
    
    for( i=istart; i < iend; i = (int)((i+1)*1.1) )
    {
        M = N = i;
        if ( M0 != 0 ) M = M0;
        if ( N0 != 0 ) N = N0;

        if( trans == MagmaNoTrans ) {
            Xm = N;
            Ym = M;
        }  else {
            Xm = M;
            Ym = N;
        }
         
        lda = ((M+31)/32)*32;
        flops = FLOPS( (double)M, (double)N ) / 1000000;

        printf(      "%5d %5d ", (int) M, (int) N );
        fprintf( fp, "%5d %5d ", (int) M, (int) N );

        /* =====================================================================
           Performs operation using CUDA-BLAS
           =================================================================== */
        magma_dsetmatrix( M, N, A, lda, dA, lda );
        magma_dsetvector( Xm, X, incx, dX, incx );
        magma_dsetvector( Ym, Y, incy, dY, incy );

        /*
         * Cublas Version
         */
        start = get_current_time();
        cublasDgemv( trans, M, N, alpha, dA, lda, dX, incx, beta, dY, incy );
        end = get_current_time();
        
        magma_dgetvector( Ym, dY, incy, Ycublas, incy );
        
        cuda_perf = flops / GetTimerValue(start, end);
        printf(     "%11.2f", cuda_perf );
        fprintf(fp, "%11.2f", cuda_perf );

        /*
         * Magma Version
         */
        magma_dsetvector( Ym, Y, incy, dY, incy );
        
        start = get_current_time();
        magmablas_dgemv( trans, M, N, alpha, dA, lda, dX, incx, beta, dY, incy );
        end = get_current_time();
        
        magma_dgetvector( Ym, dY, incx, Ymagma, incx );
        
        magma_perf = flops / GetTimerValue(start, end);
        printf(     "%11.2f", magma_perf );
        fprintf(fp, "%11.2f", magma_perf );

        /* =====================================================================
           Computing the Difference Cublas VS Magma
           =================================================================== */
        
        blasf77_daxpy( &Ym, &c_neg_one, Ymagma, &incy, Ycublas, &incy);
        error = lapackf77_dlange( "M", &Ym, &ione, Ycublas, &Ym, work );

#if 0
        printf(      "\t\t %8.6e", error / (double)Ym );
        fprintf( fp, "\t\t %8.6e", error / (double)Ym );

        /*
         * Blas comparaison
         */
        {
            char *blastrans = MagmaNoTransStr;
            if ( trans == MagmaTrans )
                blastrans = MagmaTransStr;
            else if ( trans == MagmaTrans )
                blastrans = MagmaTransStr;
            
            blasf77_dcopy( &Ym, Y, &incy, Ycublas, &incy );
            blasf77_dgemv( blastrans, &M, &N, 
                           &alpha, A,       &lda, 
                                   X,       &incx, 
                           &beta,  Ycublas, &incy );
            
            blasf77_daxpy( &Ym, &c_neg_one, Ymagma, &incy, Ycublas, &incy);
            error = lapackf77_dlange( "M", &Ym, &ione, Ycublas, &Ym, work );
        }
#endif

        printf(      "\t\t %8.6e\n", error / (double)Ym );
        fprintf( fp, "\t\t %8.6e\n", error / (double)Ym );

    }
    
    /* Free Memory */
    TESTING_FREE( A );
    TESTING_FREE( X );
    TESTING_FREE( Y );
    TESTING_FREE( Ycublas );
    TESTING_FREE( Ymagma );

    TESTING_DEVFREE( dA );
    TESTING_DEVFREE( dX );
    TESTING_DEVFREE( dY );

    /* Free device */
    TESTING_CUDA_FINALIZE();
    return EXIT_SUCCESS;
}
