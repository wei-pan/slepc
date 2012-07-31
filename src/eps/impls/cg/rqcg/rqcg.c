/*                       

   SLEPc eigensolver: "rqcg"

   Method: Rayleigh Quotient Conjugate Gradient

   Algorithm:

       Conjugate Gradient minimization of the Rayleigh quotient with
       periodic Rayleigh-Ritz acceleration.

   References:

       [1] "Parallel preconditioned conjugate gradient optimization of the
           Rayleigh quotient for the solution of sparse eigenproblems",
           Appl. Math. Comput. 175, pp. 1694-1715 (2006).

   Last update: Jul 2012

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2011, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
      
   SLEPc is free software: you can redistribute it and/or modify it under  the
   terms of version 3 of the GNU Lesser General Public License as published by
   the Free Software Foundation.

   SLEPc  is  distributed in the hope that it will be useful, but WITHOUT  ANY 
   WARRANTY;  without even the implied warranty of MERCHANTABILITY or  FITNESS 
   FOR  A  PARTICULAR PURPOSE. See the GNU Lesser General Public  License  for 
   more details.

   You  should have received a copy of the GNU Lesser General  Public  License
   along with SLEPc. If not, see <http://www.gnu.org/licenses/>.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#include <slepc-private/epsimpl.h>                /*I "slepceps.h" I*/
#include <slepcblaslapack.h>

PetscErrorCode EPSSolve_RQCG(EPS);

typedef struct {
  PetscInt nrest;
  Vec      *AV,*BV,*P,*G;
} EPS_RQCG;

#undef __FUNCT__  
#define __FUNCT__ "EPSSetUp_RQCG"
PetscErrorCode EPSSetUp_RQCG(EPS eps)
{
  PetscErrorCode ierr;
  PetscBool      precond;
  EPS_RQCG       *ctx = (EPS_RQCG*)eps->data;

  PetscFunctionBegin;
  if (!eps->ishermitian) SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"RQCG only works for Hermitian problems"); 
  if (eps->ncv) { /* ncv set */
    if (eps->ncv<eps->nev) SETERRQ(((PetscObject)eps)->comm,1,"The value of ncv must be at least nev"); 
  }
  else if (eps->mpd) { /* mpd set */
    eps->ncv = PetscMin(eps->n,eps->nev+eps->mpd);
  }
  else { /* neither set: defaults depend on nev being small or large */
    if (eps->nev<500) eps->ncv = PetscMin(eps->n,PetscMax(2*eps->nev,eps->nev+15));
    else { eps->mpd = 500; eps->ncv = PetscMin(eps->n,eps->nev+eps->mpd); }
  }
  if (!eps->mpd) eps->mpd = eps->ncv;
  if (!eps->max_it) eps->max_it = PetscMax(100,2*eps->n/eps->ncv);
  if (!eps->which) eps->which = EPS_SMALLEST_REAL;
  if (eps->which!=EPS_SMALLEST_REAL) SETERRQ(((PetscObject)eps)->comm,1,"Wrong value of eps->which");
  if (!eps->extraction) {
    ierr = EPSSetExtraction(eps,EPS_RITZ);CHKERRQ(ierr);
  } else if (eps->extraction!=EPS_RITZ) SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"Unsupported extraction type\n");
  /* Set STPrecond as the default ST */
  if (!((PetscObject)eps->OP)->type_name) {
    ierr = STSetType(eps->OP,STPRECOND);CHKERRQ(ierr);
  }
  ierr = PetscObjectTypeCompare((PetscObject)eps->OP,STPRECOND,&precond);CHKERRQ(ierr);
  if (!precond) SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"RQCG only works with precond ST");

  if (!ctx->nrest) ctx->nrest = 20;

  ierr = EPSAllocateSolution(eps);CHKERRQ(ierr);
  ierr = VecDuplicateVecs(eps->t,eps->mpd,&ctx->AV);CHKERRQ(ierr);
  ierr = VecDuplicateVecs(eps->t,eps->mpd,&ctx->BV);CHKERRQ(ierr);
  ierr = VecDuplicateVecs(eps->t,eps->mpd,&ctx->P);CHKERRQ(ierr);
  ierr = VecDuplicateVecs(eps->t,eps->mpd,&ctx->G);CHKERRQ(ierr);
  ierr = DSSetType(eps->ds,DSHEP);CHKERRQ(ierr);
  ierr = DSAllocate(eps->ds,eps->ncv);CHKERRQ(ierr);
  ierr = EPSDefaultGetWork(eps,1);CHKERRQ(ierr);

  /* dispatch solve method */
  if (eps->leftvecs) SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"Left vectors not supported in this solver");
  eps->ops->solve = EPSSolve_RQCG;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSSolve_RQCG"
