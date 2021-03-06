/*
    -- MAGMA (version 1.6.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2015

       @generated from testing_zspmv_check.cpp normal z -> s, Fri Jan 30 19:00:33 2015
       @author Hartwig Anzt
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magmasparse.h"
#include "magma_lapack.h"
#include "testings.h"



/* ////////////////////////////////////////////////////////////////////////////
   -- testing any solver 
*/
int main(  int argc, char** argv )
{
    TESTING_INIT();
    magma_queue_t queue;
    magma_queue_create( /*devices[ opts->device ],*/ &queue );
    
    int i=1;

    float one = MAGMA_S_MAKE(1.0, 0.0);
    float zero = MAGMA_S_MAKE(0.0, 0.0);
    magma_s_sparse_matrix A, B, B_d;
    magma_s_vector x, b;

    while(  i < argc ) {

        if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
            i++;
            magma_int_t laplace_size = atoi( argv[i] );
            magma_sm_5stencil(  laplace_size, &A, queue );
        } else {                        // file-matrix test
            magma_s_csr_mtx( &A,  argv[i], queue );
        }

        printf( "\n# matrix info: %d-by-%d with %d nonzeros\n\n",
                            (int) A.num_rows,(int) A.num_cols,(int) A.nnz );

        magma_int_t n = A.num_rows;
        magma_s_mtransfer( A, &B_d, Magma_CPU, Magma_DEV, queue );

        // vectors and initial guess
        magma_s_vinit( &b, Magma_DEV, A.num_cols, zero, queue );
        magma_s_vinit( &x, Magma_DEV, A.num_cols, one, queue );
        
        magma_s_vvisu( b, 90, 10, queue );
        
            magma_s_mvisu( A, queue );
            printf("\n\n\n");
            magma_s_mvisu( B_d, queue );
        
        float res;
        res = magma_snrm2(n, b.dval, 1 );
        printf("norm0: %f\n", res);
        
        magma_s_spmv( one, B_d, x, zero, b, queue );                 //  b = A x

        magma_s_vvisu( b, 0, 100, queue );
        magma_s_vvisu( b, b.num_rows-10, 10, queue );

        res = magma_snrm2(n, b.dval, 1 );
        printf("norm: %f\n", res);

        
        magma_sresidual( B_d, x, b, &res, queue);
        printf("res: %f\n", res);


        magma_s_mfree(&B_d, queue );

        magma_s_mfree(&A, queue ); 
        
        magma_s_vfree(&x, queue );
        magma_s_vfree(&b, queue );

        i++;
    }

    
    /* Shutdown */
    magma_queue_destroy( queue );
    magma_finalize();
    
    return 0;
}
