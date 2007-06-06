/*
     SVD routines for setting up the solver.

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
      SLEPc - Scalable Library for Eigenvalue Problem Computations
      Copyright (c) 2002-2007, Universidad Politecnica de Valencia, Spain

      This file is part of SLEPc. See the README file for conditions of use
      and additional information.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#include "src/svd/svdimpl.h"      /*I "slepcsvd.h" I*/

#undef __FUNCT__  
#define __FUNCT__ "SVDSetOperator"
/*@
   SVDSetOperator - Set the matrix associated with the singular value problem.

   Collective on SVD and Mat

   Input Parameters:
+  svd - the singular value solver context
-  A  - the matrix associated with the singular value problem

   Level: beginner

.seealso: SVDSolve(), SVDGetOperator()
@*/
PetscErrorCode SVDSetOperator(SVD svd,Mat mat)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_COOKIE,1);
  PetscValidHeaderSpecific(mat,MAT_COOKIE,2);
  PetscCheckSameComm(svd,1,mat,2);
  ierr = PetscObjectReference((PetscObject)mat);CHKERRQ(ierr);
  if (svd->OP) {
    ierr = MatDestroy(svd->OP);CHKERRQ(ierr);
  }
  svd->OP = mat;
  if (svd->vec_initial) {
    ierr = VecDestroy(svd->vec_initial);CHKERRQ(ierr);
    svd->vec_initial = PETSC_NULL;
  }
  svd->setupcalled = 0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SVDGetOperator"
/*@
   SVDGetOperator - Get the matrix associated with the singular value problem.

   Not collective, though parallel Mats are returned if the SVD is parallel

   Input Parameter:
.  svd - the singular value solver context

   Output Parameters:
.  A    - the matrix associated with the singular value problem

   Level: advanced

.seealso: SVDSolve(), SVDSetOperator()
@*/
PetscErrorCode SVDGetOperator(SVD svd,Mat *A)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_COOKIE,1);
  PetscValidPointer(A,2);
  *A = svd->OP;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SVDSetInitialVector"
/*@
   SVDSetInitialVector - Sets the initial vector from which the 
   singular value solver starts to iterate.

   Collective on SVD and Vec

   Input Parameters:
+  svd - the singular value solver context
-  vec - the vector

   Level: intermediate

.seealso: SVDGetInitialVector()

@*/
PetscErrorCode SVDSetInitialVector(SVD svd,Vec vec)
{
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_COOKIE,1);
  PetscValidHeaderSpecific(vec,VEC_COOKIE,2);
  PetscCheckSameComm(svd,1,vec,2);
  ierr = PetscObjectReference((PetscObject)vec);CHKERRQ(ierr);
  if (svd->vec_initial) {
    ierr = VecDestroy(svd->vec_initial); CHKERRQ(ierr);
  }
  svd->vec_initial = vec;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SVDGetInitialVector"
/*@
   SVDGetInitialVector - Gets the initial vector associated with the 
   singular value solver; if the vector was not set it will return a 0 
   pointer or a vector randomly generated by SVDSetUp().

   Not collective, but vector is shared by all processors that share the SVD

   Input Parameter:
.  svd - the singular value solver context

   Output Parameter:
.  vec - the vector

   Level: intermediate

.seealso: SVDSetInitialVector()

@*/
PetscErrorCode SVDGetInitialVector(SVD svd,Vec *vec)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_COOKIE,1);
  PetscValidPointer(vec,2);
  *vec = svd->vec_initial;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "SVDSetUp"
