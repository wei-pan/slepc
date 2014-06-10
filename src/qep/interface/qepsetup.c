/*
      QEP routines related to problem setup.

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2013, Universitat Politecnica de Valencia, Spain

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

#include <slepc-private/qepimpl.h>       /*I "slepcqep.h" I*/

#undef __FUNCT__
#define __FUNCT__ "QEPSetUp"
/*@
   QEPSetUp - Sets up all the internal data structures necessary for the
   execution of the QEP solver.

   Collective on QEP

   Input Parameter:
.  qep   - solver context

   Notes:
   This function need not be called explicitly in most cases, since QEPSolve()
   calls it. It can be useful when one wants to measure the set-up time
   separately from the solve time.

   Level: advanced

.seealso: QEPCreate(), QEPSolve(), QEPDestroy()
@*/
PetscErrorCode QEPSetUp(QEP qep)
{
  PetscErrorCode ierr;
  PetscBool      islinear,flg;
  Mat            mat[3];
  PetscInt       k;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(qep,QEP_CLASSID,1);
  if (qep->setupcalled) PetscFunctionReturn(0);
  ierr = PetscLogEventBegin(QEP_SetUp,qep,0,0,0);CHKERRQ(ierr);

  /* reset the convergence flag from the previous solves */
  qep->reason = QEP_CONVERGED_ITERATING;

  /* Set default solver type (QEPSetFromOptions was not called) */
  if (!((PetscObject)qep)->type_name) {
    ierr = QEPSetType(qep,QEPLINEAR);CHKERRQ(ierr);
  }
  ierr = PetscObjectTypeCompare((PetscObject)qep,QEPLINEAR,&islinear);CHKERRQ(ierr);
  if (!qep->st) { ierr = QEPGetST(qep,&qep->st);CHKERRQ(ierr); }
  if (!islinear) {
    if (!((PetscObject)qep->st)->type_name) {
      ierr = STSetType(qep->st,STSHIFT);CHKERRQ(ierr);
    }
  }
  if (!qep->ds) { ierr = QEPGetDS(qep,&qep->ds);CHKERRQ(ierr); }
  ierr = DSReset(qep->ds);CHKERRQ(ierr);
  if (!((PetscObject)qep->rand)->type_name) {
    ierr = PetscRandomSetFromOptions(qep->rand);CHKERRQ(ierr);
  }

  /* Check matrices, transfer them to ST */
  if (!qep->M || !qep->C || !qep->K) SETERRQ(PetscObjectComm((PetscObject)qep),PETSC_ERR_ARG_WRONGSTATE,"QEPSetOperators must be called first");
  mat[0] = qep->K;
  mat[1] = qep->C;
  mat[2] = qep->M;
  ierr = STSetOperators(qep->st,3,mat);CHKERRQ(ierr);

  /* Set problem dimensions */
  ierr = MatGetSize(qep->M,&qep->n,NULL);CHKERRQ(ierr);
  ierr = MatGetLocalSize(qep->M,&qep->nloc,NULL);CHKERRQ(ierr);

  /* Set default problem type */
  if (!qep->problem_type) {
    ierr = QEPSetProblemType(qep,QEP_GENERAL);CHKERRQ(ierr);
  }

  if (qep->nev > 2*qep->n) qep->nev = 2*qep->n;
  if (qep->ncv > 2*qep->n) qep->ncv = 2*qep->n;

  /* Call specific solver setup */
  ierr = (*qep->ops->setup)(qep);CHKERRQ(ierr);

  /* set tolerance if not yet set */
  if (qep->tol==PETSC_DEFAULT) qep->tol = SLEPC_DEFAULT_TOL;

  /* set eigenvalue comparison */
  switch (qep->which) {
    case QEP_LARGEST_MAGNITUDE:
      qep->comparison    = SlepcCompareLargestMagnitude;
      qep->comparisonctx = NULL;
      break;
    case QEP_SMALLEST_MAGNITUDE:
      qep->comparison    = SlepcCompareSmallestMagnitude;
      qep->comparisonctx = NULL;
      break;
    case QEP_LARGEST_REAL:
      qep->comparison    = SlepcCompareLargestReal;
      qep->comparisonctx = NULL;
      break;
    case QEP_SMALLEST_REAL:
      qep->comparison    = SlepcCompareSmallestReal;
      qep->comparisonctx = NULL;
      break;
    case QEP_LARGEST_IMAGINARY:
      qep->comparison    = SlepcCompareLargestImaginary;
      qep->comparisonctx = NULL;
      break;
    case QEP_SMALLEST_IMAGINARY:
      qep->comparison    = SlepcCompareSmallestImaginary;
      qep->comparisonctx = NULL;
      break;
    case QEP_TARGET_MAGNITUDE:
      qep->comparison    = SlepcCompareTargetMagnitude;
      qep->comparisonctx = &qep->target;
      break;
    case QEP_TARGET_REAL:
      qep->comparison    = SlepcCompareTargetReal;
      qep->comparisonctx = &qep->target;
      break;
    case QEP_TARGET_IMAGINARY:
      qep->comparison    = SlepcCompareTargetImaginary;
      qep->comparisonctx = &qep->target;
      break;
  }

  /* Setup ST */
  if (!islinear) {
    ierr = PetscObjectTypeCompareAny((PetscObject)qep->st,&flg,STSHIFT,STSINVERT,"");CHKERRQ(ierr);
    if (!flg) SETERRQ(PetscObjectComm((PetscObject)qep),PETSC_ERR_SUP,"Only STSHIFT and STSINVERT spectral transformations can be used in QEP");
    ierr = STSetUp(qep->st);CHKERRQ(ierr);
    /* Compute scaling factor if not set by user */
    if (!qep->sfactor_set) {
      ierr = QEPComputeScaleFactor(qep);CHKERRQ(ierr);
    }
  }

  /* process initial vectors */
  if (qep->nini<0) {
    k = -qep->nini;
    if (k>qep->ncv) SETERRQ(PetscObjectComm((PetscObject)qep),1,"The number of initial vectors is larger than ncv");
    ierr = BVInsertVecs(qep->V,0,&k,qep->IS,PETSC_TRUE);CHKERRQ(ierr);
    ierr = SlepcBasisDestroy_Private(&qep->nini,&qep->IS);CHKERRQ(ierr);
    qep->nini = k;
  }
  ierr = PetscLogEventEnd(QEP_SetUp,qep,0,0,0);CHKERRQ(ierr);
  qep->setupcalled = 1;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QEPSetOperators"
/*@
   QEPSetOperators - Sets the matrices associated with the quadratic eigenvalue problem.

   Collective on QEP and Mat

   Input Parameters:
+  qep - the eigenproblem solver context
.  M   - the first coefficient matrix
.  C   - the second coefficient matrix
-  K   - the third coefficient matrix

   Notes:
   The quadratic eigenproblem is defined as (l^2*M + l*C + K)*x = 0, where l is
   the eigenvalue and x is the eigenvector.

   Level: beginner

.seealso: QEPSolve(), QEPGetOperators()
@*/
PetscErrorCode QEPSetOperators(QEP qep,Mat M,Mat C,Mat K)
{
  PetscErrorCode ierr;
  PetscInt       m,n,m0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(qep,QEP_CLASSID,1);
  PetscValidHeaderSpecific(M,MAT_CLASSID,2);
  PetscValidHeaderSpecific(C,MAT_CLASSID,3);
  PetscValidHeaderSpecific(K,MAT_CLASSID,4);
  PetscCheckSameComm(qep,1,M,2);
  PetscCheckSameComm(qep,1,C,3);
  PetscCheckSameComm(qep,1,K,4);

  /* Check for square matrices */
  ierr = MatGetSize(M,&m,&n);CHKERRQ(ierr);
  if (m!=n) SETERRQ(PetscObjectComm((PetscObject)qep),PETSC_ERR_ARG_WRONG,"M is a non-square matrix");
  m0=m;
  ierr = MatGetSize(C,&m,&n);CHKERRQ(ierr);
  if (m!=n) SETERRQ(PetscObjectComm((PetscObject)qep),PETSC_ERR_ARG_WRONG,"C is a non-square matrix");
  if (m!=m0) SETERRQ(PetscObjectComm((PetscObject)qep),PETSC_ERR_ARG_INCOMP,"Dimensions of M and C do not match");
  ierr = MatGetSize(K,&m,&n);CHKERRQ(ierr);
  if (m!=n) SETERRQ(PetscObjectComm((PetscObject)qep),PETSC_ERR_ARG_WRONG,"K is a non-square matrix");
  if (m!=m0) SETERRQ(PetscObjectComm((PetscObject)qep),PETSC_ERR_ARG_INCOMP,"Dimensions of M and K do not match");

  /* Store a copy of the matrices */
  if (qep->setupcalled) { ierr = QEPReset(qep);CHKERRQ(ierr); }
  ierr = PetscObjectReference((PetscObject)M);CHKERRQ(ierr);
  ierr = MatDestroy(&qep->M);CHKERRQ(ierr);
  qep->M = M;
  ierr = PetscObjectReference((PetscObject)C);CHKERRQ(ierr);
  ierr = MatDestroy(&qep->C);CHKERRQ(ierr);
  qep->C = C;
  ierr = PetscObjectReference((PetscObject)K);CHKERRQ(ierr);
  ierr = MatDestroy(&qep->K);CHKERRQ(ierr);
  qep->K = K;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QEPGetOperators"
/*@
   QEPGetOperators - Gets the matrices associated with the quadratic eigensystem.

   Collective on QEP and Mat

   Input Parameter:
.  qep - the QEP context

   Output Parameters:
+  M   - the first coefficient matrix
.  C   - the second coefficient matrix
-  K   - the third coefficient matrix

   Level: intermediate

.seealso: QEPSolve(), QEPSetOperators()
@*/
PetscErrorCode QEPGetOperators(QEP qep,Mat *M,Mat *C,Mat *K)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(qep,QEP_CLASSID,1);
  if (M) { PetscValidPointer(M,2); *M = qep->M; }
  if (C) { PetscValidPointer(C,3); *C = qep->C; }
  if (K) { PetscValidPointer(K,4); *K = qep->K; }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QEPSetInitialSpace"
/*@
   QEPSetInitialSpace - Specify a basis of vectors that constitute the initial
   space, that is, the subspace from which the solver starts to iterate.

   Collective on QEP and Vec

   Input Parameter:
+  qep   - the quadratic eigensolver context
.  n     - number of vectors
-  is    - set of basis vectors of the initial space

   Notes:
   Some solvers start to iterate on a single vector (initial vector). In that case,
   the other vectors are ignored.

   These vectors do not persist from one QEPSolve() call to the other, so the
   initial space should be set every time.

   The vectors do not need to be mutually orthonormal, since they are explicitly
   orthonormalized internally.

   Common usage of this function is when the user can provide a rough approximation
   of the wanted eigenspace. Then, convergence may be faster.

   Level: intermediate
@*/
PetscErrorCode QEPSetInitialSpace(QEP qep,PetscInt n,Vec *is)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(qep,QEP_CLASSID,1);
  PetscValidLogicalCollectiveInt(qep,n,2);
  if (n<0) SETERRQ(PetscObjectComm((PetscObject)qep),PETSC_ERR_ARG_OUTOFRANGE,"Argument n cannot be negative");
  ierr = SlepcBasisReference_Private(n,is,&qep->nini,&qep->IS);CHKERRQ(ierr);
  if (n>0) qep->setupcalled = 0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QEPAllocateSolution"
/*
  QEPAllocateSolution - Allocate memory storage for common variables such
  as eigenvalues and eigenvectors. The argument extra is used for methods
  that require a working basis slightly larger than ncv.
*/
PetscErrorCode QEPAllocateSolution(QEP qep,PetscInt extra)
{
  PetscErrorCode ierr;
  PetscInt       oldsize,newc,requested;
  PetscLogDouble cnt;
  Mat            A;
  Vec            t;

  PetscFunctionBegin;
  requested = qep->ncv + extra;

  /* oldsize is zero if this is the first time setup is called */
  ierr = BVGetSizes(qep->V,NULL,NULL,&oldsize);CHKERRQ(ierr);
  newc = PetscMax(0,requested-oldsize);

  /* allocate space for eigenvalues and friends */
  if (requested != oldsize) {
    if (oldsize) {
      ierr = PetscFree4(qep->eigr,qep->eigi,qep->errest,qep->perm);CHKERRQ(ierr);
    }
    ierr = PetscMalloc4(requested,&qep->eigr,requested,&qep->eigi,requested,&qep->errest,requested,&qep->perm);CHKERRQ(ierr);
    cnt = 2*newc*sizeof(PetscScalar) + newc*sizeof(PetscReal) + newc*sizeof(PetscInt);
    ierr = PetscLogObjectMemory((PetscObject)qep,cnt);CHKERRQ(ierr);
  }

  /* allocate V */
  if (!qep->V) { ierr = QEPGetBV(qep,&qep->V);CHKERRQ(ierr); }
  if (!oldsize) {
    if (!((PetscObject)(qep->V))->type_name) {
      ierr = BVSetType(qep->V,BVSVEC);CHKERRQ(ierr);
    }
    ierr = STGetOperators(qep->st,0,&A);CHKERRQ(ierr);
    ierr = MatGetVecs(A,&t,NULL);CHKERRQ(ierr);
    ierr = BVSetSizesFromVec(qep->V,t,requested);CHKERRQ(ierr);
    ierr = VecDestroy(&t);CHKERRQ(ierr);
  } else {
    ierr = BVResize(qep->V,requested,PETSC_FALSE);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

