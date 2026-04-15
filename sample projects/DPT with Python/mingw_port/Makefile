### Makefile to build SWIG wrappers for DPT

### The command 'make python' is equivalent to
### 'make python SWIG_DIR=C:/swigwin-1.3.39 PYTHON_DIR=C:/Python25 PYTHON_LIBRARY=python25 DPT_STDAFX=../../HelloWorld! DPT_BASE=../../..'

### Use ..._DIR variables to say where SWIG and Python are installed if needed
### Use PYTHON_LIBRARY matching your version of Python if needed
### Here PYTHON_LIBRARY refers to 'C:/Python25/libs/python25.lib'
### Use DPT_STDAFX to locate StdAfx.cpp and StdAfx.h relative to Makefile
### Use DPT_BASE to locate source and include directories relative to Makefile

### Wine settings

PATH_TO_PYTHON ?= C:
POSIX_PATH_TO_PYTHON ?= C:
PATH_TO_SWIG ?= C:/swigwin-1.3.39
PATH_TO_CXX ?= 
COMPILER ?= mingw32-g++

### Common settings

CXX = $(PATH_TO_CXX)$(COMPILER)
DEFOPTS = $(DEFINES) $(OPTIONS)
CXXEXTRA = \
   -D_BBDBAPI
DPT_STDAFX ?= ../stdafx
DPT_BASE ?= ../../..
DPT_INCLUDE = -I$(DPT_BASE)/include -I$(DPT_STDAFX)
DPTAPI_INCLUDE = -I$(DPT_BASE)/include -I$(DPT_BASE)/include/dbapi -I$(DPT_STDAFX)
DPTAFX_INCLUDE = -I$(DPT_BASE)/include -I$(DPT_STDAFX)
DPT_LDFLAGS = -shared

### Main DPT folders

DPT_SRC = $(DPT_BASE)/source
DPT_INC = $(DPT_BASE)/include
DPTAPI_SRC = $(DPT_SRC)/dbapi
DPTAPI_INC = $(DPT_INC)/dbapi
DPT_RELEASE = Release

### SWIG settings

SWIG_DIR = $(PATH_TO_SWIG)
SWIG_DPTAPI = dptapi
SWIG_PYTHON = python
SWIG_LANG_SUBSET = $(SWIG_PYTHON)

### Python settings

PYTHON_VERSION ?= 25
PYTHON_DIR = $(PATH_TO_PYTHON)/Python$(PYTHON_VERSION)
POSIX_PYTHON_DIR = $(POSIX_PATH_TO_PYTHON)/Python$(PYTHON_VERSION)
PYTHON_LIBRARY = python$(PYTHON_VERSION)
PYTHON_API_INCLUDE = -I$(DPT_BASE)/include/dbapi -I$(DPT_BASE)/include -I$(PYTHON_DIR)/include
PYTHON_API_DLL = $(SWIG_PYTHON)/_$(SWIG_DPTAPI).pyd
PYTHON_INCLUDE = -I$(DPT_BASE)/include -I$(PYTHON_DIR)/include
PYTHON_LIBS = -l$(PYTHON_LIBRARY) -L$(PYTHON_DIR)/libs

### Patches

PATCH = patch
PATCH_SRC = 
PATCH_INC = 

DPT_PATCH_SRC = $(PATCH_SRC:%=$(DPT_SRC)/%.cpp)
DPT_PATCH_INC = $(PATCH_INC:%=$(DPT_INC)/%.h)

### Sources

DPT_NAMES ?= 

DPTAPI_NAMES ?= 

DPTAFX_NAMES = \
   stdafx

DPT_OBJS = $(DPT_NAMES:%=$(DPT_RELEASE)/%.o)

DPTAPI_OBJS = $(DPTAPI_NAMES:%=$(DPT_RELEASE)/%.o)

DPTAFX_OBJS = $(DPTAFX_NAMES:%=$(DPT_RELEASE)/%.o)

### Targets

.PHONY : all patch compile clean wrap $(SWIG_LANG_SUBSET) install

.INTERMEDIATE : $(SWIG_PYTHON)/$(SWIG_DPTAPI).dll

all : compile

patch : $(DPT_PATCH_SRC) $(DPT_PATCH_INC) $(DPTAPI_PATCH_SRC) $(DPTAPI_PATCH_INC)

