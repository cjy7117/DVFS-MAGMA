/*
    -- MAGMA (version 2.0.2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date May 2016

       @generated from sparse-iter/testing/testing_zmconverter.cpp normal z -> c, Mon May  2 23:31:24 2016
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

    magma_copts zopts;
    magma_queue_t queue=NULL;
    magma_queue_create( 0, &queue );

    real_Double_t res;
    magma_c_matrix Z={Magma_CSR}, Z2={Magma_CSR}, A={Magma_CSR}, A2={Magma_CSR}, 
    AT={Magma_CSR}, AT2={Magma_CSR}, B={Magma_CSR};
    int i=1;
    CHECK( magma_cparse_opts( argc, argv, &zopts, &i, queue ));

    B.blocksize = zopts.blocksize;
    B.alignment = zopts.alignment;

    while( i < argc ) {
        if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
            i++;
            magma_int_t laplace_size = atoi( argv[i] );
            CHECK( magma_cm_5stencil(  laplace_size, &Z, queue ));
        } else {                        // file-matrix test
            CHECK( magma_c_csr_mtx( &Z,  argv[i], queue ));
        }

        printf("%% matrix info: %d-by-%d with %d nonzeros\n",
                            int(Z.num_rows), int(Z.num_cols), int(Z.nnz) );
        
        // convert to be non-symmetric
        CHECK( magma_cmconvert( Z, &A, Magma_CSR, Magma_CSRL, queue ));
        CHECK( magma_cmconvert( Z, &B, Magma_CSR, Magma_CSRU, queue ));

        // transpose
        CHECK( magma_cmtranspose( A, &AT, queue ));

        // quite some conversions
                    
        //ELL
        CHECK( magma_cmconvert( AT, &AT2, Magma_CSR, Magma_ELL, queue ));
        magma_cmfree(&AT, queue );
        CHECK( magma_cmconvert( AT2, &AT, Magma_ELL, Magma_CSR, queue ));
        magma_cmfree(&AT2, queue );
        //ELLPACKT
        CHECK( magma_cmconvert( AT, &AT2, Magma_CSR, Magma_ELLPACKT, queue ));
        magma_cmfree(&AT, queue );
        CHECK( magma_cmconvert( AT2, &AT, Magma_ELLPACKT, Magma_CSR, queue ));
        magma_cmfree(&AT2, queue );
        //ELLRT
        AT2.blocksize = 8;
        AT2.alignment = 8;
        CHECK( magma_cmconvert( AT, &AT2, Magma_CSR, Magma_ELLRT, queue ));
        magma_cmfree(&AT, queue );
        CHECK( magma_cmconvert( AT2, &AT, Magma_ELLRT, Magma_CSR, queue ));
        magma_cmfree(&AT2, queue );
        //SELLP
        AT2.blocksize = 8;
        AT2.alignment = 8;
        CHECK( magma_cmconvert( AT, &AT2, Magma_CSR, Magma_SELLP, queue ));
        magma_cmfree(&AT, queue );
        CHECK( magma_cmconvert( AT2, &AT, Magma_SELLP, Magma_CSR, queue ));
        magma_cmfree(&AT2, queue );
        //ELLD
        CHECK( magma_cmconvert( AT, &AT2, Magma_CSR, Magma_ELLD, queue ));
        magma_cmfree(&AT, queue );
        CHECK( magma_cmconvert( AT2, &AT, Magma_ELLD, Magma_CSR, queue ));
        magma_cmfree(&AT2, queue );
        //CSRCOO
        CHECK( magma_cmconvert( AT, &AT2, Magma_CSR, Magma_CSRCOO, queue ));
        magma_cmfree(&AT, queue );
        CHECK( magma_cmconvert( AT2, &AT, Magma_CSRCOO, Magma_CSR, queue ));
        magma_cmfree(&AT2, queue );
        //CSRLIST
        CHECK( magma_cmconvert( AT, &AT2, Magma_CSR, Magma_CSRLIST, queue ));
        magma_cmfree(&AT, queue );
        CHECK( magma_cmconvert( AT2, &AT, Magma_CSRLIST, Magma_CSR, queue ));
        magma_cmfree(&AT2, queue );
        //CSRD
        CHECK( magma_cmconvert( AT, &AT2, Magma_CSR, Magma_CSRD, queue ));
        magma_cmfree(&AT, queue );
        CHECK( magma_cmconvert( AT2, &AT, Magma_CSRD, Magma_CSR, queue ));
        magma_cmfree(&AT2, queue );
        
        // transpose
        CHECK( magma_cmtranspose( AT, &A2, queue ));
        CHECK( magma_cmdiff( A, A2, &res, queue));
        printf("%% ||A-A2||_F = %8.2e\n", res);
        if ( res < .000001 )
            printf("%% conversion tester:  ok\n");
        else
            printf("%% conversion tester:  failed\n");
        
        CHECK( magma_cmlumerge( A2, B, &Z2, queue ));

        CHECK( magma_cmdiff( Z, Z2, &res, queue));        
        printf("%% ||Z-Z2||_F = %8.2e\n", res);
        if ( res < .000001 )
            printf("%% LUmerge tester:  ok\n");
        else
            printf("%% LUmerge tester:  failed\n");

        magma_cmfree(&A, queue );
        magma_cmfree(&A2, queue );
        magma_cmfree(&AT, queue );
        magma_cmfree(&AT2, queue );
        magma_cmfree(&B, queue );
        magma_cmfree(&Z2, queue );
        magma_cmfree(&Z, queue );

        i++;
    }

cleanup:
    magma_cmfree(&A, queue );
    magma_cmfree(&A2, queue );
    magma_cmfree(&AT, queue );
    magma_cmfree(&AT2, queue );
    magma_cmfree(&B, queue );
    magma_cmfree(&Z2, queue );
    magma_cmfree(&Z, queue );
    
    magma_queue_destroy( queue );
    TESTING_FINALIZE();
    return info;
}
