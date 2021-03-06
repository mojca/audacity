/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2015 Audacity Team.
   License: GPL v2.  See License.txt.

   Audacity.h

   Dominic Mazzoni
   Joshua Haberman
   Vaughan Johnson
   et alii

********************************************************************//*!

\file Audacity.h

  This is the main include file for Audacity.  All files which need
  any Audacity-specific \#defines or need to access any of Audacity's
  global functions should \#include this file.

*//********************************************************************/

#ifndef __AUDACITY_H__
#define __AUDACITY_H__

// We only do alpha builds and release versions.
// Most of the time we're in development, so IS_ALPHA should be defined
// to 1.
#define IS_ALPHA 1

// Increment as appropriate every time we release a NEW version.
#define AUDACITY_VERSION   2
#define AUDACITY_RELEASE   1
#define AUDACITY_REVISION  3
#define AUDACITY_MODLEVEL  0

#if IS_ALPHA
   #define AUDACITY_SUFFIX wxT("-alpha-") __TDATE__
#else
   #define AUDACITY_SUFFIX    wxT("") // for a stable release
#endif

#define AUDACITY_MAKESTR( x ) #x
#define AUDACITY_QUOTE( x ) AUDACITY_MAKESTR( x )

// Version string for visual display
#define AUDACITY_VERSION_STRING wxT( AUDACITY_QUOTE( AUDACITY_VERSION ) ) wxT(".") \
                                wxT( AUDACITY_QUOTE( AUDACITY_RELEASE ) ) wxT(".") \
                                wxT( AUDACITY_QUOTE( AUDACITY_REVISION ) ) \
                                AUDACITY_SUFFIX

// Version string for file info (under Windows)
#define AUDACITY_FILE_VERSION AUDACITY_QUOTE( AUDACITY_VERSION ) "," \
                              AUDACITY_QUOTE( AUDACITY_RELEASE ) "," \
                              AUDACITY_QUOTE( AUDACITY_REVISION ) "," \
                              AUDACITY_QUOTE( AUDACITY_MODLEVEL )

// Increment this every time the prefs need to be reset
// the first part (before the r) indicates the version the reset took place
// the second part (after the r) indicates the number of times the prefs have been reset within the same version
#define AUDACITY_PREFS_VERSION_STRING "1.1.1r1"

// Don't change this unless the file format changes
// in an irrevocable way
#define AUDACITY_FILE_FORMAT_VERSION "1.3.0"

class wxWindow;

void QuitAudacity(bool bForce);
void QuitAudacity();

// Please try to support unlimited path length instead of using PLATFORM_MAX_PATH!
// Define one constant for maximum path value, so we don't have to do
// platform-specific conditionals everywhere we want to check it.
#define PLATFORM_MAX_PATH 260 // Play it safe for default, with same value as Windows' MAX_PATH.

#ifdef __WXMAC__
#include "configmac.h"
#undef PLATFORM_MAX_PATH
#define PLATFORM_MAX_PATH PATH_MAX
#endif

#ifdef __WXGTK__
#include "configunix.h"
// Some systems do not restrict the path length and therefore PATH_MAX is undefined
#ifdef PATH_MAX
#undef PLATFORM_MAX_PATH
#define PLATFORM_MAX_PATH PATH_MAX
#endif
#endif

#ifdef __WXX11__
#include "configunix.h"
// wxX11 should also get the platform-specific definition of PLATFORM_MAX_PATH, so do not declare here.
#endif

#ifdef __WXMSW__
#include "configwin.h"
#undef PLATFORM_MAX_PATH
#define PLATFORM_MAX_PATH MAX_PATH
#endif

/* Magic for dynamic library import and export. This is unfortunately
 * compiler-specific because there isn't a standard way to do it. Currently it
 * works with the Visual Studio compiler for windows, and for GCC 4+. Anything
 * else gets all symbols made public, which gets messy */
/* The Visual Studio implementation */
#ifdef _MSC_VER
   #ifndef AUDACITY_DLL_API
      #ifdef BUILDING_AUDACITY
         #define AUDACITY_DLL_API _declspec(dllexport)
      #else
         #ifdef _DLL
            #define AUDACITY_DLL_API _declspec(dllimport)
         #else
            #define AUDACITY_DLL_API
         #endif
      #endif
   #endif
#endif //_MSC_VER

// Put extra symbol information in the release build, for the purpose of gathering
// profiling information (as from Windows Process Monitor), when there otherwise
// isn't a need for AUDACITY_DLL_API.
#if IS_ALPHA
   #define PROFILE_DLL_API AUDACITY_DLL_API
#else
   #define PROFILE_DLL_API
#endif

/* The GCC-elf implementation */
#ifdef HAVE_VISIBILITY // this is provided by the configure script, is only
// enabled for suitable GCC versions
/* The incantation is a bit weird here because it uses ELF symbol stuff. If we
 * make a symbol "default" it makes it visible (for import or export). Making it
 * "hidden" means it is invisible outside the shared object. */
   #ifndef AUDACITY_DLL_API
      #ifdef BUILDING_AUDACITY
         #define AUDACITY_DLL_API __attribute__((visibility("default")))
      #else
         #define AUDACITY_DLL_API __attribute__((visibility("default")))
      #endif
   #endif
#endif

