/*
   Basic BV routines.

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

#include <slepc-private/bvimpl.h>      /*I "slepcbv.h" I*/

PetscBool         BVRegisterAllCalled = PETSC_FALSE;
PetscFunctionList BVList = 0;

#undef __FUNCT__
#define __FUNCT__ "BVSetType"
/*@C
   BVSetType - Selects the type for the BV object.

   Logically Collective on BV

   Input Parameter:
+  bv   - the basis vectors context
-  type - a known type

   Options Database Key:
.  -bv_type <type> - Sets BV type

   Level: intermediate

.seealso: BVGetType()

@*/
PetscErrorCode BVSetType(BV bv,BVType type)
{
  PetscErrorCode ierr,(*r)(BV);
  PetscBool      match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidCharPointer(type,2);

  ierr = PetscObjectTypeCompare((PetscObject)bv,type,&match);CHKERRQ(ierr);
  if (match) PetscFunctionReturn(0);

  ierr =  PetscFunctionListFind(BVList,type,&r);CHKERRQ(ierr);
  if (!r) SETERRQ1(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_UNKNOWN_TYPE,"Unable to find requested BV type %s",type);

  if (bv->ops->destroy) { ierr = (*bv->ops->destroy)(bv);CHKERRQ(ierr); }
  ierr = PetscMemzero(bv->ops,sizeof(struct _BVOps));CHKERRQ(ierr);

  ierr = PetscObjectChangeTypeName((PetscObject)bv,type);CHKERRQ(ierr);
  if (bv->n < 0 && bv->N < 0) {
    bv->ops->create = r;
  } else {
    ierr = PetscLogEventBegin(BV_Create,bv,0,0,0);CHKERRQ(ierr);
    ierr = (*r)(bv);CHKERRQ(ierr);
    ierr = PetscLogEventEnd(BV_Create,bv,0,0,0);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVGetType"
/*@C
   BVGetType - Gets the BV type name (as a string) from the BV context.

   Not Collective

   Input Parameter:
.  bv - the basis vectors context

   Output Parameter:
.  name - name of the type of basis vectors

   Level: intermediate

.seealso: BVSetType()

@*/
PetscErrorCode BVGetType(BV bv,BVType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidPointer(type,2);
  *type = ((PetscObject)bv)->type_name;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVSetSizes"
/*@
  BVSetSizes - Sets the local and global sizes, and the number of columns.

  Collective on BV

  Input Parameters:
+ bv - the basis vectors
. n  - the local size (or PETSC_DECIDE to have it set)
. N  - the global size (or PETSC_DECIDE)
- m  - the number of columns

  Notes:
  n and N cannot be both PETSC_DECIDE.
  If one processor calls this with N of PETSC_DECIDE then all processors must,
  otherwise the program will hang.

  Level: beginner

.seealso: BVSetSizesFromVec(), BVGetSizes(), BVResize()
@*/
PetscErrorCode BVSetSizes(BV bv,PetscInt n,PetscInt N,PetscInt m)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  if (N >= 0) PetscValidLogicalCollectiveInt(bv,N,3);
  PetscValidLogicalCollectiveInt(bv,m,4);
  if (N >= 0 && n > N) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Local size %D cannot be larger than global size %D",n,N);
  if (m <= 0) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Number of columns %D must be positive",m);
  if ((bv->n >= 0 || bv->N >= 0) && (bv->n != n || bv->N != N)) SETERRQ4(PETSC_COMM_SELF,PETSC_ERR_SUP,"Cannot change/reset vector sizes to %D local %D global after previously setting them to %D local %D global",n,N,bv->n,bv->N);
  if (bv->m > 0 && bv->m != m) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_SUP,"Cannot change the number of columns to %D after previously setting it to %D; use BVResize()",m,bv->m);
  bv->n = n;
  bv->N = N;
  bv->m = m;
  bv->k = m;
  if (!bv->t) {  /* create template vector and get actual dimensions */
    ierr = VecCreate(PetscObjectComm((PetscObject)bv),&bv->t);CHKERRQ(ierr);
    ierr = VecSetSizes(bv->t,bv->n,bv->N);CHKERRQ(ierr);
    ierr = VecSetFromOptions(bv->t);CHKERRQ(ierr);
    ierr = VecGetSize(bv->t,&bv->N);CHKERRQ(ierr);
    ierr = VecGetLocalSize(bv->t,&bv->n);CHKERRQ(ierr);
  }
  if (bv->ops->create) {
    ierr = PetscLogEventBegin(BV_Create,bv,0,0,0);CHKERRQ(ierr);
    ierr = (*bv->ops->create)(bv);CHKERRQ(ierr);
    ierr = PetscLogEventEnd(BV_Create,bv,0,0,0);CHKERRQ(ierr);
    bv->ops->create = 0;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVSetSizesFromVec"
/*@
  BVSetSizesFromVec - Sets the local and global sizes, and the number of columns.
  Local and global sizes are specified indirectly by passing a template vector.

  Collective on BV

  Input Parameters:
+ bv - the basis vectors
. t  - the template vectors
- m  - the number of columns

  Level: beginner

.seealso: BVSetSizes(), BVGetSizes(), BVResize()
@*/
PetscErrorCode BVSetSizesFromVec(BV bv,Vec t,PetscInt m)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidHeaderSpecific(t,VEC_CLASSID,2);
  PetscCheckSameComm(bv,1,t,2);
  PetscValidLogicalCollectiveInt(bv,m,3);
  if (m <= 0) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Number of columns %D must be positive",m);
  if (bv->t) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"Template vector was already set by a previous call to BVSetSizes/FromVec");
  ierr = VecGetSize(t,&bv->N);CHKERRQ(ierr);
  ierr = VecGetLocalSize(t,&bv->n);CHKERRQ(ierr);
  bv->m = m;
  bv->k = m;
  bv->t = t;
  ierr = PetscObjectReference((PetscObject)t);CHKERRQ(ierr);
  if (bv->ops->create) {
    ierr = (*bv->ops->create)(bv);CHKERRQ(ierr);
    bv->ops->create = 0;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVGetSizes"
/*@
  BVGetSizes - Returns the local and global sizes, and the number of columns.

  Not Collective

  Input Parameter:
. bv - the basis vectors

  Output Parameters:
+ n  - the local size
. N  - the global size
- m  - the number of columns

  Note:
  Normal usage requires that bv has already been given its sizes, otherwise
  the call fails. However, this function can also be used to determine if
  a BV object has been initialized completely (sizes and type). For this,
  call with n=NULL and N=NULL, then a return value of m=0 indicates that
  the BV object is not ready for use yet.

  Level: beginner

.seealso: BVSetSizes(), BVSetSizesFromVec()
@*/
PetscErrorCode BVGetSizes(BV bv,PetscInt *n,PetscInt *N,PetscInt *m)
{
  PetscFunctionBegin;
  if (!bv) {
    if (m && !n && !N) *m = 0;
    PetscFunctionReturn(0);
  }
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  if (n || N) BVCheckSizes(bv,1);
  if (n) *n = bv->n;
  if (N) *N = bv->N;
  if (m) *m = bv->m;
  if (m && !n && !N && !((PetscObject)bv)->type_name) *m = 0;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVResize"
/*@
  BVResize - Change the number of columns.

  Collective on BV

  Input Parameters:
+ bv   - the basis vectors
. m    - the new number of columns
- copy - a flag indicating whether current values should be kept

  Note:
  Internal storage is reallocated. If the copy flag is set to true, then
  the contents are copied to the leading part of the new space.

  Level: advanced

.seealso: BVSetSizes(), BVSetSizesFromVec()
@*/
PetscErrorCode BVResize(BV bv,PetscInt m,PetscBool copy)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidLogicalCollectiveInt(bv,m,2);
  PetscValidLogicalCollectiveBool(bv,copy,3);
  if (m <= 0) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Number of columns %D must be positive",m);
  if (bv->m == m) PetscFunctionReturn(0);

  ierr = PetscLogEventBegin(BV_Create,bv,0,0,0);CHKERRQ(ierr);
  ierr = (*bv->ops->resize)(bv,m,copy);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(BV_Create,bv,0,0,0);CHKERRQ(ierr);
  bv->m = m;
  bv->k = PetscMin(bv->k,m);
  bv->l = PetscMin(bv->l,m);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVSetActiveColumns"
/*@
   BVSetActiveColumns - Specify the columns that will be involved in operations.

   Logically Collective on BV

   Input Parameters:
+  bv - the basis vectors context
.  l  - number of leading columns
-  k  - number of active columns

   Notes:
   In operations such as BVMult() or BVDot(), only the first k columns are
   considered. This is useful when the BV is filled from left to right, so
   the last m-k columns do not have relevant information.

   Also in operations such as BVMult() or BVDot(), the first l columns are
   normally not included in the computation. See the manpage of each
   operation.

   In orthogonalization operations, the first l columns are treated
   differently: they participate in the orthogonalization but the computed
   coefficients are not stored.

   Level: intermediate

.seealso: BVGetActiveColumns(), BVSetSizes()
@*/
PetscErrorCode BVSetActiveColumns(BV bv,PetscInt l,PetscInt k)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidLogicalCollectiveInt(bv,l,2);
  PetscValidLogicalCollectiveInt(bv,k,3);
  BVCheckSizes(bv,1);
  if (k==PETSC_DECIDE || k==PETSC_DEFAULT) {
    bv->k = bv->m;
  } else {
    if (k<0 || k>bv->m) SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of k. Must be between 0 and m");
    bv->k = k;
  }
  if (l==PETSC_DECIDE || l==PETSC_DEFAULT) {
    bv->l = 0;
  } else {
    if (l<0 || l>bv->k-1) SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of l. Must be between 0 and k-1");
    bv->l = l;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVGetActiveColumns"
/*@
   BVGetActiveColumns - Returns the current active dimensions.

   Not Collective

   Input Parameter:
.  bv - the basis vectors context

   Output Parameter:
+  l  - number of leading columns
-  k  - number of active columns

   Level: intermediate

.seealso: BVSetActiveColumns()
@*/
PetscErrorCode BVGetActiveColumns(BV bv,PetscInt *l,PetscInt *k)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  if (l) *l = bv->l;
  if (k) *k = bv->k;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVSetMatrix"
/*@
   BVSetMatrix - Specifies the inner product to be used in orthogonalization.

   Collective on BV

   Input Parameters:
+  bv    - the basis vectors context
.  B     - a symmetric matrix (may be NULL)
-  indef - a flag indicating if the matrix is indefinite

   Notes:
   This is used to specify a non-standard inner product, whose matrix
   representation is given by B. Then, all inner products required during
   orthogonalization are computed as (x,y)_B=y^H*B*x rather than the
   standard form (x,y)=y^H*x.

   Matrix B must be real symmetric (or complex Hermitian). A genuine inner
   product requires that B is also positive (semi-)definite. However, we
   also allow for an indefinite B (setting indef=PETSC_TRUE), in which
   case the orthogonalization uses an indefinite inner product.

   This affects operations BVDot(), BVDotVec(), BVNorm(), and BVOrthogonalize().

   Setting B=NULL has the same effect as if the identity matrix was passed.

   Level: advanced

.seealso: BVGetMatrix(), BVDot(), BVDotVec(), BVNorm(), BVOrthogonalize()
@*/
PetscErrorCode BVSetMatrix(BV bv,Mat B,PetscBool indef)
{
  PetscErrorCode ierr;
  PetscInt       m,n;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidType(bv,1);
  PetscValidLogicalCollectiveBool(bv,indef,3);
  if (B) {
    PetscValidHeaderSpecific(B,MAT_CLASSID,2);
    BVCheckSizes(bv,1);
    ierr = MatGetLocalSize(B,&m,&n);CHKERRQ(ierr);
    if (m!=n) SETERRQ(PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"Matrix must be square");
    if (bv->n!=n) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Mismatching local dimension BV %D, Mat %D",bv->n,n);
  }

  ierr = MatDestroy(&bv->matrix);CHKERRQ(ierr);
  if (B) PetscObjectReference((PetscObject)B);
  bv->matrix = B;
  bv->indef  = indef;
  if (!bv->Bx && B) {
    ierr = MatGetVecs(B,&bv->Bx,NULL);CHKERRQ(ierr);
    ierr = PetscLogObjectParent((PetscObject)bv,(PetscObject)bv->Bx);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVGetMatrix"
/*@C
   BVGetMatrix - Retrieves the matrix representation of the inner product.

   Not collective, though a parallel Mat may be returned

   Input Parameter:
.  bv    - the basis vectors context

   Output Parameter:
+  B     - the matrix of the inner product (may be NULL)
-  indef - the flag indicating if the matrix is indefinite

   Level: advanced

.seealso: BVSetMatrix()
@*/
PetscErrorCode BVGetMatrix(BV bv,Mat *B,PetscBool *indef)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  if (B)     *B     = bv->matrix;
  if (indef) *indef = bv->indef;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVSetSignature"
/*@
   BVSetSignature - Sets the signature matrix to be used in orthogonalization.

   Logically Collective on BV

   Input Parameter:
+  bv    - the basis vectors context
-  omega - a vector representing the diagonal of the signature matrix

   Note:
   The signature matrix Omega = V'*B*V is relevant only for an indefinite B.

   Level: developer

.seealso: BVSetMatrix(), BVGetSignature()
@*/
PetscErrorCode BVSetSignature(BV bv,Vec omega)
{
  PetscErrorCode ierr;
  PetscInt       i,n;
  PetscScalar    *pomega;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidType(bv,1);
  BVCheckSizes(bv,1);
  PetscValidHeaderSpecific(omega,VEC_CLASSID,2);
  PetscValidType(omega,2);

  ierr = VecGetSize(omega,&n);CHKERRQ(ierr);
  if (n!=bv->k) SETERRQ2(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_SIZ,"Vec argument has %D elements, should be %D",n,bv->k);
  if (bv->indef) {
    if (!bv->omega) {
      ierr = PetscMalloc1(bv->m,&bv->omega);CHKERRQ(ierr);
      ierr = PetscLogObjectMemory((PetscObject)bv,bv->m*sizeof(PetscReal));CHKERRQ(ierr);
    }
    ierr = VecGetArray(omega,&pomega);CHKERRQ(ierr);
    for (i=0;i<n;i++) bv->omega[i] = PetscRealPart(pomega[i]);
    ierr = VecRestoreArray(omega,&pomega);CHKERRQ(ierr);
  } else {
    ierr = PetscInfo(bv,"Ignoring signature because BV is not indefinite\n");CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVGetSignature"
/*@
   BVGetSignature - Retrieves the signature matrix from last orthogonalization.

   Not collective

   Input Parameter:
.  bv    - the basis vectors context

   Output Parameter:
.  omega - a vector representing the diagonal of the signature matrix

   Note:
   The signature matrix Omega = V'*B*V is relevant only for an indefinite B.

   Level: developer

.seealso: BVSetMatrix(), BVSetSignature()
@*/
PetscErrorCode BVGetSignature(BV bv,Vec omega)
{
  PetscErrorCode ierr;
  PetscInt       i,n;
  PetscScalar    *pomega;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidType(bv,1);
  BVCheckSizes(bv,1);
  PetscValidHeaderSpecific(omega,VEC_CLASSID,2);
  PetscValidType(omega,2);

  ierr = VecGetSize(omega,&n);CHKERRQ(ierr);
  if (n!=bv->k) SETERRQ2(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_SIZ,"Vec argument has %D elements, should be %D",n,bv->k);
  if (bv->indef && bv->omega) {
    ierr = VecGetArray(omega,&pomega);CHKERRQ(ierr);
    for (i=0;i<n;i++) pomega[i] = bv->omega[i];
    ierr = VecRestoreArray(omega,&pomega);CHKERRQ(ierr);
  } else {
    ierr = VecSet(omega,1.0);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVSetFromOptions"
/*@
   BVSetFromOptions - Sets BV options from the options database.

   Collective on BV

   Input Parameter:
.  bv - the basis vectors context

   Level: beginner
@*/
PetscErrorCode BVSetFromOptions(BV bv)
{
  PetscErrorCode ierr;
  char           type[256];
  PetscBool      flg;
  PetscReal      r;
  PetscInt       i,j;
  const char     *orth_list[2] = {"cgs","mgs"};
  const char     *ref_list[3] = {"ifneeded","never","always"};

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  if (!BVRegisterAllCalled) { ierr = BVRegisterAll();CHKERRQ(ierr); }
  ierr = PetscObjectOptionsBegin((PetscObject)bv);CHKERRQ(ierr);
    ierr = PetscOptionsFList("-bv_type","Basis Vectors type","BVSetType",BVList,(char*)(((PetscObject)bv)->type_name?((PetscObject)bv)->type_name:BVSVEC),type,256,&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = BVSetType(bv,type);CHKERRQ(ierr);
    }
    /*
      Set the type if it was never set.
    */
    if (!((PetscObject)bv)->type_name) {
      ierr = BVSetType(bv,BVSVEC);CHKERRQ(ierr);
    }

    i = bv->orthog_type;
    ierr = PetscOptionsEList("-bv_orthog_type","Orthogonalization method","BVSetOrthogonalization",orth_list,2,orth_list[i],&i,NULL);CHKERRQ(ierr);
    j = bv->orthog_ref;
    ierr = PetscOptionsEList("-bv_orthog_refine","Iterative refinement mode during orthogonalization","BVSetOrthogonalization",ref_list,3,ref_list[j],&j,NULL);CHKERRQ(ierr);
    r = bv->orthog_eta;
    ierr = PetscOptionsReal("-bv_orthog_eta","Parameter of iterative refinement during orthogonalization","BVSetOrthogonalization",r,&r,NULL);CHKERRQ(ierr);
    ierr = BVSetOrthogonalization(bv,(BVOrthogType)i,(BVOrthogRefineType)j,r);CHKERRQ(ierr);

    if (bv->ops->setfromoptions) {
      ierr = (*bv->ops->setfromoptions)(bv);CHKERRQ(ierr);
    }
    ierr = PetscObjectProcessOptionsHandlers((PetscObject)bv);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVSetOrthogonalization"
/*@
   BVSetOrthogonalization - Specifies the type of orthogonalization technique
   to be used (classical or modified Gram-Schmidt with or without refinement).

   Logically Collective on BV

   Input Parameters:
+  bv     - the basis vectors context
.  type   - the type of orthogonalization technique
.  refine - type of refinement
-  eta    - parameter for selective refinement

   Options Database Keys:
+  -bv_orthog_type <type> - Where <type> is cgs for Classical Gram-Schmidt orthogonalization
                         (default) or mgs for Modified Gram-Schmidt orthogonalization
.  -bv_orthog_refine <ref> - Where <ref> is one of never, ifneeded (default) or always
-  -bv_orthog_eta <eta> -  For setting the value of eta

   Notes:
   The default settings work well for most problems.

   The parameter eta should be a real value between 0 and 1 (or PETSC_DEFAULT).
   The value of eta is used only when the refinement type is "ifneeded".

   When using several processors, MGS is likely to result in bad scalability.

   Level: advanced

.seealso: BVOrthogonalize(), BVGetOrthogonalization(), BVOrthogType, BVOrthogRefineType
@*/
PetscErrorCode BVSetOrthogonalization(BV bv,BVOrthogType type,BVOrthogRefineType refine,PetscReal eta)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidLogicalCollectiveEnum(bv,type,2);
  PetscValidLogicalCollectiveEnum(bv,refine,3);
  PetscValidLogicalCollectiveReal(bv,eta,4);
  switch (type) {
    case BV_ORTHOG_CGS:
    case BV_ORTHOG_MGS:
      bv->orthog_type = type;
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_WRONG,"Unknown orthogonalization type");
  }
  switch (refine) {
    case BV_ORTHOG_REFINE_NEVER:
    case BV_ORTHOG_REFINE_IFNEEDED:
    case BV_ORTHOG_REFINE_ALWAYS:
      bv->orthog_ref = refine;
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_WRONG,"Unknown refinement type");
  }
  if (eta == PETSC_DEFAULT) {
    bv->orthog_eta = 0.7071;
  } else {
    if (eta <= 0.0 || eta > 1.0) SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_OUTOFRANGE,"Invalid eta value");
    bv->orthog_eta = eta;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVGetOrthogonalization"
/*@C
   BVGetOrthogonalization - Gets the orthogonalization settings from the BV object.

   Not Collective

   Input Parameter:
.  bv - basis vectors context

   Output Parameter:
+  type   - type of orthogonalization technique
.  refine - type of refinement
-  eta    - parameter for selective refinement

   Level: advanced

.seealso: BVOrthogonalize(), BVSetOrthogonalization(), BVOrthogType, BVOrthogRefineType
@*/
PetscErrorCode BVGetOrthogonalization(BV bv,BVOrthogType *type,BVOrthogRefineType *refine,PetscReal *eta)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  if (type)   *type   = bv->orthog_type;
  if (refine) *refine = bv->orthog_ref;
  if (eta)    *eta    = bv->orthog_eta;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVGetColumn"
/*@
   BVGetColumn - Returns a Vec object that contains the entries of the
   requested column of the basis vectors object.

   Collective on BV

   Input Parameters:
+  bv - the basis vectors context
-  j  - the index of the requested column

   Output Parameter:
.  v  - vector containing the jth column

   Notes:
   The returned Vec must be seen as a reference (not a copy) of the BV
   column, that is, modifying the Vec will change the BV entries as well.

   The returned Vec must not be destroyed. BVRestoreColumn() must be
   called when it is no longer needed. At most, two columns can be fetched,
   that is, this function can only be called twice before the corresponding
   BVRestoreColumn() is invoked.

   Level: beginner

.seealso: BVRestoreColumn()
@*/
PetscErrorCode BVGetColumn(BV bv,PetscInt j,Vec *v)
{
  PetscErrorCode ierr;
  PetscInt       l;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidType(bv,1);
  BVCheckSizes(bv,1);
  PetscValidLogicalCollectiveInt(bv,j,2);
  if (j<0) SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_OUTOFRANGE,"Column index must be non-negative");
  if (j>=bv->m) SETERRQ2(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_OUTOFRANGE,"You requested column %D but only %D are available",j,bv->m);
  if (j==bv->ci[0] || j==bv->ci[1]) SETERRQ1(PetscObjectComm((PetscObject)bv),PETSC_ERR_SUP,"Column %D already fetched in a previous call to BVGetColumn",j);
  l = BVAvailableVec;
  if (l==-1) SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_SUP,"Too many requested columns; you must call BVReleaseColumn for one of the previously fetched columns");
  ierr = (*bv->ops->getcolumn)(bv,j,v);CHKERRQ(ierr);
  bv->ci[l] = j;
  ierr = PetscObjectStateGet((PetscObject)bv->cv[l],&bv->st[l]);CHKERRQ(ierr);
  ierr = PetscObjectGetId((PetscObject)bv->cv[l],&bv->id[l]);CHKERRQ(ierr);
  *v = bv->cv[l];
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVRestoreColumn"
/*@
   BVRestoreColumn - Restore a column obtained with BVGetColumn().

   Logically Collective on BV

   Input Parameters:
+  bv - the basis vectors context
.  j  - the index of the column
-  v  - vector obtained with BVGetColumn()

   Note:
   The arguments must match the corresponding call to BVGetColumn().

   Level: beginner

.seealso: BVGetColumn()
@*/
PetscErrorCode BVRestoreColumn(BV bv,PetscInt j,Vec *v)
{
  PetscErrorCode   ierr;
  PetscObjectId    id;
  PetscObjectState st;
  PetscInt         l;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidType(bv,1);
  BVCheckSizes(bv,1);
  PetscValidLogicalCollectiveInt(bv,j,2);
  PetscValidPointer(v,3);
  PetscValidHeaderSpecific(*v,VEC_CLASSID,3);
  if (j<0) SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_OUTOFRANGE,"Column index must be non-negative");
  if (j>=bv->m) SETERRQ2(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_OUTOFRANGE,"You requested column %D but only %D are available",j,bv->m);
  if (j!=bv->ci[0] && j!=bv->ci[1]) SETERRQ1(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_WRONG,"Column %D has not been fetched with a call to BVGetColumn",j);
  l = (j==bv->ci[0])? 0: 1;
  ierr = PetscObjectGetId((PetscObject)*v,&id);CHKERRQ(ierr);
  if (id!=bv->id[l]) SETERRQ(PetscObjectComm((PetscObject)bv),PETSC_ERR_ARG_WRONG,"Argument 3 is not the same Vec that was obtained with BVGetColumn");
  ierr = PetscObjectStateGet((PetscObject)*v,&st);CHKERRQ(ierr);
  if (st!=bv->st[l]) {
    ierr = PetscObjectStateIncrease((PetscObject)bv);CHKERRQ(ierr);
  }
  if (bv->ops->restorecolumn) {
    ierr = (*bv->ops->restorecolumn)(bv,j,v);CHKERRQ(ierr);
  } else bv->cv[l] = NULL;
  bv->ci[l] = -1;
  bv->st[l] = -1;
  bv->id[l] = 0;
  *v = NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVGetVec"
/*@
   BVGetVec - Creates a new Vec object with the same type and dimensions
   as the columns of the basis vectors object.

   Collective on BV

   Input Parameter:
.  bv - the basis vectors context

   Output Parameter:
.  v  - the new vector

   Note:
   The user is responsible of destroying the returned vector.

   Level: beginner
@*/
PetscErrorCode BVGetVec(BV bv,Vec *v)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(bv,BV_CLASSID,1);
  PetscValidType(bv,1);
  BVCheckSizes(bv,1);
  PetscValidPointer(v,2);
  ierr = VecDuplicate(bv->t,v);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVCopy"
/*@
   BVCopy - Copies a basis vector object into another one, W <- V.

   Logically Collective on BV

   Input Parameter:
.  V - basis vectors context

   Output Parameter:
.  W - the copy

   Note:
   Both V and W must be distributed in the same manner; local copies are
   done. Only the active columns of V are copied.

   Level: beginner

.seealso: BVCopyVec(), BVCopyColumn()
@*/
PetscErrorCode BVCopy(BV V,BV W)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(V,BV_CLASSID,1);
  PetscValidType(V,1);
  BVCheckSizes(V,1);
  PetscValidHeaderSpecific(W,BV_CLASSID,2);
  PetscValidType(W,2);
  BVCheckSizes(W,2);
  PetscCheckSameTypeAndComm(V,1,W,2);
  if (V->n!=W->n) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Mismatching local dimension V %D, W %D",V->n,W->n);
  if (V->k>W->m) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_SIZ,"W has %D columns, not enough to store %D columns",W->m,V->k);
  if (!V->n) PetscFunctionReturn(0);

  ierr = PetscLogEventBegin(BV_Copy,V,W,0,0);CHKERRQ(ierr);
  ierr = (*V->ops->copy)(V,W);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(BV_Copy,V,W,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVCopyVec"
/*@
   BVCopyVec - Copies one of the columns of a basis vectors object into a Vec.

   Logically Collective on BV

   Input Parameter:
+  V - basis vectors context
-  j - the column number to be copied

   Output Parameter:
.  w - the copied column

   Note:
   Both V and w must be distributed in the same manner; local copies are done.

   Level: beginner

.seealso: BVCopy(), BVCopyColumn()
@*/
PetscErrorCode BVCopyVec(BV V,PetscInt j,Vec w)
{
  PetscErrorCode ierr;
  PetscInt       n,N;
  Vec            z;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(V,BV_CLASSID,1);
  PetscValidType(V,1);
  BVCheckSizes(V,1);
  PetscValidLogicalCollectiveInt(V,j,2);
  PetscValidHeaderSpecific(w,VEC_CLASSID,3);
  PetscCheckSameComm(V,1,w,3);

  ierr = VecGetSize(w,&N);CHKERRQ(ierr);
  ierr = VecGetLocalSize(w,&n);CHKERRQ(ierr);
  if (N!=V->N || n!=V->n) SETERRQ4(PetscObjectComm((PetscObject)V),PETSC_ERR_ARG_INCOMP,"Vec sizes (global %D, local %D) do not match BV sizes (global %D, local %D)",N,n,V->N,V->n);

  ierr = PetscLogEventBegin(BV_Copy,V,w,0,0);CHKERRQ(ierr);
  ierr = BVGetColumn(V,j,&z);CHKERRQ(ierr);
  ierr = VecCopy(z,w);CHKERRQ(ierr);
  ierr = BVRestoreColumn(V,j,&z);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(BV_Copy,V,w,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "BVCopyColumn"
/*@
   BVCopyColumn - Copies the values from one of the columns to another one.

   Logically Collective on BV

   Input Parameter:
+  V - basis vectors context
.  j - the number of the source column
-  i - the number of the destination column

   Level: beginner

.seealso: BVCopy(), BVCopyVec()
@*/
PetscErrorCode BVCopyColumn(BV V,PetscInt j,PetscInt i)
{
  PetscErrorCode ierr;
  Vec            z,w;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(V,BV_CLASSID,1);
  PetscValidType(V,1);
  BVCheckSizes(V,1);
  PetscValidLogicalCollectiveInt(V,j,2);
  PetscValidLogicalCollectiveInt(V,i,3);

  ierr = PetscLogEventBegin(BV_Copy,V,w,0,0);CHKERRQ(ierr);
  ierr = BVGetColumn(V,j,&z);CHKERRQ(ierr);
  ierr = BVGetColumn(V,i,&w);CHKERRQ(ierr);
  ierr = VecCopy(z,w);CHKERRQ(ierr);
  ierr = BVRestoreColumn(V,j,&z);CHKERRQ(ierr);
  ierr = BVRestoreColumn(V,i,&w);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(BV_Copy,V,w,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
