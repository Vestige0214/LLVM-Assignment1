# Created by and for Qt Creator. This file was created for editing the project sources only.
# You may attempt to use it for building too, by modifying this file here.

#TARGET = LLVM-As1

HEADERS = \
   $$PWD/include/ProfilingInstrumentationPass.h

SOURCES = \
   $$PWD/lib/callgraph-profiler-inst/ProfilingInstrumentationPass.cpp \
   $$PWD/lib/callgraph-profiler-rt/runtime.cpp \
   $$PWD/test/c/01-internal-call-once.c \
   $$PWD/test/c/02-internal-call-twice.c \
   $$PWD/test/c/03-internal-call-in-loop.c \
   $$PWD/test/c/04-internal-line-clobber.c \
   $$PWD/test/c/05-internal-multiple-files.c \
   $$PWD/test/c/06-external-call-multiple.c \
   $$PWD/test/c/07-function-pointer-one-internal-target.c \
   $$PWD/test/c/08-function-pointer-multiple-internal-targets.c \
   $$PWD/test/c/09-internal-recursion.c \
   $$PWD/tools/callgraph-profiler/main.cpp

INCLUDEPATH = \
    $$PWD/include

#DEFINES = 