PetscErrorCode EPSSolve_RQCG(EPS eps)
{
  PetscErrorCode ierr;
  EPS_RQCG       *ctx = (EPS_RQCG*)eps->data;
  PetscInt       i,j,k,ld,off,nv,ncv = eps->ncv;
  PetscScalar    *C,*Y,*gamma,g,pap,pbp,pbx,pax,nu,mu,alpha,beta;
  PetscReal      resnorm,norm,a,b,c,disc;
  PetscBool      reset,breakdown;
  Mat            A,B;
  Vec            w=eps->work[0];

  PetscFunctionBegin;
  ierr = DSGetLeadingDimension(eps->ds,&ld);CHKERRQ(ierr);
  ierr = STGetOperators(eps->OP,&A,&B);CHKERRQ(ierr);
  ierr = PetscMalloc(eps->mpd*sizeof(PetscScalar),&gamma);CHKERRQ(ierr);

  /* Complete the initial basis with random vectors and orthonormalize them */
  k = eps->nini;
  while (k<ncv) {
    ierr = SlepcVecSetRandom(eps->V[k],eps->rand);CHKERRQ(ierr);
    ierr = IPOrthogonalize(eps->ip,eps->nds,eps->defl,k,PETSC_NULL,eps->V,eps->V[k],PETSC_NULL,&norm,&breakdown);CHKERRQ(ierr); 
    if (norm>0.0 && !breakdown) {
      ierr = VecScale(eps->V[k],1.0/norm);CHKERRQ(ierr);
      k++;
    }
  }

  while (eps->reason == EPS_CONVERGED_ITERATING) {
    eps->its++;
    nv = PetscMin(eps->nconv+eps->mpd,ncv);
    ierr = DSSetDimensions(eps->ds,nv,PETSC_IGNORE,eps->nconv,0);CHKERRQ(ierr);
    reset = (eps->its>1 && (eps->its-1)%ctx->nrest==0)? PETSC_TRUE: PETSC_FALSE;
    
    if (reset) {
      /* Compute Rayleigh quotient */
      ierr = DSGetArray(eps->ds,DS_MAT_A,&C);CHKERRQ(ierr);
      for (i=eps->nconv;i<nv;i++) {
        ierr = MatMult(A,eps->V[i],ctx->AV[i-eps->nconv]);CHKERRQ(ierr);
        ierr = VecMDot(ctx->AV[i-eps->nconv],i-eps->nconv+1,eps->V+eps->nconv,C+eps->nconv+i*ld);CHKERRQ(ierr);
        for (j=eps->nconv;j<i-1;j++) C[i+j*ld] = C[j+i*ld];
      }
      ierr = DSRestoreArray(eps->ds,DS_MAT_A,&C);CHKERRQ(ierr);
      ierr = DSSetState(eps->ds,DS_STATE_RAW);CHKERRQ(ierr);

      /* Solve projected problem */
      ierr = DSSolve(eps->ds,eps->eigr,eps->eigi);CHKERRQ(ierr);
      ierr = DSSort(eps->ds,eps->eigr,eps->eigi,PETSC_NULL,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
    
      /* Update vectors V(:,idx) = V * Y(:,idx) */
      ierr = DSGetArray(eps->ds,DS_MAT_Q,&Y);CHKERRQ(ierr);
      off = eps->nconv+eps->nconv*ld;
      ierr = SlepcUpdateVectors(nv-eps->nconv,ctx->AV,0,nv-eps->nconv,Y+off,ld,PETSC_FALSE);CHKERRQ(ierr);
      ierr = SlepcUpdateVectors(nv-eps->nconv,eps->V+eps->nconv,0,nv-eps->nconv,Y+off,ld,PETSC_FALSE);CHKERRQ(ierr);
      ierr = DSRestoreArray(eps->ds,DS_MAT_Q,&Y);CHKERRQ(ierr);

    } else {
      /* No need to do Rayleigh-Ritz, just take diag(V'*A*V) */
      for (i=eps->nconv;i<nv;i++) {
        ierr = MatMult(A,eps->V[i],ctx->AV[i-eps->nconv]);CHKERRQ(ierr);
        ierr = VecDot(ctx->AV[i-eps->nconv],eps->V[i],eps->eigr+i);CHKERRQ(ierr);
      }
    }
    
    /* Compute gradient and check convergence */
    k = -1;
    for (i=eps->nconv;i<nv;i++) {
      ierr = MatMult(B,eps->V[i],ctx->BV[i-eps->nconv]);CHKERRQ(ierr);
      ierr = VecWAXPY(ctx->G[i-eps->nconv],-eps->eigr[i],ctx->BV[i-eps->nconv],ctx->AV[i-eps->nconv]);CHKERRQ(ierr);
      ierr = VecNorm(ctx->G[i-eps->nconv],NORM_2,&resnorm);CHKERRQ(ierr);
      ierr = (*eps->conv_func)(eps,eps->eigr[i],0.0,resnorm,&eps->errest[i],eps->conv_ctx);CHKERRQ(ierr);
      if (k==-1 && eps->errest[i] >= eps->tol) k = i;
    }
    if (eps->its >= eps->max_it) eps->reason = EPS_DIVERGED_ITS;
    if (k >= eps->nev) eps->reason = EPS_CONVERGED_TOL;
  
    if (eps->reason == EPS_CONVERGED_ITERATING) {

      /* Search direction */
      for (i=0;i<nv-eps->nconv;i++) {
        ierr = STAssociatedKSPSolve(eps->OP,ctx->G[i],w);CHKERRQ(ierr);
        ierr = VecDot(ctx->G[i],w,&g);CHKERRQ(ierr);
        beta = (!reset && eps->its>1)? g/gamma[i]: 0.0;
        gamma[i] = g;
        ierr = VecAXPBY(ctx->P[i],1.0,beta,w);CHKERRQ(ierr);
        ierr = IPOrthogonalize(eps->ip,eps->nds,eps->defl,i+eps->nconv,PETSC_NULL,eps->V,ctx->P[i],PETSC_NULL,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
      }

      /* Minimization problem */
      for (i=eps->nconv;i<nv;i++) {
        ierr = VecDot(eps->V[i],ctx->AV[i-eps->nconv],&nu);CHKERRQ(ierr);
        ierr = VecDot(ctx->P[i-eps->nconv],ctx->AV[i-eps->nconv],&pax);CHKERRQ(ierr);
        ierr = VecDot(ctx->P[i-eps->nconv],ctx->BV[i-eps->nconv],&pbx);CHKERRQ(ierr);
        ierr = VecDot(eps->V[i],ctx->BV[i-eps->nconv],&mu);CHKERRQ(ierr);
        ierr = MatMult(A,ctx->P[i-eps->nconv],w);CHKERRQ(ierr);
        ierr = VecDot(ctx->P[i-eps->nconv],w,&pap);CHKERRQ(ierr);
        ierr = MatMult(B,ctx->P[i-eps->nconv],w);CHKERRQ(ierr);
        ierr = VecDot(ctx->P[i-eps->nconv],w,&pbp);CHKERRQ(ierr);
        a = PetscRealPart(pap*pbx-pax*pbp);
        b = PetscRealPart(nu*pbp-mu*pap);
        c = PetscRealPart(mu*pax-nu*pbx);
        disc = b*b-4.0*a*c;
        if (b>=0.0 && a!=0.0) alpha = (b+PetscSqrtReal(disc))/(2.0*a);
        else alpha = 2.0*c/(b-PetscSqrtReal(disc));

        /* Next iterate */
        ierr = VecAXPY(eps->V[i],alpha,ctx->P[i-eps->nconv]);CHKERRQ(ierr);
        ierr = IPOrthogonalize(eps->ip,eps->nds,eps->defl,i,PETSC_NULL,eps->V,eps->V[i],PETSC_NULL,&norm,PETSC_NULL);CHKERRQ(ierr);
        ierr = VecScale(eps->V[i],1.0/norm);CHKERRQ(ierr);
      }
    }
    
    ierr = EPSMonitor(eps,eps->its,k,eps->eigr,eps->eigi,eps->errest,nv);CHKERRQ(ierr);
    eps->nconv = k;
  }
    
  ierr = PetscFree(gamma);CHKERRQ(ierr);
  /* truncate Schur decomposition and change the state to raw so that
     PSVectors() computes eigenvectors from scratch */
  ierr = DSSetDimensions(eps->ds,eps->nconv,PETSC_IGNORE,0,0);CHKERRQ(ierr);
  ierr = DSSetState(eps->ds,DS_STATE_RAW);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "EPSRQCGSetReset_RQCG"
PetscErrorCode EPSRQCGSetReset_RQCG(EPS eps,PetscInt nrest)
{
  EPS_RQCG *ctx = (EPS_RQCG*)eps->data;

  PetscFunctionBegin;
  ctx->nrest = nrest;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "EPSRQCGSetReset"
/*@
   EPSRQCGSetReset - Sets the reset parameter of the RQCG iteration. Every
   nrest iterations, the solver performs a Rayleigh-Ritz projection step.

   Logically Collective on EPS

   Input Parameters:
+  eps - the eigenproblem solver context
-  nrest - the number of iterations between resets

   Options Database Key:
.  -eps_rqcg_reset - Sets the reset parameter
   
   Level: advanced

.seealso: EPSRQCGGetReset()
@*/
PetscErrorCode EPSRQCGSetReset(EPS eps,PetscInt nrest)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidLogicalCollectiveInt(eps,nrest,2);
  ierr = PetscTryMethod(eps,"EPSRQCGSetReset_C",(EPS,PetscInt),(eps,nrest));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "EPSRQCGGetReset_RQCG"
PetscErrorCode EPSRQCGGetReset_RQCG(EPS eps,PetscInt *nrest)
{
  EPS_RQCG *ctx = (EPS_RQCG*)eps->data;

  PetscFunctionBegin;
  *nrest = ctx->nrest;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "EPSRQCGGetReset"
/*@C
   EPSRQCGGetReset - Gets the reset parameter used in the RQCG method. 

   Not Collective

   Input Parameter:
.  eps - the eigenproblem solver context

   Output Parameter:
.  nrest - the reset parameter

   Level: advanced

.seealso: EPSRQCGSetReset()
@*/
PetscErrorCode EPSRQCGGetReset(EPS eps,PetscInt *nrest)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidPointer(nrest,2);
  ierr = PetscTryMethod(eps,"EPSRQCGGetReset_C",(EPS,PetscInt*),(eps,nrest));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSReset_RQCG"
PetscErrorCode EPSReset_RQCG(EPS eps)
{
  PetscErrorCode ierr;
  EPS_RQCG       *ctx = (EPS_RQCG*)eps->data;

  PetscFunctionBegin;
  ierr = VecDestroyVecs(eps->ncv,&ctx->AV);CHKERRQ(ierr);
  ierr = VecDestroyVecs(eps->ncv,&ctx->BV);CHKERRQ(ierr);
  ierr = VecDestroyVecs(eps->ncv,&ctx->P);CHKERRQ(ierr);
  ierr = VecDestroyVecs(eps->ncv,&ctx->G);CHKERRQ(ierr);
  ierr = EPSReset_Default(eps);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSSetFromOptions_RQCG"
PetscErrorCode EPSSetFromOptions_RQCG(EPS eps)
{
  PetscErrorCode ierr;
  PetscBool      flg;
  PetscInt       nrest;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("EPS RQCG Options");CHKERRQ(ierr);
  ierr = PetscOptionsInt("-eps_rqcg_reset","RQCG reset parameter","EPSRQCGSetReset",20,&nrest,&flg);CHKERRQ(ierr);
  if (flg) { ierr = EPSRQCGSetReset(eps,nrest);CHKERRQ(ierr); }
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDestroy_RQCG"
PetscErrorCode EPSDestroy_RQCG(EPS eps)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFree(eps->data);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)eps,"EPSRQCGSetReset_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)eps,"EPSRQCGGetReset_C","",PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSView_RQCG"
PetscErrorCode EPSView_RQCG(EPS eps,PetscViewer viewer)
{
  PetscErrorCode ierr;
  EPS_RQCG       *ctx = (EPS_RQCG*)eps->data;
  PetscBool      isascii;

  PetscFunctionBegin;
  ierr = PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii);CHKERRQ(ierr);
  if (!isascii) SETERRQ1(((PetscObject)eps)->comm,1,"Viewer type %s not supported for EPS RQCG",((PetscObject)viewer)->type_name);
  ierr = PetscViewerASCIIPrintf(viewer,"  RQCG: reset every %d iterations\n",ctx->nrest);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "EPSCreate_RQCG"
PetscErrorCode EPSCreate_RQCG(EPS eps)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscNewLog(eps,EPS_RQCG,&eps->data);CHKERRQ(ierr);
  eps->ops->setup                = EPSSetUp_RQCG;
  eps->ops->setfromoptions       = EPSSetFromOptions_RQCG;
  eps->ops->destroy              = EPSDestroy_RQCG;
  eps->ops->reset                = EPSReset_RQCG;
  eps->ops->view                 = EPSView_RQCG;
  eps->ops->backtransform        = EPSBackTransform_Default;
  eps->ops->computevectors       = EPSComputeVectors_Default;
  ierr = STSetType(eps->OP,STPRECOND);CHKERRQ(ierr);
  ierr = STPrecondSetKSPHasMat(eps->OP,PETSC_TRUE);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)eps,"EPSRQCGSetReset_C","EPSRQCGSetReset_RQCG",EPSRQCGSetReset_RQCG);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)eps,"EPSRQCGGetReset_C","EPSRQCGGetReset_RQCG",EPSRQCGGetReset_RQCG);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END
