/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014

       @author Hartwig Anzt 

       @generated from zcumilu.cpp normal z -> c, Sat Nov 15 19:54:22 2014
*/
// includes CUDA
#include <cuda_runtime_api.h>
#include <cublas.h>
#include <cusparse_v2.h>
#include <cuda_profiler_api.h>

// project includes
#include "common_magma.h"
#include "magmasparse.h"

#include <assert.h>


#define PRECISION_c






/**
    Purpose
    -------

    Prepares the ILU preconditioner via the cuSPARSE.

    Arguments
    ---------

    @param[in]
    A           magma_c_sparse_matrix
                input matrix A

    @param[in,out]
    precond     magma_c_preconditioner*
                preconditioner parameters
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_cgepr
    ********************************************************************/

extern "C" magma_int_t
magma_ccumilusetup(
    magma_c_sparse_matrix A, magma_c_preconditioner *precond,
    magma_queue_t queue )
{
    //magma_c_mvisu(A, queue );
        // copy matrix into preconditioner parameter
        magma_c_sparse_matrix hA, hACSR;    
        magma_c_mtransfer( A, &hA, A.memory_location, Magma_CPU, queue );
        magma_c_mconvert( hA, &hACSR, hA.storage_type, Magma_CSR, queue );
        magma_c_mtransfer(hACSR, &(precond->M), Magma_CPU, Magma_DEV, queue );

        magma_c_mfree( &hA, queue );
        magma_c_mfree( &hACSR, queue );


            // CUSPARSE context //
            cusparseHandle_t cusparseHandle;
            cusparseStatus_t cusparseStatus;
            cusparseStatus = cusparseCreate(&cusparseHandle);
            cusparseSetStream( cusparseHandle, queue );
             if (cusparseStatus != 0)    printf("error in Handle.\n");


            cusparseMatDescr_t descrA;
            cusparseStatus = cusparseCreateMatDescr(&descrA);
             if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

            cusparseStatus =
            cusparseSetMatType(descrA,CUSPARSE_MATRIX_TYPE_GENERAL);
             if (cusparseStatus != 0)    printf("error in MatrType.\n");

            cusparseStatus =
            cusparseSetMatDiagType (descrA, CUSPARSE_DIAG_TYPE_NON_UNIT);
             if (cusparseStatus != 0)    printf("error in DiagType.\n");

            cusparseStatus =
            cusparseSetMatIndexBase(descrA,CUSPARSE_INDEX_BASE_ZERO);
             if (cusparseStatus != 0)    printf("error in IndexBase.\n");

            cusparseStatus =
            cusparseCreateSolveAnalysisInfo( &(precond->cuinfo) );
             if (cusparseStatus != 0)    printf("error in info.\n");

            // end CUSPARSE context //



            cusparseStatus =
            cusparseCcsrsm_analysis( cusparseHandle, 
                        CUSPARSE_OPERATION_NON_TRANSPOSE, 
                        precond->M.num_rows, precond->M.nnz, descrA,
                        precond->M.dval, precond->M.drow, precond->M.dcol, 
                        precond->cuinfo); 
             if (cusparseStatus != 0)    
                 printf("error in analysis:%d\n", cusparseStatus);

            cusparseStatus =
            cusparseCcsrilu0( cusparseHandle, CUSPARSE_OPERATION_NON_TRANSPOSE, 
                              precond->M.num_rows, descrA, 
                              precond->M.dval, 
                              precond->M.drow, 
                              precond->M.dcol, 
                              precond->cuinfo);
             if (cusparseStatus != 0)    
                 printf("error in ILU:%d\n", cusparseStatus);


            cusparseStatus =
            cusparseDestroySolveAnalysisInfo( precond->cuinfo );
             if (cusparseStatus != 0)    printf("error in info-free.\n");

    cusparseDestroyMatDescr( descrA );

    magma_c_sparse_matrix hL, hU;

    magma_c_mtransfer( precond->M, &hA, Magma_DEV, Magma_CPU, queue );

    hL.diagorder_type = Magma_UNITY;
    magma_c_mconvert( hA, &hL , Magma_CSR, Magma_CSRL, queue );
    hU.diagorder_type = Magma_VALUE;
    magma_c_mconvert( hA, &hU , Magma_CSR, Magma_CSRU, queue );
    magma_c_mtransfer( hL, &(precond->L), Magma_CPU, Magma_DEV, queue );
    magma_c_mtransfer( hU, &(precond->U), Magma_CPU, Magma_DEV, queue );

    cusparseMatDescr_t descrL;
    cusparseStatus = cusparseCreateMatDescr(&descrL);
     if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

    cusparseStatus =
    cusparseSetMatType(descrL,CUSPARSE_MATRIX_TYPE_TRIANGULAR);
     if (cusparseStatus != 0)    printf("error in MatrType.\n");

    cusparseStatus =
    cusparseSetMatDiagType (descrL, CUSPARSE_DIAG_TYPE_UNIT);
     if (cusparseStatus != 0)    printf("error in DiagType.\n");

    cusparseStatus =
    cusparseSetMatIndexBase(descrL,CUSPARSE_INDEX_BASE_ZERO);
     if (cusparseStatus != 0)    printf("error in IndexBase.\n");

    cusparseStatus =
    cusparseSetMatFillMode(descrL,CUSPARSE_FILL_MODE_LOWER);
     if (cusparseStatus != 0)    printf("error in fillmode.\n");


    cusparseStatus = cusparseCreateSolveAnalysisInfo(&precond->cuinfoL); 
     if (cusparseStatus != 0)    printf("error in info.\n");

    cusparseStatus =
    cusparseCcsrsm_analysis(cusparseHandle, 
        CUSPARSE_OPERATION_NON_TRANSPOSE, precond->L.num_rows, 
        precond->L.nnz, descrL, 
        precond->L.dval, precond->L.drow, precond->L.dcol, precond->cuinfoL );
     if (cusparseStatus != 0)    printf("error in analysis.\n");

    cusparseDestroyMatDescr( descrL );

    cusparseMatDescr_t descrU;
    cusparseStatus = cusparseCreateMatDescr(&descrU);
     if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

    cusparseStatus =
    cusparseSetMatType(descrU,CUSPARSE_MATRIX_TYPE_TRIANGULAR);
     if (cusparseStatus != 0)    printf("error in MatrType.\n");

    cusparseStatus =
    cusparseSetMatDiagType (descrU, CUSPARSE_DIAG_TYPE_NON_UNIT);
     if (cusparseStatus != 0)    printf("error in DiagType.\n");

    cusparseStatus =
    cusparseSetMatIndexBase(descrU,CUSPARSE_INDEX_BASE_ZERO);
     if (cusparseStatus != 0)    printf("error in IndexBase.\n");

    cusparseStatus =
    cusparseSetMatFillMode(descrU,CUSPARSE_FILL_MODE_UPPER);
     if (cusparseStatus != 0)    printf("error in fillmode.\n");

    cusparseStatus = cusparseCreateSolveAnalysisInfo(&precond->cuinfoU); 
     if (cusparseStatus != 0)    printf("error in info.\n");

    cusparseStatus =
    cusparseCcsrsm_analysis(cusparseHandle, 
        CUSPARSE_OPERATION_NON_TRANSPOSE, precond->U.num_rows, 
        precond->U.nnz, descrU, 
        precond->U.dval, precond->U.drow, precond->U.dcol, precond->cuinfoU );
     if (cusparseStatus != 0)    printf("error in analysis.\n");

    cusparseDestroyMatDescr( descrU );

    magma_c_mfree(&hA, queue );
    magma_c_mfree(&hL, queue );
    magma_c_mfree(&hU, queue );

    cusparseDestroy( cusparseHandle );

    return MAGMA_SUCCESS;
}








