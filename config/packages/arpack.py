#
#  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#  SLEPc - Scalable Library for Eigenvalue Problem Computations
#  Copyright (c) 2002-2015, Universitat Politecnica de Valencia, Spain
#
#  This file is part of SLEPc.
#
#  SLEPc is free software: you can redistribute it and/or modify it under  the
#  terms of version 3 of the GNU Lesser General Public License as published by
#  the Free Software Foundation.
#
#  SLEPc  is  distributed in the hope that it will be useful, but WITHOUT  ANY
#  WARRANTY;  without even the implied warranty of MERCHANTABILITY or  FITNESS
#  FOR  A  PARTICULAR PURPOSE. See the GNU Lesser General Public  License  for
#  more details.
#
#  You  should have received a copy of the GNU Lesser General  Public  License
#  along with SLEPc. If not, see <http://www.gnu.org/licenses/>.
#  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#

import os, commands, shutil
import log, package

class Arpack(package.Package):

  def __init__(self,argdb,log):
    package.Package.__init__(self,argdb,log)
    self.packagename    = 'arpack'
    self.installable    = True
    self.downloadable   = True
    self.url            = 'https://github.com/opencollab/arpack-ng/archive/3.3.0.tar.gz'
    self.archive        = 'arpack-ng-3.3.0.tar.gz'
    self.dirname        = 'arpack-ng-3.3.0'
    self.supportssingle = True
    self.ProcessArgs(argdb)

  def Check(self,conf,vars,cmake,petsc):
    if petsc.mpiuni:
      if petsc.scalar == 'real':
        if petsc.precision == 'single':
          functions = ['snaupd','sneupd','ssaupd','sseupd']
        else:
          functions = ['dnaupd','dneupd','dsaupd','dseupd']
      else:
        if petsc.precision == 'single':
          functions = ['cnaupd','cneupd']
        else:
          functions = ['znaupd','zneupd']
    else:
      if petsc.scalar == 'real':
        if petsc.precision == 'single':
          functions = ['psnaupd','psneupd','pssaupd','psseupd']
        else:
          functions = ['pdnaupd','pdneupd','pdsaupd','pdseupd']
      else:
        if petsc.precision == 'single':
          functions = ['pcnaupd','pcneupd']
        else:
          functions = ['pznaupd','pzneupd']

    if self.packagelibs:
      libs = [self.packagelibs]
    else:
      if petsc.mpiuni:
        libs = [['-larpack'],['-larpack_LINUX'],['-larpack_SUN4']]
      else:
        libs = [['-lparpack','-larpack'],['-lparpack_MPI','-larpack'],['-lparpack_MPI-LINUX','-larpack_LINUX'],['-lparpack_MPI-SUN4','-larpack_SUN4']]

    if self.packagedir:
      dirs = [self.packagedir]
    else:
      dirs = self.GenerateGuesses('Arpack')

    self.FortranLib(conf,vars,cmake,dirs,libs,functions)


  def Install(self,conf,vars,cmake,petsc,archdir):
    externdir = os.path.join(archdir,'externalpackages')
    builddir  = os.path.join(externdir,self.dirname)
    self.Download(externdir,builddir)

    # Build package
    confopt = '--prefix='+archdir+' F77="'+petsc.fc+'" FFLAGS="'+petsc.fc_flags.replace('-Wall','').replace('-Wshadow','')+'"'
    if not petsc.mpiuni:
      confopt = confopt+' --enable-mpi'
    result,output = commands.getstatusoutput('cd '+builddir+'&& sh bootstrap && ./configure '+confopt+' && '+petsc.make+' && '+petsc.make+' install')
    self.log.write(output)
    if result:
      self.log.Exit('ERROR: installation of ARPACK failed.')

    # Check build
    if petsc.scalar == 'real':
      if petsc.precision == 'single':
        functions = ['psnaupd','psneupd','pssaupd','psseupd']
      else:
        functions = ['pdnaupd','pdneupd','pdsaupd','pdseupd']
    else:
      if petsc.precision == 'single':
        functions = ['pcnaupd','pcneupd']
      else:
        functions = ['pznaupd','pzneupd']
    if petsc.mpiuni:
      libs = [['-larpack']]
    else:
      libs = [['-lparpack','-larpack']]
    libDir = os.path.join(archdir,'lib')
    dirs = [libDir]
    self.FortranLib(conf,vars,cmake,dirs,libs,functions)
    self.havepackage = True

