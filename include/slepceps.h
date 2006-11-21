/*
   User interface for the SLEPC eigenproblem solvers. 
*/
#if !defined(__SLEPCEPS_H)
#define __SLEPCEPS_H
#include "slepc.h"
#include "slepcst.h"
PETSC_EXTERN_CXX_BEGIN

extern PetscCookie EPS_COOKIE;

/*S
     EPS - Abstract SLEPc object that manages all the eigenvalue 
     problem solvers.

   Level: beginner

  Concepts: eigen solvers

.seealso:  EPSCreate(), ST
S*/
typedef struct _p_EPS* EPS;

#define EPSPOWER     "power"
#define EPSSUBSPACE  "subspace"
#define EPSARNOLDI   "arnoldi"
#define EPSLANCZOS   "lanczos"
#define EPSKRYLOVSCHUR "krylovschur"
#define EPSLAPACK    "lapack"
/* the next ones are interfaces to external libraries */
#define EPSARPACK    "arpack"
#define EPSBLZPACK   "blzpack"
#define EPSTRLAN     "trlan"
#define EPSBLOPEX    "blopex"
#define EPSType const char*
#define EPSPRIMME    "primme"

typedef enum { EPS_HEP=1,  EPS_GHEP,
               EPS_NHEP,   EPS_GNHEP } EPSProblemType;

typedef enum { EPS_ONE_SIDE, EPS_TWO_SIDE } EPSClass;

typedef enum { EPS_LARGEST_MAGNITUDE, EPS_SMALLEST_MAGNITUDE,
               EPS_LARGEST_REAL,      EPS_SMALLEST_REAL,
               EPS_LARGEST_IMAGINARY, EPS_SMALLEST_IMAGINARY } EPSWhich;

typedef enum { EPS_MGS_ORTH,  EPS_CGS_ORTH } EPSOrthogonalizationType;
typedef enum { EPS_ORTH_REFINE_NEVER, EPS_ORTH_REFINE_IFNEEDED,
               EPS_ORTH_REFINE_ALWAYS } EPSOrthogonalizationRefinementType;

EXTERN PetscErrorCode EPSCreate(MPI_Comm,EPS *);
EXTERN PetscErrorCode EPSDestroy(EPS);
EXTERN PetscErrorCode EPSSetType(EPS,EPSType);
EXTERN PetscErrorCode EPSGetType(EPS,EPSType*);
EXTERN PetscErrorCode EPSSetProblemType(EPS,EPSProblemType);
EXTERN PetscErrorCode EPSGetProblemType(EPS,EPSProblemType*);
EXTERN PetscErrorCode EPSSetClass(EPS,EPSClass);
EXTERN PetscErrorCode EPSGetClass(EPS,EPSClass*);
EXTERN PetscErrorCode EPSSetOperators(EPS,Mat,Mat);
EXTERN PetscErrorCode EPSSetFromOptions(EPS);
EXTERN PetscErrorCode EPSSetUp(EPS);
EXTERN PetscErrorCode EPSSolve(EPS);
EXTERN PetscErrorCode EPSView(EPS,PetscViewer);

EXTERN PetscErrorCode EPSInitializePackage(char *);

EXTERN PetscErrorCode EPSSetST(EPS,ST);
EXTERN PetscErrorCode EPSGetST(EPS,ST*);
EXTERN PetscErrorCode EPSSetTolerances(EPS,PetscReal,int);
EXTERN PetscErrorCode EPSGetTolerances(EPS,PetscReal*,int*);
EXTERN PetscErrorCode EPSSetDimensions(EPS,int,int);
EXTERN PetscErrorCode EPSGetDimensions(EPS,int*,int*);

EXTERN PetscErrorCode EPSGetConverged(EPS,int*);
EXTERN PetscErrorCode EPSGetEigenpair(EPS,int,PetscScalar*,PetscScalar*,Vec,Vec);
EXTERN PetscErrorCode EPSGetValue(EPS,int,PetscScalar*,PetscScalar*);
EXTERN PetscErrorCode EPSGetRightVector(EPS,int,Vec,Vec);
EXTERN PetscErrorCode EPSGetLeftVector(EPS,int,Vec,Vec);
EXTERN PetscErrorCode EPSComputeRelativeError(EPS,int,PetscReal*);
EXTERN PetscErrorCode EPSComputeRelativeErrorLeft(EPS,int,PetscReal*);
EXTERN PetscErrorCode EPSComputeResidualNorm(EPS,int,PetscReal*);
EXTERN PetscErrorCode EPSComputeResidualNormLeft(EPS,int,PetscReal*);
EXTERN PetscErrorCode EPSGetInvariantSubspace(EPS,Vec*);
EXTERN PetscErrorCode EPSGetLeftInvariantSubspace(EPS,Vec*);
EXTERN PetscErrorCode EPSGetErrorEstimate(EPS,int,PetscReal*);
EXTERN PetscErrorCode EPSGetErrorEstimateLeft(EPS,int,PetscReal*);