/**
    Purpose
    -------

    Performs the left triangular solves using the ILU preconditioner.

    Arguments
    ---------

    @param[in]
    b           magma_c_vector
                RHS

    @param[in,out]
    x           magma_c_vector*
                vector to precondition

    @param[in,out]
    precond     magma_c_preconditioner*
                preconditioner parameters
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_cgepr
    ********************************************************************/

extern "C" magma_int_t
magma_capplycumilu_l(
    magma_c_vector b, magma_c_vector *x, 
    magma_c_preconditioner *precond,
    magma_queue_t queue )
{
    magmaFloatComplex one = MAGMA_C_MAKE( 1.0, 0.0);

            // CUSPARSE context //
            cusparseHandle_t cusparseHandle;
            cusparseStatus_t cusparseStatus;
            cusparseStatus = cusparseCreate(&cusparseHandle);
            cusparseSetStream( cusparseHandle, queue );
             if (cusparseStatus != 0)    printf("error in Handle.\n");


            cusparseMatDescr_t descrL;
            cusparseStatus = cusparseCreateMatDescr(&descrL);
             if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

            cusparseStatus =
            cusparseSetMatType(descrL,CUSPARSE_MATRIX_TYPE_TRIANGULAR);
             if (cusparseStatus != 0)    printf("error in MatrType.\n");

            cusparseStatus =
            cusparseSetMatDiagType (descrL, CUSPARSE_DIAG_TYPE_UNIT);
             if (cusparseStatus != 0)    printf("error in DiagType.\n");

            cusparseStatus =
            cusparseSetMatIndexBase(descrL,CUSPARSE_INDEX_BASE_ZERO);
             if (cusparseStatus != 0)    printf("error in IndexBase.\n");

            cusparseStatus =
            cusparseSetMatFillMode(descrL,CUSPARSE_FILL_MODE_LOWER);
             if (cusparseStatus != 0)    printf("error in fillmode.\n");

            // end CUSPARSE context //
            cusparseStatus =
            cusparseCcsrsm_solve(   cusparseHandle, 
                                    CUSPARSE_OPERATION_NON_TRANSPOSE, 
                                    precond->L.num_rows, 
                                    b.num_rows*b.num_cols/precond->L.num_rows, 
                                    &one, 
                                    descrL,
                                    precond->L.dval,
                                    precond->L.drow,
                                    precond->L.dcol,
                                    precond->cuinfoL,
                                    b.dval,
                                    precond->L.num_rows,
                                    x->dval, 
                                    precond->L.num_rows);


             if (cusparseStatus != 0)   printf("error in L triangular solve.\n");

                
    cusparseDestroyMatDescr( descrL );
    cusparseDestroy( cusparseHandle );
    
    magma_device_sync();
    
    return MAGMA_SUCCESS;
}


