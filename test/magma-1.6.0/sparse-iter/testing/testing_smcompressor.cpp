/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014

       @generated from testing_zmcompressor.cpp normal z -> s, Sat Nov 15 19:54:24 2014
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

    magma_sopts zopts;
    magma_queue_t queue;
    magma_queue_create( /*devices[ opts->device ],*/ &queue );

    int i=1;
    real_Double_t start, end;
    magma_sparse_opts( argc, argv, &zopts, &i, queue );


    real_Double_t res;
    magma_s_sparse_matrix A, AT, A2, B, B_d;

    B.blocksize = zopts.blocksize;
    B.alignment = zopts.alignment;

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

        // scale matrix
        magma_smscale( &A, zopts.scaling, queue );

        // remove nonzeros in matrix
        start = magma_sync_wtime( queue ); 
        for (int j=0; j<10; j++)
            magma_smcsrcompressor( &A, queue );
        end = magma_sync_wtime( queue ); 
        printf( " > MAGMA CPU: %.2e seconds.\n", (end-start)/10 );
        // transpose
        magma_s_mtranspose( A, &AT, queue );

        // convert, copy back and forth to check everything works
        magma_s_mconvert( AT, &B, Magma_CSR, Magma_CSR, queue );
        magma_s_mfree(&AT, queue ); 
        magma_s_mtransfer( B, &B_d, Magma_CPU, Magma_DEV, queue );
        magma_s_mfree(&B, queue );

        start = magma_sync_wtime( queue ); 
        for (int j=0; j<10; j++)
            magma_smcsrcompressor_gpu( &B_d, queue );
        end = magma_sync_wtime( queue ); 
        printf( " > MAGMA GPU: %.2e seconds.\n", (end-start)/10 );


        magma_s_mtransfer( B_d, &B, Magma_DEV, Magma_CPU, queue );
        magma_s_mfree(&B_d, queue );
        magma_s_mconvert( B, &AT, Magma_CSR, Magma_CSR, queue );      
        magma_s_mfree(&B, queue );

        // transpose back
        magma_s_mtranspose( AT, &A2, queue );
        magma_s_mfree(&AT, queue ); 
        magma_smdiff( A, A2, &res, queue );
        printf("# ||A-B||_F = %8.2e\n", res);
        if ( res < .000001 )
            printf("# tester matrix compressor:  ok\n");
        else
            printf("# tester matrix compressor:  failed\n");

        magma_s_mfree(&A, queue ); 
        magma_s_mfree(&A2, queue ); 

        i++;
    }
    
    magma_queue_destroy( queue );
    TESTING_FINALIZE();
    return 0;
}
