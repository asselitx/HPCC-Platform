/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef _DEBUG
 #ifndef _TESTING
  #define _TESTING
 #endif
#endif

// **** Architecture detection ****
// Ref: http://sourceforge.net/p/predef/wiki/Architectures/
// Following GNU, ARMCC, ICC and MSVS macros only

// Identify all possible flags that mean ARM32, even when in Thumb mode
// further separation could be tested via #ifdef __thumb__
#if defined(__arm__) || defined(__thumb__) \
 || defined(_M_ARM) || defined(_M_ARMT) \
 || defined(__TARGET_ARCH_ARM) || defined(__TARGET_ARCH_THUMB)
#define _ARCH_ARM32_
#endif

// ARM64 has only one which all compilers follow
#ifdef __aarch64__
#define _ARCH_ARM64_
#endif

// Identify all possible flags that mean ppc64el
#if defined(__powerpc__) || defined(__powerpc64__) \
 || defined(__ppc__) || defined(__ppc64__) || defined(__ppc64el__)
#define _ARCH_PPC64EL_
#endif

// Identify all possible flags that mean x86_64
#if defined(__amd64__) || defined(__x86_64__) \
 || defined(_M_X64) || defined(_M_AMD64)
 #define _ARCH_X86_64_
#else
 // Identify all possible flags that mean x86
 #if defined(__i386__) || defined(_M_IX86)
  #define _ARCH_X86_
 #endif
#endif

#define CACHE_LINE_SIZE 64

// **** START OF X-PLATFORM SECTION ****

#if defined(_ARCH_X86_64_) || defined(_ARCH_ARM64_) || defined(_ARCH_PPC64EL_) || __WORDSIZE==64
#define __64BIT__
#endif

#ifdef _FILE_OFFSET_BITS
//#error PLATFORM.H must be included first
#endif

#define _FILE_OFFSET_BITS 64
#ifndef _WIN32
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif
#endif
#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <endian.h>
#endif
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 2
#endif
#define __BYTE_ORDER __LITTLE_ENDIAN

typedef unsigned size32_t;

#if (defined (__linux__) || defined (__FreeBSD__)  || defined (__APPLE__) || defined (EMSCRIPTEN))
typedef __SIZE_TYPE__ memsize_t;
#else
typedef size_t memsize_t;
#endif

typedef memsize_t rowsize_t;

#define count_t __int64
#define kcount_t size32_t
#define CF      I64F
#define KCF     "d"

#ifdef _WIN32
#define I64C(n) n##i64
#define U64C(n) n##ui64
#else
#define I64C(n) n##LL
#define U64C(n) n##ULL
#endif

// **** END   OF X-PLATFORM SECTION ****
#if defined(_WIN32)

#define _CRT_SECURE_NO_WARNINGS
#define NO_WARN_MBCS_MFC_DEPRECATION

#if (_MSC_VER>=1300)
#pragma warning(disable:4996)
#endif
// **** START OF WIN32 SPECIFIC SECTION ****

#ifndef ALL_WINDOWS
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef _DEBUG
 #ifndef USING_MPATROL //using mpatrol memory leak tool
  #define _CRTDBG_MAP_ALLOC
 #endif
#endif

#define NOMINMAX
#include <windows.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <winioctl.h>
#include <direct.h>

#if (_MSC_VER>=1300)
#include <string>
#include <fstream>
#endif

//#ifdef USING_SCM_WITH_STL //before #define of "debug" new
#ifdef _MSC_VER
#include <new>
#include <memory>
#define __attribute__(param) /* do nothing */
#endif

#define ThreadId DWORD
#define MutexId HANDLE
#define sleep(X) Sleep(X*1000)
#define I64F          "I64"
#define PATHSEPCHAR '\\'
#define PATHSEPSTR "\\"
#define TEXT_TRANS "t"
#define LLC(NUM) NUM
#define ENVSEPCHAR ';'
#define ENVSEPSTR ";"

#define SEPARATE_LIB_DLL_FILES
#define SharedObjectPrefix         ""
#define SharedObjectExtension      ".dll"
#define LibraryExtension           ".lib"
#define ProcessExtension           ".exe"
#define GetSharedProcedure(h,name) GetProcAddress(h,(char *)name)
#define LoadSucceeded(h)           ((memsize_t)h >= 32)
#define GetSharedObjectError()     GetLastError()
#define GetSharedObjectErrorString() strerror(errno)
#define strtok_r(a,b,c)            j_strtok_r(a,b,c)
#define __builtin_prefetch(addr)   _mm_prefetch((const char *)(addr), _MM_HINT_T0)

#define __thread __declspec(thread)

