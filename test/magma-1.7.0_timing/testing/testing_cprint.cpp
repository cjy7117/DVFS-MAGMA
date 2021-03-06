/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015
  
       @generated from testing_zprint.cpp normal z -> c, Fri Sep 11 18:29:37 2015
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cprint
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    magmaFloatComplex *hA;
    magmaFloatComplex_ptr dA;
    //magma_int_t ione     = 1;
    //magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t M, N, lda, ldda;  //size
    magma_int_t status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );

    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M     = opts.msize[itest];
            N     = opts.nsize[itest];
            lda   = M;
            ldda  = magma_roundup( M, opts.align );  // multiple of 32 by default
            //size  = lda*N;

            /* Allocate host memory for the matrix */
            TESTING_MALLOC_CPU( hA, magmaFloatComplex, lda *N );
            TESTING_MALLOC_DEV( dA, magmaFloatComplex, ldda*N );
        
            //lapackf77_clarnv( &ione, ISEED, &size, hA );
            for( int j = 0; j < N; ++j ) {
                for( int i = 0; i < M; ++i ) {
                    hA[i + j*lda] = MAGMA_C_MAKE( i + j*0.01, 0. );
                }
            }
            magma_csetmatrix( M, N, hA, lda, dA, ldda );
            
            printf( "A=" );
            magma_cprint( M, N, hA, lda );
            
            printf( "dA=" );
            magma_cprint_gpu( M, N, dA, ldda );
            
            TESTING_FREE_CPU( hA );
            TESTING_FREE_DEV( dA );
        }
    }

    TESTING_FINALIZE();
    return status;
}
