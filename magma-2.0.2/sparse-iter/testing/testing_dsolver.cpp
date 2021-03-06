/*
    -- MAGMA (version 2.0.2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date May 2016

       @generated from sparse-iter/testing/testing_zsolver.cpp normal z -> d, Mon May  2 23:31:25 2016
       @author Hartwig Anzt
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "flops.h"
#include "magma_v2.h"
#include "magma_lapack.h"
#include "testings.h"
#include "magmasparse_internal.h"



/* ////////////////////////////////////////////////////////////////////////////
   -- testing any solver
*/
int main(  int argc, char** argv )
{
    magma_int_t info = 0;
    TESTING_INIT();

    magma_dopts zopts;
    magma_queue_t queue=NULL;
    magma_queue_create( 0, &queue );
    
    double one = MAGMA_D_MAKE(1.0, 0.0);
    double zero = MAGMA_D_MAKE(0.0, 0.0);
    magma_d_matrix A={Magma_CSR}, B={Magma_CSR}, B_d={Magma_CSR};
    magma_d_matrix x={Magma_CSR}, b={Magma_CSR};
    
    int i=1;
    CHECK( magma_dparse_opts( argc, argv, &zopts, &i, queue ));
    B.blocksize = zopts.blocksize;
    B.alignment = zopts.alignment;

    CHECK( magma_dsolverinfo_init( &zopts.solver_par, &zopts.precond_par, queue ));

    while( i < argc ) {
        if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
            i++;
            magma_int_t laplace_size = atoi( argv[i] );
            CHECK( magma_dm_5stencil(  laplace_size, &A, queue ));
        } else {                        // file-matrix test
            CHECK( magma_d_csr_mtx( &A,  argv[i], queue ));
        }

        // for the eigensolver case
        zopts.solver_par.ev_length = A.num_cols;
        CHECK( magma_deigensolverinfo_init( &zopts.solver_par, queue ));

        // scale matrix
        CHECK( magma_dmscale( &A, zopts.scaling, queue ));
        
        // preconditioner
        if ( zopts.solver_par.solver != Magma_ITERREF ) {
            CHECK( magma_d_precondsetup( A, b, &zopts.solver_par, &zopts.precond_par, queue ) );
        }

        CHECK( magma_dmconvert( A, &B, Magma_CSR, zopts.output_format, queue ));
        
        printf( "\n%% matrix info: %d-by-%d with %d nonzeros\n\n",
                            int(A.num_rows), int(A.num_cols), int(A.nnz) );
        
        printf("matrixinfo = [ \n");
        printf("%%   size   (m x n)     ||   nonzeros (nnz)   ||   nnz/m   ||   stored nnz\n");
        printf("%%======================================================================"
                            "======%%\n");
        printf("  %8d  %8d      %10d             %4d        %10d\n",
            int(B.num_rows), int(B.num_cols), int(B.true_nnz), int(B.true_nnz/B.num_rows), int(B.nnz) );
        printf("%%======================================================================"
        "======%%\n");
        printf("];\n");

        CHECK( magma_dmtransfer( B, &B_d, Magma_CPU, Magma_DEV, queue ));

        // vectors and initial guess
        CHECK( magma_dvinit( &b, Magma_DEV, A.num_rows, 1, one, queue ));
        //magma_dvinit( &x, Magma_DEV, A.num_cols, 1, one, queue );
        //magma_d_spmv( one, B_d, x, zero, b, queue );                 //  b = A x
        //magma_dmfree(&x, queue );
        CHECK( magma_dvinit( &x, Magma_DEV, A.num_cols, 1, zero, queue ));
        
        info = magma_d_solver( B_d, b, &x, &zopts, queue );
        if( info != 0 ) {
            printf("%%error: solver returned: %s (%d).\n",
                magma_strerror( info ), int(info) );
        }
        printf("data = [\n");
        magma_dsolverinfo( &zopts.solver_par, &zopts.precond_par, queue );
        printf("];\n\n");
        
        printf("precond_info = [\n");
        printf("%%   setup  runtime\n");        
        printf("  %.6f  %.6f\n",
           zopts.precond_par.setuptime, zopts.precond_par.runtime );
        printf("];\n\n");
        magma_dmfree(&B_d, queue );
        magma_dmfree(&B, queue );
        magma_dmfree(&A, queue );
        magma_dmfree(&x, queue );
        magma_dmfree(&b, queue );
        i++;
    }

cleanup:
    magma_dmfree(&B_d, queue );
    magma_dmfree(&B, queue );
    magma_dmfree(&A, queue );
    magma_dmfree(&x, queue );
    magma_dmfree(&b, queue );
    magma_dsolverinfo_free( &zopts.solver_par, &zopts.precond_par, queue );
    magma_queue_destroy( queue );
    TESTING_FINALIZE();
    return info;
}