EXTERN PetscErrorCode EPSSetMonitor(EPS,int (*)(EPS,int,int,PetscScalar*,PetscScalar*,PetscReal*,int,void*),void*);
EXTERN PetscErrorCode EPSClearMonitor(EPS);
EXTERN PetscErrorCode EPSGetMonitorContext(EPS,void **);
EXTERN PetscErrorCode EPSGetIterationNumber(EPS,int*);
EXTERN PetscErrorCode EPSGetOperationCounters(EPS,int*,int*,int*);

EXTERN PetscErrorCode EPSSetInitialVector(EPS,Vec);
EXTERN PetscErrorCode EPSGetInitialVector(EPS,Vec*);
EXTERN PetscErrorCode EPSSetLeftInitialVector(EPS,Vec);
EXTERN PetscErrorCode EPSGetLeftInitialVector(EPS,Vec*);
EXTERN PetscErrorCode EPSSetWhichEigenpairs(EPS,EPSWhich);
EXTERN PetscErrorCode EPSGetWhichEigenpairs(EPS,EPSWhich*);
EXTERN PetscErrorCode EPSSetOrthogonalization(EPS,EPSOrthogonalizationType,EPSOrthogonalizationRefinementType,PetscReal);
EXTERN PetscErrorCode EPSGetOrthogonalization(EPS,EPSOrthogonalizationType*,EPSOrthogonalizationRefinementType*,PetscReal*);
EXTERN PetscErrorCode EPSIsGeneralized(EPS,PetscTruth*);
EXTERN PetscErrorCode EPSIsHermitian(EPS,PetscTruth*);

EXTERN PetscErrorCode EPSDefaultMonitor(EPS,int,int,PetscScalar*,PetscScalar*,PetscReal*,int,void*);
EXTERN PetscErrorCode EPSLGMonitor(EPS,int,int,PetscScalar*,PetscScalar*,PetscReal*,int,void*);

EXTERN PetscErrorCode EPSAttachDeflationSpace(EPS,int,Vec*,PetscTruth);
EXTERN PetscErrorCode EPSRemoveDeflationSpace(EPS);

EXTERN PetscErrorCode EPSSetOptionsPrefix(EPS,const char*);
EXTERN PetscErrorCode EPSAppendOptionsPrefix(EPS,const char*);
EXTERN PetscErrorCode EPSGetOptionsPrefix(EPS,const char*[]);

typedef enum {/* converged */
              EPS_CONVERGED_TOL                =  2,
              /* diverged */
              EPS_DIVERGED_ITS                 = -3,
              EPS_DIVERGED_BREAKDOWN           = -4,
              EPS_DIVERGED_NONSYMMETRIC        = -5,
              EPS_CONVERGED_ITERATING          =  0} EPSConvergedReason;

EXTERN PetscErrorCode EPSGetConvergedReason(EPS,EPSConvergedReason *);

EXTERN PetscErrorCode EPSSortEigenvalues(int,PetscScalar*,PetscScalar*,EPSWhich,int,int*);
EXTERN PetscErrorCode EPSDenseNHEP(int,PetscScalar*,PetscScalar*,PetscScalar*,PetscScalar*,PetscScalar*);
EXTERN PetscErrorCode EPSDenseGNHEP(int,PetscScalar*,PetscScalar*,PetscScalar*,PetscScalar*,PetscScalar*,PetscScalar*);
EXTERN PetscErrorCode EPSDenseHEP(int,PetscScalar*,int,PetscReal*,PetscScalar*);
EXTERN PetscErrorCode EPSDenseGHEP(int,PetscScalar*,PetscScalar*,PetscReal*,PetscScalar*);
EXTERN PetscErrorCode EPSDenseHessenberg(int,int,PetscScalar*,int,PetscScalar*);
EXTERN PetscErrorCode EPSDenseSchur(int,int,PetscScalar*,int,PetscScalar*,PetscScalar*,PetscScalar*);
EXTERN PetscErrorCode EPSSortDenseSchur(int,int,PetscScalar*,int,PetscScalar*,PetscScalar*,PetscScalar*,EPSWhich);
EXTERN PetscErrorCode EPSDenseTridiagonal(int,PetscScalar*,int,PetscReal*,PetscScalar*);

