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

#include <petsc-private/fortranimpl.h>
#include <slepc-private/slepcimpl.h>
#include <slepc-private/qepimpl.h>

#if defined(PETSC_HAVE_FORTRAN_CAPS)
#define qepdestroy_                 QEPDESTROY
#define qepview_                    QEPVIEW
#define qepsetoptionsprefix_        QEPSETOPTIONSPREFIX
#define qepappendoptionsprefix_     QEPAPPENDOPTIONSPREFIX
#define qepgetoptionsprefix_        QEPGETOPTIONSPREFIX
#define qepcreate_                  QEPCREATE
#define qepsettype_                 QEPSETTYPE
#define qepgettype_                 QEPGETTYPE
#define qepmonitorall_              QEPMONITORALL
#define qepmonitorlg_               QEPMONITORLG
#define qepmonitorlgall_            QEPMONITORLGALL
#define qepmonitorset_              QEPMONITORSET
#define qepmonitorconverged_        QEPMONITORCONVERGED
#define qepmonitorfirst_            QEPMONITORFIRST
#define qepgetip_                   QEPGETIP
#define qepgetds_                   QEPGETDS
#define qepgetwhicheigenpairs_      QEPGETWHICHEIGENPAIRS
#define qepgetproblemtype_          QEPGETPROBLEMTYPE
#define qepgetconvergedreason_      QEPGETCONVERGEDREASON
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define qepdestroy_                 qepdestroy
#define qepview_                    qepview
#define qepsetoptionsprefix_        qepsetoptionsprefix
#define qepappendoptionsprefix_     qepappendoptionsprefix
#define qepgetoptionsprefix_        qepgetoptionsprefix
#define qepcreate_                  qepcreate
#define qepsettype_                 qepsettype
#define qepgettype_                 qepgettype
#define qepmonitorall_              qepmonitorall
#define qepmonitorlg_               qepmonitorlg
#define qepmonitorlgall_            qepmonitorlgall
#define qepmonitorset_              qepmonitorset
#define qepmonitorconverged_        qepmonitorconverged
#define qepmonitorfirst_            qepmonitorfirst
#define qepgetip_                   qepgetip
#define qepgetds_                   qepgetds
#define qepgetwhicheigenpairs_      qepgetwhicheigenpairs
#define qepgetproblemtype_          qepgetproblemtype
#define qepgetconvergedreason_      qepgetconvergedreason
#endif