typedef unsigned __int64 off64_t;
typedef int socklen_t;
#ifdef _WIN64
typedef __int64 ssize_t; // correct return to type for unix read/write/pread etc.
#else
typedef int ssize_t; // correct return to type for unix read/write/pread etc.
#endif
#define fpos_ht fpos_t

typedef long double LDouble;
typedef unsigned long MaxCard;

/* Floating point constants */
#define MaxValidExp         +308
#define MinValidExp         -330
#define MaxRealMant         1.7
#define MinRealMant         2.33

#define MaxRealValue        1.7E+308

#define S_IXUSR    0x40     // Execute by owner
#define S_IWUSR    0x80     // Write by owner
#define S_IRUSR   0x100     // Read by owner
#define S_IXGRP   0x200     // Execute by group
#define S_IWGRP   0x400     // Write by group
#define S_IRGRP   0x800     // Read by group
#define S_IXOTH  0x1000     // Execute by other
#define S_IWOTH  0x2000     // Write by other
#define S_IROTH  0x4000     // Read by other

#define S_IRWXU (S_IXGRP|S_IWGRP|S_IRGRP)
#define S_IRWXG (S_IXGRP|S_IWGRP|S_IRGRP)
#define S_IRWXO (S_IXGRP|S_IWGRP|S_IRGRP)

// MSVC 2008 in debug (perhaps other versions too) will throw exception if negative passed (see bug: #32013)
#if (_MSC_VER>=1400) // >=vs2005

//Defining overloaded versions of these functions is almost certainly undefined behaviour in the c++ standard.
//However it causes fewer problems than the previous fix using macros to redefine the functions.
#undef isalpha
inline bool isalpha(char c) { return isalpha((int)(unsigned char)c); }

#undef isupper
inline bool isupper(char c) { return isupper((int)(unsigned char)(c)); }

#undef islower
inline bool islower(char c) { return islower((int)(unsigned char)(c)); }

#undef isdigit
inline bool isdigit(char c) { return isdigit((int)(unsigned char)(c)); }

#undef isxdigit
inline bool isxdigit(char c) { return isxdigit((int)(unsigned char)(c)); }

#undef isspace
inline bool isspace(char c) { return isspace((int)(unsigned char)(c)); }

#undef ispunct
inline bool ispunct(char c) { return ispunct((int)(unsigned char)(c)); }

#undef isalnum
inline bool isalnum(char c) { return isalnum((int)(unsigned char)(c)); }

#undef isprint
inline bool isprint(char c) { return isprint((int)(unsigned char)(c)); }

#undef isgraph
inline bool isgraph(char c) { return isgraph((int)(unsigned char)(c)); }

#undef iscntrl
inline bool iscntrl(char c) { return iscntrl((int)(unsigned char)(c)); }

#undef tolower
inline int tolower(char c) { return tolower((int)(unsigned char)(c)); }

#undef toupper
inline int toupper(char c) { return toupper((int)(unsigned char)(c)); }

#endif // (_MSC_VER>=1400)

#define likely(x)       (x)
#define unlikely(x)     (x)
inline int daemon(int, int) { return -1; }

// **** END   OF WIN32 SPECIFIC SECTION ****
#else
// **** START OF UNIX GENERAL SECTION ****

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define _stdcall
#define __stdcall
#define _fastcall
#define __fastcall
#define _fastcall
#define __cdecl


#if defined(__linux__) || defined (__FreeBSD__)  || defined (__APPLE__) || defined(EMSCRIPTEN)
// **** START OF LINUX SPECIFIC SECTION ****
#if not defined(EMSCRIPTEN)
  #include <aio.h>
#else
  #include <utime.h>
  #include <sys/types.h> // Include for off_t
  #include <signal.h>    // Include for struct sigevent
  struct aiocb
  {
    int aio_fildes;		/* File descriptor.  */
    int aio_lio_opcode;		/* Operation to be performed.  */
    int aio_reqprio;		/* Request priority offset.  */
    volatile void *aio_buf;	/* Location of buffer.  */
    size_t aio_nbytes;		/* Length of transfer.  */
    struct sigevent aio_sigevent;	/* Signal number and value.  */

    /* Internal members.  */
    struct aiocb *__next_prio;
    int __abs_prio;
    int __policy;
    int __error_code;
    ssize_t __return_value;

  #ifndef __USE_FILE_OFFSET64
    __off_t aio_offset;		/* File offset.  */
    char __pad[sizeof (__off64_t) - sizeof (__off_t)];
  #else
    off64_t aio_offset;		/* File offset.  */
  #endif
    char __glibc_reserved[32];
  };