EXTERN PetscErrorCode EPSOrthogonalize(EPS,int,PetscTruth*,Vec*,Vec,PetscScalar*,PetscReal*,PetscTruth*);
EXTERN PetscErrorCode EPSBiOrthogonalize(EPS,int,Vec*,Vec*,Vec,PetscScalar*,PetscReal*);
EXTERN PetscErrorCode EPSQRDecomposition(EPS,Vec*,int,int,PetscScalar*,int);
EXTERN PetscErrorCode EPSGetStartVector(EPS,int,Vec,PetscTruth*);
EXTERN PetscErrorCode EPSGetLeftStartVector(EPS,int,Vec);

/* --------- options specific to particular eigensolvers -------- */

typedef enum { EPSPOWER_SHIFT_CONSTANT, EPSPOWER_SHIFT_RAYLEIGH,
               EPSPOWER_SHIFT_WILKINSON } EPSPowerShiftType;

EXTERN PetscErrorCode EPSPowerSetShiftType(EPS,EPSPowerShiftType);
EXTERN PetscErrorCode EPSPowerGetShiftType(EPS,EPSPowerShiftType*);

EXTERN PetscErrorCode EPSArnoldiSetDelayed(EPS,PetscTruth);
EXTERN PetscErrorCode EPSArnoldiGetDelayed(EPS,PetscTruth*);

typedef enum { EPSLANCZOS_REORTHOG_LOCAL, 
               EPSLANCZOS_REORTHOG_FULL,
               EPSLANCZOS_REORTHOG_SELECTIVE,
               EPSLANCZOS_REORTHOG_PERIODIC,
               EPSLANCZOS_REORTHOG_PARTIAL, 
	       EPSLANCZOS_REORTHOG_DELAYED } EPSLanczosReorthogType;

EXTERN PetscErrorCode EPSLanczosSetReorthog(EPS,EPSLanczosReorthogType);
EXTERN PetscErrorCode EPSLanczosGetReorthog(EPS,EPSLanczosReorthogType*);

EXTERN PetscErrorCode EPSBlzpackSetBlockSize(EPS,int);
EXTERN PetscErrorCode EPSBlzpackSetInterval(EPS,PetscReal,PetscReal);
EXTERN PetscErrorCode EPSBlzpackSetNSteps(EPS,int);

typedef enum {
  EPSPRIMME_DYNAMIC,
  EPSPRIMME_DEFAULT_MIN_TIME,
  EPSPRIMME_DEFAULT_MIN_MATVECS,
  EPSPRIMME_ARNOLDI,
  EPSPRIMME_GD,
  EPSPRIMME_GD_PLUSK,
  EPSPRIMME_GD_OLSEN_PLUSK,
  EPSPRIMME_JD_OLSEN_PLUSK,
  EPSPRIMME_RQI,
  EPSPRIMME_JDQR,
  EPSPRIMME_JDQMR,
  EPSPRIMME_JDQMR_ETOL,
  EPSPRIMME_SUBSPACE_ITERATION,
  EPSPRIMME_LOBPCG_ORTHOBASIS,
  EPSPRIMME_LOBPCG_ORTHOBASIS_WINDOW
} EPSPRIMMEMethod;

typedef enum {
  EPSPRIMME_NONE,
  EPSPRIMME_DIAGONAL
} EPSPRIMMEPrecond;

EXTERN PetscErrorCode EPSPRIMMESetBlockSize(EPS eps,int bs);
EXTERN PetscErrorCode EPSPRIMMESetMethod(EPS eps, EPSPRIMMEMethod method);
EXTERN PetscErrorCode EPSPRIMMESetPrecond(EPS eps, EPSPRIMMEPrecond precond);
EXTERN PetscErrorCode EPSPRIMMEGetBlockSize(EPS eps,int *bs);
EXTERN PetscErrorCode EPSPRIMMEGetMethod(EPS eps, EPSPRIMMEMethod *method);
EXTERN PetscErrorCode EPSPRIMMEGetPrecond(EPS eps, EPSPRIMMEPrecond *precond);

PETSC_EXTERN_CXX_END
#endif

