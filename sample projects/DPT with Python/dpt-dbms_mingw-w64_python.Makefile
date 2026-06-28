# dpt-dbms_mingw-w64_python.Makefile
# Run in a shell on a BSD or Linux with wine installed.

# make -f dpt-dbms_mingw-w64_python.Makefile
# builds the Python interface to DPT.

# On a BSD usually replace 'make' by 'gmake' in such commands.

# Packages such as mingw-w64 are usually installed at /usr/... on
# Linux distributions but at /usr/local/... on BSD.
# Packages built locally from source might be anywhere.
# This makefile needs to know where some DLLs are located when
# installing the Python package.
# Add OTHER_DLLS=<whatever> to the command if this default is not
# suitable.
OTHER_DLLS = usr

TOOL_CHAIN = mingw-w64
TOOL_CHAIN_VERSION = 
CPPSTANDARD = 
PATH_TO_CXX =
WINE = wine

# Replace x86_64 by i686 for 32-bit builds.
# This name is for the msvcrt version of the g++ compiler advertised on
# mingw-w64 website home page in 'Getting started' section.
# (After replacing 'gcc' with 'g++'.)
# The other option is the ucrt version named 'x86_64-w64-mingw32ucrt-g++'.
# The package on a BSD or Linux may call them something else.
# This name is offered by Debian.
COMPILER = x86_64-w64-mingw32-g++

# Python statement 'import dpt_dbms.dptapi' will report these dlls cannot
# be found if links are not provided in the Python package. 
# Links are created in site-packages/dpt_dbms but boxes other than Debian
# may need different names, and on Debian some, or all, of these names
# will become wrong when later versions of compiler become the default.
OTHER_DLLS_DIRECTORY = /$(OTHER_DLLS)/lib/gcc/x86_64-w64-mingw32/14-win32
LIBSTDC = libstdc++-6.dll
LIBGCC_S_SEH = libgcc_s_seh-1.dll

# Value of the --platform-tag argument for 'wheel tags ...' command.

# Not sure what PLATFORM_TAG should be if a mingw compiler is used,
# perhaps mingw_x86_64 like the Msys case, but win_amd64 is likely
# correct for wine.
PLATFORM_TAG = win_amd64

# Set value of VERSION_DPT_DBMS_DPTDB.
include version_dpt_dbms_dptdb

PATH_TO_SWIG_WITHOUT_VERSION = c:/swigwin-
SWIG_VERSION = 4.4.1
PATH_TO_SWIG = $(PATH_TO_SWIG_WITHOUT_VERSION)$(SWIG_VERSION)

SWIG_DPTAPI = dptapi
PYTHON_API_DLL = _$(SWIG_DPTAPI).pyd
DPT_DBMS = ../..
DPT_PYTHON = .
DPT_DBMS_OBJECT = $(DPT_PYTHON)/object/$(TOOL_CHAIN)
DPT_DBMS_API_OBJECT = $(DPT_DBMS_OBJECT)/dbapi
DPT_DBMS_STDAFX_OBJECT = $(DPT_DBMS_OBJECT)/stdafx
DPT_DBMS_OBJECT_SWIG = $(DPT_DBMS_OBJECT)/swig
DPT_DBMS_OBJECT_SWIG_PYTHON = $(DPT_DBMS_OBJECT_SWIG)/$(TOOL_CHAIN)
DPT_DBMS_PACKAGE = $(DPT_PYTHON)/package/$(TOOL_CHAIN)
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
WINE_SRC = $(DPT_PYTHON)/$(TOOL_CHAIN)/source
WINE_INC = $(DPT_PYTHON)/$(TOOL_CHAIN)/include
WINE_API_SRC = $(WINE_SRC)/dbapi
WINE_API_INC = $(WINE_INC)/dbapi

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

# Compiler settings.

CXX = $(PATH_TO_CXX)$(COMPILER)
DEFOPTS = $(DEFINES) $(OPTIONS)
CXXEXTRA = -D_BBDBAPI
DPT_LDFLAGS = -shared

# Build includes (-I) options for compiler.

DPT_DBMS_INCLUDE = -I$(WINE_INC) -I$(DPT_DBMS_STDAFX)
DPT_DBMS_API_INCLUDE = -I$(WINE_INC) -I$(WINE_API_INC) -I$(DPT_DBMS_STDAFX)
DPTAFX_INCLUDE = -I$(WINE_INC) -I$(DPT_DBMS_STDAFX)

# Pattern rules for editing DPT C++ to replace '\' path separator by '/'.

SED_PATH_SEPARATOR_EDIT = -f sed_path_separator_edit