#endif
#define __BYTE_ORDER __LITTLE_ENDIAN
#define _atoi64     atoll
#define _lseek      lseek
#define _llseek     ::lseek
#define _lseeki64   ::lseek
#define _vsnprintf  vsnprintf
#define _snprintf   snprintf
#define strnicmp    strncasecmp
#define INFINITE    0xFFFFFFFF

#ifdef _LARGEFILE64_SOURCE
// needed by <sys/types.h>
#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64
#endif
#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64
#endif
#endif

// **** END   OF LINUX SPECIFIC SECTION ****
#else
#undef __BYTE_ORDER
#define __BYTE_ORDER __BIG_ENDIAN   // e.g. Solaris
#endif
#if defined(__SVR4)
typedef int socklen_t;
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#define PASCAL
#define __declspec(dllexport)

#define __int32 int
#define __int16 short
#define __int8  char
#define __int64 long long

#ifndef __TIMESTAMP__
#define __TIMESTAMP__ "<__TIMESTAMP__ unsupported>"
#endif
#define ENVSEPCHAR ':'
#define ENVSEPSTR ":"
#define PATHSEPCHAR '/'
#define PATHSEPSTR "/"
#define TEXT_TRANS
#define LLC(NUM) NUM ## LL
#define wsprintf sprintf
#define _lread      _read
#define _lclose     _close
#define _lwrite     _write
#define _lopen      _open
inline bool DeleteFile(const char * name) { return unlink(name)==0; }
#define wvsprintf   vsprintf
#define _close      ::close
#define _stat       stat
#define _fstat      ::fstat
#define _dup        ::dup
#define _chdir          ::chdir
#define _setmode(a,b)

#define TRUE    1
#define FALSE   0
#define HFILE   int
#define UINT    unsigned int
#define DWORD   unsigned long
#define VOID    void
#define LPBYTE  char *
#define LPSTR   char *
#define LPTSTR  char *
#define LPVOID
#define FAR
#define WINAPI
#define fpos_ht off_t
#define handle_t    void *
#define HINSTANCE   void *
#define HANDLE      int
#define HMODULE     void *
#define _MAX_PATH   PATH_MAX
#define HFILE_ERROR     -1
#define OF_READWRITE    O_RDWR, S_IRUSR | S_IWUSR
#define OF_READ     O_RDONLY, S_IRUSR | S_IWUSR
#define _O_RDWR     O_RDWR
#define _O_RDONLY   O_RDONLY
#define _O_CREAT    O_CREAT
#define _O_TRUNC    O_TRUNC
#define _O_APPEND   O_APPEND
#define _O_BINARY   0
#define _O_SEQUENTIAL   0
#define _O_TEXT     0
#define _O_RDONLY   O_RDONLY
#define _O_WRONLY   O_WRONLY
#define _O_RANDOM   0
#define _S_IREAD    S_IRUSR | S_IRGRP | S_IROTH
#define _S_IWRITE   S_IWUSR | S_IWGRP | S_IWOTH
#define _S_IEXEC    S_IXUSR | S_IXGRP | S_IXOTH
#define _S_IFDIR    S_IFDIR

#define FILE_CURRENT    SEEK_CUR
#define FILE_END        SEEK_END
#define FILE_BEGIN      SEEK_SET

#define SOCKET int
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define SOCKADDR_IN struct sockaddr_in
#define SOCKADDR struct sockaddr
#define PSOCKADDR SOCKADDR *
#define closesocket close
#define ioctlsocket ioctl
#define WSAGetLastError()   0

#define GetCurrentDirectory(size, buf) getcwd(buf, size)
#define SetCurrentDirectory(path) chdir(path)
#define CreateDirectory(path,X) (_mkdir(path)==0)
#define GetTempPath(size, buff) strncpy(buff, P_tmpdir, size); // return value not valid
#define GetLastError() errno

#define memicmp j_memicmp

#define I64F          "ll"

#ifndef stricmp
#define stricmp   strcasecmp
#endif

#ifndef __FreeBSD__
#ifndef __APPLE__
#include <malloc.h>
#include <alloca.h>
#endif
#endif
#include <dlfcn.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#if defined(EMSCRIPTEN)
#include <errno.h>
#else
#include <sys/errno.h>
#endif
#include <sys/utsname.h>

#if defined (__FreeBSD__) || defined (__APPLE__)
#define MAP_ANONYMOUS MAP_ANON
#endif
#if defined(__FreeBSD__) || defined(__linux__) || defined(__CYGWIN__) || defined (__APPLE__) || defined(EMSCRIPTEN)
#include <sys/ioctl.h>
#else
#include <sys/filio.h>
#include <stropts.h>
#include <sys/asynch.h>

