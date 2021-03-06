/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014

       @generated from testing_zio.cpp normal z -> d, Sat Nov 15 19:54:24 2014
       @author Hartwig Anzt
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

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

    magma_dopts zopts;
    magma_queue_t queue;
    magma_queue_create( /*devices[ opts->device ],*/ &queue );
    
    int i=1;
    magma_dparse_opts( argc, argv, &zopts, &i, queue );


    real_Double_t res;
    magma_d_sparse_matrix A, A2, A3, A4, A5;

    while(  i < argc ) {

        if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
            i++;
            magma_int_t laplace_size = atoi( argv[i] );
            magma_dm_5stencil(  laplace_size, &A, queue );
        } else {                        // file-matrix test
            magma_d_csr_mtx( &A,  argv[i], queue );
        }

        printf( "# matrix info: %d-by-%d with %d nonzeros\n",
                            (int) A.num_rows,(int) A.num_cols,(int) A.nnz );

        // filename for temporary matrix storage
        const char *filename = "testmatrix.mtx";

        // write to file
        write_d_csrtomtx( A, filename, queue );

        // read from file
        magma_d_csr_mtx( &A2, filename, queue );

        // delete temporary matrix
        unlink( filename );
                
        //visualize
        printf("A2:\n");
        magma_d_mvisu( A2, queue );
        
        //visualize
        magma_d_mconvert(A2, &A4, Magma_CSR, Magma_CSRL, queue );
        printf("A4:\n");
        magma_d_mvisu( A4, queue );
        magma_d_mconvert(A4, &A5, Magma_CSR, Magma_ELL, queue );
        printf("A5:\n");
        magma_d_mvisu( A5, queue );

        // pass it to another application and back
        magma_int_t m, n;
        magma_index_t *row, *col;
        double *val;
        magma_dcsrget( A2, &m, &n, &row, &col, &val, queue );
        magma_dcsrset( m, n, row, col, val, &A3, queue );

        magma_dmdiff( A, A2, &res, queue );
        printf("# ||A-B||_F = %8.2e\n", res);
        if ( res < .000001 )
            printf("# tester IO:  ok\n");
        else
            printf("# tester IO:  failed\n");

        magma_dmdiff( A, A3, &res, queue );
        printf("# ||A-B||_F = %8.2e\n", res);
        if ( res < .000001 )
            printf("# tester matrix interface:  ok\n");
        else
            printf("# tester matrix interface:  failed\n");

        magma_d_mfree(&A, queue ); 
        magma_d_mfree(&A2, queue ); 
        magma_d_mfree(&A4, queue ); 
        magma_d_mfree(&A5, queue ); 


        i++;
    }
    
    magma_queue_destroy( queue );
    TESTING_FINALIZE();
    return 0;
}
