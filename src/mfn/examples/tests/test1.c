/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2015, Universitat Politecnica de Valencia, Spain

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

static char help[] = "Computes exp(A)*v for a matrix loaded from file.\n\n"
  "The command line options are:\n"
  "  -file <filename>, where <filename> = matrix file in PETSc binary form.\n\n";

#include <slepcmfn.h>

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  Mat                A;           /* problem matrix */
  MFN                mfn;
  FN                 f;
  PetscReal          norm;
  PetscScalar        t=2.0;
  Vec                v,y;
  PetscInt           its;
  PetscErrorCode     ierr;
  PetscViewer        viewer;
  PetscBool          flg;
  char               filename[PETSC_MAX_PATH_LEN];
  MFNConvergedReason reason;

  SlepcInitialize(&argc,&argv,(char*)0,help);

  ierr = PetscOptionsGetScalar(NULL,NULL,"-t",&t,NULL);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\nMatrix exponential y=exp(t*A)*e, loaded from file\n\n");CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                Load matrix A from binary file
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = PetscOptionsGetString(NULL,NULL,"-file",filename,PETSC_MAX_PATH_LEN,&flg);CHKERRQ(ierr);
  if (!flg) SETERRQ(PETSC_COMM_WORLD,1,"Must indicate a file name with the -file option");

#if defined(PETSC_USE_COMPLEX)
  ierr = PetscPrintf(PETSC_COMM_WORLD," Reading COMPLEX matrix from a binary file...\n");CHKERRQ(ierr);
#else
  ierr = PetscPrintf(PETSC_COMM_WORLD," Reading REAL matrix from a binary file...\n");CHKERRQ(ierr);
#endif
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,filename,FILE_MODE_READ,&viewer);CHKERRQ(ierr);
  ierr = MatCreate(PETSC_COMM_WORLD,&A);CHKERRQ(ierr);
  ierr = MatSetFromOptions(A);CHKERRQ(ierr);
  ierr = MatLoad(A,viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);

  /* set v = ones(n,1) */
  ierr = MatCreateVecs(A,NULL,&y);CHKERRQ(ierr);
  ierr = MatCreateVecs(A,NULL,&v);CHKERRQ(ierr);
  ierr = VecSet(v,1.0);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                Create the solver and set various options
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = MFNCreate(PETSC_COMM_WORLD,&mfn);CHKERRQ(ierr);
  ierr = MFNSetOperator(mfn,A);CHKERRQ(ierr);
  ierr = MFNGetFN(mfn,&f);CHKERRQ(ierr);
  ierr = FNSetType(f,FNEXP);CHKERRQ(ierr);
  ierr = FNSetScale(f,t,1.0);CHKERRQ(ierr);  
  ierr = MFNSetFromOptions(mfn);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                      Solve the problem, y=exp(t*A)*v
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = MFNSolve(mfn,v,y);CHKERRQ(ierr);
  ierr = MFNGetConvergedReason(mfn,&reason);CHKERRQ(ierr);
  if (reason<0) SETERRQ(PETSC_COMM_WORLD,1,"Solver did not converge");
  ierr = VecNorm(y,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Computed vector at time t=%.4g has norm %g\n\n",(double)PetscRealPart(t),(double)norm);CHKERRQ(ierr);
  
  /*
     Optional: Get some information from the solver and display it
  */
  ierr = MFNGetIterationNumber(mfn,&its);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Number of iterations of the method: %D\n",its);CHKERRQ(ierr);

  /* 
     Free work space
  */
  ierr = MFNDestroy(&mfn);CHKERRQ(ierr);
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = VecDestroy(&v);CHKERRQ(ierr);
  ierr = VecDestroy(&y);CHKERRQ(ierr);
  ierr = SlepcFinalize();
  return ierr;
}