$(WINE_SRC)/%.cpp : $(DPT_DBMS_SRC)/%.cpp
	@mkdir -p $(WINE_SRC)
	sed $(SED_PATH_SEPARATOR_EDIT) $< > $@

$(WINE_INC)/%.h : $(DPT_DBMS_INC)/%.h
	@mkdir -p $(WINE_INC)
	sed $(SED_PATH_SEPARATOR_EDIT) $< > $@

$(WINE_API_SRC)/%.cpp : $(DPT_DBMS_API_SRC)/%.cpp
	@mkdir -p $(WINE_API_SRC)
	sed $(SED_PATH_SEPARATOR_EDIT) $< > $@

$(WINE_API_INC)/%.h : $(DPT_DBMS_API_INC)/%.h
	@mkdir -p $(WINE_API_INC)
	sed $(SED_PATH_SEPARATOR_EDIT) $< > $@

# Pattern rules DPT C++.

# '-' prefix to see all errors since the make fails for various reasons:
# some trivial like '\' in path names (#include <sys\utime> for example),
# and some serious like different parts of the toolchain disagreeing on
# things, like macros to define max and min for example.

$(DPT_DBMS_OBJECT)/%.o : $(WINE_SRC)/%.cpp
	@mkdir -p $(DPT_DBMS_OBJECT)
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPT_DBMS_INCLUDE) $(DEFOPTS) -o $@ $<

$(DPT_DBMS_API_OBJECT)/%.o : $(WINE_API_SRC)/%.cpp
	@mkdir -p $(DPT_DBMS_API_OBJECT)
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPT_DBMS_API_INCLUDE) $(DEFOPTS) -o $@ $<

$(DPT_DBMS_STDAFX_OBJECT)/%.o : $(DPT_DBMS_STDAFX)/%.cpp
	@mkdir -p  $(DPT_DBMS_STDAFX_OBJECT)
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) $(DPTAFX_INCLUDE) $(DEFOPTS) -o $@ $<

# Object file names.

OBJS = $(NAMES:%=$(DPT_DBMS_OBJECT)/%.o)

API_OBJS = $(API_NAMES:%=$(DPT_DBMS_API_OBJECT)/%.o)

AFX_OBJS = $(AFX_NAMES:%=$(DPT_DBMS_STDAFX_OBJECT)/%.o)

OBJECT_FILES = $(OBJS) $(AFX_OBJS) $(API_OBJS)

$(OBJECT_FILES) : $(DPT_DBMS_STDAFX)/stdafx.h $(INCS) $(API_INCS)

# Python distribution settings.

PYTHON_NAME = Python
SWIG_PYTHON = python
PYTHON_MAJOR = 3
PYTHON_MINOR = 14
PYTHON_MAJOR_MINOR = $(PYTHON_MAJOR)$(PYTHON_MINOR)
PYTHON_VERSION = $(PYTHON_MAJOR_MINOR)
PYTHON_TAG = cp$(PYTHON_MAJOR)$(PYTHON_MINOR)
ABI_TAG = cp$(PYTHON_MAJOR)$(PYTHON_MINOR)

PYTHON_HEADER = $(PYTHON_NAME).h
PYTHON_VERSION_PATH = Programs/$(PYTHON_NAME)/$(PYTHON_NAME)$(PYTHON_VERSION)
WINE_APPDATA = $(HOME)/.wine/drive_c/users/$(USER)/AppData/Local
PYTHON_INCLUDE = $(WINE_APPDATA)/$(PYTHON_VERSION_PATH)/include
PYTHON_LIBS_DIRECTORY = $(WINE_APPDATA)/$(PYTHON_VERSION_PATH)/libs
PYTHON_LIBRARY = $(SWIG_PYTHON)$(PYTHON_MAJOR_MINOR)

# Used when creating symbolic links to extra DLLs.
USER_SITE_PACKAGES = $(HOME)/.wine/drive_c/users/$(USER)/AppData/Roaming/$(PYTHON_NAME)/$(PYTHON_NAME)$(PYTHON_VERSION)/site-packages/$(PROJECT_NAME)

WRAP_CXX = $(DPT_DBMS_SWIG)/$(SWIG_DPTAPI)_$(SWIG_PYTHON)_wrap.cxx
WRAP_O = $(DPT_DBMS_OBJECT_SWIG_PYTHON)/$(SWIG_DPTAPI)_$(SWIG_PYTHON)_wrap.o

# Copy package template directory and build wrapper.

