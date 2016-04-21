/************************************************************************************

Filename    :   OVR_Allocator.h
Content     :   Installable memory allocator
Created     :   September 19, 2012
Notes       : 

Copyright   :   Copyright 2014-2016 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.3 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Allocator_h
#define OVR_Allocator_h

#include "OVR_Types.h"
#include "OVR_Atomic.h"
#include "OVR_Std.h"
#include "stdlib.h"
#include "stdint.h"
#include <string.h>
#include <exception>


//-----------------------------------------------------------------------------------

// ***** Disable template-unfriendly MS VC++ warnings
#if defined(OVR_CC_MSVC)
#pragma warning(push)
// Pragma to prevent long name warnings in in VC++
#pragma warning(disable : 4503)
#pragma warning(disable : 4786)
// In MSVC 7.1, warning about placement new POD default initializer
#pragma warning(disable : 4345)
#endif

// Un-define new so that placement constructors work
#undef new


//-----------------------------------------------------------------------------------
// ***** Placement new overrides

// Calls constructor on own memory created with "new(ptr) type"
#ifndef __PLACEMENT_NEW_INLINE
#define __PLACEMENT_NEW_INLINE

#   if defined(OVR_CC_MWERKS) || defined(OVR_CC_BORLAND) || defined(OVR_CC_GNU)
#      include <new>
#   else
    // Useful on MSVC
    OVR_FORCE_INLINE void* operator new     (size_t n, void *ptr) { OVR_UNUSED(n); return ptr; }
    OVR_FORCE_INLINE void  operator delete  (void *, void *)     { }
#   endif

#endif // __PLACEMENT_NEW_INLINE



//------------------------------------------------------------------------
// ***** Macros to redefine class new/delete operators
//
// Types specifically declared to allow disambiguation of address in
// class member operator new. This is intended to be used with a
// macro like OVR_CHECK_DELETE(class_name, p) in the example below.
//
// Example usage:
//    class Widget
//    {
//    public:
//        Widget();
//
//        #ifdef OVR_BUILD_DEBUG
//            #define OVR_MEMORY_CHECK_DELETE(class_name, p)   \
//                do { if (p) checkInvalidDelete((class_name*)p); } while(0)
//        #else
//            #define OVR_MEMORY_CHECK_DELETE(class_name, p)
//        #endif
//
//        OVR_MEMORY_REDEFINE_NEW_IMPL(Widget, OVR_MEMORY_CHECK_DELETE)
//    };
//

#define OVR_MEMORY_REDEFINE_NEW_IMPL(class_name, check_delete)  \
    void* operator new(size_t sz)                               \
    {                                                           \
        void* p = OVR_ALLOC_DEBUG(sz, __FILE__, __LINE__);      \
        if (!p)                                                 \
            throw OVR::bad_alloc();                             \
        return p;                                               \
    }                                                           \
                                                                \
    void* operator new(size_t sz, const char* file, int line)   \
    {                                                           \
        OVR_UNUSED2(file, line);                                \
        void* p = OVR_ALLOC_DEBUG(sz, file, line);              \
        if (!p)                                                 \
            throw OVR::bad_alloc();                             \
        return p;                                               \
    }                                                           \
                                                                \
    void operator delete(void* p)                               \
    {                                                           \
        if (p)                                                  \
        {                                                       \
            check_delete(class_name, p);                        \
            OVR_FREE(p);                                        \
        }                                                       \
    }                                                           \
                                                                \
    void operator delete(void* p, const char*, int)             \
    {                                                           \
        if (p)                                                  \
        {                                                       \
            check_delete(class_name, p);                        \
            OVR_FREE(p);                                        \
        }                                                       \
    }


// Used by OVR_MEMORY_REDEFINE_NEW
#define OVR_MEMORY_CHECK_DELETE_NONE(class_name, p)

// Redefine all delete/new operators in a class without custom memory initialization.
#define OVR_MEMORY_REDEFINE_NEW(class_name) \
    OVR_MEMORY_REDEFINE_NEW_IMPL(class_name, OVR_MEMORY_CHECK_DELETE_NONE)


namespace OVR {


// We subclass std::bad_alloc for the purpose of overriding the 'what' function
// to provide additional information about the exception, such as context about
// how or where the exception occurred in our code. We subclass std::bad_alloc
// instead of creating a new type because it's intended to override std::bad_alloc
// and be caught by code that uses catch(std::bad_alloc&){}. Also, the std::bad_alloc
// constructor actually attempts to allocate memory!

struct bad_alloc : public std::bad_alloc
{
    bad_alloc(const char* description = "OVR::bad_alloc") OVR_NOEXCEPT;

    bad_alloc(const bad_alloc& oba) OVR_NOEXCEPT
    {
        OVR_strlcpy(Description, oba.Description, sizeof(Description));
    }

    bad_alloc& operator=(const bad_alloc& oba) OVR_NOEXCEPT
    {
        OVR_strlcpy(Description, oba.Description, sizeof(Description));
        return *this;
    }

    virtual const char* what() const OVR_NOEXCEPT
    {
        return Description;
    }

    char Description[256]; // Fixed size because we cannot allocate memory.
};



//-----------------------------------------------------------------------------------
// ***** Construct / Destruct

// Construct/Destruct functions are useful when new is redefined, as they can
// be called instead of placement new constructors.


template <class T>
OVR_FORCE_INLINE T*  Construct(void *p)
{
    return ::new(p) T();
}

template <class T>
OVR_FORCE_INLINE T*  Construct(void *p, const T& source)
{
    return ::new(p) T(source);
}

// Same as above, but allows for a different type of constructor.
template <class T, class S>
OVR_FORCE_INLINE T*  ConstructAlt(void *p, const S& source)
{
    return ::new(p) T(source);
}

template <class T, class S1, class S2>
OVR_FORCE_INLINE T*  ConstructAlt(void *p, const S1& src1, const S2& src2)
{
    return ::new(p) T(src1, src2);
}

// Note: These ConstructArray functions don't properly support the case of a C++ exception occurring midway 
// during construction, as they don't deconstruct the successfully constructed array elements before returning.
template <class T>
OVR_FORCE_INLINE void ConstructArray(void *p, size_t count)
{
    uint8_t *pdata = (uint8_t*)p;
    for (size_t i=0; i< count; ++i, pdata += sizeof(T))
    {
        Construct<T>(pdata);
    }
}

template <class T>
OVR_FORCE_INLINE void ConstructArray(void *p, size_t count, const T& source)
{
    uint8_t *pdata = (uint8_t*)p;
    for (size_t i=0; i< count; ++i, pdata += sizeof(T))
    {
        Construct<T>(pdata, source);
    }
}

template <class T>
OVR_FORCE_INLINE void Destruct(T *pobj)
{
    pobj->~T();
    OVR_UNUSED1(pobj); // Fix incorrect 'unused variable' MSVC warning.
}

template <class T>
OVR_FORCE_INLINE void DestructArray(T *pobj, size_t count)
{   
    for (size_t i=0; i<count; ++i, ++pobj)
        pobj->~T();
}


//-----------------------------------------------------------------------------------
// ***** Allocator

// Allocator defines a memory allocation interface that developers can override
// to to provide memory for OVR; an instance of this class is typically created on
// application startup and passed into System or OVR::System constructor.
// 
//
// Users implementing this interface must provide three functions: Alloc, Free,
// and Realloc. Implementations of these functions must honor the requested alignment.
// Although arbitrary alignment requests are possible, requested alignment will
// typically be small, such as 16 bytes or less.

class Allocator
{
    friend class System;

public:
    virtual ~Allocator()
    {
    }

    // Returns the pointer to the current globally installed Allocator instance.
    // This pointer is used for most of the memory allocations.
    static Allocator* GetInstance();


    // *** Standard Alignment Alloc/Free

    // Allocate memory of specified size with default alignment.
    // Alloc of size==0 will allocate a tiny block & return a valid pointer;
    // this makes it suitable for new operator.
    virtual void*   Alloc(size_t size) = 0;
    
    // Same as Alloc, but provides an option of passing debug data.
    virtual void*   AllocDebug(size_t size, const char* /*file*/, unsigned /*line*/)
    { return Alloc(size); }

    // Reallocate memory block to a new size, copying data if necessary. Returns the pointer to
    // new memory block, which may be the same as original pointer. Will return 0 if reallocation
    // failed, in which case previous memory is still valid.
    // Realloc to decrease size will never fail.
    // Realloc of pointer == 0 is equivalent to Alloc
    // Realloc to size == 0, shrinks to the minimal size, pointer remains valid and requires Free().
    virtual void*   Realloc(void* p, size_t newSize) = 0;

    // Frees memory allocated by Alloc/Realloc.
    // Free of null pointer is valid and will do nothing.
    virtual void    Free(void *p) = 0;


    // *** Standard Alignment Alloc/Free

    // Allocate memory of specified alignment.
    // Memory allocated with AllocAligned MUST be freed with FreeAligned.
    // Default implementation will delegate to Alloc/Free after doing rounding.
    virtual void*   AllocAligned(size_t size, size_t align);    
    // Frees memory allocated with AllocAligned.
    virtual void    FreeAligned(void* p);