/* The GCC-win32 implementation */
// bizzarely, GCC-for-win32 supports Visual Studio style symbol visibility, so
// we use that if building on Cygwin
#if defined __CYGWIN__ && defined __GNUC__
   #ifndef AUDACITY_DLL_API
      #ifdef BUILDING_AUDACITY
         #define AUDACITY_DLL_API _declspec(dllexport)
      #else
         #ifdef _DLL
            #define AUDACITY_DLL_API _declspec(dllimport)
         #else
            #define AUDACITY_DLL_API
         #endif
      #endif
   #endif
#endif

// These macros are used widely, so declared here.
#define QUANTIZED_TIME(time, rate) (((double)((sampleCount)floor(((double)(time) * (rate)) + 0.5))) / (rate))
// dB - linear amplitude convesions
#define DB_TO_LINEAR(x) (pow(10.0, (x) / 20.0))
#define LINEAR_TO_DB(x) (20.0 * log10(x))

// Marks strings for extraction only...must use wxGetTranslation() to translate.
#define XO(s) wxT(s)

#include <memory>
#include <utility>

// This renames a good use of this C++ keyword that we don't need to review when hunting for leaks.
#define PROHIBITED = delete

// Reviewed, certified, non-leaky uses of NEW that immediately entrust their results to RAII objects.
// You may use it in NEW code when constructing a wxWindow subclass with non-NULL parent window.
// You may use it in NEW code when the NEW expression is the constructor argument for a standard smart
// pointer like std::unique_ptr or std::shared_ptr.
#define safenew new

#if !defined(__WXMSW__)
/* replicate the very useful C++14 make_unique for those build environments
   that don't implement it yet.

   typical useage:

   auto p = std::make_unique<Myclass>(ctorArg1, ctorArg2, ... ctorArgN);
   p->DoSomething();
   auto q = std::make_unique<Myclass[]>(count);
   q[0].DoSomethingElse();

   The first hides naked NEW and DELETE from the source code.
   The second hides NEW[] and DELETE[].  Both of course ensure destruction if
   you don't use something like std::move(p) or q.release().  Both expressions require
   that you identify the type only once, which is brief and less error prone.

   (Whereas this omission of [] might invite a runtime error:
   std::unique_ptr<Myclass> q { new Myclass[count] }; )

   Some C++11 tricks needed here are (1) variadic argument lists and
   (2) making the compile-time dispatch work correctly.  You can't have
   a partially specialized template function, but you get the effect of that
   by other metaprogramming means.
*/

namespace std {
   // For overloading resolution
   template <typename X> struct __make_unique_result {
      using scalar_case = unique_ptr<X>;
   };

   // Partial specialization of the struct for array case
   template <typename X> struct __make_unique_result<X[]> {
      using array_case = unique_ptr<X[]>;
      using element = X;
   };

   // Now the scalar version of unique_ptr
   template<typename X, typename... Args> inline
      typename __make_unique_result<X>::scalar_case
      make_unique(Args&&... args)
   {
      return typename __make_unique_result<X>::scalar_case
         { safenew X( forward<Args>(args)... ) };
   }

   // Now the array version of unique_ptr
   // The compile-time dispatch trick is that the non-existence
   // of the scalar_case type makes the above overload
   // unavailable when the template parameter is explicit
   template<typename X> inline
      typename __make_unique_result<X>::array_case
      make_unique(size_t count)
   {
      return typename __make_unique_result<X>::array_case
         { safenew typename __make_unique_result<X>::element[count] };
   }
}
#endif

/*
* template class Maybe<X>
* Can be used for monomorphic objects that are stack-allocable, but only conditionally constructed.
* You might also use it as a member.
* Initialize with create(), then use like a smart pointer,
* with *, ->, get(), reset(), or in if()
*/

template<typename X>
class Maybe {
public:

   // Construct as NULL
   Maybe() {}

   // Supply the copy and move, so you might use this as a class member too
   Maybe(const Maybe &that)
   {
      if (that.get())
         create(*that);
   }

   Maybe& operator= (const Maybe &that)
   {
      if (this != &that) {
         if (that.get())
            create(*that);
         else
            reset();
      }
      return *this;
   }

   Maybe(Maybe &&that)
   {
      if (that.get())
         create(::std::move(*that));
   }

   Maybe& operator= (Maybe &&that)
   {
      if (this != &that) {
         if (that.get())
            create(::std::move(*that));
         else
            reset();
      }
      return *this;
   }

   // Make an object in the buffer, passing constructor arguments,
   // but destroying any previous object first
   // Note that if constructor throws, we remain in a consistent
   // NULL state -- giving exception safety but only weakly
   // (previous value was lost if present)
   template<typename... Args>
   void create(Args... args)
   {
      // Lose any old value
      reset();
      // Create new value
      pp = safenew(address()) X( std::forward<Args>(args)... );
   }

   // Destroy any object that was built in it
   ~Maybe()
   {
      reset();
   }

   // Pointer-like operators

   // Dereference, with the usual bad consequences if NULL
   X &operator* () const
   {
      return *pp;
   }

   X *operator-> () const
   {
      return pp;
   }

   X* get() const
   {
      return pp;
   }

   void reset()
   {
      if (pp)
         pp->~X(), pp = nullptr;
   }

   // So you can say if(ptr)
   explicit operator bool() const
   {
      return pp != nullptr;
   }

private:
   X* address()
   {
      return reinterpret_cast<X*>(&storage);
   }

   // Data
   typename ::std::aligned_storage<
      sizeof(X)
      // , alignof(X) // Not here yet in all compilers
   >::type storage{};
   X* pp{ nullptr };
};

#endif // __AUDACITY_H__