$(WRAP_CXX) : $(DPT_DBMS_SWIG_INTERFACE)
	@mkdir -p $(DPT_DBMS_PACKAGE)/src
	cp -p package-template/LICENCE $(DPT_DBMS_PACKAGE)/
	sed -e '/Xproject_nameX/s//\\$(PROJECT_NAME)\\/g' package-template/MANIFEST.in > $(DPT_DBMS_PACKAGE)/MANIFEST.in
	cp -p package-template/README.rst $(DPT_DBMS_PACKAGE)/
	cp -Rp package-template/tests $(DPT_DBMS_PACKAGE)/
	cp -Rp package-template/src/$(PROJECT_NAME) $(DPT_DBMS_PACKAGE)/src/
	sed -e '/"invalid"/s/invalid/$(VERSION_DPT_DBMS_DPTDB)/g' package-template/pyproject.toml > $(DPT_DBMS_PACKAGE)/pyproject.toml
	@mkdir -p $(DPT_DBMS_SWIG)
	$(WINE) $(PATH_TO_SWIG)/swig.exe -c++ -$(SWIG_PYTHON) -o $@ -outdir $(DPT_DBMS_PACKAGE_DPTDB) -I$(DPT_DBMS_API_INC) $(DPT_DBMS_SWIG_INTERFACE)

$(WRAP_O) : $(WRAP_CXX)
	@mkdir -p $(DPT_DBMS_OBJECT_SWIG_PYTHON)
	$(CXX) -c $(CXXEXTRA) $(CXXFLAGS) -I$(DPT_DBMS_API_INC) -I$(DPT_DBMS_INC) -I$(PYTHON_INCLUDE) $(DEFOPTS) -o $@ $(WRAP_CXX)

# The .pyd file built from SWIG.

$(DPT_DBMS_PACKAGE_DPTDB)/$(PYTHON_API_DLL) : $(OBJECT_FILES) $(WRAP_O)
	@mkdir -p $(DPT_DBMS_PACKAGE_DPTDB)
	$(CXX) $(DPT_LDFLAGS) -o $@ $(WRAP_O) $(API_OBJS) $(OBJS) -l$(PYTHON_LIBRARY) -L$(PYTHON_LIBS_DIRECTORY)

clean :
	-rm $(DPT_DBMS_OBJECT)/*.o
	-rm $(DPT_DBMS_STDAFX_OBJECT)/*.o
	-rm $(DPT_DBMS_API_OBJECT)/*.o
	-rm $(DPT_DBMS_SWIG)/*
	-rm $(DPT_DBMS_OBJECT_SWIG_PYTHON)/*.o
#	-rm $(DPT_DBMS_OBJECT_SWIG)/*.o
	-rm -r $(DPT_DBMS_PACKAGE)
	-rm $(WINE_SRC)/*.cpp
	-rm $(WINE_API_SRC)/*.cpp
	-rm $(WINE_INC)/*.h
	-rm $(WINE_API_INC)/*.h

SRCS = $(NAMES:%=$(WINE_SRC)/%.cpp)

API_SRCS = $(API_NAMES:%=$(WINE_API_SRC)/%.cpp)

INCS = $(INC_NAMES:%=$(WINE_INC)/%.h)

API_INCS = $(API_INC_NAMES:%=$(WINE_API_INC)/%.h)

SOURCE_FILES = $(SRCS) $(INCS) $(API_SRCS) $(API_INCS)

distribution : $(SOURCE_FILES) $(DPT_DBMS_PACKAGE_DPTDB)/$(PYTHON_API_DLL)
	$(WINE) python -m build --wheel --no-isolation $(DPT_DBMS_PACKAGE)
	$(WINE) python -m wheel tags --python-tag=$(PYTHON_TAG) --abi-tag=$(ABI_TAG) --platform-tag=$(PLATFORM_TAG) --remove $(DPT_DBMS_PACKAGE)/dist/$(PROJECT_NAME)-$(VERSION_DPT_DBMS_DPTDB)-py3-none-any.whl

local-install : distribution
	-rm $(USER_SITE_PACKAGES)/$(LIBSTDC)
	-rm $(USER_SITE_PACKAGES)/$(LIBGCC_S_SEH)
	$(WINE) python -m pip install --user --no-index --find-links $(DPT_DBMS_PACKAGE)/dist $(PROJECT_NAME)
	ln --symbolic $(OTHER_DLLS_DIRECTORY)/$(LIBSTDC) $(USER_SITE_PACKAGES)/$(LIBSTDC)
	ln --symbolic $(OTHER_DLLS_DIRECTORY)/$(LIBGCC_S_SEH) $(USER_SITE_PACKAGES)/$(LIBGCC_S_SEH)