/*
   These are not usually called from Fortran but allow Fortran users
   to transparently set these monitors from .F code, hence no STDCALL
*/
PETSC_EXTERN void qepmonitorall_(QEP *qep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = QEPMonitorAll(*qep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

PETSC_EXTERN void qepmonitorlg_(QEP *qep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = QEPMonitorLG(*qep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

PETSC_EXTERN void qepmonitorlgall_(QEP *qep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = QEPMonitorLGAll(*qep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

PETSC_EXTERN void qepmonitorconverged_(QEP *qep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = QEPMonitorConverged(*qep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

PETSC_EXTERN void qepmonitorfirst_(QEP *qep,PetscInt *it,PetscInt *nconv,PetscScalar *eigr,PetscScalar *eigi,PetscReal *errest,PetscInt *nest,void *ctx,PetscErrorCode *ierr)
{
  *ierr = QEPMonitorFirst(*qep,*it,*nconv,eigr,eigi,errest,*nest,ctx);
}

static struct {
  PetscFortranCallbackId monitor;
  PetscFortranCallbackId monitordestroy;
} _cb;

/* These are not extern C because they are passed into non-extern C user level functions */
#undef __FUNCT__
#define __FUNCT__ "ourmonitor"
static PetscErrorCode ourmonitor(QEP qep,PetscInt i,PetscInt nc,PetscScalar *er,PetscScalar *ei,PetscReal *d,PetscInt l,void* ctx)
{
  PetscObjectUseFortranCallback(qep,_cb.monitor,(QEP*,PetscInt*,PetscInt*,PetscScalar*,PetscScalar*,PetscReal*,PetscInt*,void*,PetscErrorCode*),(&qep,&i,&nc,er,ei,d,&l,_ctx,&ierr));
  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "ourdestroy"
static PetscErrorCode ourdestroy(void** ctx)
{
  QEP qep = (QEP)*ctx;
  PetscObjectUseFortranCallback(qep,_cb.monitordestroy,(void*,PetscErrorCode*),(_ctx,&ierr));
  return 0;
}

PETSC_EXTERN void PETSC_STDCALL qepdestroy_(QEP *qep,PetscErrorCode *ierr)
{
  *ierr = QEPDestroy(qep);
}

PETSC_EXTERN void PETSC_STDCALL qepview_(QEP *qep,PetscViewer *viewer,PetscErrorCode *ierr)
{
  PetscViewer v;
  PetscPatchDefaultViewers_Fortran(viewer,v);
  *ierr = QEPView(*qep,v);
}

PETSC_EXTERN void PETSC_STDCALL qepsettype_(QEP *qep,CHAR type PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(type,len,t);
  *ierr = QEPSetType(*qep,t);
  FREECHAR(type,t);
}

PETSC_EXTERN void PETSC_STDCALL qepgettype_(QEP *qep,CHAR name PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  QEPType tname;

  *ierr = QEPGetType(*qep,&tname);if (*ierr) return;
  *ierr = PetscStrncpy(name,tname,len);
  FIXRETURNCHAR(PETSC_TRUE,name,len);
}

PETSC_EXTERN void PETSC_STDCALL qepsetoptionsprefix_(QEP *qep,CHAR prefix PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(prefix,len,t);
  *ierr = QEPSetOptionsPrefix(*qep,t);
  FREECHAR(prefix,t);
}

PETSC_EXTERN void PETSC_STDCALL qepappendoptionsprefix_(QEP *qep,CHAR prefix PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  char *t;

  FIXCHAR(prefix,len,t);
  *ierr = QEPAppendOptionsPrefix(*qep,t);
  FREECHAR(prefix,t);
}

PETSC_EXTERN void PETSC_STDCALL qepcreate_(MPI_Fint *comm,QEP *qep,PetscErrorCode *ierr)
{
  *ierr = QEPCreate(MPI_Comm_f2c(*(comm)),qep);
}

PETSC_EXTERN void PETSC_STDCALL qepgetoptionsprefix_(QEP *qep,CHAR prefix PETSC_MIXED_LEN(len),PetscErrorCode *ierr PETSC_END_LEN(len))
{
  const char *tname;

  *ierr = QEPGetOptionsPrefix(*qep,&tname); if (*ierr) return;
  *ierr = PetscStrncpy(prefix,tname,len);
}

PETSC_EXTERN void PETSC_STDCALL qepmonitorset_(QEP *qep,void (PETSC_STDCALL *monitor)(QEP*,PetscInt*,PetscInt*,PetscScalar*,PetscScalar*,PetscReal*,PetscInt*,void*,PetscErrorCode*),void *mctx,void (PETSC_STDCALL *monitordestroy)(void *,PetscErrorCode*),PetscErrorCode *ierr)
{
  SlepcConvMonitor ctx;

  CHKFORTRANNULLOBJECT(mctx);
  CHKFORTRANNULLFUNCTION(monitordestroy);
  if ((PetscVoidFunction)monitor == (PetscVoidFunction)qepmonitorall_) {
    *ierr = QEPMonitorSet(*qep,QEPMonitorAll,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)qepmonitorlg_) {
    *ierr = QEPMonitorSet(*qep,QEPMonitorLG,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)qepmonitorlgall_) {
    *ierr = QEPMonitorSet(*qep,QEPMonitorLGAll,0,0);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)qepmonitorconverged_) {
    if (!FORTRANNULLOBJECT(mctx)) {
      PetscError(PetscObjectComm((PetscObject)*qep),__LINE__,"qepmonitorset_",__FILE__,PETSC_ERR_ARG_WRONG,PETSC_ERROR_INITIAL,"Must provide PETSC_NULL_OBJECT as a context in the Fortran interface to QEPMonitorSet");
      *ierr = 1;
      return;
    }
    *ierr = PetscNew(struct _n_SlepcConvMonitor,&ctx);
    if (*ierr) return;
    ctx->viewer = NULL;
    *ierr = QEPMonitorSet(*qep,QEPMonitorConverged,ctx,(PetscErrorCode (*)(void**))SlepcConvMonitorDestroy);
  } else if ((PetscVoidFunction)monitor == (PetscVoidFunction)qepmonitorfirst_) {
    *ierr = QEPMonitorSet(*qep,QEPMonitorFirst,0,0);
  } else {
    *ierr = PetscObjectSetFortranCallback((PetscObject)*qep,PETSC_FORTRAN_CALLBACK_CLASS,&_cb.monitor,(PetscVoidFunction)monitor,mctx); if (*ierr) return;
    if (!monitordestroy) {
      *ierr = QEPMonitorSet(*qep,ourmonitor,*qep,0);
    } else {
      *ierr = PetscObjectSetFortranCallback((PetscObject)*qep,PETSC_FORTRAN_CALLBACK_CLASS,&_cb.monitordestroy,(PetscVoidFunction)monitordestroy,mctx); if (*ierr) return;
      *ierr = QEPMonitorSet(*qep,ourmonitor,*qep,ourdestroy);
    }
  }
}

PETSC_EXTERN void PETSC_STDCALL qepgetip_(QEP *qep,IP *ip,PetscErrorCode *ierr)
{
  *ierr = QEPGetIP(*qep,ip);
}

PETSC_EXTERN void PETSC_STDCALL qepgetds_(QEP *qep,DS *ds,PetscErrorCode *ierr)
{
  *ierr = QEPGetDS(*qep,ds);
}

PETSC_EXTERN void PETSC_STDCALL qepgetwhicheigenpairs_(QEP *qep,QEPWhich *which,PetscErrorCode *ierr)
{
  *ierr = QEPGetWhichEigenpairs(*qep,which);
}

PETSC_EXTERN void PETSC_STDCALL qepgetproblemtype_(QEP *qep,QEPProblemType *type,PetscErrorCode *ierr)
{
  *ierr = QEPGetProblemType(*qep,type);
}

PETSC_EXTERN void PETSC_STDCALL qepgetconvergedreason_(QEP *qep,QEPConvergedReason *reason,PetscErrorCode *ierr)
{
  *ierr = QEPGetConvergedReason(*qep,reason);
}