/**
    Purpose
    -------

    Performs the right triangular solves using the ILU preconditioner.

    Arguments
    ---------

    @param[in]
    b           magma_c_vector
                RHS

    @param[in,out]
    x           magma_c_vector*
                vector to precondition

    @param[in,out]
    precond     magma_c_preconditioner*
                preconditioner parameters
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_cgepr
    ********************************************************************/

extern "C" magma_int_t
magma_capplycumilu_r(
    magma_c_vector b, magma_c_vector *x, 
    magma_c_preconditioner *precond,
    magma_queue_t queue )
{
    magmaFloatComplex one = MAGMA_C_MAKE( 1.0, 0.0);

            // CUSPARSE context //
            cusparseHandle_t cusparseHandle;
            cusparseStatus_t cusparseStatus;
            cusparseStatus = cusparseCreate(&cusparseHandle);
            cusparseSetStream( cusparseHandle, queue );
             if (cusparseStatus != 0)    printf("error in Handle.\n");


            cusparseMatDescr_t descrU;
            cusparseStatus = cusparseCreateMatDescr(&descrU);
             if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

            cusparseStatus =
            cusparseSetMatType(descrU,CUSPARSE_MATRIX_TYPE_TRIANGULAR);
             if (cusparseStatus != 0)    printf("error in MatrType.\n");

            cusparseStatus =
            cusparseSetMatDiagType (descrU, CUSPARSE_DIAG_TYPE_NON_UNIT);
             if (cusparseStatus != 0)    printf("error in DiagType.\n");

            cusparseStatus =
            cusparseSetMatIndexBase(descrU,CUSPARSE_INDEX_BASE_ZERO);
             if (cusparseStatus != 0)    printf("error in IndexBase.\n");

            cusparseStatus =
            cusparseSetMatFillMode(descrU,CUSPARSE_FILL_MODE_UPPER);
             if (cusparseStatus != 0)    printf("error in fillmode.\n");

            // end CUSPARSE context //
            cusparseStatus =
            cusparseCcsrsm_solve(   cusparseHandle, 
                                    CUSPARSE_OPERATION_NON_TRANSPOSE, 
                                    precond->U.num_rows, 
                                    b.num_rows*b.num_cols/precond->U.num_rows, 
                                    &one, 
                                    descrU,
                                    precond->U.dval,
                                    precond->U.drow,
                                    precond->U.dcol,
                                    precond->cuinfoU,
                                    b.dval,
                                    precond->U.num_rows,
                                    x->dval, 
                                    precond->U.num_rows);


             if (cusparseStatus != 0)   printf("error in U triangular solve.\n");

    cusparseDestroyMatDescr( descrU );
    cusparseDestroy( cusparseHandle );
    
    magma_device_sync();

    return MAGMA_SUCCESS;
}




