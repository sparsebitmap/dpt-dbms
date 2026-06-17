# dpt-dbms_msys2_python.Makefile
# Run in a Msys2 CLANG64 or UCRT64 shell on Microsoft Windows.

# make -f dpt-dbms_msys2_python.Makefile
# builds the Python interface to DPT.

TOOL_CHAIN = msys2
TOOL_CHAIN_VERSION = 
CPPSTANDARD = 
PATH_TO_CXX =
COMPILER = clang++

# Value of the --platform-tag argument for 'wheel tags ...' command.

# BITNESS x86-64 or i686
# RUNTIME ucrt or msvcrt
# CHAIN   llvm or gnu

BITNESS = x86_64
RUNTIME = ucrt
CHAIN = llvm
PLATFORM_TAG = mingw_$(BITNESS)_$(RUNTIME)_$(CHAIN)

# Set value of VERSION_DPT_DBMS_DPTDB.
include version_dpt_dbms_dptdb

SWIG_DPTAPI = dptapi
PYTHON_API_DLL = _$(SWIG_DPTAPI).pyd
DPT_DBMS = ../..
DPT_PYTHON = .
DPT_DBMS_OBJECT = $(DPT_PYTHON)/object/$(PLATFORM_TAG)
DPT_DBMS_API_OBJECT = $(DPT_DBMS_OBJECT)/dbapi
DPT_DBMS_STDAFX_OBJECT = $(DPT_DBMS_OBJECT)/stdafx
DPT_DBMS_OBJECT_SWIG = $(DPT_DBMS_OBJECT)/swig
DPT_DBMS_OBJECT_SWIG_PYTHON = $(DPT_DBMS_OBJECT_SWIG)/$(PLATFORM_TAG)
DPT_DBMS_PACKAGE = $(DPT_PYTHON)/package/$(PLATFORM_TAG)
DPT_DBMS_PACKAGE_DPTDB = $(DPT_DBMS_PACKAGE)/src/$(PROJECT_NAME)

# Main targets (phony build targets are defined near the build targets).

.PHONY : all clean distribution local-install

all : distribution

DPT_LICENCE = licence.txt

# DPT extract and build folders.

DPT_DBMS_SRC = $(DPT_DBMS)/source
DPT_DBMS_INC = $(DPT_DBMS)/include
DPT_DBMS_API_SRC = $(DPT_DBMS_SRC)/dbapi
DPT_DBMS_API_INC = $(DPT_DBMS_INC)/dbapi
DPT_DBMS_STDAFX = $(DPT_PYTHON)/stdafx

# The *.i file defining the SWIG interface.
SWIG_INTERFACE = dptapi_python.i
DPT_DBMS_SWIG = $(DPT_PYTHON)/swig
DPT_DBMS_SWIG_INTERFACE = $(DPT_PYTHON)/$(SWIG_INTERFACE)

# Source file names.