protected:
    // *** Tracking of allocations w/ callstacks for debug builds.

    // Add the allocation & the callstack to the tracking database
    void            trackAlloc(void* p, size_t size);
    // Remove the allocation from the tracking database
    void            untrackAlloc(void* p);

    // Lock used during LibOVR execution to guard the tracked allocation list.
    Lock TrackLock;

protected:
    Allocator() {}

public:
    //------------------------------------------------------------------------
    // ***** DumpMemory

    // Enable/disable leak tracking mode and check if currently tracking.
    static void     SetLeakTracking(bool enabled);
    static bool     IsTrackingLeaks();

    // Displays information about outstanding allocations, typically for the 
    // purpose of reporting leaked memory on application or module shutdown.
    // This should be used instead of, for example, VC++ _CrtDumpMemoryLeaks 
    // because it allows us to dump additional information about our allocations.
    // Returns the number of currently outstanding heap allocations.
    static int      DumpMemory();
};


//------------------------------------------------------------------------
// ***** DefaultAllocator

// This allocator is created and used if no other allocator is installed.
// Default allocator delegates to system malloc.

class DefaultAllocator : public Allocator
{
public:
    virtual void*   Alloc(size_t size);
    virtual void*   AllocDebug(size_t size, const char* file, unsigned line);
    virtual void*   Realloc(void* p, size_t newSize);
    virtual void    Free(void *p);
};