/**
    Purpose
    -------

    Prepares the IC preconditioner via cuSPARSE.

    Arguments
    ---------

    @param[in]
    A           magma_c_sparse_matrix
                input matrix A

    @param[in,out]
    precond     magma_c_preconditioner*
                preconditioner parameters
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_chepr
    ********************************************************************/

extern "C" magma_int_t
magma_ccumiccsetup(
    magma_c_sparse_matrix A, magma_c_preconditioner *precond,
    magma_queue_t queue )
{
    magma_c_sparse_matrix hA, hACSR, U, hD, hR, hAt;
    magma_c_mtransfer( A, &hA, A.memory_location, Magma_CPU, queue );
    U.diagorder_type = Magma_VALUE;
    magma_c_mconvert( hA, &hACSR, hA.storage_type, Magma_CSR, queue );
    magma_c_mconvert( hACSR, &U, Magma_CSR, Magma_CSRL, queue );
    magma_c_mfree( &hACSR, queue );
    magma_c_mtransfer(U, &(precond->M), Magma_CPU, Magma_DEV, queue );

    // CUSPARSE context //
    cusparseHandle_t cusparseHandle;
    cusparseStatus_t cusparseStatus;
    cusparseStatus = cusparseCreate(&cusparseHandle);
    cusparseSetStream( cusparseHandle, queue );
     if (cusparseStatus != 0)    printf("error in Handle.\n");

    cusparseMatDescr_t descrA;
    cusparseStatus = cusparseCreateMatDescr(&descrA);
     if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

    cusparseStatus =
    cusparseSetMatType(descrA,CUSPARSE_MATRIX_TYPE_SYMMETRIC);
     if (cusparseStatus != 0)    printf("error in MatrType.\n");

    cusparseStatus =
    cusparseSetMatDiagType (descrA, CUSPARSE_DIAG_TYPE_NON_UNIT);
     if (cusparseStatus != 0)    printf("error in DiagType.\n");

    cusparseStatus =
    cusparseSetMatIndexBase(descrA,CUSPARSE_INDEX_BASE_ZERO);
     if (cusparseStatus != 0)    printf("error in IndexBase.\n");

    cusparseStatus =
    cusparseSetMatFillMode(descrA,CUSPARSE_FILL_MODE_LOWER);
     if (cusparseStatus != 0)    printf("error in fillmode.\n");


    cusparseStatus =
    cusparseCreateSolveAnalysisInfo( &(precond->cuinfo) );
     if (cusparseStatus != 0)    printf("error in info.\n");

    // end CUSPARSE context //

    cusparseStatus =
    cusparseCcsrsm_analysis( cusparseHandle, 
                CUSPARSE_OPERATION_NON_TRANSPOSE, 
                precond->M.num_rows, precond->M.nnz, descrA,
                precond->M.dval, precond->M.drow, precond->M.dcol, 
                precond->cuinfo); 

     if (cusparseStatus != 0)    printf("error in analysis IC.\n");

    cusparseStatus =
    cusparseCcsric0( cusparseHandle, CUSPARSE_OPERATION_NON_TRANSPOSE, 
                      precond->M.num_rows, descrA, 
                      precond->M.dval, 
                      precond->M.drow, 
                      precond->M.dcol, 
                      precond->cuinfo);

    cusparseStatus =
    cusparseDestroySolveAnalysisInfo( precond->cuinfo );
     if (cusparseStatus != 0)    printf("error in info-free.\n");

     if (cusparseStatus != 0)    printf("error in ICC.\n");

    cusparseMatDescr_t descrL;
    cusparseStatus = cusparseCreateMatDescr(&descrL);
     if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

    cusparseStatus =
    cusparseSetMatType(descrL,CUSPARSE_MATRIX_TYPE_TRIANGULAR);
     if (cusparseStatus != 0)    printf("error in MatrType.\n");

    cusparseStatus =
    cusparseSetMatDiagType (descrL, CUSPARSE_DIAG_TYPE_NON_UNIT);
     if (cusparseStatus != 0)    printf("error in DiagType.\n");

    cusparseStatus =
    cusparseSetMatIndexBase(descrL,CUSPARSE_INDEX_BASE_ZERO);
     if (cusparseStatus != 0)    printf("error in IndexBase.\n");

    cusparseStatus =
    cusparseSetMatFillMode(descrL,CUSPARSE_FILL_MODE_LOWER);
     if (cusparseStatus != 0)    printf("error in fillmode.\n");


    cusparseStatus = cusparseCreateSolveAnalysisInfo(&precond->cuinfoL); 
     if (cusparseStatus != 0)    printf("error in info.\n");

    cusparseStatus =
    cusparseCcsrsm_analysis(cusparseHandle, 
        CUSPARSE_OPERATION_NON_TRANSPOSE, precond->M.num_rows, 
        precond->M.nnz, descrL, 
        precond->M.dval, precond->M.drow, precond->M.dcol, precond->cuinfoL );
     if (cusparseStatus != 0)    printf("error in analysis L.\n");

    cusparseDestroyMatDescr( descrL );

    cusparseMatDescr_t descrU;
    cusparseStatus = cusparseCreateMatDescr(&descrU);
     if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

    cusparseStatus =
    cusparseSetMatType(descrU,CUSPARSE_MATRIX_TYPE_TRIANGULAR);
     if (cusparseStatus != 0)    printf("error in MatrType.\n");

    cusparseStatus =
    cusparseSetMatDiagType (descrU, CUSPARSE_DIAG_TYPE_NON_UNIT);
     if (cusparseStatus != 0)    printf("error in DiagType.\n");

    cusparseStatus =
    cusparseSetMatIndexBase(descrU,CUSPARSE_INDEX_BASE_ZERO);
     if (cusparseStatus != 0)    printf("error in IndexBase.\n");

    cusparseStatus =
    cusparseSetMatFillMode(descrU,CUSPARSE_FILL_MODE_LOWER);
     if (cusparseStatus != 0)    printf("error in fillmode.\n");

    cusparseStatus = cusparseCreateSolveAnalysisInfo(&precond->cuinfoU); 
     if (cusparseStatus != 0)    printf("error in info.\n");

    cusparseStatus =
    cusparseCcsrsm_analysis(cusparseHandle, 
        CUSPARSE_OPERATION_TRANSPOSE, precond->M.num_rows, 
        precond->M.nnz, descrU, 
        precond->M.dval, precond->M.drow, precond->M.dcol, precond->cuinfoU );
     if (cusparseStatus != 0)    printf("error in analysis U.\n");

    cusparseDestroyMatDescr( descrU );
    cusparseDestroyMatDescr( descrA );
    cusparseDestroy( cusparseHandle );

    magma_c_mfree(&U, queue );
    magma_c_mfree(&hA, queue );

/*
    // to enable also the block-asynchronous iteration for the triangular solves
    magma_c_mtransfer( precond->M, &hA, Magma_DEV, Magma_CPU, queue );
    hA.storage_type = Magma_CSR;

    magma_ccsrsplit( 256, hA, &hD, &hR, queue );

    magma_c_mtransfer( hD, &precond->LD, Magma_CPU, Magma_DEV, queue );
    magma_c_mtransfer( hR, &precond->L, Magma_CPU, Magma_DEV, queue );

    magma_c_mfree(&hD, queue );
    magma_c_mfree(&hR, queue );

    magma_c_cucsrtranspose(   hA, &hAt, queue );

    magma_ccsrsplit( 256, hAt, &hD, &hR, queue );

    magma_c_mtransfer( hD, &precond->UD, Magma_CPU, Magma_DEV, queue );
    magma_c_mtransfer( hR, &precond->U, Magma_CPU, Magma_DEV, queue );
    
    magma_c_mfree(&hD, queue );
    magma_c_mfree(&hR, queue );
    magma_c_mfree(&hA, queue );
    magma_c_mfree(&hAt, queue );
*/

    return MAGMA_SUCCESS;
}