/*@
   SVDSetUp - Sets up all the internal data structures necessary for the
   execution of the singular value solver.

   Collective on SVD

   Input Parameter:
.  svd   - singular value solver context

   Level: advanced

   Notes:
   This function need not be called explicitly in most cases, since SVDSolve()
   calls it. It can be useful when one wants to measure the set-up time 
   separately from the solve time.

.seealso: SVDCreate(), SVDSolve(), SVDDestroy()
@*/
PetscErrorCode SVDSetUp(SVD svd)
{
  PetscErrorCode ierr;
  int            i;
  PetscTruth     flg;
  PetscInt       M,N;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_COOKIE,1);
  if (svd->setupcalled) PetscFunctionReturn(0);
  ierr = PetscLogEventBegin(SVD_SetUp,svd,0,0,0);CHKERRQ(ierr);

  /* Set default solver type */
  if (!svd->type_name) {
    ierr = SVDSetType(svd,SVDCROSS);CHKERRQ(ierr);
  }

  /* check matrix */
  if (!svd->OP)
    SETERRQ(PETSC_ERR_ARG_WRONGSTATE, "SVDSetOperator must be called first"); 
  
  /* determine how to build the transpose */
  if (svd->transmode == PETSC_DECIDE) {
    ierr = MatHasOperation(svd->OP,MATOP_TRANSPOSE,&flg);CHKERRQ(ierr);    
    if (flg) svd->transmode = SVD_TRANSPOSE_EXPLICIT;
    else svd->transmode = SVD_TRANSPOSE_IMPLICIT;
  }
  
  /* build transpose matrix */
  if (svd->A) { ierr = MatDestroy(svd->A);CHKERRQ(ierr); }
  if (svd->AT) { ierr = MatDestroy(svd->AT);CHKERRQ(ierr); }
  ierr = MatGetSize(svd->OP,&M,&N);CHKERRQ(ierr);
  ierr = PetscObjectReference((PetscObject)svd->OP);CHKERRQ(ierr);
  switch (svd->transmode) {
    case SVD_TRANSPOSE_EXPLICIT:
      ierr = MatHasOperation(svd->OP,MATOP_TRANSPOSE,&flg);CHKERRQ(ierr);
      if (!flg) SETERRQ(1,"Matrix has not defined the MatTranpose operation");
      if (M>=N) {
        svd->A = svd->OP;
        ierr = MatTranspose(svd->OP,&svd->AT);CHKERRQ(ierr);
      } else {
        ierr = MatTranspose(svd->OP,&svd->A);CHKERRQ(ierr);
        svd->AT = svd->OP;
      }
      break;
    case SVD_TRANSPOSE_IMPLICIT:
      ierr = MatHasOperation(svd->OP,MATOP_MULT_TRANSPOSE,&flg);CHKERRQ(ierr);
      if (!flg) SETERRQ(1,"Matrix has not defined the MatMultTranpose operation");
      if (M>=N) {
        svd->A = svd->OP;
        svd->AT = PETSC_NULL;    
      } else {
        svd->A = PETSC_NULL;
        svd->AT = svd->OP;
      }
      break;
    default:
      SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"Invalid transpose mode"); 
  }

  /* set initial vector */
  if (!svd->vec_initial) {
    ierr = SVDMatGetVecs(svd,&svd->vec_initial,PETSC_NULL);CHKERRQ(ierr);
    ierr = SlepcVecSetRandom(svd->vec_initial);CHKERRQ(ierr);
  }

  /* call specific solver setup */
  ierr = (*svd->ops->setup)(svd);CHKERRQ(ierr);

  if (svd->ncv > M || svd->ncv > N)
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"ncv bigger than matrix dimensions");
  if (svd->nsv > svd->ncv)
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,"nsv bigger than ncv");

  if (svd->ncv != svd->n) {
    /* free memory for previous solution  */
    if (svd->n) { 
      ierr = PetscFree(svd->sigma);CHKERRQ(ierr);
      ierr = PetscFree(svd->errest);CHKERRQ(ierr);
      for (i=0;i<svd->n;i++) {
	ierr = VecDestroy(svd->V[i]);CHKERRQ(ierr); 
      }
      ierr = PetscFree(svd->V);CHKERRQ(ierr);
    }
    /* allocate memory for next solution */
    ierr = PetscMalloc(svd->ncv*sizeof(PetscReal),&svd->sigma);CHKERRQ(ierr);
    ierr = PetscMalloc(svd->ncv*sizeof(PetscReal),&svd->errest);CHKERRQ(ierr);
    ierr = PetscMalloc(svd->ncv*sizeof(Vec),&svd->V);CHKERRQ(ierr);
    for (i=0;i<svd->ncv;i++) {
      ierr = SVDMatGetVecs(svd,svd->V+i,PETSC_NULL);CHKERRQ(ierr);
    }
    svd->n = svd->ncv;
  }

  ierr = PetscLogEventEnd(SVD_SetUp,svd,0,0,0);CHKERRQ(ierr);
  svd->setupcalled = 1;
  PetscFunctionReturn(0);
}