//------------------------------------------------------------------------
// ***** DebugPageAllocator
// 
// Implements a page-protected allocator:
//   Detects use-after-free and memory overrun bugs immediately at the time of usage via an exception.
//   Can detect a memory read or write beyond the valid memory immediately at the 
//       time of usage via an exception (if EnableOverrunDetection is enabled). 
//       This doesn't replace valgrind but implements a subset of its functionality
//       in a way that performs well enough to avoid interfering with app execution.
//   The point of this is that immediately detects these two classes of errors while
//       being much faster than external tools such as valgrind, etc. This is at a cost of 
//       as much as a page of extra bytes per allocation (two if EnableOverrunDetection is enabled).
//   On Windows the Alloc and Free functions average about 12000 cycles each. This isn't small but
//       it should be low enough for many testing circumstances with apps that are prudent with
//       memory allocation volume.
//   The amount of system memory needed for this isn't as high as one might initially guess, as it 
//       takes hundreds of thousands of memory allocations in order to make a dent in the gigabytes of
//       memory most computers have.
//
//   
// Technical design for the Windows platform:
//   Every Alloc is satisfied via a VirtualAlloc return of a memory block of one or more pages; 
//       the minimum needed to satisy the user's size and alignment requirements.
//   Upon Free the memory block (which is one or more pages) is not passed to VirtualFree but rather 
//       is converted to PAGE_NOACCESS and put into a delayed free list (FreedBlockArray) to be passed 
//       to VirtualFree later. The result of this is that any further attempts to read or write the 
//       memory will result in an exception.
//   The delayed-free list increases each time Free is called until it reached maximum capacity,
//       at which point the oldest memory block in the list is passed to VirtualFree and its
//       entry in the list is filled with this newly Freed (PAGE_NOACCESS) memory block. 
//   Once the delayed-free list reaches maximum capacity it thus acts as a ring buffer of blocks.
//       The maximum size of this list is currently determined at compile time as a constant.
//   The EnableOverrunDetection is an additional feature which allows reads or writes beyond valid
//       memory to be detected as they occur. This is implemented by adding an allocating an additional
//       page of memory at the end of the usual pages and leaving it uncommitted (MEM_RESERVE).
//       When this option is used, we return a pointer to the user that's at the end of the valid
//       memory block as opposed to at the beginning. This is so that the space right after the 
//       user space is invalid. If there are some odd bytes remaining between the end of the user's
//       space and the page (due to alignment requirements), we optionally fill these with guard bytes.
//       We do not currently support memory underrun detection, which could be implemented via an
//       extra un-accessible page before the user page(s). In practice this is rarely needed.
//   Currently the choice to use EnableOverrunDetection must be done before any calls to Alloc, etc.
//       as the logic is simpler and faster if we don't have to dynamically handle either case at runtime.
//   We store within the memory block the size of the block and the size of the original user Alloc
//       request. This is done as two size_t values written before the memory returned to the user.
//       Thus the pointer returned to the user will never be at the very beginning of the memory block,
//       because there will be two size_t's before it.
//   This class itself allocates no memory, as that could interfere with its ability to supply
//       memory, especially if the global malloc and new functions are replaced with this class.
//       We could in fact support this class allocating memory as long as it used a system allocator
//       and not malloc, new, etc. 
//   As of this writing we don't do debug fill patterns in the returned memory, because we mostly
//       don't need it because memory exceptions take the place of unexpected fill value validation.
//       However, there is some value in doing a small debug fill of the last few bytes after the
//       user's bytes but before the next page, which will happen for odd sizes passed to Alloc.
//
// Technical design for Mac and Linux platforms:
//   Apple's XCode malloc functionality includes something called MallocGuardEdges which is similar
//       to DebugPageAllocator, though it protects only larger sized allocations and not smaller ones.
//   Our approach for this on Mac and Linux is to use mmap and mprotect in a way similar to VirtualAlloc and 
//       VirtualProtect. Unix doesn't have the concept of Windows MEM_RESERVE vs. MEM_COMMIT, but we can 
//       simulate MEM_RESERVE by having an extra page that's PROT_NONE instead of MEM_RESERVE. Since Unix  
//       platforms don't commit pages pages to physical memory until they are first accessed, this extra 
//       page will in practice act similar to Windows MEM_RESERVE at runtime.
//
// Allocation inteface:
//   Alloc sizes can be any size_t >= 0.
//   An alloc size of 0 returns a non-nullptr.
//   Alloc functions may fail (usually due to insufficent memory), in which case they return nullptr.
//   All returned allocations are aligned on a power-of-two boundary of at least DebugPageAllocator::DefaultAlignment.
//   AllocAligned supports any alignment power-of-two value from 1 to 256. Other values result in undefined behavior.
//   AllocAligned may return a pointer that's aligned greater than the requested alignment.
//   Realloc acts as per the C99 Standard realloc.
//   Free requires the supplied pointer to be a valid pointer returned by this allocator's Alloc functions, else the behavior is undefined.
//   You may not Free a pointer a second time, else the behavior is undefined.
//   Free otherwise always succeeds.
//   Allocations made with AllocAligned or ReallocAligned must be Freed via FreeAligned, as per the base class requirement.
//