compile : patch $(DPT_RELEASE) $(DPT_OBJS) $(DPTAPI_OBJS) $(DPTAFX_OBJS)

clean :
	-rm $(DPT_OBJS) $(DPTAPI_OBJS) $(DPTAFX_OBJS)
	-rmdir $(DPT_RELEASE)
	-rm -r $(SWIG_LANG_SUBSET)
	-rm $(SWIG_LANG_SUBSET:%=$(SWIG_DPTAPI)_%_wrap.cxx)
	-rm $(SWIG_LANG_SUBSET:%=$(SWIG_DPTAPI)_%_wrap.o)

wrap : patch $(SWIG_LANG_SUBSET:%=$(SWIG_DPTAPI)_%_wrap.cxx)

$(DPT_RELEASE) :
	-mkdir $(DPT_RELEASE)

### Python wrapper

### dpt API version

$(SWIG_DPTAPI)_$(SWIG_PYTHON)_wrap.cxx : SWIG_LANG_OPTS =
$(SWIG_DPTAPI)_$(SWIG_PYTHON)_wrap.o : SWIG_LANG_INCLUDE = $(PYTHON_API_INCLUDE)
$(PYTHON_API_DLL) : SWIG_LANG_LIBS = $(PYTHON_LIBS)
$(PYTHON_API_DLL) : SWIG_LANG_DLL = $(PYTHON_API_DLL)

$(SWIG_PYTHON) : compile $(PYTHON_API_DLL)

### install dptdb package in site-packages
### python documentation suggests that distutils may be able to do the whole build
### while admitting that its SWIG interface is flaky.

install : $(SWIG_PYTHON)
	cp ../__init__.py $(SWIG_PYTHON)/
	$(PYTHON_DIR)/python ../dptversion.py $(DPT_BASE)
	-mkdir $(POSIX_PYTHON_DIR)/Lib/site-packages/dptdb
	cp $(SWIG_PYTHON)/__init__.py $(POSIX_PYTHON_DIR)/Lib/site-packages/dptdb
	cp $(SWIG_PYTHON)/version.py $(POSIX_PYTHON_DIR)/Lib/site-packages/dptdb
	cp $(SWIG_PYTHON)/$(SWIG_DPTAPI).py $(POSIX_PYTHON_DIR)/Lib/site-packages/dptdb
	cp $(PYTHON_API_DLL) $(POSIX_PYTHON_DIR)/Lib/site-packages/dptdb

uninstall :
	-rm -r $(POSIX_PYTHON_DIR)/Lib/site-packages/dptdb

$(PYTHON_API_DLL) : $(SWIG_PYTHON)/$(SWIG_DPTAPI).dll

### Pattern rules DPT C++

$(DPT_SRC)/%.cpp : %.cpp.patch
	patch -b -N -i $< -r $*.cpp.rej $@

$(DPT_INC)/%.h : %.h.patch
	patch -b -N -i $< -r $*.h.rej $@

$(DPT_RELEASE)/%.o : $(DPT_SRC)/%.cpp
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPT_INCLUDE) $(DEFOPTS) -o $@ $<

$(DPT_RELEASE)/%.o : $(DPTAPI_SRC)/%.cpp
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPTAPI_INCLUDE) $(DEFOPTS) -o $@ $<

$(DPT_RELEASE)/%.o : $(DPT_STDAFX)/%.cpp
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPTAFX_INCLUDE) $(DEFOPTS) -o $@ $<

### Pattern rules SWIG wrappers for dpt API version

$(SWIG_DPTAPI)_%_wrap.cxx : ../$(SWIG_DPTAPI)_%.i
	-mkdir $*
	$(SWIG_DIR)/swig -c++ -$* $(SWIG_LANG_OPTS) -o $@ -outdir $* -I$(DPTAPI_INC) $<

$(SWIG_DPTAPI)_%_wrap.o : $(SWIG_DPTAPI)_%_wrap.cxx
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(SWIG_LANG_INCLUDE) $(DEFOPTS) -o $@ $<

%/$(SWIG_DPTAPI).dll : $(SWIG_DPTAPI)_%_wrap.o
	$(CXX) $(DPT_LDFLAGS) -o $(SWIG_LANG_DLL) $< $(DPTAPI_OBJS) $(DPT_OBJS) $(SWIG_LANG_LIBS)

