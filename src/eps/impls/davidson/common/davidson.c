/*
  Method: General Davidson Method (includes GD and JD)

  References:
    - Ernest R. Davidson. Super-matrix methods. Computer Physics Communications,
      53:49–60, May 1989.

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2010, Universidad Politecnica de Valencia, Spain

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

#include "davidson.h"

PetscErrorCode EPSView_Davidson(EPS eps,PetscViewer viewer);

typedef struct {
  /**** Solver options ****/
  PetscInt blocksize,     /* block size */
    initialsize,          /* initial size of V */
    minv,                 /* size of V after restarting */
    plusk;                /* keep plusk eigenvectors from the last iteration */
  PetscBool  ipB;         /* true if V'B*V=I */
  PetscInt   method;      /* method for improving the approximate solution */
  PetscReal  fix;         /* the fix parameter */
  PetscBool  krylovstart; /* true if the starting subspace is a Krylov basis */

  /**** Solver data ****/
  dvdDashboard ddb;

  /**** Things to destroy ****/
  PetscScalar *wS;
  Vec         *wV;
  PetscInt    size_wV;
} EPS_DAVIDSON;

#undef __FUNCT__  
#define __FUNCT__ "EPSCreate_Davidson"
PetscErrorCode EPSCreate_Davidson(EPS eps)
{
  PetscErrorCode ierr;
  EPS_DAVIDSON   *data;

  PetscFunctionBegin;
  ierr = STSetType(eps->OP,STPRECOND);CHKERRQ(ierr);
  ierr = STPrecondSetKSPHasMat(eps->OP,PETSC_FALSE);CHKERRQ(ierr);

  eps->OP->ops->getbilinearform  = STGetBilinearForm_Default;
  eps->ops->solve                = EPSSolve_Davidson;
  eps->ops->setup                = EPSSetUp_Davidson;
  eps->ops->reset                = EPSReset_Davidson;
  eps->ops->backtransform        = EPSBackTransform_Default;
  eps->ops->computevectors       = EPSComputeVectors_QZ;
  eps->ops->view                 = EPSView_Davidson;

  ierr = PetscMalloc(sizeof(EPS_DAVIDSON),&data);CHKERRQ(ierr);
  eps->data = data;

  /* Set default values */
  ierr = EPSDavidsonSetKrylovStart_Davidson(eps,PETSC_FALSE);CHKERRQ(ierr);
  ierr = EPSDavidsonSetBlockSize_Davidson(eps,1);CHKERRQ(ierr);
  ierr = EPSDavidsonSetRestart_Davidson(eps,6,0);CHKERRQ(ierr);
  ierr = EPSDavidsonSetInitialSize_Davidson(eps,5);CHKERRQ(ierr);
  ierr = EPSDavidsonSetFix_Davidson(eps,0.01);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSSetUp_Davidson"
PetscErrorCode EPSSetUp_Davidson(EPS eps)
{
  PetscErrorCode ierr;
  EPS_DAVIDSON   *data = (EPS_DAVIDSON*)eps->data;
  dvdDashboard   *dvd = &data->ddb;
  dvdBlackboard  b;
  PetscInt       nvecs,nscalars,min_size_V,plusk,bs,initv;
  Mat            A,B;
  KSP            ksp;
  PetscBool      t,ipB,ispositive;
  HarmType_t     harm;
  InitType_t     init;
  PetscReal      fix;
  Vec            tvec;

  PetscFunctionBegin;
  /* Setup EPS options and get the problem specification */
  ierr = EPSDavidsonGetBlockSize_Davidson(eps,&bs);CHKERRQ(ierr);
  if (bs <= 0) bs = 1;
  if(eps->ncv) {
    if (eps->ncv<eps->nev) SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"The value of ncv must be at least nev"); 
  } else if (eps->mpd) eps->ncv = eps->mpd + eps->nev + bs;
  else if (eps->nev<500)
    eps->ncv = PetscMin(eps->n,PetscMax(2*eps->nev,eps->nev+15))+bs;
  else
    eps->ncv = PetscMin(eps->n,eps->nev+500)+bs;
  if (!eps->mpd) eps->mpd = eps->ncv;
  if (eps->mpd > eps->ncv)
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"The mpd has to be less or equal than ncv");
  if (eps->mpd < 2)
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"The mpd has to be greater than 2");
  if (!eps->max_it) eps->max_it = PetscMax(100,2*eps->n/eps->ncv);
  if (!eps->which) eps->which = EPS_LARGEST_MAGNITUDE;
  if (eps->ishermitian && (eps->which==EPS_LARGEST_IMAGINARY || eps->which==EPS_SMALLEST_IMAGINARY))
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"Wrong value of eps->which");
  if (!(eps->nev + bs <= eps->ncv))
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"The ncv has to be greater than nev plus blocksize!");

  ierr = EPSDavidsonGetRestart_Davidson(eps,&min_size_V,&plusk);CHKERRQ(ierr);
  if (!min_size_V) min_size_V = PetscMin(PetscMax(bs,5),eps->mpd/2);
  if (!(min_size_V+bs <= eps->mpd))
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"The value of minv must be less than mpd minus blocksize");
  ierr = EPSDavidsonGetInitialSize_Davidson(eps,&initv);CHKERRQ(ierr);
  if (eps->mpd < initv)
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"The initv has to be less or equal than mpd");

  /* Davidson solvers do not support left eigenvectors */
  if (eps->leftvecs) SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"Left vectors not supported in this solver");

  /* Change the default sigma to inf if necessary */
  if (eps->which == EPS_LARGEST_MAGNITUDE || eps->which == EPS_LARGEST_REAL ||
      eps->which == EPS_LARGEST_IMAGINARY) {
    ierr = STSetDefaultShift(eps->OP,3e300);CHKERRQ(ierr);
  }
 
  /* Davidson solvers only support STPRECOND */
  ierr = STSetUp(eps->OP);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)eps->OP,STPRECOND,&t);CHKERRQ(ierr);
  if (!t) SETERRQ1(((PetscObject)eps)->comm,PETSC_ERR_SUP,"%s only works with precond spectral transformation",
    ((PetscObject)eps)->type_name);

  /* Setup problem specification in dvd */
  ierr = STGetOperators(eps->OP,&A,&B);CHKERRQ(ierr);
  ierr = PetscMemzero(dvd,sizeof(dvdDashboard));CHKERRQ(ierr);
  dvd->A = A; dvd->B = eps->isgeneralized? B : PETSC_NULL;
  ispositive = eps->ispositive;
  dvd->sA = DVD_MAT_IMPLICIT |
            (eps->ishermitian? DVD_MAT_HERMITIAN : 0) |
            ((ispositive && !eps->isgeneralized) ? DVD_MAT_POS_DEF : 0);
  /* Asume -eps_hermitian means hermitian-definite in generalized problems */
  if (!ispositive && !eps->isgeneralized && eps->ishermitian) ispositive = PETSC_TRUE;
  if (!eps->isgeneralized)
    dvd->sB = DVD_MAT_IMPLICIT | DVD_MAT_HERMITIAN | DVD_MAT_IDENTITY |
              DVD_MAT_UNITARY | DVD_MAT_POS_DEF;
  else 
    dvd->sB = DVD_MAT_IMPLICIT |
              (eps->ishermitian? DVD_MAT_HERMITIAN : 0) |
              (ispositive? DVD_MAT_POS_DEF : 0);
  ipB = DVD_IS(dvd->sB,DVD_MAT_POS_DEF)?PETSC_TRUE:PETSC_FALSE;
  dvd->sEP = ((!eps->isgeneralized || (eps->isgeneralized && ipB))? DVD_EP_STD : 0) |
             (ispositive? DVD_EP_HERMITIAN : 0);
  dvd->nev = eps->nev;
  dvd->which = eps->which;
  switch(eps->which) {
  case EPS_TARGET_MAGNITUDE:
  case EPS_TARGET_REAL:
  case EPS_TARGET_IMAGINARY:
    dvd->withTarget = PETSC_TRUE;
    dvd->target[0] = eps->target; dvd->target[1] = 1.0;
    break;

  case EPS_LARGEST_MAGNITUDE:
  case EPS_LARGEST_REAL:
  case EPS_LARGEST_IMAGINARY: //TODO: think about this case
  default:
    dvd->withTarget = PETSC_TRUE;
    dvd->target[0] = 1.0; dvd->target[1] = 0.0;
    break;
 
  case EPS_SMALLEST_MAGNITUDE:
  case EPS_SMALLEST_REAL:
  case EPS_SMALLEST_IMAGINARY: //TODO: think about this case
    dvd->withTarget = PETSC_TRUE;
    dvd->target[0] = 0.0; dvd->target[1] = 1.0;
  }
  dvd->tol = eps->tol;
  dvd->eps = eps;

  /* Setup the extraction technique */
  if (!eps->extraction) {
    ierr = EPSSetExtraction(eps,EPS_RITZ);CHKERRQ(ierr);
  }
  switch(eps->extraction) {
  case EPS_RITZ:              harm = DVD_HARM_NONE; break;
  case EPS_HARMONIC:          harm = DVD_HARM_RR; break;
  case EPS_HARMONIC_RELATIVE: harm = DVD_HARM_RRR; break;
  case EPS_HARMONIC_RIGHT:    harm = DVD_HARM_REIGS; break;
  case EPS_HARMONIC_LARGEST:  harm = DVD_HARM_LEIGS; break;
  default: SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_SUP,"Unsupported extraction type");
  }

  /* Setup the type of starting subspace */
  ierr = EPSDavidsonGetKrylovStart_Davidson(eps,&t);CHKERRQ(ierr);
  init = (!t)? DVD_INITV_CLASSIC : DVD_INITV_KRYLOV;

  /* Setup IP */
  if (ipB && dvd->B) {
    ierr = IPSetMatrix(eps->ip,dvd->B);CHKERRQ(ierr);
  } else {
    ierr = IPSetMatrix(eps->ip,PETSC_NULL);CHKERRQ(ierr);
  }

  /* Get the fix parameter */
  ierr = EPSDavidsonGetFix_Davidson(eps,&fix);CHKERRQ(ierr);

  /* Orthonormalize the DS */
  ierr = dvd_orthV(eps->ip,PETSC_NULL,0,PETSC_NULL,0,eps->DS,0,
                   PetscAbs(eps->nds),PETSC_NULL,0,eps->rand);CHKERRQ(ierr);

  /* Preconfigure dvd */
  ierr = STGetKSP(eps->OP,&ksp);CHKERRQ(ierr);
  ierr = dvd_schm_basic_preconf(dvd,&b,eps->ncv,eps->mpd,min_size_V,bs,
                                initv,
                                PetscAbs(eps->nini),
                                plusk,harm,
                                PETSC_NULL,init,eps->trackall);CHKERRQ(ierr);

  /* Allocate memory */
  nvecs = b.max_size_auxV + b.own_vecs;
  nscalars = b.own_scalars + b.max_size_auxS;
  ierr = PetscMalloc(nscalars*sizeof(PetscScalar),&data->wS);CHKERRQ(ierr);
  ierr = VecCreateMPIWithArray(((PetscObject)eps)->comm,eps->nloc,PETSC_DECIDE,PETSC_NULL,&tvec);CHKERRQ(ierr);
  ierr = SlepcVecDuplicateVecs(tvec,nvecs,&data->wV);CHKERRQ(ierr);
  ierr = VecDestroy(&tvec);CHKERRQ(ierr);
  data->size_wV = nvecs;
  b.free_vecs = data->wV;
  b.free_scalars = data->wS;
  dvd->auxV = data->wV + b.own_vecs;
  dvd->auxS = b.free_scalars + b.own_scalars;
  dvd->size_auxV = b.max_size_auxV;
  dvd->size_auxS = b.max_size_auxS;

  /* Configure dvd for a basic GD */
  ierr = dvd_schm_basic_conf(dvd,&b,eps->ncv,eps->mpd,min_size_V,bs,
                             initv,
                             PetscAbs(eps->nini),plusk,
                             eps->ip,harm,dvd->withTarget,
                             eps->target,ksp,
                             fix,init,eps->trackall);CHKERRQ(ierr);

  /* Associate the eigenvalues to the EPS */
  eps->eigr = dvd->eigr;
  eps->eigi = dvd->eigi;
  eps->errest = dvd->errest;
  eps->V = dvd->V;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSSolve_Davidson"