/**
    Purpose
    -------

    Performs the left triangular solves using the ICC preconditioner.

    Arguments
    ---------

    @param[in]
    b           magma_c_vector
                RHS

    @param[in,out]
    x           magma_c_vector*
                vector to precondition

    @param[in,out]
    precond     magma_c_preconditioner*
                preconditioner parameters
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_chepr
    ********************************************************************/

extern "C" magma_int_t
magma_capplycumicc_l(
    magma_c_vector b, magma_c_vector *x, 
    magma_c_preconditioner *precond,
    magma_queue_t queue )
{
    magmaFloatComplex one = MAGMA_C_MAKE( 1.0, 0.0);

            // CUSPARSE context //
            cusparseHandle_t cusparseHandle;
            cusparseStatus_t cusparseStatus;
            cusparseStatus = cusparseCreate(&cusparseHandle);
            cusparseSetStream( cusparseHandle, queue );
             if (cusparseStatus != 0)    printf("error in Handle.\n");


            cusparseMatDescr_t descrL;
            cusparseStatus = cusparseCreateMatDescr(&descrL);
             if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

            cusparseStatus =
            cusparseSetMatType(descrL,CUSPARSE_MATRIX_TYPE_TRIANGULAR);
             if (cusparseStatus != 0)    printf("error in MatrType.\n");

            cusparseStatus =
            cusparseSetMatDiagType (descrL, CUSPARSE_DIAG_TYPE_NON_UNIT);
             if (cusparseStatus != 0)    printf("error in DiagType.\n");


            cusparseStatus =
            cusparseSetMatFillMode(descrL,CUSPARSE_FILL_MODE_LOWER);
             if (cusparseStatus != 0)    printf("error in fillmode.\n");

            cusparseStatus =
            cusparseSetMatIndexBase(descrL,CUSPARSE_INDEX_BASE_ZERO);
             if (cusparseStatus != 0)    printf("error in IndexBase.\n");


            // end CUSPARSE context //
            cusparseStatus =
            cusparseCcsrsm_solve(   cusparseHandle, 
                                    CUSPARSE_OPERATION_NON_TRANSPOSE, 
                                    precond->M.num_rows, 
                                    b.num_rows*b.num_cols/precond->M.num_rows, 
                                    &one, 
                                    descrL,
                                    precond->M.dval,
                                    precond->M.drow,
                                    precond->M.dcol,
                                    precond->cuinfoL,
                                    b.dval,
                                    precond->M.num_rows,
                                    x->dval, 
                                    precond->M.num_rows);


            if (cusparseStatus != 0)   
                printf("error in L triangular solve:%d.\n", precond->cuinfoL );


    cusparseDestroyMatDescr( descrL );
    cusparseDestroy( cusparseHandle );
    
    magma_device_sync();

    return MAGMA_SUCCESS;
}