class DebugPageAllocator : public Allocator
{
public:
    DebugPageAllocator();
    virtual ~DebugPageAllocator();

    void  Init();
    void  Shutdown();

    void   SetMaxDelayedFreeCount(size_t delayedFreeCount); // Sets how many freed blocks we should save before purging the oldest of them.
    size_t GetMaxDelayedFreeCount() const;                  // Returns the max number of delayed free allocations before the oldest ones are purged (finally freed).
    void   EnableOverrunDetection(bool enableOverrunDetection, bool enableOverrunGuardBytes);  // enableOverrunDetection is by default. enableOverrunGuardBytes is enabled by default in debug builds.

    void*  Alloc(size_t size);
    void*  AllocAligned(size_t size, size_t align);
    void*  Realloc(void* p, size_t newSize);
    void*  ReallocAligned(void* p, size_t newSize, size_t newAlign);
    void   Free(void* p);
    void   FreeAligned(void* p);
    size_t GetAllocSize(const void* p) const { return GetUserSize(p); }
    size_t GetPageSize() const { return PageSize; }

protected:
    struct Block
    {
        void*  BlockPtr;    // The pointer to the first page of the contiguous set of pages that make up this block.
        size_t BlockSize;   // (page size) * (page count). Will be >= (SizeStorageSize + UserSize).

