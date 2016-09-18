//--------------------------------------------------------------
// Copyright © 2015 Rimmer Motzheim - All rights reserved
//--------------------------------------------------------------

#ifndef Tritium_Core_Targets_h
#define Tritium_Core_Targets_h

/*
 Defined targets in this file:
 __WINDOWS__ : compiling on any desktop Windows OS.
 __OSX__ : compiling on Mac OSX
 __IOS__ : compiling on iOS.
 __LINUX__ : compiling on any linux platform.
 __POSIX__ : compiling on any (partly) posix-compatible platform (Linux, OSX, etc).
 __X64__ : compiling in the 64-bit-version of an architecture.
 __ARM__ : compiling for the ARM architecture.
 __X86__ : compiling for the x86 architecture.
 */



// Windows platforms
#ifdef _WIN32
#define __WINDOWS__ 1

//Define this to disable Windows XP support.
//Doing so allows us to use some OS features only available in Vista and above.
#define NO_XP_SUPPORT 1

//Define this to completely disable the use of the standard library,
//allowing compilation with -nostdlib.
//#define USE_NO_STDLIB 1

#endif // _WIN32



//Darwin platforms
#ifdef __APPLE__

#include "TargetConditionals.h"

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#define __IOS__ 1
#else
#define __OSX__ 1
#endif

#endif // __APPLE__



//Linux platforms
#ifdef __linux__
#define __LINUX__ 1
#endif


#if defined (__APPLE__) || defined (__LINUX__)
#define __POSIX__ 1
#endif


//Check if we are on ARM
#if defined(__arm__) || defined(_M_ARM)
#define __ARM__ 1
#endif // __arm__ || _M_ARM

#ifdef __aarch64__
#define __ARM__ 1
#define __X64__ 1
#endif


//Check if we are on x86.
#if defined(__i386__) || defined(_M_IX86)
#define __X86__ 1
#endif

//Check if we are on x86-64.
#if defined(__x86_64__) || defined(_M_X64)
#define __X86__ 1
#define __X64__ 1
#endif

#endif // Tritium_Core_Targets_h