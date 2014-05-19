/*
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

static char help[] = "Test indefinite IP.\n\n";

#include <slepcip.h>

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscErrorCode ierr;
  Mat            A;
  IP             ip;
  Vec            v;
  PetscInt       i,n=15,Istart,Iend;
  PetscReal      norm;

  SlepcInitialize(&argc,&argv,(char*)0,help);
  ierr = PetscOptionsGetInt(NULL,"-n",&n,NULL);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Indefinite norm of a random vector of length %D.\n",n);CHKERRQ(ierr);

  /* Create sip matrix (standard involutionary permutation) */
  ierr = MatCreate(PETSC_COMM_WORLD,&A);CHKERRQ(ierr);
  ierr = MatSetSizes(A,PETSC_DECIDE,PETSC_DECIDE,n,n);CHKERRQ(ierr);
  ierr = MatSetFromOptions(A);CHKERRQ(ierr);
  ierr = MatSetUp(A);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(A,&Istart,&Iend);CHKERRQ(ierr);
  for (i=Istart;i<Iend;i++) {
    ierr = MatSetValue(A,i,n-i-1,1.0,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* Create vector */
  ierr = MatGetVecs(A,NULL,&v);CHKERRQ(ierr);
  for (i=Istart;i<Iend;i++) {
    ierr = VecSetValue(v,i,i-n/2,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(v);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(v);CHKERRQ(ierr);

  /* Create IP object */
  ierr = IPCreate(PETSC_COMM_WORLD,&ip);CHKERRQ(ierr);
  ierr = IPSetType(ip,IPINDEFINITE);CHKERRQ(ierr);
  ierr = IPSetMatrix(ip,A,1.0);CHKERRQ(ierr);
  ierr = IPSetFromOptions(ip);CHKERRQ(ierr);
  ierr = IPView(ip,NULL);CHKERRQ(ierr);

  /* Check indefinite norm */
  ierr = IPNorm(ip,v,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Computed norm=%g\n",(double)norm);CHKERRQ(ierr);

  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = IPDestroy(&ip);CHKERRQ(ierr);
  ierr = VecDestroy(&v);CHKERRQ(ierr);
  ierr = SlepcFinalize();
  return 0;
}