#define _llseek     ::llseek
#define _lseeki64   ::llseek

#endif

#define _write      ::write
#define _read       ::read
#define _open       ::open
#define _mkdir(P1)  mkdir(P1, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)

#define GetCurrentThreadId pthread_self
#define GetCurrentProcessId getpid
#define _stdcall

// #define  inline

#define SharedObjectPrefix          "lib"
#if defined (__APPLE__)
#define SharedObjectExtension       ".dylib"
#define LibraryExtension            ".dylib"
#else
#define SharedObjectExtension       ".so"
#define LibraryExtension            ".so"
#endif
#define ProcessExtension            ""
#define GetSharedProcedure(h,name)  dlsym(h,(char *)name)
#define LoadSucceeded(h)            (h != NULL)
#define GetSharedObjectError()      errno
#define GetSharedObjectErrorString() dlerror()

#define ThreadId pthread_t
#define MutexId pthread_mutex_t

// **** END   OF UNIX SPECIFIC SECTION ****
#endif

#define FLOAT_SIG_DIGITS    7
#define DOUBLE_SIG_DIGITS   16

#define MAX_DECIMAL_LEADING    32          // Maximum number of leading digits in a decimal field
#define MAX_DECIMAL_PRECISION  32          // Maximum digits in a decimal field
#define MAX_DECIMAL_DIGITS     (MAX_DECIMAL_LEADING+MAX_DECIMAL_PRECISION)
#define MAX_DECIMAL_CHARS      (MAX_DECIMAL_DIGITS+3) // Maximum digits with sign, decimal point, and null terminator

#define strtok(a,b)   j_strtok_deprecated(a,b)  // will disappear at some point

typedef unsigned __int64 hash64_t;
typedef unsigned __int64 __uint64;
typedef __uint64 offset_t;
typedef unsigned char byte;
typedef __uint64 cycle_t;                 // This must be unsigned to avoid integer overflow issues when subtracting
typedef unsigned __int64 timestamp_type;

// BUILD_TAG not needed here anymore - defined in build_tag.h
//#define BUILD_TAG "build_0000" // Will get substituted during pre-build

#ifdef _WINDOWS
#ifndef popen
#define popen   _popen
#endif
#ifndef pclose
#define pclose  _pclose
#endif
#endif

//Use the following DECL_XXX to annotate items that are exported from dlls, or objects that are thrown:
//DECL_EXPORT for items that are exported from the dll containing the current file being compiled
//DECL_IMPORT for items that are imported from another dll
//DECL_LOCAL is not generally required but could be used if there was an unexported class in a header file
//DECL_EXCEPTION for any non exported/imported objects which are thrown (to avoid problems with RTTI)
#ifdef _MSC_VER
 #define DECL_EXPORT __declspec(dllexport)
 #define DECL_IMPORT __declspec(dllimport)
 #define DECL_LOCAL
 #define DECL_EXCEPTION
#elif __GNUC__ >= 4
 #define DECL_EXPORT __attribute__ ((visibility ("default")))
 #define DECL_IMPORT __attribute__ ((visibility ("default")))
 #define DECL_LOCAL  __attribute__ ((visibility ("hidden")))
 #define DECL_EXCEPTION DECL_EXPORT
#else
 #define DECL_EXPORT
 #define DECL_IMPORT
 #define DECL_LOCAL
 #define DECL_EXCEPTION
#endif

//Sanitize support is different on every compiler
//use these macros before a function definition to disable a particular sanitize option for the body of that function

#ifdef __clang__
  #define NO_SANITIZE(a) [[clang::no_sanitize(a)]]
  #define NO_SANITIZE_FUNCTION [[clang::no_sanitize("function")]]
#elif __GNUC__ >= 8
 #define NO_SANITIZE(a) [[gnu::no_sanitize(a)]]
 #define NO_SANITIZE_FUNCTION [[gnu::no_sanitize("undefined")]]
#else
 #define NO_SANITIZE(a)
 #define NO_SANITIZE_FUNCTION
#endif

//Versions of memcpy etc which are safe to use with null parameters if the size is 0
inline void * memcpy_iflen(void * dest, const void * src, size_t n)   { return (likely(n)) ? memcpy(dest, src, n) : dest; }
inline void * memmove_iflen(void * dest, const void * src, size_t n)  { return (likely(n)) ? memmove(dest, src, n) : dest; }
inline void * memset_iflen(void * dest, int c, size_t n)              { return (likely(n)) ? memset(dest, c, n) : dest; }
inline int memcmp_iflen(const void * l, const void * r, size_t n)     { return likely(n) ? memcmp(l, r, n) : 0; }

#endif
