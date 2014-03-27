!
!  Include file for Fortran use of the PEP object in SLEPc
!
!  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
!  SLEPc - Scalable Library for Eigenvalue Problem Computations
!  Copyright (c) 2002-2013, Universitat Politecnica de Valencia, Spain
!
!  This file is part of SLEPc.
!
!  SLEPc is free software: you can redistribute it and/or modify it under  the
!  terms of version 3 of the GNU Lesser General Public License as published by
!  the Free Software Foundation.
!
!  SLEPc  is  distributed in the hope that it will be useful, but WITHOUT  ANY
!  WARRANTY;  without even the implied warranty of MERCHANTABILITY or  FITNESS
!  FOR  A  PARTICULAR PURPOSE. See the GNU Lesser General Public  License  for
!  more details.
!
!  You  should have received a copy of the GNU Lesser General  Public  License
!  along with SLEPc. If not, see <http://www.gnu.org/licenses/>.
!  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
!
#if !defined(__SLEPCPEP_H)
#define __SLEPCPEP_H

#include "finclude/slepcipdef.h"
#include "finclude/slepcstdef.h"
#include "finclude/slepcdsdef.h"
#include "finclude/slepcepsdef.h"

#if !defined(PETSC_USE_FORTRAN_DATATYPES)
#define PEP                PetscFortranAddr
#endif

#define PEPType            character*(80)
#define PEPProblemType     PetscEnum
#define PEPWhich           PetscEnum
#define PEPConvergedReason PetscEnum

#define PEPLINEAR    'linear'
#define PEPPARNOLDI  'parnoldi'
#define PEPPLANCZOS  'planczos'
#define PEPTOAR      'toar'
#define PEPSTOAR     'stoar'

#endif