
#ifndef _EPSIMPL
#define _EPSIMPL

#include "slepceps.h"

typedef struct _EPSOps *EPSOps;

struct _EPSOps {
  int  (*solve)(EPS);                   /* actual solver */
  int  (*setup)(EPS);
  int  (*setfromoptions)(EPS);
  int  (*publishoptions)(EPS);
  int  (*destroy)(EPS);
  int  (*view)(EPS,PetscViewer);
  int  (*backtransform)(EPS);
};

/*
     Maximum number of monitors you can run with a single EPS
*/
#define MAXEPSMONITORS 5 

/*
   Defines the EPS data structure.
*/
struct _p_EPS {
  PETSCHEADER(struct _EPSOps)
  /*------------------------- User parameters --------------------------*/
  int        max_it,            /* maximum number of iterations */
             nev,               /* number of eigenvalues to compute */
             ncv,               /* number of basis vectors */
             allocated_ncv,     /* number of basis vectors allocated */
             nds;               /* size of deflation space */
  PetscReal  tol;               /* tolerance */
  EPSWhich   which;             /* which part of the spectrum to be sought */
  PetscTruth dropvectors;       /* do not compute eigenvectors */
  EPSProblemType problem_type;  /* which kind of problem to be solved */

  /*------------------------- Working data --------------------------*/
  Vec         vec_initial;      /* initial vector for iterative methods */
  Vec         *V,               /* set of basis vectors */
              *DS,              /* deflation space */
              *DSV;             /* deflation space and basis vectors*/
  PetscScalar *eigr, *eigi;     /* real and imaginary parts of eigenvalues */
  PetscReal  *errest;           /* error estimates */
  ST         OP;                /* spectral transformation object */
  void       *data;             /* holder for misc stuff associated 
                                   with a particular solver */
  int        nconv,             /* number of converged eigenvalues */
             its;               /* number of iterations so far computed */
  int        *perm;             /* permutation for eigenvalue ordering */

  /* ---------------- Default work-area and status vars -------------------- */
  int        nwork;
  Vec        *work;

  int        setupcalled;
  PetscTruth isgeneralized,
             ishermitian;
  EPSConvergedReason reason;     

  int        (*monitor[MAXEPSMONITORS])(EPS,int,int,PetscScalar*,PetscScalar*,PetscReal*,int,void*); 
  int        (*monitordestroy[MAXEPSMONITORS])(void*);
  void       *monitorcontext[MAXEPSMONITORS];
  int        numbermonitors; 

  /* --------------- Orthogonalization --------------------- */
  int        (*orthog)(EPS,int,Vec*,Vec,PetscScalar*,PetscReal*);
  PetscReal  orth_eta;
  PetscTruth ds_ortho;
  EPSOrthogonalizationType orth_type;   /* which orthogonalization to use */
  
};

#define EPSMonitor(eps,it,nconv,eigr,eigi,errest,nest) \
        { int _ierr,_i,_im = eps->numbermonitors; \
          for ( _i=0; _i<_im; _i++ ) {\
            _ierr=(*eps->monitor[_i])(eps,it,nconv,eigr,eigi,errest,nest,eps->monitorcontext[_i]);\
            CHKERRQ(_ierr); \
	  } \
	}

extern int EPSDestroy_Default(EPS);
extern int EPSDefaultGetWork(EPS,int);
extern int EPSDefaultFreeWork(EPS);
extern int EPSAllocateSolution(EPS);
extern int EPSFreeSolution(EPS);
extern int EPSAllocateSolutionContiguous(EPS);
extern int EPSFreeSolutionContiguous(EPS);
extern int EPSModifiedGramSchmidtOrthogonalization(EPS,int,Vec*,Vec,PetscScalar*,PetscReal*);
extern int EPSClassicalGramSchmidtOrthogonalization(EPS,int,Vec*,Vec,PetscScalar*,PetscReal*);
extern int EPSBackTransform_Default(EPS);

#endif
