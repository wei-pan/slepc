
static char help[] = "This example solves the same eigenproblem as in "
  "example ex2.c, but using a shell matrix. The problem is a standard "
  "symmetric eigenproblem corresponding to the 2-D Laplacian operator.\n\n"
  "The command line options are:\n\n"
  "  -n <n>, where <n> = number of grid subdivisions in both x and y dimensions.\n\n";

#include "slepceps.h"
#include "petscblaslapack.h"

/* 
   User-defined routines
*/
extern int MatLaplacian2D_Mult( Mat A, Vec x, Vec y );

#undef __FUNCT__
#define __FUNCT__ "main"
int main( int argc, char **argv )
{
  Vec         *x;              /* basis vectors */
  Mat         A;               /* operator matrix */
  EPS         eps;             /* eigenproblem solver context */
  EPSType     type;
  PetscReal   *error, tol;
  PetscScalar *kr, *ki;
  int         size, N, n=10, nev, ierr, maxit, i, its, nconv;

  SlepcInitialize(&argc,&argv,(char*)0,help);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  if (size != 1) SETERRQ(1,"This is a uniprocessor example only!");

  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);
  N = n*n;
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\n2-D Laplacian Eigenproblem (matrix-free version), N=%d (%dx%d grid)\n\n",N,n,n);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
     Compute the operator matrix that defines the eigensystem, Ax=kx
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = MatCreateShell(PETSC_COMM_WORLD,N,N,N,N,&n,&A);CHKERRQ(ierr);
  ierr = MatSetFromOptions(A);CHKERRQ(ierr);
  ierr = MatShellSetOperation(A,MATOP_MULT,(void(*)())MatLaplacian2D_Mult);CHKERRQ(ierr);
  ierr = MatShellSetOperation(A,MATOP_MULT_TRANSPOSE,(void(*)())MatLaplacian2D_Mult);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                Create the eigensolver and set various options
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* 
     Create eigensolver context
  */
  ierr = EPSCreate(PETSC_COMM_WORLD,&eps);CHKERRQ(ierr);

  /* 
     Set operators. In this case, it is a standard eigenvalue problem
  */
  ierr = EPSSetOperators(eps,A,PETSC_NULL);CHKERRQ(ierr);

  /*
     Set solver parameters at runtime
  */
  ierr = EPSSetFromOptions(eps);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                      Solve the eigensystem
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = EPSSolve(eps);CHKERRQ(ierr);
  ierr = EPSGetIterationNumber(eps, &its);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Number of iterations of the method: %d\n",its);CHKERRQ(ierr);

  /*
     Optional: Get some information from the solver and display it
  */
  ierr = EPSGetType(eps,&type);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Solution method: %s\n\n",type);CHKERRQ(ierr);
  ierr = EPSGetDimensions(eps,&nev,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Number of requested eigenvalues: %d\n",nev);CHKERRQ(ierr);
  ierr = EPSGetTolerances(eps,&tol,&maxit);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Stopping condition: tol=%.4g, maxit=%d\n",tol,maxit);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                    Display solution and clean up
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* 
     Get number of converged eigenpairs
  */
  ierr = EPSGetConverged(eps,&nconv);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," Number of converged approximate eigenpairs: %d\n\n",nconv);CHKERRQ(ierr);

  if (nconv>0) {
    /* 
       Get converged eigenpairs: i-th eigenvalue is stored in kr[i] (real part) and
       ki[i] (imaginary part), and the corresponding eigenvector is stored in x[i]
    */
    ierr = EPSGetSolution(eps,&kr,&ki,&x);CHKERRQ(ierr);

    /*
       Compute the relative error associated to each eigenpair
    */
    ierr = PetscMalloc(nconv*sizeof(PetscReal),&error);CHKERRQ(ierr);
    ierr = EPSComputeError(eps,error);CHKERRQ(ierr);

    /*
       Display eigenvalues and relative errors
    */
    ierr = PetscPrintf(PETSC_COMM_WORLD,
         "           k           ||Ax-kx||/|kx|\n"
         "   ----------------- -----------------\n" );CHKERRQ(ierr);
    for( i=0; i<nconv; i++ ) {
      if (ki[i]!=0.0) {
        ierr = PetscPrintf(PETSC_COMM_WORLD," %9f%+9f j %12f\n",kr[i],ki[i],error[i]);CHKERRQ(ierr); }
      else {
        ierr = PetscPrintf(PETSC_COMM_WORLD,"   %12f       %12f\n",kr[i],error[i]);CHKERRQ(ierr); }
    }
    ierr = PetscPrintf(PETSC_COMM_WORLD,"\n" );CHKERRQ(ierr);
    ierr = PetscFree(error);CHKERRQ(ierr);
  }
  
  /* 
     Free work space
  */
  ierr = EPSDestroy(eps);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = SlepcFinalize();CHKERRQ(ierr);
  return 0;
}

/*
    Compute the matrix vector multiplication y<---T*x where T is a nx by nx
    tridiagonal matrix with DD on the diagonal, DL on the subdiagonal, and 
    DU on the superdiagonal.
 */   
static void tv( int nx, PetscScalar *x, PetscScalar *y )
{
  PetscScalar dd, dl, du;
  int         j;

  dd  = 4.0;
  dl  = -1.0;
  du  = -1.0;

  y[0] =  dd*x[0] + du*x[1];
  for( j=1; j<nx-1; j++ )
    y[j] = dl*x[j-1] + dd*x[j] + du*x[j+1]; 
  y[nx-1] = dl*x[nx-2] + dd*x[nx-1]; 
}

#undef __FUNCT__
#define __FUNCT__ "MatLaplacian2D_Mult"
/*
    Matrix-vector product subroutine for the 2D Laplacian.

    The matrix used is the 2 dimensional discrete Laplacian on unit square with
    zero Dirichlet boundary condition.
 
    Computes y <-- A*x, where A is the block tridiagonal matrix
 
                 | T -I          | 
                 |-I  T -I       |
             A = |   -I  T       |
                 |        ...  -I|
                 |           -I T|
 
    The subroutine TV is called to compute y<--T*x.
 */
int MatLaplacian2D_Mult( Mat A, Vec x, Vec y )
{
  void        *ctx;
  int         ierr, nx, lo, j, one=1;
  PetscScalar *px, *py, dmone=-1.0;
  
  ierr = MatShellGetContext( A, &ctx ); CHKERRQ(ierr);
  nx = *(int *)ctx;
  ierr = VecGetArray( x, &px ); CHKERRQ(ierr);
  ierr = VecGetArray( y, &py ); CHKERRQ(ierr);

  tv( nx, &px[0], &py[0] );
  BLaxpy_( &nx, &dmone, &px[nx], &one, &py[0], &one );

  for( j=2; j<nx; j++ ) {
    lo = (j-1)*nx;
    tv( nx, &px[lo], &py[lo]);
    BLaxpy_( &nx, &dmone, &px[lo-nx], &one, &py[lo], &one );
    BLaxpy_( &nx, &dmone, &px[lo+nx], &one, &py[lo], &one );
  }

  lo = (nx-1)*nx;
  tv( nx, &px[lo], &py[lo]);
  BLaxpy_( &nx, &dmone, &px[lo-nx], &one, &py[lo], &one );

  ierr = VecRestoreArray( x, &px ); CHKERRQ(ierr);
  ierr = VecRestoreArray( y, &py ); CHKERRQ(ierr);

  return 0;
}