        void Clear() { BlockPtr = nullptr; BlockSize = 0; }
    };

    Block*    FreedBlockArray;                            // Currently a very simple array-like container that acts as a ring buffer of delay-freed (but inaccessible) blocks. 
    size_t    FreedBlockArrayMaxSize;                     // The max number of Freed blocks to put into FreedBlockArray before they start getting purged. Must be <= kFreedBlockArrayCapacity.
    size_t    FreedBlockArraySize;                        // The amount of valid elements within FreedBlockArray. Increases as elements are added until it reaches kFreedBlockArrayCapacity. Then stays that way until Shutdown.
    size_t    FreedBlockArrayOldest;                      // The oldest entry in the FreedBlockArray ring buffer.
    size_t    AllocationCount;                            // Number of currently live Allocations. Incremented by successful calls to Alloc (etc.)  Decremented by successful calss to Free.
    bool      OverrunPageEnabled;                         // If true then we implement memory overrun detection, at the cost of an extra page per user allocation.
    bool      OverrunGuardBytesEnabled;                   // If true then any remaining bytes between the end of the user's allocation and the end of the page are filled with guard bytes and verified upon Free. Valid only if OverrunPageEnabled is true. 
    size_t    PageSize;                                   // The current default platform memory page size (e.g. 4096). We allocated blocks in multiples of pages.
    OVR::Lock Lock;                                       // Mutex which allows an instance of this class to be used by multiple threads simultaneously.

public:
    #if defined(_WIN64) || defined(_M_IA64) || defined(__LP64__) || defined(__LP64__) || defined(__arch64__) || defined(__APPLE__)
    static const size_t  DefaultAlignment = 16;               // 64 bit platforms and all Apple platforms.
    #else
    static const size_t  DefaultAlignment = 8;                // 32 bit platforms. We want DefaultAlignment as low as possible because that means less unused bytes between a user allocation and the end of the page.
    #endif
    #if defined(_WIN32)
    static const size_t  MaxAlignment = 2048;                 // Half a page size.
    #else
    static const size_t  MaxAlignment = DefaultAlignment;     // Currently a low limit because we don't have full page allocator support yet.
    #endif

protected:
    static const size_t  SizeStorageSize = DefaultAlignment;  // Where the user size and block size is stored. Needs to be at least 2 * sizeof(size_t).
    static const size_t  UserSizeIndex   = 0;                 // We store block sizes within the memory itself, and this serves to identify it.
    static const size_t  BlockSizeIndex  = 1;
    static const uint8_t GuardFillByte   = 0xfd;              // Same value VC++ uses for heap guard bytes.

    static size_t  GetUserSize(const void* p);                // Returns the size that the user requested in Alloc, etc.
    static size_t  GetBlockSize(const void* p);               // Returns the actual number of bytes in the returned block. Will be a multiple of PageSize.
    static size_t* GetSizePosition(const void* p);            // We store the user and block size as two size_t values within the returned memory to the user, before the user pointer. This gets that location.

    void* GetBlockPtr(void* p);
    void* GetUserPosition(void* pPageMemory, size_t blockSize, size_t userSize, size_t userAlignment);
    void* AllocCommittedPageMemory(size_t blockSize);
    void* EnablePageMemory(void* pPageMemory, size_t blockSize);
    void  DisablePageMemory(void* pPageMemory, size_t blockSize);
    void  FreePageMemory(void* pPageMemory, size_t blockSize);
};




