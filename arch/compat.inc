#
# Makefile compiler compatibility checks.
# Define flags for optional compiler flags or settings to keep most
# of the compiler version check jank out of the main Makefile.
#

#
# Assume GCC until proven wrong...
#
COMPILER := ${shell \
 compiler="gcc"; \
 tmp=`${CC} --version`; \
 echo $$tmp | grep -qi "clang version" && compiler="clang"; \
 echo $$tmp | grep -qi "LLVM version" && compiler="clang"; \
 echo $$tmp | grep -qi "emcc" && compiler="clang"; \
 echo $$compiler; }

#
# Get compiler version (GCC).
#
ifeq (${COMPILER},gcc)
GCC_VER := ${shell ${CC} -dumpfullversion -dumpversion |\
 awk -F. '{print $$3+100*($$2+100*$$1)}'}

GCC_VER_GE_4     := ${shell test ${GCC_VER} -ge 40000; echo $$?}
endif

#
# Get compiler version (clang).
#
ifeq (${COMPILER},clang)
CLANG_VER_FLAG = "--version"

#
# Emscripten uses completely different version numbering and the easiest way
# to get the clang version is to run it with verbose sanity checks instead.
# Because of this, stderr needs to be redirected to stdout for this command.
#
ifeq (${CC},emcc)
CLANG_VER_FLAG = "-v"
endif

CLANG_VER := ${shell ${CC} ${CLANG_VER_FLAG} 2>&1 |\
 grep -oi "version.* [0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*" |\
 grep -o "[0-9][0-9]*.*" |\
 awk -F. '{print $$3+100*($$2+100*$$1)}'}

COMPILERXX := ${shell \
 compiler="gcc"; \
 tmp=`${CXX} --version`; \
 echo $$tmp | grep -qi "clang version" && compiler="clang"; \
 echo $$tmp | grep -qi "LLVM version" && compiler="clang"; \
 echo $$tmp | grep -qi "emcc" && compiler="clang"; \
 echo $$compiler; }

ifneq (${COMPILERXX},clang)
$(warning CC is clang but CXX isn't clang++; disabling compiler-specific checks.)
COMPILER := unknown
endif

GCC_VER      := 40201
GCC_VER_GE_4 := 0
endif

#
# Features requiring GCC 4 minimum and compatible with clang.
#
ifeq (${GCC_VER_GE_4},0)
HAS_PEDANTIC ?= 1
HAS_F_VISIBILITY ?= 1
HAS_W_NO_VARIADIC_MACROS ?= 1

GCC_VER_GE_4_1   := ${shell test ${GCC_VER} -ge 40100; echo $$?}
ifeq (${GCC_VER_GE_4_1},0)
HAS_F_STACK_PROTECTOR ?= 1
endif

GCC_VER_GE_4_3   := ${shell test ${GCC_VER} -ge 40300; echo $$?}
ifeq (${GCC_VER_GE_4_3},0)
HAS_W_VLA ?= 1
endif
endif

#
# Features requiring higher versions of GCC.
#
ifeq (${COMPILER},gcc)

GCC_VER_GE_4_6   := ${shell test ${GCC_VER} -ge 40600; echo $$?}
ifeq ($(GCC_VER_GE_4_6),0)
HAS_W_NO_UNUSED_BUT_SET_VARIABLE ?= 1
endif

GCC_VER_GE_7     := ${shell test ${GCC_VER} -ge 70000; echo $$?}
ifeq ($(GCC_VER_GE_7),0)
HAS_W_NO_FORMAT_TRUNCATION ?= 1
HAS_W_NO_IMPLICIT_FALLTHROUGH ?= 1
endif

GCC_VER_GE_10    := ${shell test ${GCC_VER} -ge 100000; echo $$?}
ifeq (${GCC_VER_GE_10},0)
HAS_F_ANALYZER ?= 1
endif
endif

#
# C++11 support.
# C++11 is optional or unused for all core MegaZeux features, but may be
# required in the future for optional features (e.g. the network layer).
#
ifeq (${COMPILER},gcc)
GCC_VER_GE_4_8   := ${shell test ${GCC_VER} -ge 40800; echo $$?}
ifeq (${GCC_VER_GE_4_8},0)
HAS_CXX_11 ?= 1
endif
endif
ifeq (${COMPILER},clang)
CLANG_VER_GE_3_3 := ${shell test "${CLANG_VER}" -ge 30300; echo $$?}
ifeq (${CLANG_VER_GE_3_3},0)
HAS_CXX_11 ?= 1
endif
endif
