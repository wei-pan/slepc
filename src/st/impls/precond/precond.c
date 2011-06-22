/*
      Implements the ST class for preconditioned eigenvalue methods.

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

#include <private/stimpl.h>          /*I "slepcst.h" I*/

typedef struct {
  PetscBool setmat;
} ST_PRECOND;

#undef __FUNCT__  
#define __FUNCT__ "STSetFromOptions_Precond"
PetscErrorCode STSetFromOptions_Precond(ST st) 
{
  PetscErrorCode ierr;
  PC             pc;
  const PCType   pctype;
  Mat            P;
  PetscBool      t0,t1;

  PetscFunctionBegin;
  ierr = KSPGetPC(st->ksp,&pc);CHKERRQ(ierr);
  ierr = PetscObjectGetType((PetscObject)pc,&pctype);CHKERRQ(ierr);
  ierr = STPrecondGetMatForPC(st,&P);CHKERRQ(ierr);
  if (!pctype && st->A) {
    if (P || st->shift_matrix == ST_MATMODE_SHELL) {
      ierr = PCSetType(pc,PCJACOBI);CHKERRQ(ierr);
    } else {
      ierr = MatHasOperation(st->A,MATOP_DUPLICATE,&t0);CHKERRQ(ierr);
      if (st->B) {
        ierr = MatHasOperation(st->A,MATOP_AXPY,&t1);CHKERRQ(ierr);
      } else {
        t1 = PETSC_TRUE;
      }
      ierr = PCSetType(pc,(t0 && t1)?PCJACOBI:PCNONE);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "STSetUp_Precond"
PetscErrorCode STSetUp_Precond(ST st)
{
  Mat            P;
  PC             pc;
  PetscBool      t0,setmat,destroyP=PETSC_FALSE,builtP;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* if the user did not set the shift, use the target value */
  if (!st->sigma_set) st->sigma = st->defsigma;

  /* If pc is none and no matrix has to be set, exit */
  ierr = STSetFromOptions_Precond(st);CHKERRQ(ierr);
  if (!st->ksp) { ierr = STGetKSP(st,&st->ksp);CHKERRQ(ierr); }
  ierr = KSPGetPC(st->ksp,&pc);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)pc,PCNONE,&t0);CHKERRQ(ierr);
  ierr = STPrecondGetKSPHasMat(st,&setmat);CHKERRQ(ierr); 
  if (t0 && !setmat) PetscFunctionReturn(0);

  /* Check if a user matrix is set */
  ierr = STPrecondGetMatForPC(st,&P);CHKERRQ(ierr);

  /* If not, create A - shift*B */
  if (P) {
    builtP = PETSC_FALSE;
    destroyP = PETSC_TRUE;
    ierr = PetscObjectReference((PetscObject)P);CHKERRQ(ierr);
  } else {
    builtP = PETSC_TRUE;

    if (!(PetscAbsScalar(st->sigma) < PETSC_MAX_REAL) && st->B) {
      P = st->B;
      destroyP = PETSC_FALSE;
    } else if (st->sigma == 0.0) {
      P = st->A;
      destroyP = PETSC_FALSE;
    } else if (PetscAbsScalar(st->sigma) < PETSC_MAX_REAL && st->shift_matrix != ST_MATMODE_SHELL) {
      if (st->shift_matrix == ST_MATMODE_INPLACE) {
        P = st->A;
        destroyP = PETSC_FALSE;
      } else {
        ierr = MatDuplicate(st->A,MAT_COPY_VALUES,&P);CHKERRQ(ierr);
        destroyP = PETSC_TRUE;
      } 
      if (st->B) {
        ierr = MatAXPY(P,-st->sigma,st->B,st->str);CHKERRQ(ierr); 
      } else {
        ierr = MatShift(P,-st->sigma);CHKERRQ(ierr); 
      }
    } else 
      builtP = PETSC_FALSE;
  }

  /* If P was not possible to obtain, set pc to PCNONE */
  if (!P) {
    ierr = PCSetType(pc,PCNONE);CHKERRQ(ierr);

    /* If some matrix has to be set to ksp, set ksp to KSPPREONLY */
    if (setmat) {
      ierr = STMatShellCreate(st,&P);CHKERRQ(ierr);
      destroyP = PETSC_TRUE;
      ierr = KSPSetType(st->ksp,KSPPREONLY);CHKERRQ(ierr);
    }
  }

  ierr = KSPSetOperators(st->ksp,setmat?P:PETSC_NULL,P,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);

  if (destroyP) {
    ierr = MatDestroy(&P);CHKERRQ(ierr);
  } else if (st->shift_matrix == ST_MATMODE_INPLACE && builtP) {
    if (st->sigma != 0.0 && PetscAbsScalar(st->sigma) < PETSC_MAX_REAL) {
      if (st->B) {
        ierr = MatAXPY(st->A,st->sigma,st->B,st->str);CHKERRQ(ierr); 
      } else { 
        ierr = MatShift(st->A,st->sigma);CHKERRQ(ierr); 
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "STSetShift_Precond"
PetscErrorCode STSetShift_Precond(ST st,PetscScalar newshift)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* Nothing to be done if STSetUp has not been called yet */
  if (!st->setupcalled) PetscFunctionReturn(0);
  st->sigma = newshift;
  if (st->shift_matrix != ST_MATMODE_SHELL) {
    ierr = STSetUp_Precond(st);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "STPrecondGetMatForPC_Precond"
PetscErrorCode STPrecondGetMatForPC_Precond(ST st,Mat *mat)
{
  PetscErrorCode ierr;
  PC             pc;
  PetscBool      flag;

  PetscFunctionBegin;
  if (!st->ksp) { ierr = STGetKSP(st,&st->ksp);CHKERRQ(ierr); }
  ierr = KSPGetPC(st->ksp,&pc);CHKERRQ(ierr);
  ierr = PCGetOperatorsSet(pc,PETSC_NULL,&flag);CHKERRQ(ierr);
  if (flag) {
    ierr = PCGetOperators(pc,PETSC_NULL,mat,PETSC_NULL);CHKERRQ(ierr);
  } else *mat = PETSC_NULL;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "STPrecondGetMatForPC"
/*@
   STPrecondGetMatForPC - Returns the matrix previously set by STPrecondSetMatForPC().

   Not Collective, but the Mat is shared by all processors that share the ST

   Input Parameter:
.  st - the spectral transformation context

   Output Parameter:
.  mat - the matrix that will be used in constructing the preconditioner or
   PETSC_NULL if no matrix was set by STPrecondSetMatForPC().

   Level: advanced

.seealso: STPrecondSetMatForPC()
@*/
PetscErrorCode STPrecondGetMatForPC(ST st,Mat *mat)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidPointer(mat,2);
  ierr = PetscTryMethod(st,"STPrecondGetMatForPC_C",(ST,Mat*),(st,mat));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "STPrecondSetMatForPC_Precond"
PetscErrorCode STPrecondSetMatForPC_Precond(ST st,Mat mat)
{
  PC             pc;
  Mat            A;
  PetscBool      flag;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!st->ksp) { ierr = STGetKSP(st,&st->ksp);CHKERRQ(ierr); }
  ierr = KSPGetPC(st->ksp,&pc);CHKERRQ(ierr);
  /* Yes, all these lines are needed to safely set mat as the preconditioner
     matrix in pc */
  ierr = PCGetOperatorsSet(pc,&flag,PETSC_NULL);CHKERRQ(ierr);
  if (flag) {
    ierr = PCGetOperators(pc,&A,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
    ierr = PetscObjectReference((PetscObject)A);CHKERRQ(ierr);
  } else
    A = PETSC_NULL;
  ierr = PetscObjectReference((PetscObject)mat);CHKERRQ(ierr);
  ierr = PCSetOperators(pc,A,mat,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = MatDestroy(&mat);CHKERRQ(ierr);
  ierr = STPrecondSetKSPHasMat(st,PETSC_TRUE);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "STPrecondSetMatForPC"
/*@
   STPrecondSetMatForPC - Sets the matrix that must be used to build the preconditioner.

   Logically Collective on ST and Mat

   Input Parameter:
+  st - the spectral transformation context
-  mat - the matrix that will be used in constructing the preconditioner

   Level: advanced

   Notes:
   This matrix will be passed to the KSP object (via KSPSetOperators) as
   the matrix to be used when constructing the preconditioner.
   If no matrix is set or mat is set to PETSC_NULL, A - sigma*B will
   be used to build the preconditioner, being sigma the value set by STSetShift().

.seealso: STPrecondSetMatForPC(), STSetShift()
@*/
PetscErrorCode STPrecondSetMatForPC(ST st,Mat mat)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidHeaderSpecific(mat,MAT_CLASSID,2);
  PetscCheckSameComm(st,1,mat,2);
  ierr = PetscTryMethod(st,"STPrecondSetMatForPC_C",(ST,Mat),(st,mat));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "STPrecondSetKSPHasMat_Precond"
PetscErrorCode STPrecondSetKSPHasMat_Precond(ST st,PetscBool setmat)
{
  ST_PRECOND *data = (ST_PRECOND*)st->data;

  PetscFunctionBegin;
  data->setmat = setmat;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "STPrecondSetKSPHasMat"
/*@
   STPrecondSetKSPHasMat - Sets if during the STSetUp the KSP matrix associated
   to the linear system is set with the matrix for building the preconditioner.

   Collective on ST

   Input Parameter:
+  st - the spectral transformation context
-  setmat - if true, the KSP matrix associated to linear system is set with
   the matrix for building the preconditioner

   Level: developer

.seealso: STPrecondGetKSPHasMat(), STSetShift()
@*/
PetscErrorCode STPrecondSetKSPHasMat(ST st,PetscBool setmat)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidLogicalCollectiveBool(st,setmat,2);
  ierr = PetscTryMethod(st,"STPrecondSetKSPHasMat_C",(ST,PetscBool),(st,setmat));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__
#define __FUNCT__ "STPrecondGetKSPHasMat_Precond"
PetscErrorCode STPrecondGetKSPHasMat_Precond(ST st,PetscBool *setmat)
{
  ST_PRECOND *data = (ST_PRECOND*)st->data;

  PetscFunctionBegin;
  *setmat = data->setmat;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "STPrecondGetKSPHasMat"
/*@
   STPrecondGetKSPHasMat - Gets if during the STSetUp the KSP matrix associated
   to the linear system is set with the matrix for building the preconditioner.

   Not Collective

   Input Parameter:
.  st - the spectral transformation context

   Output Parameter:
.  setmat - if true, the KSP matrix associated to linear system is set with
   the matrix for building the preconditioner

   Level: developer

.seealso: STPrecondSetKSPHasMat(), STSetShift()
@*/
PetscErrorCode STPrecondGetKSPHasMat(ST st,PetscBool *setmat)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidPointer(setmat,2);
  ierr = PetscTryMethod(st,"STPrecondGetKSPHasMat_C",(ST,PetscBool*),(st,setmat));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "STDestroy_Precond"
PetscErrorCode STDestroy_Precond(ST st)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFree(st->data);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)st,"STPrecondGetMatForPC_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)st,"STPrecondSetMatForPC_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)st,"STPrecondGetKSPHasMat_C","",PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)st,"STPrecondSetKSPHasMat_C","",PETSC_NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
#undef __FUNCT__  
#define __FUNCT__ "STCreate_Precond"
PetscErrorCode STCreate_Precond(ST st)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscNewLog(st,ST_PRECOND,&st->data);CHKERRQ(ierr);
  st->ops->getbilinearform = STGetBilinearForm_Default;
  st->ops->setup           = STSetUp_Precond;
  st->ops->setshift        = STSetShift_Precond;
  st->ops->destroy         = STDestroy_Precond;
  st->ops->setfromoptions  = STSetFromOptions_Precond;

  ierr = PetscObjectComposeFunctionDynamic((PetscObject)st,"STPrecondGetMatForPC_C","STPrecondGetMatForPC_Precond",STPrecondGetMatForPC_Precond);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)st,"STPrecondSetMatForPC_C","STPrecondSetMatForPC_Precond",STPrecondSetMatForPC_Precond);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)st,"STPrecondGetKSPHasMat_C","STPrecondGetKSPHasMat_Precond",STPrecondGetKSPHasMat_Precond);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunctionDynamic((PetscObject)st,"STPrecondSetKSPHasMat_C","STPrecondSetKSPHasMat_Precond",STPrecondSetKSPHasMat_Precond);CHKERRQ(ierr);

  ierr = STPrecondSetKSPHasMat_Precond(st,PETSC_TRUE);CHKERRQ(ierr);
  if (!st->ksp) { ierr = STGetKSP(st,&st->ksp);CHKERRQ(ierr); }
  ierr = KSPSetType(st->ksp,KSPPREONLY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END

