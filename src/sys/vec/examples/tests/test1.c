/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2016, Universitat Politecnica de Valencia, Spain

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

static char help[] = "Test VecComp.\n\n";

#include <slepcvec.h>

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  Vec            v,w,x,vc,wc,xc,vparent,vchild[2];
  const Vec      *varray;
  PetscMPIInt    size,rank;
  PetscInt       i,n,k,Nx[2];
  PetscReal      norm,normc;
  PetscScalar    vmax,vmin;
  PetscErrorCode ierr;

  ierr = SlepcInitialize(&argc,&argv,(char*)0,help);if (ierr) return ierr;
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  if (size != 2) SETERRQ(PETSC_COMM_WORLD,1,"This test needs two processes");
  ierr = PetscPrintf(PETSC_COMM_WORLD,"VecComp test\n");CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create standard vectors
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = VecCreate(PETSC_COMM_WORLD,&v);CHKERRQ(ierr);
  ierr = VecSetSizes(v,4,8);CHKERRQ(ierr);
  ierr = VecSetFromOptions(v);CHKERRQ(ierr);

  if (!rank) {
    ierr = VecSetValue(v,0,2.0,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(v,1,-1.0,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(v,2,3.0,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(v,3,3.5,INSERT_VALUES);CHKERRQ(ierr);
  } else {
    ierr = VecSetValue(v,4,1.2,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(v,5,1.8,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(v,6,-2.2,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(v,7,2.0,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(v);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(v);CHKERRQ(ierr);
  ierr = VecDuplicate(v,&w);CHKERRQ(ierr);
  ierr = VecSet(w,1.0);CHKERRQ(ierr);
  ierr = VecDuplicate(v,&x);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create veccomp vectors
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = VecCreate(PETSC_COMM_WORLD,&vparent);CHKERRQ(ierr);
  ierr = VecSetSizes(vparent,2,4);CHKERRQ(ierr);
  ierr = VecSetFromOptions(vparent);CHKERRQ(ierr);

  /* create a veccomp vector with two subvectors */
  ierr = VecDuplicate(vparent,&vchild[0]);CHKERRQ(ierr);
  ierr = VecDuplicate(vparent,&vchild[1]);CHKERRQ(ierr);
  if (!rank) {
    ierr = VecSetValue(vchild[0],0,2.0,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(vchild[0],1,-1.0,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(vchild[1],0,1.2,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(vchild[1],1,1.8,INSERT_VALUES);CHKERRQ(ierr);
  } else {
    ierr = VecSetValue(vchild[0],2,3.0,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(vchild[0],3,3.5,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(vchild[1],2,-2.2,INSERT_VALUES);CHKERRQ(ierr);
    ierr = VecSetValue(vchild[1],3,2.0,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(vchild[0]);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(vchild[1]);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(vchild[0]);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(vchild[1]);CHKERRQ(ierr);
  ierr = VecCreateCompWithVecs(vchild,2,vparent,&vc);CHKERRQ(ierr);
  ierr = VecDestroy(&vchild[0]);CHKERRQ(ierr);
  ierr = VecDestroy(&vchild[1]);CHKERRQ(ierr);

  ierr = VecGetSize(vc,&k);CHKERRQ(ierr);
  if (k!=8) SETERRQ(PETSC_COMM_WORLD,1,"Vector global length should be 8");

  /* create an empty veccomp vector with two subvectors */
  Nx[0] = 4;
  Nx[1] = 4;
  ierr = VecCreateComp(PETSC_COMM_WORLD,Nx,2,VECMPI,vparent,&wc);CHKERRQ(ierr);
  ierr = VecCompGetSubVecs(wc,&n,&varray);CHKERRQ(ierr);
  if (n!=2) SETERRQ(PETSC_COMM_WORLD,1,"n should be 2");
  for (i=0;i<2;i++) {
    ierr = VecSet(varray[i],1.0);CHKERRQ(ierr);
  }

  ierr = VecGetSize(wc,&k);CHKERRQ(ierr);
  if (k!=8) SETERRQ(PETSC_COMM_WORLD,1,"Vector global length should be 8");

  ierr = VecDuplicate(vc,&xc);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Operate with vectors
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = VecCopy(w,x);CHKERRQ(ierr);
  ierr = VecAXPBY(x,1.0,-2.0,v);CHKERRQ(ierr);
  ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);
  ierr = VecCopy(wc,xc);CHKERRQ(ierr);
  ierr = VecAXPBY(xc,1.0,-2.0,vc);CHKERRQ(ierr);
  ierr = VecNorm(xc,NORM_2,&normc);CHKERRQ(ierr);
  if (PetscAbsReal(norm-normc)>10*PETSC_MACHINE_EPSILON) SETERRQ(PETSC_COMM_WORLD,1,"Norms are different");

  ierr = VecCopy(w,x);CHKERRQ(ierr);
  ierr = VecWAXPY(x,-2.0,w,v);CHKERRQ(ierr);
  ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);
  ierr = VecCopy(wc,xc);CHKERRQ(ierr);
  ierr = VecWAXPY(xc,-2.0,wc,vc);CHKERRQ(ierr);
  ierr = VecNorm(xc,NORM_2,&normc);CHKERRQ(ierr);
  if (PetscAbsReal(norm-normc)>10*PETSC_MACHINE_EPSILON) SETERRQ(PETSC_COMM_WORLD,1,"Norms are different");

  ierr = VecMax(xc,NULL,&vmax);CHKERRQ(ierr);
  ierr = VecMin(xc,NULL,&vmin);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Result has max value %g min value %g\n",(double)PetscRealPart(vmax),(double)PetscRealPart(vmin));CHKERRQ(ierr);

  ierr = VecDestroy(&v);CHKERRQ(ierr);
  ierr = VecDestroy(&w);CHKERRQ(ierr);
  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = VecDestroy(&vparent);CHKERRQ(ierr);
  ierr = VecDestroy(&vc);CHKERRQ(ierr);
  ierr = VecDestroy(&wc);CHKERRQ(ierr);
  ierr = VecDestroy(&xc);CHKERRQ(ierr);
  ierr = SlepcFinalize();
  return ierr;
}