PetscErrorCode EPSSolve_Davidson(EPS eps)
{
  EPS_DAVIDSON   *data = (EPS_DAVIDSON*)eps->data;
  dvdDashboard   *d = &data->ddb;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* Call the starting routines */
  DVD_FL_CALL(d->startList,d);

  for(eps->its=0; eps->its < eps->max_it; eps->its++) {
    /* Initialize V, if it is needed */
    if (d->size_V == 0) { ierr = d->initV(d);CHKERRQ(ierr); }

    /* Find the best approximated eigenpairs in V, X */
    ierr = d->calcPairs(d);CHKERRQ(ierr);

    /* Expand the subspace */
    ierr = d->updateV(d);CHKERRQ(ierr);

    /* Monitor progress */
    eps->nconv = d->nconv;
    ierr = EPSMonitor(eps,eps->its+1,eps->nconv,eps->eigr,eps->eigi,eps->errest,d->size_H+d->nconv);CHKERRQ(ierr);

    /* Test for convergence */
    if (eps->nconv >= eps->nev) break;
  }

  /* Call the ending routines */
  DVD_FL_CALL(d->endList,d);

  if (eps->nconv >= eps->nev) eps->reason = EPS_CONVERGED_TOL;
  else eps->reason = EPS_DIVERGED_ITS;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSReset_Davidson"
PetscErrorCode EPSReset_Davidson(EPS eps)
{
  EPS_DAVIDSON   *data = (EPS_DAVIDSON*)eps->data;
  dvdDashboard   *dvd = &data->ddb;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* Call step destructors and destroys the list */
  DVD_FL_CALL(dvd->destroyList,dvd);
  DVD_FL_DEL(dvd->destroyList);
  DVD_FL_DEL(dvd->startList);
  DVD_FL_DEL(dvd->endList);

  ierr = SlepcVecDestroyVecs(data->size_wV,&data->wV);CHKERRQ(ierr);
  ierr = PetscFree(data->wS);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSView_Davidson"
PetscErrorCode EPSView_Davidson(EPS eps,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscBool      isascii,opb;
  PetscInt       opi,opi0;
  const char*    name;

  PetscFunctionBegin;
  name = ((PetscObject)eps)->type_name;
  ierr = PetscTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii);CHKERRQ(ierr);
  if (!isascii) {
    SETERRQ2(((PetscObject)eps)->comm,1,"Viewer type %s not supported for %s",((PetscObject)viewer)->type_name,name);
  }
  
  ierr = EPSDavidsonGetBlockSize_Davidson(eps,&opi);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  Davidson: block size=%d\n",opi);CHKERRQ(ierr);
  ierr = EPSDavidsonGetKrylovStart_Davidson(eps,&opb);CHKERRQ(ierr);
  if(!opb) {
    ierr = PetscViewerASCIIPrintf(viewer,"  Davidson: type of the initial subspace: non-Krylov\n");CHKERRQ(ierr);
  } else {
    ierr = PetscViewerASCIIPrintf(viewer,"  Davidson: type of the initial subspace: Krylov\n");CHKERRQ(ierr);
  }
  ierr = EPSDavidsonGetRestart_Davidson(eps,&opi,&opi0);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  Davidson: size of the subspace after restarting: %d\n",opi);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer,"  Davidson: number of vectors after restarting from the previous iteration: %d\n",opi0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonSetKrylovStart_Davidson"
PetscErrorCode EPSDavidsonSetKrylovStart_Davidson(EPS eps,PetscBool krylovstart)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  data->krylovstart = krylovstart;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonGetKrylovStart_Davidson"
PetscErrorCode EPSDavidsonGetKrylovStart_Davidson(EPS eps,PetscBool *krylovstart)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  *krylovstart = data->krylovstart;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonSetBlockSize_Davidson"
PetscErrorCode EPSDavidsonSetBlockSize_Davidson(EPS eps,PetscInt blocksize)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  if(blocksize == PETSC_DEFAULT || blocksize == PETSC_DECIDE) blocksize = 1;
  if(blocksize <= 0)
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Invalid blocksize value");
  data->blocksize = blocksize;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonGetBlockSize_Davidson"
PetscErrorCode EPSDavidsonGetBlockSize_Davidson(EPS eps,PetscInt *blocksize)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  *blocksize = data->blocksize;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonSetRestart_Davidson"
PetscErrorCode EPSDavidsonSetRestart_Davidson(EPS eps,PetscInt minv,PetscInt plusk)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  if(minv == PETSC_DEFAULT || minv == PETSC_DECIDE) minv = 5;
  if(minv <= 0)
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Invalid minv value");
  if(plusk == PETSC_DEFAULT || plusk == PETSC_DECIDE) plusk = 5;
  if(plusk < 0)
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Invalid plusk value");
  data->minv = minv;
  data->plusk = plusk;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonGetRestart_Davidson"
PetscErrorCode EPSDavidsonGetRestart_Davidson(EPS eps,PetscInt *minv,PetscInt *plusk)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  *minv = data->minv;
  *plusk = data->plusk;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonGetInitialSize_Davidson"
PetscErrorCode EPSDavidsonGetInitialSize_Davidson(EPS eps,PetscInt *initialsize)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  *initialsize = data->initialsize;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonSetInitialSize_Davidson"
PetscErrorCode EPSDavidsonSetInitialSize_Davidson(EPS eps,PetscInt initialsize)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  if(initialsize == PETSC_DEFAULT || initialsize == PETSC_DECIDE) initialsize = 5;
  if(initialsize <= 0)
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Invalid initial size value");
  data->initialsize = initialsize;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonGetFix_Davidson"
PetscErrorCode EPSDavidsonGetFix_Davidson(EPS eps,PetscReal *fix)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  *fix = data->fix;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSDavidsonSetFix_Davidson"
PetscErrorCode EPSDavidsonSetFix_Davidson(EPS eps,PetscReal fix)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  if(fix == PETSC_DEFAULT || fix == PETSC_DECIDE) fix = 0.01;
  if(fix < 0.0)
    SETERRQ(((PetscObject)eps)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Invalid fix value");
  data->fix = fix;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "EPSComputeVectors_QZ"
/*
  EPSComputeVectors_QZ - Compute eigenvectors from the vectors
  provided by the eigensolver. This version is intended for solvers 
  that provide Schur vectors from the QZ decompositon. Given the partial
  Schur decomposition OP*V=V*T, the following steps are performed:
      1) compute eigenvectors of (S,T): S*Z=T*Z*D
      2) compute eigenvectors of OP: X=V*Z
  If left eigenvectors are required then also do Z'*Tl=D*Z', Y=W*Z
 */
PetscErrorCode EPSComputeVectors_QZ(EPS eps)
{
  PetscErrorCode ierr;
  EPS_DAVIDSON   *data = (EPS_DAVIDSON*)eps->data;
  dvdDashboard   *d = &data->ddb;
  PetscScalar    *pX,*auxS;
  PetscInt       size_auxS;

  PetscFunctionBegin;
  /* Compute the eigenvectors associated to (cS, cT) */
  ierr = PetscMalloc(sizeof(PetscScalar)*d->nconv*d->nconv,&pX);CHKERRQ(ierr);
  size_auxS = 11*d->nconv + 4*d->nconv*d->nconv; 
  ierr = PetscMalloc(sizeof(PetscScalar)*size_auxS,&auxS);CHKERRQ(ierr);
  ierr = dvd_compute_eigenvectors(d->nconv,d->cS,d->ldcS,d->cT,d->ldcT,
                                  pX,d->nconv,PETSC_NULL,0,auxS,
                                  size_auxS,PETSC_FALSE);CHKERRQ(ierr);

  /* pX[i] <- pX[i] / ||pX[i]|| */
  ierr = SlepcDenseNorm(pX,d->nconv,d->nconv,d->nconv,d->ceigi);CHKERRQ(ierr);

  /* V <- cX * pX */ 
  ierr = SlepcUpdateVectorsZ(eps->V,0.0,1.0,d->cX,d->size_cX,pX,
                             d->nconv,d->nconv,d->nconv);CHKERRQ(ierr);

  ierr = PetscFree(pX);CHKERRQ(ierr);
  ierr = PetscFree(auxS);CHKERRQ(ierr);

  eps->evecsavailable = PETSC_TRUE;
  PetscFunctionReturn(0);
}
