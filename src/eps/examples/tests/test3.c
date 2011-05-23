/*
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

static char help[] = "Tests multiple calls to EPSSolve with different matrix.\n\n";

#include <slepceps.h>

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  Mat            A1,A2;       /* problem matrices */
  EPS            eps;         /* eigenproblem solver context */
  PetscReal      error,re;
  PetscScalar    kr,value[3];
  Vec            d,xr;
  PetscInt       n=30,i,Istart,Iend,col[3],nconv;
  PetscBool      FirstBlock=PETSC_FALSE,LastBlock=PETSC_FALSE;
  PetscRandom    myrand;
  PetscErrorCode ierr;

  SlepcInitialize(&argc,&argv,(char*)0,help);

  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\nTridiagonal with random diagonal, n=%d\n\n",n);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
           Create matrix tridiag([-1 0 -1])
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = MatCreate(PETSC_COMM_WORLD,&A1);CHKERRQ(ierr);
  ierr = MatSetSizes(A1,PETSC_DECIDE,PETSC_DECIDE,n,n);CHKERRQ(ierr);
  ierr = MatSetFromOptions(A1);CHKERRQ(ierr);
  
  ierr = MatGetOwnershipRange(A1,&Istart,&Iend);CHKERRQ(ierr);
  if (Istart==0) FirstBlock=PETSC_TRUE;
  if (Iend==n) LastBlock=PETSC_TRUE;
  value[0]=-1.0; value[1]=0.0; value[2]=-1.0;
  for (i=(FirstBlock? Istart+1: Istart); i<(LastBlock? Iend-1: Iend); i++) {
    col[0]=i-1; col[1]=i; col[2]=i+1;
    ierr = MatSetValues(A1,1,&i,3,col,value,INSERT_VALUES);CHKERRQ(ierr);
  }
  if (LastBlock) {
    i=n-1; col[0]=n-2; col[1]=n-1;
    ierr = MatSetValues(A1,1,&i,2,col,value,INSERT_VALUES);CHKERRQ(ierr);
  }
  if (FirstBlock) {
    i=0; col[0]=0; col[1]=1; value[0]=0.0; value[1]=-1.0;
    ierr = MatSetValues(A1,1,&i,2,col,value,INSERT_VALUES);CHKERRQ(ierr);
  }

  ierr = MatAssemblyBegin(A1,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A1,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatGetVecs(A1,PETSC_NULL,&xr);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
       Create two matrices by filling the diagonal with rand values
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = MatDuplicate(A1,MAT_COPY_VALUES,&A2);CHKERRQ(ierr);
  ierr = MatGetVecs(A1,PETSC_NULL,&d);CHKERRQ(ierr);
  ierr = PetscRandomCreate(PETSC_COMM_WORLD,&myrand);CHKERRQ(ierr);
  ierr = PetscRandomSetFromOptions(myrand);CHKERRQ(ierr);
  ierr = VecSetRandom(d,myrand);CHKERRQ(ierr);
  ierr = MatDiagonalSet(A1,d,INSERT_VALUES);CHKERRQ(ierr);
  ierr = VecSetRandom(d,myrand);CHKERRQ(ierr);
  ierr = MatDiagonalSet(A2,d,INSERT_VALUES);CHKERRQ(ierr);
  ierr = VecDestroy(&d);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(&myrand);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                        Create the eigensolver
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = EPSCreate(PETSC_COMM_WORLD,&eps);CHKERRQ(ierr);
  ierr = EPSSetProblemType(eps,EPS_HEP);CHKERRQ(ierr);
  ierr = EPSSetOperators(eps,A1,PETSC_NULL);CHKERRQ(ierr);
  ierr = EPSSetFromOptions(eps);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                        Solve first eigenproblem
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = EPSSolve(eps);CHKERRQ(ierr);
  ierr = EPSGetConverged(eps,&nconv);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Largest eigenvalues: %d converged eigenpairs\n\n",nconv);CHKERRQ(ierr);
  if (nconv>0) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,
         "           k          ||Ax-kx||/||kx||\n"
         "   ----------------- ------------------\n");CHKERRQ(ierr);

    for (i=0;i<nconv;i++) {
      ierr = EPSGetEigenpair(eps,i,&kr,PETSC_NULL,xr,PETSC_NULL);CHKERRQ(ierr);
      ierr = EPSComputeRelativeError(eps,i,&error);CHKERRQ(ierr);
#if defined(PETSC_USE_COMPLEX)
      re = PetscRealPart(kr);
#else
      re = kr;
#endif 
      ierr = PetscPrintf(PETSC_COMM_WORLD,"   %12f       %12g\n",re,error);CHKERRQ(ierr); 
    }
    ierr = PetscPrintf(PETSC_COMM_WORLD,"\n");CHKERRQ(ierr);
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                        Solve second eigenproblem
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = EPSSetOperators(eps,A2,PETSC_NULL);CHKERRQ(ierr);
  ierr = EPSSolve(eps);CHKERRQ(ierr);
  ierr = EPSGetConverged(eps,&nconv);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Smallest eigenvalues: %d converged eigenpairs\n\n",nconv);CHKERRQ(ierr);
  if (nconv>0) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,
         "           k          ||Ax-kx||/||kx||\n"
         "   ----------------- ------------------\n");CHKERRQ(ierr);

    for (i=0;i<nconv;i++) {
      ierr = EPSGetEigenpair(eps,i,&kr,PETSC_NULL,xr,PETSC_NULL);CHKERRQ(ierr);
      ierr = EPSComputeRelativeError(eps,i,&error);CHKERRQ(ierr);
#if defined(PETSC_USE_COMPLEX)
      re = PetscRealPart(kr);
#else
      re = kr;
#endif 
      ierr = PetscPrintf(PETSC_COMM_WORLD,"   %12f       %12g\n",re,error);CHKERRQ(ierr); 
    }
    ierr = PetscPrintf(PETSC_COMM_WORLD,"\n");CHKERRQ(ierr);
  }
  
  ierr = EPSDestroy(&eps);CHKERRQ(ierr);
  ierr = MatDestroy(&A1);CHKERRQ(ierr);
  ierr = MatDestroy(&A2);CHKERRQ(ierr);
  ierr = VecDestroy(&xr);CHKERRQ(ierr);
  ierr = SlepcFinalize();CHKERRQ(ierr);
  return 0;
}