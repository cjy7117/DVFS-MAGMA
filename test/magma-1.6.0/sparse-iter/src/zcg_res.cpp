/*
    -- MAGMA (version 1.6.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2014

       @author Hartwig Anzt 

       @precisions normal z -> s d c
*/

#include "common_magma.h"
#include "magmasparse.h"

#include <assert.h>

#define RTOLERANCE     lapackf77_dlamch( "E" )
#define ATOLERANCE     lapackf77_dlamch( "E" )


/**
    Purpose
    -------

    Solves a system of linear equations
       A * X = B
    where A is a complex Hermitian N-by-N positive definite matrix A.
    This is a GPU implementation of the Conjugate Gradient method.

    Arguments
    ---------

    @param[in]
    A           magma_z_sparse_matrix
                input matrix A

    @param[in]
    b           magma_z_vector
                RHS b

    @param[in,out]
    x           magma_z_vector*
                solution approximation

    @param[in,out]
    solver_par  magma_z_solver_par*
                solver parameters
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_zhesv
    ********************************************************************/

extern "C" magma_int_t
magma_zcg_res(
    magma_z_sparse_matrix A, magma_z_vector b, magma_z_vector *x,  
    magma_z_solver_par *solver_par,
    magma_queue_t queue )
{
    // set queue for old dense routines
    magma_queue_t orig_queue;
    magmablasGetKernelStream( &orig_queue );

    // prepare solver feedback
    solver_par->solver = Magma_CG;
    solver_par->numiter = 0;
    solver_par->info = MAGMA_SUCCESS; 

    // local variables
    magmaDoubleComplex c_zero = MAGMA_Z_ZERO, c_one = MAGMA_Z_ONE;
    
    magma_int_t dofs = A.num_rows;

    // GPU workspace
    magma_z_vector r, p, q;
    magma_z_vinit( &r, Magma_DEV, dofs, c_zero, queue );
    magma_z_vinit( &p, Magma_DEV, dofs, c_zero, queue );
    magma_z_vinit( &q, Magma_DEV, dofs, c_zero, queue );
    
    // solver variables
    magmaDoubleComplex alpha, beta;
    double nom, nom0, r0, betanom, betanomsq, den, res;

    // solver setup
    magma_zscal( dofs, c_zero, x->dval, 1) ;                     // x = 0
    magma_zcopy( dofs, b.dval, 1, r.dval, 1 );                    // r = b
    magma_zcopy( dofs, b.dval, 1, p.dval, 1 );                    // p = b
    nom0 = betanom = magma_dznrm2( dofs, r.dval, 1 );           
    nom  = nom0 * nom0;                                // nom = r' * r
    magma_z_spmv( c_one, A, p, c_zero, q, queue );                     // q = A p
    den = MAGMA_Z_REAL( magma_zdotc(dofs, p.dval, 1, q.dval, 1) );// den = p dot q
    solver_par->init_res = nom0;
    if ( (r0 = nom * solver_par->epsilon) < ATOLERANCE ) 
        r0 = ATOLERANCE;
    if ( nom < r0 ) {
        magmablasSetKernelStream( orig_queue );
        return MAGMA_SUCCESS;
    }
    // check positive definite
    if (den <= 0.0) {
        printf("Operator A is not postive definite. (Ar,r) = %f\n", den);
        magmablasSetKernelStream( orig_queue );
        return MAGMA_NONSPD;
        solver_par->info = MAGMA_NONSPD;;
    }

    //Chronometry
    real_Double_t tempo1, tempo2;
    tempo1 = magma_sync_wtime( queue );
    if ( solver_par->verbose > 0 ) {
        solver_par->res_vec[0] = (real_Double_t)nom0;
        solver_par->timing[0] = 0.0;
    }
    
    // start iteration
    for( solver_par->numiter= 1; solver_par->numiter<solver_par->maxiter; 
                                                    solver_par->numiter++ ) {
        alpha = MAGMA_Z_MAKE(nom/den, 0.);
        magma_zaxpy(dofs,  alpha, p.dval, 1, x->dval, 1);     // x = x + alpha p
        magma_zaxpy(dofs, -alpha, q.dval, 1, r.dval, 1);      // r = r - alpha q
        res = betanom = magma_dznrm2(dofs, r.dval, 1);       // betanom = || r ||
        betanomsq = betanom * betanom;                      // betanoms = r' * r

        if ( solver_par->verbose > 0 ) {
            tempo2 = magma_sync_wtime( queue );
            if ( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) res;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) tempo2-tempo1;
            }
        }

        if (  res/nom0  < solver_par->epsilon ) {
            break;
        }

        beta = MAGMA_Z_MAKE(betanomsq/nom, 0.);           // beta = betanoms/nom
        magma_zscal(dofs, beta, p.dval, 1);                // p = beta*p
        magma_zaxpy(dofs, c_one, r.dval, 1, p.dval, 1);     // p = p + r 
        magma_z_spmv( c_one, A, p, c_zero, q, queue );           // q = A p
        den = MAGMA_Z_REAL(magma_zdotc(dofs, p.dval, 1, q.dval, 1));    
                // den = p dot q
        nom = betanomsq;
    } 
    tempo2 = magma_sync_wtime( queue );
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    double residual;
    magma_zresidual( A, b, *x, &residual, queue );
    solver_par->iter_res = res;
    solver_par->final_res = residual;

    if ( solver_par->numiter < solver_par->maxiter) {
        solver_par->info = MAGMA_SUCCESS;
    } else if ( solver_par->init_res > solver_par->final_res ) {
        if ( solver_par->verbose > 0 ) {
            if ( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) betanom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        solver_par->info = MAGMA_SLOW_CONVERGENCE;
    }
    else {
        if ( solver_par->verbose > 0 ) {
            if ( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) betanom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose] 
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        solver_par->info = MAGMA_DIVERGENCE;
    }
    magma_z_vfree(&r, queue );
    magma_z_vfree(&p, queue );
    magma_z_vfree(&q, queue );

    magmablasSetKernelStream( orig_queue );
    return MAGMA_SUCCESS;
}   /* magma_zcg */


