
/*
    Routines to set ST methods and options.
*/

#include "src/st/stimpl.h"      /*I "slepcst.h" I*/
#include "petscsys.h"

PetscTruth STRegisterAllCalled = PETSC_FALSE;
/*
   Contains the list of registered EPS routines
*/
PetscFList STList = 0;

#undef __FUNCT__  
#define __FUNCT__ "STSetType"
/*@C
   STSetType - Builds ST for a particular spectral transformation.

   Collective on ST

   Input Parameter:
+  st   - the spectral transformation context.
-  type - a known type

   Options Database Key:
.  -st_type <type> - Sets ST type

   Use -help for a list of available transformations

   Notes:
   See "slepc/include/slepcst.h" for available transformations 

   Normally, it is best to use the EPSSetFromOptions() command and
   then set the ST type from the options database rather than by using
   this routine.  Using the options database provides the user with
   maximum flexibility in evaluating the many different transformations. 

   Level: intermediate

.seealso: EPSSetType()

@*/
int STSetType(ST st,STType type)
{
  int ierr,(*r)(ST);
  PetscTruth match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_COOKIE,1);
  PetscValidCharPointer(type,2);

  ierr = PetscTypeCompare((PetscObject)st,type,&match);CHKERRQ(ierr);
  if (match) PetscFunctionReturn(0);

  if (st->ops->destroy) {ierr =  (*st->ops->destroy)(st);CHKERRQ(ierr);}
  ierr = PetscFListDestroy(&st->qlist);CHKERRQ(ierr);
  st->data        = 0;
  st->setupcalled = 0;

  /* Get the function pointers for the method requested */
  if (!STRegisterAllCalled) {ierr = STRegisterAll(0); CHKERRQ(ierr);}

  /* Determine the STCreateXXX routine for a particular type */
  ierr =  PetscFListFind(st->comm, STList, type,(void (**)(void)) &r );CHKERRQ(ierr);
  if (!r) SETERRQ1(1,"Unable to find requested ST type %s",type);
  if (st->data) {ierr = PetscFree(st->data);CHKERRQ(ierr);}

  ierr = PetscMemzero(st->ops,sizeof(struct _STOps));CHKERRQ(ierr);

  /* Call the STCreateXXX routine for this particular type */
  ierr = (*r)(st);CHKERRQ(ierr);

  ierr = PetscObjectChangeTypeName((PetscObject)st,type);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "STRegisterDestroy"