NAMES = $(basename $(notdir $(wildcard $(DPT_DBMS_SRC)/*.cpp)))

API_NAMES = $(basename $(notdir $(wildcard $(DPT_DBMS_API_SRC)/*.cpp)))

INC_NAMES = $(basename $(notdir $(wildcard $(DPT_DBMS_INC)/*.h)))

API_INC_NAMES = $(basename $(notdir $(wildcard $(DPT_DBMS_API_INC)/*.h)))

AFX_NAMES = stdafx

SRCS = $(NAMES:%=$(DPT_DBMS_SRC)/%.cpp)

API_SRCS = $(API_NAMES:%=$(DPT_DBMS_API_SRC)/%.cpp)

INCS = $(INC_NAMES:%=$(DPT_DBMS_INC)/%.h)

API_INCS = $(API_INC_NAMES:%=$(DPT_DBMS_API_INC)/%.h)

AFX_SRCS = $(AFX_NAMES:%=$(DPT_DBMS_STDAFX)/%.cpp)

AFX_INCS = $(AFX_NAMES:%=$(DPT_DBMS_STDAFX)/%.h)

# Compiler settings.

CXX = $(PATH_TO_CXX)$(COMPILER)
DEFOPTS = $(DEFINES) $(OPTIONS)
CXXEXTRA = -D_BBDBAPI
DPT_LDFLAGS = -shared

# Build includes (-I) options for compiler.

DPT_DBMS_INCLUDE = -I$(DPT_DBMS_INC) -I$(DPT_DBMS_STDAFX)
DPT_DBMS_API_INCLUDE = -I$(DPT_DBMS_INC) -I$(DPT_DBMS_API_INC) -I$(DPT_DBMS_STDAFX)
DPTAFX_INCLUDE = -I$(DPT_DBMS_INC) -I$(DPT_DBMS_STDAFX)

# Pattern rules DPT C++.

$(DPT_DBMS_OBJECT)/%.o : $(DPT_DBMS_SRC)/%.cpp
	@mkdir -p $(DPT_DBMS_OBJECT)
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPT_DBMS_INCLUDE) $(DEFOPTS) -o $@ $<

$(DPT_DBMS_API_OBJECT)/%.o : $(DPT_DBMS_API_SRC)/%.cpp
	@mkdir -p $(DPT_DBMS_API_OBJECT)
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPT_DBMS_API_INCLUDE) $(DEFOPTS) -o $@ $<

$(DPT_DBMS_STDAFX_OBJECT)/%.o : $(DPT_DBMS_STDAFX)/%.cpp
	@mkdir -p $(DPT_DBMS_STDAFX_OBJECT)
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPTAFX_INCLUDE) $(DEFOPTS) -o $@ $<

# Object file names.

OBJS = $(NAMES:%=$(DPT_DBMS_OBJECT)/%.o)

API_OBJS = $(API_NAMES:%=$(DPT_DBMS_API_OBJECT)/%.o)

AFX_OBJS = $(AFX_NAMES:%=$(DPT_DBMS_STDAFX_OBJECT)/%.o)

OBJECT_FILES = $(OBJS) $(AFX_OBJS) $(API_OBJS)

$(OBJECT_FILES) : $(DPT_DBMS_STDAFX)/stdafx.h $(INCS) $(API_INCS)

PYTHON_NAME = python
SWIG_PYTHON = $(PYTHON_NAME)
PYTHON_MAJOR ?= 3
PYTHON_MINOR ?= 12
PYTHON_MAJOR_MINOR = $(PYTHON_MAJOR).$(PYTHON_MINOR)
PYTHON_VERSION = $(PYTHON_MAJOR_MINOR)
PYTHON_TAG = cp$(PYTHON_MAJOR)$(PYTHON_MINOR)
ABI_TAG = cp$(PYTHON_MAJOR)$(PYTHON_MINOR)

PYTHON_HEADER = $(PYTHON_NAME)$(PYTHON_VERSION)
PYTHON_VERSION_DIRECTORY = $(MSYSTEM_PREFIX)
PYTHON_INCLUDE_DIRECTORY = $(PYTHON_VERSION_DIRECTORY)/include
PYTHON_INCLUDE = $(PYTHON_INCLUDE_DIRECTORY)/$(PYTHON_HEADER)
PYTHON_LIBRARY = $(SWIG_PYTHON)$(PYTHON_VERSION)
SYSTEM_LIBRARY = $(MSYSTEM_PREFIX)/bin

BREAK_SYSTEM_PACKAGES =

WRAP_CXX = $(DPT_DBMS_SWIG)/$(SWIG_DPTAPI)_$(SWIG_PYTHON)_wrap.cxx
WRAP_O = $(DPT_DBMS_OBJECT_SWIG_PYTHON)/$(SWIG_DPTAPI)_$(SWIG_PYTHON)_wrap.o

# Copy package template directory and build wrapper.

$(WRAP_CXX) : $(DPT_DBMS_SWIG_INTERFACE)
	@mkdir -p $(DPT_DBMS_PACKAGE)/src
	cp -p package-template/LICENCE $(DPT_DBMS_PACKAGE)/
	sed -e '/Xproject_nameX/s//\/$(PROJECT_NAME)\//g' package-template/MANIFEST.in > $(DPT_DBMS_PACKAGE)/MANIFEST.in
	cp -p package-template/README.rst $(DPT_DBMS_PACKAGE)/
	cp -Rp package-template/tests $(DPT_DBMS_PACKAGE)/
	cp -Rp package-template/src/$(PROJECT_NAME) $(DPT_DBMS_PACKAGE)/src/
	sed -e '/"invalid"/s/invalid/$(VERSION_DPT_DBMS_DPTDB)/g' package-template/pyproject.toml > $(DPT_DBMS_PACKAGE)/pyproject.toml
	@mkdir -p $(DPT_DBMS_SWIG)
	swig -c++ -$(SWIG_PYTHON) -o $@ -outdir $(DPT_DBMS_PACKAGE_DPTDB) -I$(DPT_DBMS_API_INC) $(DPT_DBMS_SWIG_INTERFACE)

$(WRAP_O) : $(WRAP_CXX)
	@mkdir -p $(DPT_DBMS_OBJECT_SWIG_PYTHON)
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) -I$(DPT_DBMS_API_INC) -I$(DPT_DBMS_INC) -I$(PYTHON_INCLUDE) $(DEFOPTS) -o $@ $(WRAP_CXX)

# The .pyd file built from SWIG.

$(DPT_DBMS_PACKAGE_DPTDB)/$(PYTHON_API_DLL) : $(OBJECT_FILES) $(WRAP_O)
	@mkdir -p $(DPT_DBMS_PACKAGE_DPTDB)
	$(CXX) $(DPT_LDFLAGS) -o $@ $(WRAP_O) $(API_OBJS) $(OBJS) -l$(PYTHON_LIBRARY) -L$(SYSTEM_LIBRARY)

SOURCE_FILES = $(SRCS) $(INCS) $(API_SRCS) $(API_INCS)

distribution : $(SOURCE_FILES) $(DPT_DBMS_PACKAGE_DPTDB)/$(PYTHON_API_DLL)
	python -m build --wheel --no-isolation $(DPT_DBMS_PACKAGE)
	python -m wheel tags --python-tag=$(PYTHON_TAG) --abi-tag=$(ABI_TAG) --platform-tag=$(PLATFORM_TAG) --remove $(DPT_DBMS_PACKAGE)/dist/$(PROJECT_NAME)-$(VERSION_DPT_DBMS_DPTDB)-py3-none-any.whl

local-install : distribution
	python -m pip install --user $(BREAK_SYSTEM_PACKAGES) --no-index --find-links $(DPT_DBMS_PACKAGE)/dist $(PROJECT_NAME)

clean :
	-rm $(DPT_DBMS_OBJECT)/*.o
	-rm $(DPT_DBMS_STDAFX_OBJECT)/*.o
	-rm $(DPT_DBMS_API_OBJECT)/*.o
	-rm $(DPT_DBMS_SWIG)/*
	-rm $(DPT_DBMS_OBJECT_SWIG_PYTHON)/*.o
#	-rm $(DPT_DBMS_OBJECT_SWIG)/*.o
	-rm -r $(DPT_DBMS_PACKAGE)