/**
    Purpose
    -------

    Performs the right triangular solves using the ICC preconditioner.

    Arguments
    ---------

    @param[in]
    b           magma_c_vector
                RHS

    @param[in,out]
    x           magma_c_vector*
                vector to precondition

    @param[in,out]
    precond     magma_c_preconditioner*
                preconditioner parameters
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_chepr
    ********************************************************************/

extern "C" magma_int_t
magma_capplycumicc_r(
    magma_c_vector b, magma_c_vector *x, 
    magma_c_preconditioner *precond,
    magma_queue_t queue )
{
    magmaFloatComplex one = MAGMA_C_MAKE( 1.0, 0.0);

            // CUSPARSE context //
            cusparseHandle_t cusparseHandle;
            cusparseStatus_t cusparseStatus;
            cusparseStatus = cusparseCreate(&cusparseHandle);
            cusparseSetStream( cusparseHandle, queue );
             if (cusparseStatus != 0)    printf("error in Handle.\n");


            cusparseMatDescr_t descrU;
            cusparseStatus = cusparseCreateMatDescr(&descrU);
             if (cusparseStatus != 0)    printf("error in MatrDescr.\n");

            cusparseStatus =
            cusparseSetMatType(descrU,CUSPARSE_MATRIX_TYPE_TRIANGULAR);
             if (cusparseStatus != 0)    printf("error in MatrType.\n");

            cusparseStatus =
            cusparseSetMatDiagType (descrU, CUSPARSE_DIAG_TYPE_NON_UNIT);
             if (cusparseStatus != 0)    printf("error in DiagType.\n");

            cusparseStatus =
            cusparseSetMatIndexBase(descrU,CUSPARSE_INDEX_BASE_ZERO);
             if (cusparseStatus != 0)    printf("error in IndexBase.\n");


            cusparseStatus =
            cusparseSetMatFillMode(descrU,CUSPARSE_FILL_MODE_LOWER);
             if (cusparseStatus != 0)    printf("error in fillmode.\n");

            // end CUSPARSE context //
            magma_int_t dofs = precond->M.num_rows;


            cusparseStatus =
            cusparseCcsrsm_solve(   cusparseHandle, 
                                    CUSPARSE_OPERATION_TRANSPOSE, 
                                    precond->M.num_rows, 
                                    b.num_rows*b.num_cols/precond->M.num_rows, 
                                    &one, 
                                    descrU,
                                    precond->M.dval,
                                    precond->M.drow,
                                    precond->M.dcol,
                                    precond->cuinfoU,
                                    b.dval,
                                    precond->M.num_rows,
                                    x->dval, 
                                    precond->M.num_rows);
             if (cusparseStatus != 0)   
                 printf("error in U triangular solve:%d.\n", precond->cuinfoU );  


    cusparseDestroyMatDescr( descrU );
    cusparseDestroy( cusparseHandle );
    
    magma_device_sync();

    return MAGMA_SUCCESS;
}