/*@C
   STRegisterDestroy - Frees the list of spectral transformations that were
   registered by STRegisterDynamic().

   Not Collective

   Level: advanced

.seealso: STRegisterAll(), STRegisterAll()

@*/
int STRegisterDestroy(void)
{
  int ierr;

  PetscFunctionBegin;
  if (STList) {
    ierr = PetscFListDestroy(&STList);CHKERRQ(ierr);
    STList = 0;
  }
  STRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "STGetType"
/*@C
   STGetType - Gets the ST type name (as a string) from the ST context.

   Not Collective

   Input Parameter:
.  st - the spectral transformation context

   Output Parameter:
.  name - name of the spectral transformation 

   Level: intermediate

.seealso: STSetType()

@*/
int STGetType(ST st,STType *meth)
{
  PetscFunctionBegin;
  *meth = (STType) st->type_name;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "STSetFromOptions"
/*@
   STSetFromOptions - Sets ST options from the options database.
   This routine must be called before STSetUp() if the user is to be
   allowed to set the type of transformation.

   Collective on ST

   Input Parameter:
.  st - the spectral transformation context

   Level: beginner

.seealso: 

@*/
int STSetFromOptions(ST st)
{
  int        ierr,i;
  char       type[256];
  PetscTruth flg;
  const char *mode_list[3] = { "copy", "inplace", "shell" };
  const char *structure_list[3] = { "same", "different", "subset" };

  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_COOKIE,1);

  if (!STRegisterAllCalled) {ierr = STRegisterAll(PETSC_NULL);CHKERRQ(ierr);}
  ierr = PetscOptionsBegin(st->comm,st->prefix,"Spectral Transformation (ST) Options","ST");CHKERRQ(ierr);
    ierr = PetscOptionsList("-st_type","Spectral Transformation type","STSetType",STList,(char*)(st->type_name?st->type_name:STSHIFT),type,256,&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = STSetType(st,type);CHKERRQ(ierr);
    }
    /*
      Set the type if it was never set.
    */
    if (!st->type_name) {
      ierr = STSetType(st,STSHIFT);CHKERRQ(ierr);
    }

    if (st->numberofshifts>0) {
      ierr = PetscOptionsScalar("-st_shift","Value of the shift","STSetShift",st->sigma,&st->sigma,PETSC_NULL); CHKERRQ(ierr);
    }

    ierr = PetscOptionsEList("-st_matmode", "Shift matrix mode","STSetMatMode",mode_list,3,mode_list[st->shift_matrix],&i,&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = STSetMatMode(st, (STMatMode)i);CHKERRQ(ierr);
    }

    ierr = PetscOptionsEList("-st_matstructure", "Shift nonzero pattern","STSetMatStructure",structure_list,3,structure_list[st->str],&i,&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = STSetMatStructure(st, (MatStructure)i);CHKERRQ(ierr);
    }
    
    if (st->ops->setfromoptions) {
      ierr = (*st->ops->setfromoptions)(st);CHKERRQ(ierr);
    }

  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  if (st->ksp) { ierr = KSPSetFromOptions(st->ksp);CHKERRQ(ierr); }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "STSetMatStructure"
/*@
   STSetMatStructure - Sets an internal MatStructure attribute to 
   indicate which is the relation of the sparsity pattern of the two matrices
   A and B constituting the generalized eigenvalue problem. This function
   has no effect in the case of standard eigenproblems.

   Collective on ST

   Input Parameters:
+  st  - the spectral transformation context
-  str - either SAME_NONZERO_PATTERN, DIFFERENT_NONZERO_PATTERN or
         SUBSET_NONZERO_PATTERN

   Options Database Key:
+  -st_sinvert_same_pattern - Indicates A and B have the same nonzero pattern
.  -st_sinvert_different_pattern - Indicates A and B have different nonzero pattern
-  -st_sinvert_subset_pattern - Indicates B's nonzero pattern is a subset of B's

   Note:
   By default, the sparsity patterns are assumed to be different. If the
   patterns are equal or a subset then it is recommended to set this attribute
   for efficiency reasons (in particular, for internal MatAXPY() operations).
   
   Level: advanced

.seealso: STSetOperators(), MatAXPY()
@*/
int STSetMatStructure(ST st,MatStructure str)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_COOKIE,1);
  st->str = str;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "STSetMatMode"
/*@
   STSetMatMode - Sets a flag to indicate how the matrix is
   being shifted in the shift-and-invert and Cayley spectral transformations.

   Collective on ST

   Input Parameters:
+  st - the spectral transformation context
-  mode - the mode flag, one of STMATMODE_COPY, 
          STMATMODE_INPLACE or STMATMODE_SHELL

   Options Database Key:
.  -st_sinvert_matmode <mode> - Activates STSinvertSetMatMode()

   Note:
   By default (STMATMODE_COPY), a copy of matrix A is made and then 
   this copy is shifted explicitly, e.g. A <- (A - s B). 

   With STMATMODE_INPLACE, the original matrix A is shifted at 
   STSetUp() and unshifted at the end of the computations. With respect to
   the previous one, this mode avoids a copy of matrix A. However, a
   backdraw is that the recovered matrix might be slightly different 
   from the original one (due to roundoff).

   With STMATMODE_SHELL, the solver works with an implicit shell 
   matrix that represents the shifted matrix. This mode is the most efficient 
   in creating the shifted matrix but it places serious limitations to the 
   linear solves performed in each iteration of the eigensolver (typically,
   only interative solvers with Jacobi preconditioning can be used).
   
   In the case of generalized problems, in the two first modes the matrix
   A - s B has to be computed explicitly. The efficiency of this computation 
   can be controlled with STSinvertSetMatStructure().

   Level: intermediate

.seealso: STSetOperators(), STSetMatStructure()
@*/
int STSetMatMode(ST st,STMatMode mode)
{
  int        ierr;
  PC         pc;

  PetscFunctionBegin;
  st->shift_matrix = mode;
  if (mode == STMATMODE_SHELL) {
    /* if shift_mat is set then the default preconditioner is ILU,
       otherwise set Jacobi as the default */
    ierr = KSPGetPC(st->ksp,&pc); CHKERRQ(ierr);
    ierr = PCSetType(pc,PCJACOBI);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

