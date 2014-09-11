/*

   SLEPc eigensolver: "krylovschur"

   Method: Krylov-Schur with spectrum slicing for symmetric eigenproblems

   References:

       [1] R.G. Grimes et al., "A shifted block Lanczos algorithm for
           solving sparse symmetric generalized eigenproblems", SIAM J.
           Matrix Anal. Appl. 15(1):228-272, 1994.

       [2] C. Campos and J.E. Roman, "Spectrum slicing strategies based
           on restarted Lanczos methods", Numer. Algor. 60(2):279-295,
           2012.

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2014, Universitat Politecnica de Valencia, Spain

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

#include <slepc-private/epsimpl.h>
#include "krylovschur.h"

#undef __FUNCT__
#define __FUNCT__ "EPSSliceAllocateSolution"
/*
  EPSSliceAllocateSolution - Allocate memory storage for common variables such
  as eigenvalues and eigenvectors. The argument extra is used for methods
  that require a working basis slightly larger than ncv.
*/
static PetscErrorCode EPSSliceAllocateSolution(EPS eps,PetscInt extra)
{
  PetscErrorCode ierr;
  EPS_KRYLOVSCHUR *ctx=(EPS_KRYLOVSCHUR*)eps->data;
  PetscReal       eta;
  PetscLogDouble  cnt;
  BVType          type;
  BVOrthogType    orthog_type;
  BVOrthogRefineType orthog_ref;
  Mat             matrix;
  Vec             t;
  EPS_SR          sr = ctx->sr;

  PetscFunctionBegin;
  /* allocate space for eigenvalues and friends */
  ierr = PetscMalloc4(sr->numEigs,&sr->eigr,sr->numEigs,&sr->eigi,sr->numEigs,&sr->errest,sr->numEigs,&sr->perm);CHKERRQ(ierr);
  cnt = 2*sr->numEigs*sizeof(PetscScalar) + 2*sr->numEigs*sizeof(PetscReal) + sr->numEigs*sizeof(PetscInt);
  ierr = PetscLogObjectMemory((PetscObject)eps,cnt);CHKERRQ(ierr);
  
  /* allocate sr->V and transfer options from eps->V */
  ierr = BVCreate(PetscObjectComm((PetscObject)eps),&sr->V);CHKERRQ(ierr);
  ierr = PetscLogObjectParent((PetscObject)eps,(PetscObject)sr->V);CHKERRQ(ierr);
  if (!eps->V) { ierr = EPSGetBV(eps,&eps->V);CHKERRQ(ierr); }
  if (!((PetscObject)(eps->V))->type_name) {
    ierr = BVSetType(sr->V,BVSVEC);CHKERRQ(ierr);
  } else {
    ierr = BVGetType(eps->V,&type);CHKERRQ(ierr);
    ierr = BVSetType(sr->V,type);CHKERRQ(ierr);
  }
  ierr = STMatGetVecs(eps->st,&t,NULL);CHKERRQ(ierr);
  ierr = BVSetSizesFromVec(sr->V,t,cnt);CHKERRQ(ierr);
  ierr = VecDestroy(&t);CHKERRQ(ierr);
  ierr = EPS_SetInnerProduct(eps);CHKERRQ(ierr);
  ierr = BVGetMatrix(eps->V,&matrix,NULL);CHKERRQ(ierr);
  ierr = BVSetMatrix(sr->V,matrix,PETSC_FALSE);CHKERRQ(ierr);
  ierr = BVGetOrthogonalization(eps->V,&orthog_type,&orthog_ref,&eta);CHKERRQ(ierr);
  ierr = BVSetOrthogonalization(sr->V,orthog_type,orthog_ref,eta);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "EPSSliceGetEPS"
static PetscErrorCode EPSSliceGetEPS(EPS eps)
{
  PetscErrorCode     ierr;
  EPS_KRYLOVSCHUR    *ctx=(EPS_KRYLOVSCHUR*)eps->data,*ctx_local;
  BV                 V;
  BVType             type;
  PetscReal          eta;
  BVOrthogType       orthog_type;
  BVOrthogRefineType orthog_ref;
  Mat                A,B,Ar,Br;
  PetscInt           i;
  PetscReal          h,a,b;
  PetscMPIInt        rank;
  EPS_SR             sr=ctx->sr;
  PC                 pc;
  KSP                ksp;
  const MatSolverPackage stype;

  PetscFunctionBegin;
  ierr = EPSGetOperators(eps,&A,&B);CHKERRQ(ierr);
  if (ctx->npart==1) {
    ierr = EPSCreate(((PetscObject)eps)->comm,&ctx->eps);CHKERRQ(ierr);
    ierr = EPSSetType(ctx->eps,((PetscObject)eps)->type_name);CHKERRQ(ierr);
    ierr = EPSSetST(ctx->eps,eps->st);CHKERRQ(ierr);
    a = eps->inta; b = eps->intb;    
  } else {
    /* Create context for subcommunicators */
    ierr = PetscSubcommCreate(PetscObjectComm((PetscObject)eps),&ctx->subc);CHKERRQ(ierr);
    ierr = PetscSubcommSetNumber(ctx->subc,ctx->npart);CHKERRQ(ierr);CHKERRQ(ierr);
    ierr = PetscSubcommSetType(ctx->subc,PETSC_SUBCOMM_CONTIGUOUS);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory((PetscObject)eps,sizeof(PetscSubcomm));CHKERRQ(ierr);

    /* Determine subintervals */
    if (!ctx->subintset) { /* uniform distribution if no set by user */
      if (!sr->hasEnd) SETERRQ(PetscObjectComm((PetscObject)eps),PETSC_ERR_ARG_WRONG,"Global interval must be bounded for splitting it in uniform subintervals");
      h = (eps->intb-eps->inta)/ctx->npart;
      a = eps->inta+ctx->subc->color*h;
      b = (ctx->subc->color==ctx->npart-1)?eps->intb:eps->inta+(ctx->subc->color+1)*h;
      ierr = PetscFree(ctx->subintervals);CHKERRQ(ierr);
      ierr = PetscMalloc1(ctx->npart+1,&ctx->subintervals);CHKERRQ(ierr);
      for (i=0;i<ctx->npart;i++) ctx->subintervals[i] = eps->inta+h*i;
      ctx->subintervals[ctx->npart] = eps->intb;
    } else {
      a = ctx->subintervals[ctx->subc->color];
      b = ctx->subintervals[ctx->subc->color+1];
    }

    /* Duplicate matrices */
    ierr = MatGetRedundantMatrix(A,0,ctx->subc->comm,MAT_INITIAL_MATRIX,&Ar);CHKERRQ(ierr);    
    ierr = MatGetRedundantMatrix(B,0,ctx->subc->comm,MAT_INITIAL_MATRIX,&Br);CHKERRQ(ierr);    

    /* Create auxiliary EPS */
    ierr = EPSCreate(ctx->subc->comm,&ctx->eps);CHKERRQ(ierr);
    ierr = EPSSetType(ctx->eps,((PetscObject)eps)->type_name);CHKERRQ(ierr);
    ierr = EPSSetOperators(ctx->eps,Ar,Br);CHKERRQ(ierr);

    /* Transfer options for ST, KSP and PC */
    ierr = STSetType(ctx->eps->st,STSINVERT);CHKERRQ(ierr);
    ierr = STGetKSP(eps->st,&ksp);CHKERRQ(ierr);
    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
    ierr = PCFactorGetMatSolverPackage(pc,&stype);CHKERRQ(ierr);
    ierr = STGetKSP(ctx->eps->st,&ksp);CHKERRQ(ierr);
    ierr = KSPSetType(ksp,KSPPREONLY);CHKERRQ(ierr);
    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
    ierr = PCSetType(pc,PCCHOLESKY);CHKERRQ(ierr);
    ierr = PCFactorSetMatSolverPackage(pc,stype);CHKERRQ(ierr);

    /* Create scatters for sending vectors for deflation  ///// PENDING  ///// */

    /* Create subcommunicator grouping processes with same rank */
    ierr = MPI_Comm_rank(ctx->subc->comm,&rank);CHKERRQ(ierr);
    ierr = MPI_Comm_split(((PetscObject)eps)->comm,rank,ctx->subc->color,&ctx->commrank);CHKERRQ(ierr);
  }
  ierr = EPSSetInterval(ctx->eps,a,b);CHKERRQ(ierr);
  ctx_local = (EPS_KRYLOVSCHUR*)ctx->eps->data;
  ctx_local->npart = ctx->npart;
  ctx_local->global = PETSC_FALSE;
  ctx_local->eps = eps;
  ctx_local->subc = ctx->subc;
  ctx_local->commrank = ctx->commrank;

  ierr = EPSSetDimensions(ctx->eps,ctx->nev,ctx->ncv,ctx->mpd);CHKERRQ(ierr);

  /* transfer options from eps->V */
  ierr = EPSGetBV(ctx->eps,&V);CHKERRQ(ierr);
  if (!((PetscObject)(eps->V))->type_name) {
    ierr = BVSetType(V,BVSVEC);CHKERRQ(ierr);
  } else {
    ierr = BVGetType(eps->V,&type);CHKERRQ(ierr);
    ierr = BVSetType(V,type);CHKERRQ(ierr);
  }
  ierr = BVGetOrthogonalization(eps->V,&orthog_type,&orthog_ref,&eta);CHKERRQ(ierr);
  ierr = BVSetOrthogonalization(V,orthog_type,orthog_ref,eta);CHKERRQ(ierr);
  ctx->eps->which = eps->which;
  ctx->eps->max_it = eps->max_it;
  ctx->eps->tol = eps->tol;
  if (eps->tol==PETSC_DEFAULT) eps->tol = SLEPC_DEFAULT_TOL;
  ierr = EPSSetProblemType(ctx->eps,eps->problem_type);CHKERRQ(ierr);
  ierr = EPSSetUp(ctx->eps);CHKERRQ(ierr);
  ctx->eps->nconv = 0;
  ctx->eps->its   = 0;
  for (i=0;i<ctx->eps->ncv;i++) {
    ctx->eps->eigr[i]   = 0.0;
    ctx->eps->eigi[i]   = 0.0;
    ctx->eps->errest[i] = 0.0;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "EPSSliceGetInertia"
static PetscErrorCode EPSSliceGetInertia(EPS eps,PetscScalar shift,PetscInt *inertia)
{
  PetscErrorCode ierr;
  KSP            ksp;
  PC             pc;
  Mat            F;
  
  PetscFunctionBegin;
  if (shift >= PETSC_MAX_REAL) { /* Right-open interval */
    *inertia = eps->n;
  } else if (shift <= PETSC_MIN_REAL) {
    *inertia = 0;
  } else {
    ierr = STSetShift(eps->st,shift);CHKERRQ(ierr);
    ierr = STSetUp(eps->st);CHKERRQ(ierr);
    ierr = STGetKSP(eps->st,&ksp);CHKERRQ(ierr);
    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
    ierr = PCFactorGetMatrix(pc,&F);CHKERRQ(ierr);
    ierr = MatGetInertia(F,inertia,NULL,NULL);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "EPSSetUp_KrylovSchur_Slice"
PetscErrorCode EPSSetUp_KrylovSchur_Slice(EPS eps)
{
  PetscErrorCode  ierr;
  PetscBool       issinv;
  EPS_KRYLOVSCHUR *ctx = (EPS_KRYLOVSCHUR*)eps->data;
  EPS_SR          sr,sr_loc,sr_glob;
  PetscInt        nEigs,dssz=1,i;
  KSP             ksp;
  PetscMPIInt     nproc,rank;
  MPI_Request     req;

  PetscFunctionBegin;
  if (ctx->global) {
ierr = PetscOptionsGetInt(NULL,"-slice_npart",&ctx->npart,NULL);   ////////
    if (eps->inta==0.0 && eps->intb==0.0) SETERRQ(PetscObjectComm((PetscObject)eps),PETSC_ERR_ARG_WRONG,"Must define a computational interval when using EPS_ALL");
    if (!eps->ishermitian) SETERRQ(PetscObjectComm((PetscObject)eps),PETSC_ERR_SUP,"Spectrum slicing only available for symmetric/Hermitian eigenproblems");
    if (eps->arbitrary) SETERRQ(PetscObjectComm((PetscObject)eps),PETSC_ERR_SUP,"Arbitrary selection of eigenpairs cannot be used with spectrum slicing");
    if (!((PetscObject)(eps->st))->type_name) { /* default to shift-and-invert */
      ierr = STSetType(eps->st,STSINVERT);CHKERRQ(ierr);
    }
    ierr = PetscObjectTypeCompareAny((PetscObject)eps->st,&issinv,STSINVERT,STCAYLEY,"");CHKERRQ(ierr);
    if (!issinv) SETERRQ(PetscObjectComm((PetscObject)eps),PETSC_ERR_SUP,"Shift-and-invert or Cayley ST is needed for spectrum slicing");
    if (eps->tol==PETSC_DEFAULT) eps->tol = SLEPC_DEFAULT_TOL*1e-2;  /* use tighter tolerance */
    if (!eps->max_it) eps->max_it = 100;
    if (ctx->nev==1) ctx->nev = 20;  /* nev not set, use default value */
    if (ctx->nev<10) SETERRQ(PetscObjectComm((PetscObject)eps),PETSC_ERR_ARG_WRONG,"nev cannot be less than 10 in spectrum slicing runs");
  }
  eps->ops->backtransform = NULL;

  /* create spectrum slicing context and initialize it */
  ierr = EPSReset_KrylovSchur(eps);CHKERRQ(ierr);
  ierr = PetscNewLog(eps,&sr);CHKERRQ(ierr);
  ctx->sr = sr;
  sr->itsKs = 0;
  sr->nleap = 0;
  sr->nMAXCompl = ctx->nev/4;
  sr->iterCompl = eps->max_it/4;
  sr->sPres = NULL;
  sr->nS = 0;

  if (ctx->npart==1 || ctx->global) {
    /* check presence of ends and finding direction */
    if ((eps->inta > PETSC_MIN_REAL && eps->inta != 0.0) || eps->intb >= PETSC_MAX_REAL) {
      sr->int0 = eps->inta;
      sr->int1 = eps->intb;
      sr->dir = 1;
      if (eps->intb >= PETSC_MAX_REAL) { /* Right-open interval */
        sr->hasEnd = PETSC_FALSE;
      } else sr->hasEnd = PETSC_TRUE;
    } else {
      sr->int0 = eps->intb;
      sr->int1 = eps->inta;
      sr->dir = -1;
      if (eps->inta <= PETSC_MIN_REAL) { /* Left-open interval */
        sr->hasEnd = PETSC_FALSE;
      }
    }
    ierr = EPSSliceGetInertia(eps,sr->int1,&sr->inertia1);CHKERRQ(ierr);
    if (ctx->npart>1) {
      ierr = STGetKSP(eps->st,&ksp);CHKERRQ(ierr);
      ierr = KSPReset(ksp);CHKERRQ(ierr);
    }
  }
  if (ctx->global) {
    ierr = EPSSetDimensions_Default(eps,ctx->nev,&ctx->ncv,&ctx->mpd);CHKERRQ(ierr);
    /* create subintervals and initialize auxiliary eps for slicing runs */
    ierr = EPSSliceGetEPS(eps);CHKERRQ(ierr);
    sr_loc = ((EPS_KRYLOVSCHUR*)ctx->eps->data)->sr;
    if (ctx->npart>1) {
      ierr = PetscMalloc1(ctx->npart,&ctx->nconv_loc);CHKERRQ(ierr);
      ierr = MPI_Comm_size(((PetscObject)eps)->comm,&nproc);CHKERRQ(ierr);
      if (nproc%ctx->npart==0) { /* subcommunicators with the same size */
        ierr = MPI_Allgather(&sr_loc->numEigs,1,MPIU_INT,ctx->nconv_loc,1,MPIU_INT,ctx->commrank);CHKERRQ(ierr);
      } else {
        ierr = MPI_Comm_rank(ctx->subc->comm,&rank);CHKERRQ(ierr);
        if (rank==0) {
          ierr = MPI_Allgather(&sr_loc->numEigs,1,MPIU_INT,ctx->nconv_loc,1,MPIU_INT,ctx->commrank);CHKERRQ(ierr);
        }
        ierr = MPI_Bcast( ctx->nconv_loc,ctx->npart, MPIU_INT,0,ctx->subc->comm);CHKERRQ(ierr);
      }
      nEigs = 0;
      for (i=0;i<ctx->npart;i++) nEigs += ctx->nconv_loc[i];
    } else nEigs = sr_loc->numEigs;
    sr->numEigs = nEigs;
    eps->nev = nEigs;
    eps->ncv = nEigs;
    eps->mpd = nEigs;
  } else {
    if (ctx->npart>1) {
      sr_glob = ((EPS_KRYLOVSCHUR*)ctx->eps->data)->sr;
      sr->dir = sr_glob->dir;
      sr->int0 = (sr->dir==1)?eps->inta:eps->intb;
      sr->int1 = (sr->dir==1)?eps->intb:eps->inta;
      if (ctx->subc->color==ctx->npart) sr->hasEnd = sr_glob->hasEnd;
      else sr->hasEnd = PETSC_TRUE;
    }

    /* compute inertia0 */
    ierr = EPSSliceGetInertia(eps,sr->int0,&sr->inertia0);CHKERRQ(ierr);   

    if (ctx->npart>1) {
      /* inertia1 is received from neighbour */
      ierr = MPI_Comm_rank(ctx->subc->comm,&rank);CHKERRQ(ierr);
      if (!rank) {
        if (ctx->subc->color>0) { /* send inertia0 to neighbour0 */
          ierr = MPI_Isend(&(sr->inertia0),1,MPIU_INT,ctx->subc->color-1,0,ctx->commrank,&req);CHKERRQ(ierr);
        }
        if (ctx->subc->color<ctx->npart-1) { /* receive inertia1 from neighbour1 */
          ierr = MPI_Recv(&(sr->inertia1),1,MPIU_INT,ctx->subc->color+1,0,ctx->commrank,MPI_STATUS_IGNORE);
        }
      }
      if (ctx->subc->color<ctx->npart-1) {
        ierr = MPI_Bcast(&sr->inertia1,1,MPIU_INT,0,ctx->subc->comm);CHKERRQ(ierr);
      } else sr->inertia1 = sr_glob->inertia1; 
    }
    /* number of eigenvalues in interval */
    sr->numEigs = (sr->dir)*(sr->inertia1 - sr->inertia0);
    if (ctx->npart>1 && sr->numEigs) {
      /* memory allocate for subinterval eigenpairs */
      ierr = EPSSliceAllocateSolution(eps,1);CHKERRQ(ierr);
    }
    dssz = eps->ncv+1;
  }
  ierr = DSSetType(eps->ds,DSHEP);CHKERRQ(ierr);
  ierr = DSSetCompact(eps->ds,PETSC_TRUE);CHKERRQ(ierr);
  ierr = DSAllocate(eps->ds,dssz);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "EPSSliceGatherSolution"
static PetscErrorCode EPSSliceGatherSolution(EPS eps)
{
  PetscErrorCode ierr;
  PetscMPIInt    rank,nproc;
  Vec            v,vg,v_loc;
  IS             is1,is2;
  VecScatter     vec_sc;
  EPS_KRYLOVSCHUR *ctx=(EPS_KRYLOVSCHUR*)eps->data;
  PetscInt       nloc,m0,n0,i,si,idx,*idx1,*idx2,j,*disp,*perm_loc,off=0;
  PetscScalar    *array,*eigr_loc;
  EPS_SR         sr_loc;
  BV             V_loc;

  PetscFunctionBegin;
  eps->nconv = 0; 
  for (i=0;i<ctx->npart;i++) eps->nconv += ctx->nconv_loc[i];
  /* Gather eigenvalues (same ranks have fully set of eigenvalues)*/
  sr_loc = ((EPS_KRYLOVSCHUR*)ctx->eps->data)->sr;
  eigr_loc = sr_loc->eigr;
  V_loc = sr_loc->V;
  perm_loc = sr_loc->perm;
  ierr = MPI_Comm_size(((PetscObject)eps)->comm,&nproc);CHKERRQ(ierr);
  ierr = PetscMalloc1(ctx->npart,&disp);CHKERRQ(ierr);
  disp[0] = 0;
  for (i=1;i<ctx->npart;i++) disp[i] = disp[i-1]+ctx->nconv_loc[i-1]; 
  if (nproc%ctx->npart==0) { /* subcommunicators with the same size */
    ierr = MPI_Allgatherv(eigr_loc,sr_loc->numEigs,MPIU_SCALAR,eps->eigr,ctx->nconv_loc,disp,MPIU_SCALAR,ctx->commrank);CHKERRQ(ierr);
    ierr = MPI_Allgatherv(perm_loc,sr_loc->numEigs,MPIU_INT,eps->perm,ctx->nconv_loc,disp,MPIU_INT,ctx->commrank);CHKERRQ(ierr);
  } else {
    ierr = MPI_Comm_rank(ctx->subc->comm,&rank);CHKERRQ(ierr);
    if (rank==0) {
      ierr = MPI_Allgatherv(eigr_loc,sr_loc->numEigs,MPIU_SCALAR,eps->eigr,ctx->nconv_loc,disp,MPIU_SCALAR,ctx->commrank);CHKERRQ(ierr);
      ierr = MPI_Allgatherv(perm_loc,sr_loc->numEigs,MPIU_INT,eps->perm,ctx->nconv_loc,disp,MPIU_INT,ctx->commrank);CHKERRQ(ierr);
      }
      ierr = MPI_Bcast(eps->eigr,eps->nconv,MPIU_SCALAR,0,ctx->subc->comm);CHKERRQ(ierr);
      ierr = MPI_Bcast(eps->perm,eps->nconv,MPIU_INT,0,ctx->subc->comm);CHKERRQ(ierr);
  }
  idx = ctx->nconv_loc[0];
  for (i=1;i<ctx->npart;i++) {
    off += ctx->nconv_loc[i-1];
    for (j=0;j<ctx->nconv_loc[i];j++) eps->perm[idx++] += off;
   }

  /* Gather parallel eigenvectors */
  ierr = BVGetColumn(eps->V,0,&v);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(v,&n0,&m0);CHKERRQ(ierr);
  ierr = BVRestoreColumn(eps->V,0,&v);CHKERRQ(ierr);
  ierr = BVGetColumn(ctx->eps->V,0,&v);CHKERRQ(ierr);
  ierr = VecGetLocalSize(v,&nloc);CHKERRQ(ierr);
  ierr = BVRestoreColumn(ctx->eps->V,0,&v);CHKERRQ(ierr);
  ierr = PetscMalloc2(m0-n0,&idx1,m0-n0,&idx2);CHKERRQ(ierr);
  ierr = VecCreateMPI(PetscObjectComm((PetscObject)eps),nloc,PETSC_DECIDE,&vg);CHKERRQ(ierr);
  idx = -1;
  for (si=0;si<ctx->npart;si++) {
    j = 0;
    for (i=n0;i<m0;i++) {
      idx1[j]   = i;
      idx2[j++] = i+eps->n*si;
    }
    ierr = ISCreateGeneral(PetscObjectComm((PetscObject)eps),(m0-n0),idx1,PETSC_COPY_VALUES,&is1);CHKERRQ(ierr);
    ierr = ISCreateGeneral(PetscObjectComm((PetscObject)eps),(m0-n0),idx2,PETSC_COPY_VALUES,&is2);CHKERRQ(ierr);
    ierr = BVGetColumn(eps->V,0,&v);CHKERRQ(ierr);
    ierr = VecScatterCreate(v,is1,vg,is2,&vec_sc);CHKERRQ(ierr);
    ierr = BVRestoreColumn(eps->V,0,&v);CHKERRQ(ierr);
    ierr = ISDestroy(&is1);CHKERRQ(ierr);
    ierr = ISDestroy(&is2);CHKERRQ(ierr);
    for (i=0;i<ctx->nconv_loc[si];i++) {
      ierr = BVGetColumn(eps->V,++idx,&v);CHKERRQ(ierr);
      ierr = BVGetColumn(V_loc,i,&v_loc);CHKERRQ(ierr);
      ierr = VecGetArray(v_loc,&array);CHKERRQ(ierr);
      ierr = VecPlaceArray(vg,array);CHKERRQ(ierr);
      ierr = VecScatterBegin(vec_sc,vg,v,INSERT_VALUES,SCATTER_REVERSE);CHKERRQ(ierr);
      ierr = VecScatterEnd(vec_sc,vg,v,INSERT_VALUES,SCATTER_REVERSE);CHKERRQ(ierr);
      ierr = VecResetArray(vg);CHKERRQ(ierr);
      ierr = VecRestoreArray(v_loc,&array);CHKERRQ(ierr);
      ierr = BVRestoreColumn(V_loc,i,&v_loc);CHKERRQ(ierr);
      ierr = BVRestoreColumn(eps->V,idx,&v);CHKERRQ(ierr);
    } 
    ierr = VecScatterDestroy(&vec_sc);CHKERRQ(ierr);
  }
  ierr = PetscFree2(idx1,idx2);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
   Fills the fields of a shift structure
*/
#undef __FUNCT__
#define __FUNCT__ "EPSCreateShift"
static PetscErrorCode EPSCreateShift(EPS eps,PetscReal val,EPS_shift neighb0,EPS_shift neighb1)
{
  PetscErrorCode  ierr;
  EPS_shift       s,*pending2;
  PetscInt        i;
  EPS_SR          sr;
  EPS_KRYLOVSCHUR *ctx=(EPS_KRYLOVSCHUR*)eps->data;

  PetscFunctionBegin;
  sr = ctx->sr;
  ierr = PetscNewLog(eps,&s);CHKERRQ(ierr);
  s->value = val;
  s->neighb[0] = neighb0;
  if (neighb0) neighb0->neighb[1] = s;
  s->neighb[1] = neighb1;
  if (neighb1) neighb1->neighb[0] = s;
  s->comp[0] = PETSC_FALSE;
  s->comp[1] = PETSC_FALSE;
  s->index = -1;
  s->neigs = 0;
  s->nconv[0] = s->nconv[1] = 0;
  s->nsch[0] = s->nsch[1]=0;
  /* Inserts in the stack of pending shifts */
  /* If needed, the array is resized */
  if (sr->nPend >= sr->maxPend) {
    sr->maxPend *= 2;
    ierr = PetscMalloc1(sr->maxPend,&pending2);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory((PetscObject)eps,sizeof(EPS_shift));CHKERRQ(ierr);
    for (i=0;i<sr->nPend;i++) pending2[i] = sr->pending[i];
    ierr = PetscFree(sr->pending);CHKERRQ(ierr);
    sr->pending = pending2;
  }
  sr->pending[sr->nPend++]=s;
  PetscFunctionReturn(0);
}

/* Prepare for Rational Krylov update */
#undef __FUNCT__
#define __FUNCT__ "EPSPrepareRational"
static PetscErrorCode EPSPrepareRational(EPS eps)
{
  EPS_KRYLOVSCHUR  *ctx=(EPS_KRYLOVSCHUR*)eps->data;
  PetscErrorCode   ierr;
  PetscInt         dir,i,k,ld,nv;
  PetscScalar      *A;
  EPS_SR           sr = ctx->sr;
  Vec              v;

  PetscFunctionBegin;
  ierr = DSGetLeadingDimension(eps->ds,&ld);CHKERRQ(ierr);
  dir = (sr->sPres->neighb[0] == sr->sPrev)?1:-1;
  dir*=sr->dir;
  k = 0;
  for (i=0;i<sr->nS;i++) {
    if (dir*PetscRealPart(sr->S[i])>0.0) {
      sr->S[k] = sr->S[i];
      sr->S[sr->nS+k] = sr->S[sr->nS+i];
      ierr = BVGetColumn(sr->Vnext,k,&v);CHKERRQ(ierr);
      ierr = BVCopyVec(eps->V,eps->nconv+i,v);CHKERRQ(ierr);
      ierr = BVRestoreColumn(sr->Vnext,k,&v);CHKERRQ(ierr);
      k++;
      if (k>=sr->nS/2)break;
    }
  }
  /* Copy to DS */
  ierr = DSGetArray(eps->ds,DS_MAT_A,&A);CHKERRQ(ierr);
  ierr = PetscMemzero(A,ld*ld*sizeof(PetscScalar));CHKERRQ(ierr);
  for (i=0;i<k;i++) {
    A[i*(1+ld)] = sr->S[i];
    A[k+i*ld] = sr->S[sr->nS+i];
  }
  sr->nS = k;
  ierr = DSRestoreArray(eps->ds,DS_MAT_A,&A);CHKERRQ(ierr);
  ierr = DSGetDimensions(eps->ds,&nv,NULL,NULL,NULL,NULL);CHKERRQ(ierr);
  ierr = DSSetDimensions(eps->ds,nv,0,0,k);CHKERRQ(ierr);
  /* Append u to V */
  ierr = BVGetColumn(sr->Vnext,sr->nS,&v);CHKERRQ(ierr);
  ierr = BVCopyVec(eps->V,sr->nv,v);CHKERRQ(ierr);
  ierr = BVRestoreColumn(sr->Vnext,sr->nS,&v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* Provides next shift to be computed */
#undef __FUNCT__
#define __FUNCT__ "EPSExtractShift"
static PetscErrorCode EPSExtractShift(EPS eps)
{
  PetscErrorCode   ierr;
  PetscInt         iner;
  Mat              F;
  PC               pc;
  KSP              ksp;
  EPS_KRYLOVSCHUR  *ctx=(EPS_KRYLOVSCHUR*)eps->data;
  EPS_SR           sr;

  PetscFunctionBegin;
  sr = ctx->sr;
  if (sr->nPend > 0) {
    sr->sPrev = sr->sPres;
    sr->sPres = sr->pending[--sr->nPend];
    ierr = STSetShift(eps->st,sr->sPres->value);CHKERRQ(ierr);
    ierr = STGetKSP(eps->st,&ksp);CHKERRQ(ierr);
    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
    ierr = PCFactorGetMatrix(pc,&F);CHKERRQ(ierr);
    ierr = MatGetInertia(F,&iner,NULL,NULL);CHKERRQ(ierr);
    sr->sPres->inertia = iner;
    eps->target = sr->sPres->value;
    eps->reason = EPS_CONVERGED_ITERATING;
    eps->its = 0;
  } else sr->sPres = NULL;
  PetscFunctionReturn(0);
}

/*
   Symmetric KrylovSchur adapted to spectrum slicing:
   Allows searching an specific amount of eigenvalues in the subintervals left and right.
   Returns whether the search has succeeded
*/
#undef __FUNCT__
#define __FUNCT__ "EPSKrylovSchur_Slice"
static PetscErrorCode EPSKrylovSchur_Slice(EPS eps)
{
  PetscErrorCode  ierr;
  EPS_KRYLOVSCHUR *ctx=(EPS_KRYLOVSCHUR*)eps->data;
  PetscInt        i,conv,k,l,ld,nv,*iwork,j,p;
  Mat             U;
  PetscScalar     *Q,*A,rtmp;
  PetscReal       *a,*b,beta;
  PetscBool       breakdown;
  PetscInt        count0,count1;
  PetscReal       lambda;
  EPS_shift       sPres;
  PetscBool       complIterating;
  PetscBool       sch0,sch1;
  PetscInt        iterCompl=0,n0,n1;
  EPS_SR          sr = ctx->sr;

  PetscFunctionBegin;
  /* Spectrum slicing data */
  sPres = sr->sPres;
  complIterating =PETSC_FALSE;
  sch1 = sch0 = PETSC_TRUE;
  ierr = DSGetLeadingDimension(eps->ds,&ld);CHKERRQ(ierr);
  ierr = PetscMalloc1(2*ld,&iwork);CHKERRQ(ierr);
  count0=0;count1=0; /* Found on both sides */
  if (sr->nS > 0 && (sPres->neighb[0] == sr->sPrev || sPres->neighb[1] == sr->sPrev)) {
    /* Rational Krylov */
    ierr = DSTranslateRKS(eps->ds,sr->sPrev->value-sPres->value);CHKERRQ(ierr);
    ierr = DSGetDimensions(eps->ds,NULL,NULL,NULL,&l,NULL);CHKERRQ(ierr);
    ierr = DSSetDimensions(eps->ds,l+1,0,0,0);CHKERRQ(ierr);
    ierr = BVSetActiveColumns(eps->V,0,l+1);CHKERRQ(ierr);
    ierr = DSGetMat(eps->ds,DS_MAT_Q,&U);CHKERRQ(ierr);
    ierr = BVMultInPlace(eps->V,U,0,l+1);CHKERRQ(ierr);
    ierr = MatDestroy(&U);CHKERRQ(ierr);
  } else {
    /* Get the starting Lanczos vector */
    ierr = EPSGetStartVector(eps,0,NULL);CHKERRQ(ierr);
    l = 0;
  }
  /* Restart loop */
  while (eps->reason == EPS_CONVERGED_ITERATING) {
    eps->its++; sr->itsKs++;
    /* Compute an nv-step Lanczos factorization */
    nv = PetscMin(eps->nconv+eps->mpd,eps->ncv);
    ierr = DSGetArrayReal(eps->ds,DS_MAT_T,&a);CHKERRQ(ierr);
    b = a + ld;
    ierr = EPSFullLanczos(eps,a,b,eps->nconv+l,&nv,&breakdown);CHKERRQ(ierr);
    sr->nv = nv;
    beta = b[nv-1];
    ierr = DSRestoreArrayReal(eps->ds,DS_MAT_T,&a);CHKERRQ(ierr);
    ierr = DSSetDimensions(eps->ds,nv,0,eps->nconv,eps->nconv+l);CHKERRQ(ierr);
    if (l==0) {
      ierr = DSSetState(eps->ds,DS_STATE_INTERMEDIATE);CHKERRQ(ierr);
    } else {
      ierr = DSSetState(eps->ds,DS_STATE_RAW);CHKERRQ(ierr);
    }
    ierr = BVSetActiveColumns(eps->V,eps->nconv,nv);CHKERRQ(ierr);

    /* Solve projected problem and compute residual norm estimates */
    if (eps->its == 1 && l > 0) {/* After rational update */
      ierr = DSGetArray(eps->ds,DS_MAT_A,&A);CHKERRQ(ierr);
      ierr = DSGetArrayReal(eps->ds,DS_MAT_T,&a);CHKERRQ(ierr);
      b = a + ld;
      k = eps->nconv+l;
      A[k*ld+k-1] = A[(k-1)*ld+k];
      A[k*ld+k] = a[k];
      for (j=k+1; j< nv; j++) {
        A[j*ld+j] = a[j];
        A[j*ld+j-1] = b[j-1] ;
        A[(j-1)*ld+j] = b[j-1];
      }
      ierr = DSRestoreArray(eps->ds,DS_MAT_A,&A);CHKERRQ(ierr);
      ierr = DSRestoreArrayReal(eps->ds,DS_MAT_T,&a);CHKERRQ(ierr);
      ierr = DSSolve(eps->ds,eps->eigr,NULL);CHKERRQ(ierr);
      ierr = DSSort(eps->ds,eps->eigr,NULL,NULL,NULL,NULL);CHKERRQ(ierr);
      ierr = DSSetCompact(eps->ds,PETSC_TRUE);CHKERRQ(ierr);
    } else { /* Restart */
      ierr = DSSolve(eps->ds,eps->eigr,NULL);CHKERRQ(ierr);
      ierr = DSSort(eps->ds,eps->eigr,NULL,NULL,NULL,NULL);CHKERRQ(ierr);
    }
    /* Residual */
    ierr = EPSKrylovConvergence(eps,PETSC_TRUE,eps->nconv,nv-eps->nconv,beta,1.0,&k);CHKERRQ(ierr);

    /* Check convergence */
    ierr = DSGetArrayReal(eps->ds,DS_MAT_T,&a);CHKERRQ(ierr);
    b = a + ld;
    conv = 0;
    j = k = eps->nconv;
    for (i=eps->nconv;i<nv;i++) if (eps->errest[i] < eps->tol) conv++;
    for (i=eps->nconv;i<nv;i++) {
      if (eps->errest[i] < eps->tol) {
        iwork[j++]=i;
      } else iwork[conv+k++]=i;
    }
    for (i=eps->nconv;i<nv;i++) {
      a[i]=PetscRealPart(eps->eigr[i]);
      b[i]=eps->errest[i];
    }
    for (i=eps->nconv;i<nv;i++) {
      eps->eigr[i] = a[iwork[i]];
      eps->errest[i] = b[iwork[i]];
    }
    for (i=eps->nconv;i<nv;i++) {
      a[i]=PetscRealPart(eps->eigr[i]);
      b[i]=eps->errest[i];
    }
    ierr = DSRestoreArrayReal(eps->ds,DS_MAT_T,&a);CHKERRQ(ierr);
    ierr = DSGetArray(eps->ds,DS_MAT_Q,&Q);CHKERRQ(ierr);
    for (i=eps->nconv;i<nv;i++) {
      p=iwork[i];
      if (p!=i) {
        j=i+1;
        while (iwork[j]!=i) j++;
        iwork[j]=p;iwork[i]=i;
        for (k=0;k<nv;k++) {
          rtmp=Q[k+p*ld];Q[k+p*ld]=Q[k+i*ld];Q[k+i*ld]=rtmp;
        }
      }
    }
    ierr = DSRestoreArray(eps->ds,DS_MAT_Q,&Q);CHKERRQ(ierr);
    k=eps->nconv+conv;

    /* Checking values obtained for completing */
    for (i=0;i<k;i++) {
      sr->back[i]=eps->eigr[i];
    }
    ierr = STBackTransform(eps->st,k,sr->back,eps->eigi);CHKERRQ(ierr);
    count0=count1=0;
    for (i=0;i<k;i++) {
      lambda = PetscRealPart(sr->back[i]);
      if (((sr->dir)*(sPres->value - lambda) > 0) && ((sr->dir)*(lambda - sPres->ext[0]) > 0)) count0++;
      if (((sr->dir)*(lambda - sPres->value) > 0) && ((sr->dir)*(sPres->ext[1] - lambda) > 0)) count1++;
    }
    if (k>eps->nev && eps->ncv-k<5) eps->reason = EPS_CONVERGED_TOL;
    else {
      /* Checks completion */
      if ((!sch0||count0 >= sPres->nsch[0]) && (!sch1 ||count1 >= sPres->nsch[1])) {
        eps->reason = EPS_CONVERGED_TOL;
      } else {
        if (!complIterating && eps->its >= eps->max_it) eps->reason = EPS_DIVERGED_ITS;
        if (complIterating) {
          if (--iterCompl <= 0) eps->reason = EPS_DIVERGED_ITS;
        } else if (k >= eps->nev) {
          n0 = sPres->nsch[0]-count0;
          n1 = sPres->nsch[1]-count1;
          if (sr->iterCompl>0 && ((n0>0 && n0<= sr->nMAXCompl)||(n1>0&&n1<=sr->nMAXCompl))) {
            /* Iterating for completion*/
            complIterating = PETSC_TRUE;
            if (n0 >sr->nMAXCompl)sch0 = PETSC_FALSE;
            if (n1 >sr->nMAXCompl)sch1 = PETSC_FALSE;
            iterCompl = sr->iterCompl;
          } else eps->reason = EPS_CONVERGED_TOL;
        }
      }
    }
    /* Update l */
    if (eps->reason == EPS_CONVERGED_ITERATING) l = PetscMax(1,(PetscInt)((nv-k)*ctx->keep));
    else l = nv-k;
    if (breakdown) l=0;

    if (eps->reason == EPS_CONVERGED_ITERATING) {
      if (breakdown) {
        /* Start a new Lanczos factorization */
        ierr = PetscInfo2(eps,"Breakdown in Krylov-Schur method (it=%D norm=%g)\n",eps->its,(double)beta);CHKERRQ(ierr);
        ierr = EPSGetStartVector(eps,k,&breakdown);CHKERRQ(ierr);
        if (breakdown) {
          eps->reason = EPS_DIVERGED_BREAKDOWN;
          ierr = PetscInfo(eps,"Unable to generate more start vectors\n");CHKERRQ(ierr);
        }
      } else {
        /* Prepare the Rayleigh quotient for restart */
        ierr = DSGetArrayReal(eps->ds,DS_MAT_T,&a);CHKERRQ(ierr);
        ierr = DSGetArray(eps->ds,DS_MAT_Q,&Q);CHKERRQ(ierr);
        b = a + ld;
        for (i=k;i<k+l;i++) {
          a[i] = PetscRealPart(eps->eigr[i]);
          b[i] = PetscRealPart(Q[nv-1+i*ld]*beta);
        }
        ierr = DSRestoreArrayReal(eps->ds,DS_MAT_T,&a);CHKERRQ(ierr);
        ierr = DSRestoreArray(eps->ds,DS_MAT_Q,&Q);CHKERRQ(ierr);
      }
    }
    /* Update the corresponding vectors V(:,idx) = V*Q(:,idx) */
    ierr = DSGetMat(eps->ds,DS_MAT_Q,&U);CHKERRQ(ierr);
    ierr = BVMultInPlace(eps->V,U,eps->nconv,k+l);CHKERRQ(ierr);
    ierr = MatDestroy(&U);CHKERRQ(ierr);

    /* Normalize u and append it to V */
    if (eps->reason == EPS_CONVERGED_ITERATING && !breakdown) {
      ierr = BVCopyColumn(eps->V,nv,k+l);CHKERRQ(ierr);
    }
    /* Monitor */
    eps->nconv = k;
    ierr = EPSMonitor(eps,ctx->sr->itsKs,eps->nconv,sr->eigr,sr->eigi,sr->errest,nv);CHKERRQ(ierr);
    if (eps->reason != EPS_CONVERGED_ITERATING) {
      /* Store approximated values for next shift */
      ierr = DSGetArray(eps->ds,DS_MAT_Q,&Q);CHKERRQ(ierr);
      sr->nS = l;
      for (i=0;i<l;i++) {
        sr->S[i] = eps->eigr[i+k];/* Diagonal elements */
        sr->S[i+l] = Q[nv-1+(i+k)*ld]*beta; /* Out of diagonal elements */
      }
      ierr = DSRestoreArray(eps->ds,DS_MAT_Q,&Q);CHKERRQ(ierr);
    }
  }
  /* Check for completion */
  for (i=0;i< eps->nconv; i++) {
    if ((sr->dir)*PetscRealPart(eps->eigr[i])>0) sPres->nconv[1]++;
    else sPres->nconv[0]++;
  }
  sPres->comp[0] = (count0 >= sPres->nsch[0])?PETSC_TRUE:PETSC_FALSE;
  sPres->comp[1] = (count1 >= sPres->nsch[1])?PETSC_TRUE:PETSC_FALSE;
  if (count0 > sPres->nsch[0] || count1 > sPres->nsch[1])SETERRQ(PetscObjectComm((PetscObject)eps),1,"Unexpected error in Spectrum Slicing!\nMismatch between number of values found and information from inertia");
  ierr = PetscFree(iwork);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
  Obtains value of subsequent shift
*/
#undef __FUNCT__
#define __FUNCT__ "EPSGetNewShiftValue"
static PetscErrorCode EPSGetNewShiftValue(EPS eps,PetscInt side,PetscReal *newS)
{
  PetscReal       lambda,d_prev;
  PetscInt        i,idxP;
  EPS_SR          sr;
  EPS_shift       sPres,s;
  EPS_KRYLOVSCHUR *ctx=(EPS_KRYLOVSCHUR*)eps->data;

  PetscFunctionBegin;
  sr = ctx->sr;
  sPres = sr->sPres;
  if (sPres->neighb[side]) {
  /* Completing a previous interval */
    if (!sPres->neighb[side]->neighb[side] && sPres->neighb[side]->nconv[side]==0) { /* One of the ends might be too far from eigenvalues */
      if (side) *newS = (sPres->value + PetscRealPart(sr->eigr[sr->perm[sr->indexEig-1]]))/2;
      else *newS = (sPres->value + PetscRealPart(sr->eigr[sr->perm[0]]))/2;
    } else *newS=(sPres->value + sPres->neighb[side]->value)/2;
  } else { /* (Only for side=1). Creating a new interval. */
    if (sPres->neigs==0) {/* No value has been accepted*/
      if (sPres->neighb[0]) {
        /* Multiplying by 10 the previous distance */
        *newS = sPres->value + 10*(sr->dir)*PetscAbsReal(sPres->value - sPres->neighb[0]->value);
        sr->nleap++;
        /* Stops when the interval is open and no values are found in the last 5 shifts (there might be infinite eigenvalues) */
        if (!sr->hasEnd && sr->nleap > 5) SETERRQ(PetscObjectComm((PetscObject)eps),1,"Unable to compute the wanted eigenvalues with open interval");
      } else { /* First shift */
        if (eps->nconv != 0) {
          /* Unaccepted values give information for next shift */
          idxP=0;/* Number of values left from shift */
          for (i=0;i<eps->nconv;i++) {
            lambda = PetscRealPart(sr->eigr[i]);
            if ((sr->dir)*(lambda - sPres->value) <0) idxP++;
            else break;
          }
          /* Avoiding subtraction of eigenvalues (might be the same).*/
          if (idxP>0) {
            d_prev = PetscAbsReal(sPres->value - PetscRealPart(sr->eigr[0]))/(idxP+0.3);
          } else {
            d_prev = PetscAbsReal(sPres->value - PetscRealPart(sr->eigr[eps->nconv-1]))/(eps->nconv+0.3);
          }
          *newS = sPres->value + ((sr->dir)*d_prev*ctx->nev)/2;
        } else { /* No values found, no information for next shift */
          SETERRQ(PetscObjectComm((PetscObject)eps),1,"First shift renders no information");
        }
      }
    } else { /* Accepted values found */
      sr->nleap = 0;
      /* Average distance of values in previous subinterval */
      s = sPres->neighb[0];
      while (s && PetscAbs(s->inertia - sPres->inertia)==0) {
        s = s->neighb[0];/* Looking for previous shifts with eigenvalues within */
      }
      if (s) {
        d_prev = PetscAbsReal((sPres->value - s->value)/(sPres->inertia - s->inertia));
      } else { /* First shift. Average distance obtained with values in this shift */
        /* first shift might be too far from first wanted eigenvalue (no values found outside the interval)*/
        if ((sr->dir)*(PetscRealPart(sr->eigr[0])-sPres->value)>0 && PetscAbsReal((PetscRealPart(sr->eigr[sr->indexEig-1]) - PetscRealPart(sr->eigr[0]))/PetscRealPart(sr->eigr[0])) > PetscSqrtReal(eps->tol)) {
          d_prev =  PetscAbsReal((PetscRealPart(sr->eigr[sr->indexEig-1]) - PetscRealPart(sr->eigr[0])))/(sPres->neigs+0.3);
        } else {
          d_prev = PetscAbsReal(PetscRealPart(sr->eigr[sr->indexEig-1]) - sPres->value)/(sPres->neigs+0.3);
        }
      }
      /* Average distance is used for next shift by adding it to value on the right or to shift */
      if ((sr->dir)*(PetscRealPart(sr->eigr[sPres->index + sPres->neigs -1]) - sPres->value)>0) {
        *newS = PetscRealPart(sr->eigr[sPres->index + sPres->neigs -1])+ ((sr->dir)*d_prev*(ctx->nev))/2;
      } else { /* Last accepted value is on the left of shift. Adding to shift */
        *newS = sPres->value + ((sr->dir)*d_prev*(ctx->nev))/2;
      }
    }
    /* End of interval can not be surpassed */
    if ((sr->dir)*(sr->int1 - *newS) < 0) *newS = sr->int1;
  }/* of neighb[side]==null */
  PetscFunctionReturn(0);
}

/*
  Function for sorting an array of real values
*/
#undef __FUNCT__
#define __FUNCT__ "sortRealEigenvalues"
static PetscErrorCode sortRealEigenvalues(PetscScalar *r,PetscInt *perm,PetscInt nr,PetscBool prev,PetscInt dir)
{
  PetscReal      re;
  PetscInt       i,j,tmp;

  PetscFunctionBegin;
  if (!prev) for (i=0;i<nr;i++) perm[i] = i;
  /* Insertion sort */
  for (i=1;i<nr;i++) {
    re = PetscRealPart(r[perm[i]]);
    j = i-1;
    while (j>=0 && dir*(re - PetscRealPart(r[perm[j]])) <= 0) {
      tmp = perm[j]; perm[j] = perm[j+1]; perm[j+1] = tmp; j--;
    }
  }
  PetscFunctionReturn(0);
}

/* Stores the pairs obtained since the last shift in the global arrays */
#undef __FUNCT__
#define __FUNCT__ "EPSStoreEigenpairs"
static PetscErrorCode EPSStoreEigenpairs(EPS eps)
{
  PetscErrorCode  ierr;
  EPS_KRYLOVSCHUR *ctx=(EPS_KRYLOVSCHUR*)eps->data;
  PetscReal       lambda,err,norm;
  PetscInt        i,count;
  PetscBool       iscayley;
  EPS_SR          sr = ctx->sr;
  EPS_shift       sPres;
  Vec             v,w;
 
  PetscFunctionBegin;
  sPres = sr->sPres;
  sPres->index = sr->indexEig;
  count = sr->indexEig;
  /* Back-transform */
  ierr = STBackTransform(eps->st,eps->nconv,eps->eigr,eps->eigi);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)eps->st,STCAYLEY,&iscayley);CHKERRQ(ierr);
  /* Sort eigenvalues */
  ierr = sortRealEigenvalues(eps->eigr,eps->perm,eps->nconv,PETSC_FALSE,sr->dir);CHKERRQ(ierr);
  /* Values stored in global array */
  for (i=0;i<eps->nconv;i++) {
    lambda = PetscRealPart(eps->eigr[eps->perm[i]]);
    err = eps->errest[eps->perm[i]];

    if ((sr->dir)*(lambda - sPres->ext[0]) > 0 && (sr->dir)*(sPres->ext[1] - lambda) > 0) {/* Valid value */
      if (count>=sr->numEigs) SETERRQ(PetscObjectComm((PetscObject)eps),1,"Unexpected error in Spectrum Slicing");
      sr->eigr[count] = lambda;
      sr->errest[count] = err;
      /* Explicit purification */
      ierr = BVGetColumn(sr->V,count,&v);CHKERRQ(ierr);
      ierr = BVGetColumn(eps->V,eps->perm[i],&w);CHKERRQ(ierr);
      ierr = STApply(eps->st,w,v);CHKERRQ(ierr);
      ierr = BVRestoreColumn(sr->V,count,&v);CHKERRQ(ierr);
      ierr = BVRestoreColumn(eps->V,eps->perm[i],&w);CHKERRQ(ierr);
      ierr = BVNormColumn(sr->V,count,NORM_2,&norm);CHKERRQ(ierr);
      ierr = BVScaleColumn(sr->V,count,1.0/norm);CHKERRQ(ierr);
      count++;
    }
  }
  sPres->neigs = count - sr->indexEig;
  sr->indexEig = count;
  /* Global ordering array updating */
  ierr = sortRealEigenvalues(sr->eigr,sr->perm,count,PETSC_TRUE,sr->dir);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "EPSLookForDeflation"
static PetscErrorCode EPSLookForDeflation(EPS eps)
{
  PetscErrorCode  ierr;
  PetscReal       val;
  PetscInt        i,count0=0,count1=0;
  EPS_shift       sPres;
  PetscInt        ini,fin,k,idx0,idx1;
  EPS_SR          sr;
  Vec             v;
  EPS_KRYLOVSCHUR *ctx=(EPS_KRYLOVSCHUR*)eps->data;

  PetscFunctionBegin;
  sr = ctx->sr;
  sPres = sr->sPres;

  if (sPres->neighb[0]) ini = (sr->dir)*(sPres->neighb[0]->inertia - sr->inertia0);
  else ini = 0;
  fin = sr->indexEig;
  /* Selection of ends for searching new values */
  if (!sPres->neighb[0]) sPres->ext[0] = sr->int0;/* First shift */
  else sPres->ext[0] = sPres->neighb[0]->value;
  if (!sPres->neighb[1]) {
    if (sr->hasEnd) sPres->ext[1] = sr->int1;
    else sPres->ext[1] = (sr->dir > 0)?PETSC_MAX_REAL:PETSC_MIN_REAL;
  } else sPres->ext[1] = sPres->neighb[1]->value;
  /* Selection of values between right and left ends */
  for (i=ini;i<fin;i++) {
    val=PetscRealPart(sr->eigr[sr->perm[i]]);
    /* Values to the right of left shift */
    if ((sr->dir)*(val - sPres->ext[1]) < 0) {
      if ((sr->dir)*(val - sPres->value) < 0) count0++;
      else count1++;
    } else break;
  }
  /* The number of values on each side are found */
  if (sPres->neighb[0]) {
    sPres->nsch[0] = (sr->dir)*(sPres->inertia - sPres->neighb[0]->inertia)-count0;
    if (sPres->nsch[0]<0)SETERRQ(PetscObjectComm((PetscObject)eps),1,"Unexpected error in Spectrum Slicing!\nMismatch between number of values found and information from inertia");
  } else sPres->nsch[0] = 0;

  if (sPres->neighb[1]) {
    sPres->nsch[1] = (sr->dir)*(sPres->neighb[1]->inertia - sPres->inertia) - count1;
    if (sPres->nsch[1]<0)SETERRQ(PetscObjectComm((PetscObject)eps),1,"Unexpected error in Spectrum Slicing!\nMismatch between number of values found and information from inertia");
  } else sPres->nsch[1] = (sr->dir)*(sr->inertia1 - sPres->inertia);

  /* Completing vector of indexes for deflation */
  idx0 = ini;
  idx1 = ini+count0+count1;
  k=0;
  for (i=idx0;i<idx1;i++) sr->idxDef[k++]=sr->perm[i];
  ierr = BVDuplicateResize(eps->V,k+eps->ncv+1,&sr->Vnext);CHKERRQ(ierr);
  ierr = BVSetNumConstraints(sr->Vnext,k);CHKERRQ(ierr);
  for (i=0;i<k;i++) {
    ierr = BVGetColumn(sr->Vnext,-i-1,&v);CHKERRQ(ierr);
    ierr = BVCopyVec(sr->V,sr->idxDef[i],v);CHKERRQ(ierr);
    ierr = BVRestoreColumn(sr->Vnext,-i-1,&v);CHKERRQ(ierr);
  }

  /* For rational Krylov */
  if (sr->nS>0 && (sr->sPrev == sr->sPres->neighb[0] || sr->sPrev == sr->sPres->neighb[1])) {
    ierr = EPSPrepareRational(eps);CHKERRQ(ierr);
  }
  eps->nconv = 0;
  /* Get rid of temporary Vnext */
  ierr = BVDestroy(&eps->V);CHKERRQ(ierr);
  eps->V = sr->Vnext;
  sr->Vnext = NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "EPSSolve_KrylovSchur_Slice"
PetscErrorCode EPSSolve_KrylovSchur_Slice(EPS eps)
{
  PetscErrorCode  ierr;
  PetscInt        i,lds;
  PetscReal       newS;
  EPS_KRYLOVSCHUR *ctx=(EPS_KRYLOVSCHUR*)eps->data;
  EPS_SR          sr=ctx->sr;

  PetscFunctionBegin;
  if (ctx->global) {
    ierr = EPSSolve_KrylovSchur_Slice(ctx->eps);CHKERRQ(ierr);
    ctx->eps->state = EPS_STATE_SOLVED;
    eps->reason = EPS_CONVERGED_TOL;
    if (ctx->npart>1) {
      /* Gather solution from subsolvers */
      ierr = EPSSliceGatherSolution(eps);CHKERRQ(ierr);
    } else eps->nconv = sr->numEigs; 
  } else {
    if (ctx->npart==1) {
      sr->eigr   = ctx->eps->eigr;
      sr->eigi   = ctx->eps->eigi;
      sr->perm   = ctx->eps->perm;
      sr->errest = ctx->eps->errest;
      sr->V      = ctx->eps->V;
    }
    /* Only with eigenvalues present in the interval ...*/
    if (sr->numEigs==0) {
      eps->reason = EPS_CONVERGED_TOL;
      PetscFunctionReturn(0);
    }
    /* Array of pending shifts */
    sr->maxPend = 100; /* Initial size */
    sr->nPend = 0;
    ierr = PetscMalloc1(sr->maxPend,&sr->pending);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory((PetscObject)eps,(sr->maxPend)*sizeof(EPS_shift));CHKERRQ(ierr);
    ierr = EPSCreateShift(eps,sr->int0,NULL,NULL);CHKERRQ(ierr);
    /* extract first shift */
    sr->sPrev = NULL;
    sr->sPres = sr->pending[--sr->nPend];
    sr->sPres->inertia = sr->inertia0;
    eps->target = sr->sPres->value;
    sr->s0 = sr->sPres;
    sr->indexEig = 0;
    /* Memory reservation for auxiliary variables */
    lds = PetscMin(eps->mpd,eps->ncv);
    ierr = PetscCalloc1(lds*lds,&sr->S);CHKERRQ(ierr);
    ierr = PetscMalloc1(eps->ncv,&sr->back);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory((PetscObject)eps,(sr->numEigs+2*eps->ncv)*sizeof(PetscScalar));CHKERRQ(ierr);
    for (i=0;i<sr->numEigs;i++) {
      sr->eigr[i]   = 0.0;
      sr->eigi[i]   = 0.0;
      sr->errest[i] = 0.0;
    }
    for (i=0;i<sr->numEigs;i++) sr->perm[i] = i;
    /* Vectors for deflation */
    ierr = PetscMalloc1(sr->numEigs,&sr->idxDef);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory((PetscObject)eps,sr->numEigs*sizeof(PetscInt));CHKERRQ(ierr);
    sr->indexEig = 0;
    /* Main loop */
    while (sr->sPres) {
      /* Search for deflation */
      ierr = EPSLookForDeflation(eps);CHKERRQ(ierr);
      /* KrylovSchur */
      ierr = EPSKrylovSchur_Slice(eps);CHKERRQ(ierr);
  
      ierr = EPSStoreEigenpairs(eps);CHKERRQ(ierr);
      /* Select new shift */
      if (!sr->sPres->comp[1]) {
        ierr = EPSGetNewShiftValue(eps,1,&newS);CHKERRQ(ierr);
        ierr = EPSCreateShift(eps,newS,sr->sPres,sr->sPres->neighb[1]);CHKERRQ(ierr);
      }
      if (!sr->sPres->comp[0]) {
        /* Completing earlier interval */
        ierr = EPSGetNewShiftValue(eps,0,&newS);CHKERRQ(ierr);
        ierr = EPSCreateShift(eps,newS,sr->sPres->neighb[0],sr->sPres);CHKERRQ(ierr);
      }
      /* Preparing for a new search of values */
      ierr = EPSExtractShift(eps);CHKERRQ(ierr);
    }
  
    /* Updating eps values prior to exit */
    ierr = PetscFree(sr->S);CHKERRQ(ierr);
    ierr = PetscFree(sr->idxDef);CHKERRQ(ierr);
    ierr = PetscFree(sr->pending);CHKERRQ(ierr);
    ierr = PetscFree(sr->back);CHKERRQ(ierr);
    eps->nconv  = sr->indexEig;
    eps->reason = EPS_CONVERGED_TOL;
    eps->its    = sr->itsKs;
    eps->nds    = 0;
  }
  PetscFunctionReturn(0);
}

