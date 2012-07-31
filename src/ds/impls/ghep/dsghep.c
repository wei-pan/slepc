/*
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

#include <slepc-private/dsimpl.h>      /*I "slepcds.h" I*/
#include <slepcblaslapack.h>


#undef __FUNCT__  
#define __FUNCT__ "DSAllocate_GHEP"
PetscErrorCode DSAllocate_GHEP(DS ds,PetscInt ld)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DSAllocateMat_Private(ds,DS_MAT_A);CHKERRQ(ierr); 
  ierr = DSAllocateMat_Private(ds,DS_MAT_B);CHKERRQ(ierr); 
  ierr = DSAllocateMat_Private(ds,DS_MAT_Q);CHKERRQ(ierr);
  ierr = PetscFree(ds->perm);CHKERRQ(ierr);
  ierr = PetscMalloc(ld*sizeof(PetscInt),&ds->perm);CHKERRQ(ierr);
  ierr = PetscLogObjectMemory(ds,ld*sizeof(PetscInt));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSView_GHEP"
PetscErrorCode DSView_GHEP(DS ds,PetscViewer viewer)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;


  ierr = DSViewMat_Private(ds,viewer,DS_MAT_A);CHKERRQ(ierr); 
  ierr = DSViewMat_Private(ds,viewer,DS_MAT_B);CHKERRQ(ierr); 
  if (ds->state>DS_STATE_INTERMEDIATE) {
    ierr = DSViewMat_Private(ds,viewer,DS_MAT_Q);CHKERRQ(ierr); 
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSVectors_GHEP"
PetscErrorCode DSVectors_GHEP(DS ds,DSMatType mat,PetscInt *j,PetscReal *rnorm)
{
  PetscScalar    *Q = ds->mat[DS_MAT_Q];
  PetscInt       ld = ds->ld,i;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (rnorm) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"Not implemented yet");
  switch (mat) {
    case DS_MAT_X:
      if (j) {
        if(ds->state >= DS_STATE_CONDENSED){
          ierr = PetscMemcpy(ds->mat[mat]+(*j)*ld,Q+(*j)*ld,ld*sizeof(PetscScalar));CHKERRQ(ierr);
        }else{
          ierr = PetscMemzero(ds->mat[mat]+(*j)*ld,ld*sizeof(PetscScalar));CHKERRQ(ierr);
          *(ds->mat[mat]+(*j)+(*j)*ld) = 1.0;
        }
      } else {
        if(ds->state >= DS_STATE_CONDENSED){
          ierr = PetscMemcpy(ds->mat[mat],Q,ld*ld*sizeof(PetscScalar));CHKERRQ(ierr);
        }else{
          ierr = PetscMemzero(ds->mat[mat],ld*ld*sizeof(PetscScalar));CHKERRQ(ierr);
          for(i=0;i<ds->n;i++) *(ds->mat[mat]+i+i*ld) = 1.0;
        }
      }
      break;
    case DS_MAT_Y:
    case DS_MAT_U:
    case DS_MAT_VT:
      SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"Not implemented yet");
      break;
    default:
      SETERRQ(((PetscObject)ds)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Invalid mat parameter"); 
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "DSNormalize_GHEP"
PetscErrorCode DSNormalize_GHEP(DS ds,DSMatType mat,PetscInt col)
{
 PetscFunctionBegin;
  if (ds->state<DS_STATE_CONDENSED) SETERRQ(((PetscObject)ds)->comm,PETSC_ERR_ORDER,"Must call DSSolve() first");
  switch (mat) {
    case DS_MAT_X:
    case DS_MAT_Q:
      /* All the matrices resulting from DSVectors and DSSolve are already B-normalized */
      break;
    case DS_MAT_Y:
    case DS_MAT_U:
    case DS_MAT_VT:
      SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"Not implemented yet");
      break;
    default:
      SETERRQ(((PetscObject)ds)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Invalid mat parameter"); 
  }
  PetscFunctionReturn(0);

}

#undef __FUNCT__  
#define __FUNCT__ "DSSort_GHEP"
PetscErrorCode DSSort_GHEP(DS ds,PetscScalar *wr,PetscScalar *wi,PetscScalar *rr,PetscScalar *ri,PetscInt *k)
{
  PetscErrorCode ierr;
  PetscInt       n,l,i,*perm,ld=ds->ld;
  PetscScalar    *A;
  
  PetscFunctionBegin;
  if (!ds->comp_fun) PetscFunctionReturn(0);
  n = ds->n;
  l = ds->l;
  A  = ds->mat[DS_MAT_A];
  perm = ds->perm;
  for(i=l;i<n;i++) wr[i] = A[i+i*ld];
  if (rr) {
    ierr = DSSortEigenvalues_Private(ds,rr,ri,perm,PETSC_FALSE);CHKERRQ(ierr);
  } else {
    ierr = DSSortEigenvalues_Private(ds,wr,PETSC_NULL,perm,PETSC_FALSE);CHKERRQ(ierr);
  } 
  for (i=l;i<n;i++) A[i+i*ld] = wr[perm[i]];
  for (i=l;i<n;i++) wr[i] = A[i+i*ld];
  ierr = DSPermuteColumns_Private(ds,l,n,DS_MAT_Q,perm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DSSolve_GHEP"
PetscErrorCode DSSolve_GHEP(DS ds,PetscScalar *wr,PetscScalar *wi)
{
#if defined(SLEPC_MISSING_LAPACK_SYGVD)
  PetscFunctionBegin;
  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"SYGVD - Lapack routine is unavailable.");
#else
  PetscErrorCode ierr;
  PetscScalar    *work,*A,*B,*Q;
  PetscBLASInt   itype = 1,*iwork,info,n1,liwork,ld,lrwork=0,lwork;
  PetscInt       off,i;
#if defined(PETSC_USE_COMPLEX)
  PetscReal      *rwork,*rr;
#endif 

  PetscFunctionBegin;
  n1 = PetscBLASIntCast(ds->n-ds->l);
  ld = PetscBLASIntCast(ds->ld);
  liwork = PetscBLASIntCast(5*ds->n+3);
#if defined(PETSC_USE_COMPLEX)
  lwork  = PetscBLASIntCast(ds->n*ds->n+2*ds->n);
  lrwork = PetscBLASIntCast(2*ds->n*ds->n+5*ds->n+1+n1);
#else
  lwork  = PetscBLASIntCast(2*ds->n*ds->n+6*ds->n+1);
#endif
  ierr = DSAllocateWork_Private(ds,lwork,lrwork,liwork);CHKERRQ(ierr);
  work = ds->work;
  iwork = ds->iwork;
  off = ds->l+ds->l*ld;
  A = ds->mat[DS_MAT_A];
  B = ds->mat[DS_MAT_B];
  Q = ds->mat[DS_MAT_Q];
#if defined(PETSC_USE_COMPLEX)
  rr = ds->rwork;
  rwork = ds->rwork + n1;
  lrwork = ds->lrwork - n1;
  LAPACKsygvd_(&itype,"V","U",&n1,A+off,&ld,B+off,&ld,rr,work,&lwork,rwork,&lrwork,iwork,&liwork,&info);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack ZHEGVD %d",info);
  for(i=0;i<n1;i++) wr[ds->l+i] = rr[i];
#else
  LAPACKsygvd_(&itype,"V","U",&n1,A+off,&ld,B+off,&ld,wr+ds->l,work,&lwork,iwork,&liwork,&info);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack DSYGVD %d",info);
#endif 
  ierr = PetscMemzero(Q+ds->l*ld,n1*ld*sizeof(PetscScalar));CHKERRQ(ierr);
  for(i=ds->l;i<ds->n;i++){
    ierr = PetscMemcpy(Q+ds->l+i*ld,A+ds->l+i*ld,n1*sizeof(PetscScalar));CHKERRQ(ierr);
  }
  ierr = PetscMemzero(B+ds->l*ld,n1*ld*sizeof(PetscScalar));CHKERRQ(ierr);
  ierr = PetscMemzero(A+ds->l*ld,n1*ld*sizeof(PetscScalar));CHKERRQ(ierr);
  for(i=ds->l;i<ds->n;i++){
    wi[i] = 0.0;
    B[i+i*ld] = 1.0;
    A[i+i*ld] = wr[i];
  }
  PetscFunctionReturn(0);
#endif 
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "DSCreate_GHEP"
PetscErrorCode DSCreate_GHEP(DS ds)
{
  PetscFunctionBegin;
  ds->ops->allocate      = DSAllocate_GHEP;
  ds->ops->view          = DSView_GHEP;
  ds->ops->vectors       = DSVectors_GHEP;
  ds->ops->solve[0]      = DSSolve_GHEP;
  ds->ops->sort          = DSSort_GHEP;
  ds->ops->normalize     = DSNormalize_GHEP;
  PetscFunctionReturn(0);
}
EXTERN_C_END