///------------------------------------------------------------------------
/// ***** OVR_malloca / OVR_freea
///
/// Implements a safer version of alloca. However, see notes below.
///
/// Allocates memory from the stack via alloca (or similar) for smaller 
/// allocation sizes, else falls back to operator new. This is very similar 
/// to the Microsoft _malloca and _freea functions, and the implementation 
/// is nearly the same aside from using operator new instead of malloc.
///
/// Unlike alloca, calls to OVR_malloca must be matched by calls to OVR_freea,
/// and the OVR_freea call must be in the same function scope as the original
/// call to OVR_malloca. 
///
/// Note:
/// While this function reduces the likelihood of a stack overflow exception,
/// it cannot guarantee it, as even small allocation sizes done by alloca
/// can exhaust the stack when it is nearly full. However, the majority of 
/// stack overflows due to alloca usage are due to large allocation size 
/// requests.
///
/// Declarations:
///     void* OVR_malloca(size_t size);
///     void  OVR_freea(void* p);
///
/// Example usage:
///     void TestMalloca()
///     {
///         char* charArray = (char*)OVR_malloca(37000);
///
///         if(charArray)
///         {
///             // <use charArray>
///             OVR_freea(charArray);
///         }
///     }
///
#if !defined(OVR_malloca)
    #define OVR_MALLOCA_ALLOCA_ID  UINT32_C(0xcccccccc)
    #define OVR_MALLOCA_MALLOC_ID  UINT32_C(0xdddddddd)
    #define OVR_MALLOCA_ID_SIZE            16          // Needs to be at least 2 * sizeof(uint32_t) and at least the minimum alignment for malloc on the platform. 16 works for all platforms.
    #if defined(_MSC_VER)
    #define OVR_MALLOCA_SIZE_THRESHOLD   8192
    #else
    #define OVR_MALLOCA_SIZE_THRESHOLD   1024          // Non-Microsoft platforms tend to exhaust stack space sooner due to non-automatic stack expansion.
    #endif

    #define OVR_malloca(size)                                                                                     \
        ((((size) + OVR_MALLOCA_ID_SIZE) < OVR_MALLOCA_SIZE_THRESHOLD) ?                                          \
            OVR::malloca_SetId(static_cast<char*>(alloca((size) + OVR_MALLOCA_ID_SIZE)), OVR_MALLOCA_ALLOCA_ID) : \
            OVR::malloca_SetId(static_cast<char*>(new char[(size) + OVR_MALLOCA_ID_SIZE]), OVR_MALLOCA_MALLOC_ID))

    inline void* malloca_SetId(char* p, uint32_t id)
    {
        if(p)
        {
            *reinterpret_cast<uint32_t*>(p) = id;
            p = reinterpret_cast<char*>(p) + OVR_MALLOCA_ID_SIZE;
        }

        return p;
    }
#endif

#if !defined(OVR_freea)
    #define OVR_freea(p) OVR::freea_Impl(reinterpret_cast<char*>(p))

    inline void freea_Impl(char* p)
    {
        if (p)
        {
            // We store the allocation type id at the first uint32_t in the returned memory.
            static_assert(OVR_MALLOCA_ID_SIZE >= sizeof(uint32_t), "Insufficient OVR_MALLOCA_ID_SIZE size.");
            p -= OVR_MALLOCA_ID_SIZE;
            uint32_t id = *reinterpret_cast<uint32_t*>(p);

            if(id == OVR_MALLOCA_MALLOC_ID)
                delete[] p;
        #if defined(OVR_BUILD_DEBUG)
            else if(id != OVR_MALLOCA_ALLOCA_ID)
                OVR_FAIL_M("OVR_freea memory corrupt or not allocated by OVR_alloca.");
        #endif
        }
    }
#endif


///------------------------------------------------------------------------
/// ***** OVR_newa / OVR_deletea
///
/// Implements a C++ array version of OVR_malloca/OVR_freea.
/// Expresses failure via a nullptr return value and not via a C++ exception.
/// If a handled C++ exception occurs midway during construction in OVR_newa, 
//  there is no automatic destruction of the successfully constructed elements.
///
/// Declarations:
///     T*   OVR_newa(T, size_t count);
///     void OVR_deletea(T, T* pTArray);
///
/// Example usage:
///     void TestNewa()
///     {
///         Widget* pWidgetArray = OVR_newa(Widget, 37000);
///
///         if(pWidgetArray)
///         {
///             // <use pWidgetArray>
///             OVR_deletea(Widget, pWidgetArray);
///         }
///     }
///

#if !defined(OVR_newa)
    #define OVR_newa(T, count) OVR::newa_Impl<T>(static_cast<char*>(OVR_malloca(count * sizeof(T))), count)
#endif

template<class T>
T* newa_Impl(char* pTArray, size_t count)
{
    if(pTArray)
    {
        OVR::ConstructArray<T>(pTArray, count);

        // We store the count at the second uint32_t in the returned memory.
        static_assert(OVR_MALLOCA_ID_SIZE >= (2 * sizeof(uint32_t)), "Insufficient OVR_MALLOCA_ID_SIZE size.");
        reinterpret_cast<uint32_t*>((reinterpret_cast<char*>(pTArray) - OVR_MALLOCA_ID_SIZE))[1] = (uint32_t)count;
    }
    return reinterpret_cast<T*>(pTArray);
}

#if !defined(OVR_deletea)
    #define OVR_deletea(T, pTArray) OVR::deletea_Impl<T>(pTArray)
#endif

template<class T>
void deletea_Impl(T* pTArray)
{
    if(pTArray)
    {
        uint32_t count = reinterpret_cast<uint32_t*>((reinterpret_cast<char*>(pTArray) - OVR_MALLOCA_ID_SIZE))[1];
        OVR::DestructArray<T>(pTArray, count);
        OVR_freea(pTArray);
    }
}





//------------------------------------------------------------------------
// ***** Memory Allocation Macros

// These macros should be used for global allocation. In the future, these
// macros will allows allocation to be extended with debug file/line information
// if necessary.

#define OVR_REALLOC(p,s)        OVR::Allocator::GetInstance()->Realloc((p),(s))
#define OVR_FREE(p)             OVR::Allocator::GetInstance()->Free((p))
#define OVR_ALLOC_ALIGNED(s,a)  OVR::Allocator::GetInstance()->AllocAligned((s),(a))
#define OVR_FREE_ALIGNED(p)     OVR::Allocator::GetInstance()->FreeAligned((p))

#ifdef OVR_BUILD_DEBUG
#define OVR_ALLOC(s)            OVR::Allocator::GetInstance()->AllocDebug((s), __FILE__, __LINE__)
#define OVR_ALLOC_DEBUG(s,f,l)  OVR::Allocator::GetInstance()->AllocDebug((s), f, l)
#else
#define OVR_ALLOC(s)            OVR::Allocator::GetInstance()->Alloc((s))
#define OVR_ALLOC_DEBUG(s,f,l)  OVR::Allocator::GetInstance()->Alloc((s))
#endif


//------------------------------------------------------------------------

// Base class that overrides the new and delete operators.
// Deriving from this class, even as a multiple base, incurs no space overhead.
class NewOverrideBase
{
public:

    // Redefine all new & delete operators.
    OVR_MEMORY_REDEFINE_NEW(NewOverrideBase)
};


//------------------------------------------------------------------------
// ***** Mapped memory allocation
//
// Equates to VirtualAlloc/VirtualFree on Windows, mmap/munmap on Unix.
// These are useful for when you need system-supplied memory pages. 
// These are also useful for when you need to allocate memory in a way 
// that doesn't affect the application heap.

void* SafeMMapAlloc(size_t size);
void  SafeMMapFree (const void* memory, size_t size);


} // OVR


// Redefine operator 'new' if necessary.
#if defined(OVR_DEFINE_NEW)
#define new OVR_DEFINE_NEW
#endif

#if defined(OVR_CC_MSVC)
#pragma warning(pop)
#endif

#endif // OVR_Allocator_h
