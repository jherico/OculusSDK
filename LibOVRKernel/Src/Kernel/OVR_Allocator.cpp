/************************************************************************************

Filename    :   OVR_Allocator.cpp
Content     :   Installable memory allocator implementation
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

#include "OVR_Allocator.h"
#include "OVR_DebugHelp.h"
#include "Kernel/OVR_Std.h"
#include <stdlib.h>
#include <stdio.h>
#include <exception>

// Define this to use jemalloc rather than the default CRT allocator.
//#define OVR_USE_JEMALLOC

// Only use jemalloc in release mode, since in debug mode we use the DebugPageAllocator,
// or are use the debug CRT that has more features for debugging memory issues.
#if !defined(OVR_BUILD_DEBUG) && !defined(OVR_USE_JEMALLOC)
    // If on Visual Studio,
    #if defined(_MSC_VER)
        // We currently only have static libraries for Visual Studio 2013 built.
        // The jemalloc project is built as part of the OVRServer solution,
        // and its binaries are checked in so that it does not slow the build.
        // If compiling with Visual Studio 2013, 2015, and newer,
        #if (_MSC_VER >= 1800)
            #define OVR_USE_JEMALLOC
        #endif
    #endif
#endif // _WIN32

// prevent jemalloc to be used until we figure out the issue
#undef OVR_USE_JEMALLOC

#ifdef OVR_USE_JEMALLOC
    #include "src/jemalloc/jemalloc.h"
#endif // OVR_USE_JEMALLOC

#if defined(OVR_OS_MS)
    #include "OVR_Win32_IncludeWindows.h"
#else
    #include <unistd.h>
    #include <sys/mman.h>
    #include <execinfo.h>
#endif

// This will cause an assertion to trip whenever an allocation occurs outside of our
// custom allocator.  This helps track down allocations that are not being done
// correctly via OVR_ALLOC().
// #define OVR_HUNT_UNTRACKED_ALLOCS

// If we are benchmarking the allocator, define this.
// !!Do not check this in uncommented!!
//#define OVR_BENCHMARK_ALLOCATOR

namespace OVR {



#ifdef OVR_BENCHMARK_ALLOCATOR
#error "This code should not be compiled!  It really hurts performance.  Only enable this during testing."

    // This gets the double constant that can convert ::QueryPerformanceCounter
    // LARGE_INTEGER::QuadPart into a number of seconds.
    // This is the same as in the Timer code except we cannot use Timer code
    // because the allocator gets called during static initializers before
    // the Timer code is initialized.
    static double GetPerfFrequencyInverse()
    {
        // Static value containing frequency inverse of performance counter
        static double PerfFrequencyInverse = 0.;

        // If not initialized,
        if (PerfFrequencyInverse == 0.)
        {
            // Initialize the inverse (same as in Timer code)
            LARGE_INTEGER freq;
            ::QueryPerformanceFrequency(&freq);
            PerfFrequencyInverse = 1.0 / (double)freq.QuadPart;
        }

        return PerfFrequencyInverse;
    }

    // Record a delta timestamp for an allocator operation
    static void ReportDT(LARGE_INTEGER& t0, LARGE_INTEGER& t1)
    {
        // Stats lock to avoid multiple threads corrupting the shared stats
        // This lock is the reason we cannot enable this code.
        static Lock theLock;

        // Running stats
        static double timeSum = 0.; // Sum of dts
        static double timeMax = 0.; // Max dt in set
        static int timeCount = 0; // Number of dts recorded

        // Calculate delta time between start and end of operation
        // based on the provided QPC timestamps
        double dt = (t1.QuadPart - t0.QuadPart) * GetPerfFrequencyInverse();

        // Init the average and max to print to zero.
        // If they stay zero we will not print them.
        double ravg = 0., rmax = 0.;
        {
            // Hold the stats lock
            Lock::Locker locker(&theLock);

            // Accumulate stats
            timeSum += dt;
            if (dt > timeMax)
                timeMax = dt;

            // Every X recordings,
            if (++timeCount >= 1000)
            {
                // Set average/max to print
                ravg = timeSum / timeCount;
                rmax = timeMax;

                timeSum = 0;
                timeMax = 0;
                timeCount = 0;
            }
        }

        // If printing,
        if (rmax != 0.)
        {
            LogText("------- Allocator Stats: AvgOp = %lf usec, MaxOp = %lf usec\n", ravg * 1000000., rmax * 1000000.);
        }
    }
#define OVR_ALLOC_BENCHMARK_START() LARGE_INTEGER t0; ::QueryPerformanceCounter(&t0);
#define OVR_ALLOC_BENCHMARK_END()   LARGE_INTEGER t1; ::QueryPerformanceCounter(&t1); ReportDT(t0, t1);
#else
#define OVR_ALLOC_BENCHMARK_START()
#define OVR_ALLOC_BENCHMARK_END()
#endif // OVR_BENCHMARK_ALLOCATOR


bad_alloc::bad_alloc(const char* description) OVR_NOEXCEPT
{
    if(description)
        OVR_strlcpy(Description, description, sizeof(Description));
    else
        Description[0] = '\0';

    OVR_strlcat(Description, " at ", sizeof(Description));

    // read the current backtrace
    // We cannot attempt to symbolize this here as that would attempt to 
    // allocate memory. That would be unwise within a bad_alloc exception.
    void* backtrace_data[20];
    char  addressDescription[256] = {}; // Write into this temporary instead of member Description in case an exception is thrown.

    #if defined(OVR_OS_MS)
        int count = CaptureStackBackTrace(2, sizeof(backtrace_data)/sizeof(backtrace_data[0]), backtrace_data, nullptr);
    #else
        int count = backtrace(backtrace_data, sizeof(backtrace_data)/sizeof(backtrace_data[0]));
    #endif

    for(int i = 0; i < count; ++i)
    {
        char address[(sizeof(void*) * 2) + 1 + 1]; // hex address string plus possible space plus null terminator.
        OVR_snprintf(address, sizeof(address), "%x%s", backtrace_data[i], (i + 1 < count) ? " " : "");
        OVR_strlcat(addressDescription, address, sizeof(addressDescription));
    }

    OVR_strlcat(Description, addressDescription, sizeof(Description));
}



//-----------------------------------------------------------------------------------
// ***** Allocator

Allocator* Allocator::GetInstance()
{
    static Allocator* pAllocator = nullptr;

    if(!pAllocator)
    {
        static DefaultAllocator defaultAllocator;
        pAllocator = &defaultAllocator;

        bool safeToUseDebugAllocator = true;

        #if defined(OVR_BUILD_DEBUG) && defined(OVR_CC_MSVC)
            // Make _CrtIsValidHeapPointer always return true. The VC++ concurrency library has a bug in that
            // it's calling _CrtIsValidHeapPointer, which is invalid and recommended against by Microsoft themselves.
            // We need to deal with this nevertheless. The problem is that the VC++ concurrency library is
            // calling _CrtIsValidHeapPointer on the default heap instead of the current heap (DebugPageAllocator).
            // So we modify the _CrtIsValidHeapPointer implementation to always return true. The primary risk
            // with this change is that there's some code somewhere that uses it for a non-diagnostic purpose.
            // However this os Oculus-debug-internal and so has no effect on any formally published software.
            safeToUseDebugAllocator = OVR::KillCdeclFunction(_CrtIsValidHeapPointer, true); // If we can successfully kill _CrtIsValidHeapPointer, use our debug allocator.
        #endif

        // This is restricted to X64 builds due address space exhaustion in 32-bit builds
        #if defined(OVR_BUILD_DEBUG) && defined(OVR_CPU_X86_64)
            if (safeToUseDebugAllocator)
            {
                static DebugPageAllocator debugAllocator;
                pAllocator = &debugAllocator;
            }
        #endif

        OVR_UNUSED(safeToUseDebugAllocator);
    }

    return pAllocator;
}

// Default AlignedAlloc implementation will delegate to Alloc/Free after doing rounding.
void* Allocator::AllocAligned(size_t size, size_t align)
{
#ifdef OVR_USE_JEMALLOC
    OVR_ALLOC_BENCHMARK_START();
    void* p = je_aligned_alloc(align, size);
    OVR_ALLOC_BENCHMARK_END();
    return p;
#else // OVR_USE_JEMALLOC

    OVR_ALLOC_BENCHMARK_START();
    OVR_ASSERT((align & (align - 1)) == 0);
    align = (align > sizeof(size_t)) ? align : sizeof(size_t);
    size_t p = (size_t)Alloc(size + align);
    size_t aligned = 0;
    if (p)
    {
        aligned = (size_t(p) + align - 1) & ~(align - 1);
        if (aligned == p)
            aligned += align;
        *(((size_t*)aligned) - 1) = aligned - p;
    }
    OVR_ALLOC_BENCHMARK_END();
    return (void*)(aligned);

#endif // OVR_USE_JEMALLOC
}

void Allocator::FreeAligned(void* p)
{
    OVR_ALLOC_BENCHMARK_START();
#ifdef OVR_USE_JEMALLOC
    je_free(p);
#else // OVR_USE_JEMALLOC
    if (p)
    {
        size_t src = size_t(p) - *(((size_t*)p) - 1);
        Free((void*)src);
    }
#endif // OVR_USE_JEMALLOC
    OVR_ALLOC_BENCHMARK_END();
}


//------------------------------------------------------------------------
// ***** Default Allocator

// This allocator is created and used if no other allocator is installed.
// Default allocator delegates to system malloc.

void* DefaultAllocator::Alloc(size_t size)
{
    OVR_ALLOC_BENCHMARK_START();
#ifdef OVR_USE_JEMALLOC
    void* p = je_malloc(size);
#else // OVR_USE_JEMALLOC
    void* p = malloc(size);
#endif // OVR_USE_JEMALLOC
    OVR_ALLOC_BENCHMARK_END();

    trackAlloc(p, size);
    return p;
}

void* DefaultAllocator::AllocDebug(size_t size, const char* file, unsigned line)
{
    void* p;

#ifdef OVR_USE_JEMALLOC
    OVR_UNUSED2(file, line);

    p = Alloc(size);
#else // OVR_USE_JEMALLOC

#if defined(OVR_CC_MSVC) && defined(_CRTDBG_MAP_ALLOC)
    p = _malloc_dbg(size, _NORMAL_BLOCK, file, line);
#else
    OVR_UNUSED2(file, line); // should be here for debugopt config
    p = malloc(size);
#endif

#endif // OVR_USE_JEMALLOC

    trackAlloc(p, size);
    return p;
}

void* DefaultAllocator::Realloc(void* p, size_t newSize)
{
    OVR_ALLOC_BENCHMARK_START();
#ifdef OVR_USE_JEMALLOC
    void* newP = je_realloc(p, newSize);
#else // OVR_USE_JEMALLOC
    void* newP = realloc(p, newSize);
#endif // OVR_USE_JEMALLOC
    OVR_ALLOC_BENCHMARK_END();

    // This used to more efficiently check if (newp != p) but static analyzers were erroneously flagging this.
    if (newP) // Need to check newP because realloc doesn't free p unless it returns a valid newP.
    {
#if !defined(__clang_analyzer__)  // The analyzer complains that we are using p after it was freed.
        untrackAlloc(p);
#endif
    }
    trackAlloc(newP, newSize);
    return newP;
}

void DefaultAllocator::Free(void *p)
{
    untrackAlloc(p);
    OVR_ALLOC_BENCHMARK_START();
#ifdef OVR_USE_JEMALLOC
    je_free(p);
#else // OVR_USE_JEMALLOC
    free(p);
#endif // OVR_USE_JEMALLOC
    OVR_ALLOC_BENCHMARK_END();
}


// System block allocator:

void* SafeMMapAlloc(size_t size)
{
    #if defined(OVR_OS_MS)
        return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); // size is rounded up to a page. // Returned memory is 0-filled.

    #elif defined(OVR_OS_MAC) || defined(OVR_OS_UNIX)
        #if !defined(MAP_FAILED)
            #define MAP_FAILED ((void*)-1)
        #endif

        void* result = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0); // Returned memory is 0-filled.
        if(result == MAP_FAILED) // mmap returns MAP_FAILED (-1) upon failure.
            result = nullptr;
        return result;
    #endif
}

void SafeMMapFree(const void* memory, size_t size)
{
    #if defined(OVR_OS_MS)
        OVR_UNUSED(size);
        VirtualFree(const_cast<void*>(memory), 0, MEM_RELEASE);

    #elif defined(OVR_OS_MAC) || defined(OVR_OS_UNIX)
        size_t pageSize = getpagesize();
        size = (((size + (pageSize - 1)) / pageSize) * pageSize);
        munmap(const_cast<void*>(memory), size); // Must supply the size to munmap.
    #endif
}


//------------------------------------------------------------------------
// ***** SetLeakTracking

static bool IsLeakTracking = false;

void Allocator::SetLeakTracking(bool enabled)
{
#if defined(OVR_OS_WIN32) && !defined(OVR_OS_WIN64)
    // HACK: Currently 32-bit leak tracing is too slow to run in real-time on Windows.
    // Note: We can possibly fix this by making a custom Win32 backtrace function which 
    // takes advantage of the fact that we have enabled stack frames in all builds.
    enabled = false;
#endif

    if (enabled)
    {
        SymbolLookup::Initialize();
    }

    IsLeakTracking = enabled;
}

bool Allocator::IsTrackingLeaks()
{
    return IsLeakTracking;
}


//------------------------------------------------------------------------
// ***** Track Allocations

struct TrackedAlloc
{
    TrackedAlloc* pNext;
    TrackedAlloc* pPrev;

    void*         pAlloc;
    void*         Callstack[64];
    uint32_t      FrameCount;
    uint32_t      Size;
};

static uint32_t PointerHash(const void* p)
{
    uintptr_t key = (uintptr_t)p;
#ifdef OVR_64BIT_POINTERS
    key = (~key) + (key << 18);
    key = key ^ (key >> 31);
    key = key * 21;
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
#else
    key = (key ^ 61) ^ (key >> 16);
    key = key + (key << 3);
    key = key ^ (key >> 4);
    key = key * 0x27d4eb2d;
    key = key ^ (key >> 15);
#endif
    return (uint32_t)key;
}

#define OVR_HASH_BITS 10
#define OVR_HASH_SIZE (1 << OVR_HASH_BITS)
#define OVR_HASH_MASK (OVR_HASH_SIZE - 1)

static TrackedAlloc* AllocHashMap[OVR_HASH_SIZE] = {nullptr};
static SymbolLookup Symbols;

void Allocator::trackAlloc(void* p, size_t size)
{
    if (!p || !IsLeakTracking)
        return;

#ifdef OVR_USE_JEMALLOC
    TrackedAlloc* tracked = (TrackedAlloc*)je_malloc(sizeof(TrackedAlloc));
#else // OVR_USE_JEMALLOC
    TrackedAlloc* tracked = (TrackedAlloc*)malloc(sizeof(TrackedAlloc));
#endif // OVR_USE_JEMALLOC
    tracked->pAlloc = p;
    tracked->FrameCount = (uint32_t)Symbols.GetBacktrace(tracked->Callstack, OVR_ARRAY_COUNT(tracked->Callstack), 2);
    tracked->Size = (uint32_t)size;

    uint32_t key = PointerHash(p) & OVR_HASH_MASK;

    // Hold track lock and verify leak tracking is still going on
    Lock::Locker locker(&TrackLock);
    if (!IsLeakTracking)
    {
#ifdef OVR_USE_JEMALLOC
        je_free(tracked);
#else // OVR_USE_JEMALLOC
        free(tracked);
#endif // OVR_USE_JEMALLOC
        return;
    }

    // Insert into the hash map
    TrackedAlloc* head = AllocHashMap[key];
    tracked->pPrev = nullptr;
    tracked->pNext = head;
    if (head)
    {
        head->pPrev = tracked;
    }
    AllocHashMap[key] = tracked;
}

void Allocator::untrackAlloc(void* p)
{
    if (!p || !IsLeakTracking)
        return;

    uint32_t key = PointerHash(p) & OVR_HASH_MASK;

    Lock::Locker locker(&TrackLock);

    TrackedAlloc* head = AllocHashMap[key];

    for (TrackedAlloc* t = head; t; t = t->pNext)
    {
        if (t->pAlloc == p)
        {
            if (t->pPrev)
            {
                t->pPrev->pNext = t->pNext;
            }
            if (t->pNext)
            {
                t->pNext->pPrev = t->pPrev;
            }
            if (head == t)
            {
                AllocHashMap[key] = t->pNext;
            }
#ifdef OVR_USE_JEMALLOC
            je_free(t);
#else // OVR_USE_JEMALLOC
            free(t);
#endif // OVR_USE_JEMALLOC

            break;
        }
    }
}

int Allocator::DumpMemory()
{
    const bool symbolLookupWasInitialized = SymbolLookup::IsInitialized();
    const bool symbolLookupAvailable = SymbolLookup::Initialize();

    if(!symbolLookupWasInitialized) // If SymbolLookup::Initialize was the first time being initialized, we need to refresh the Symbols view of modules, etc.
        Symbols.Refresh();

    // If we're dumping while LibOVR is running, then we should hold the lock.
    Allocator* pAlloc = Allocator::GetInstance();

    // It's possible this is being called after the Allocator was shut down, at which 
    // point we assume we are the only instance that can be executing at his time.
    Lock* lock = pAlloc ? &pAlloc->TrackLock : nullptr;
    if (lock) 
        lock->DoLock();

    int measuredLeakCount = 0;
    int reportedLeakCount = 0;      // = realLeakCount minus leaks we ignore (e.g. C++ runtime concurrency leaks).
    const size_t leakReportBufferSize = 8192;
    char* leakReportBuffer = nullptr;

    for (int i = 0; i < OVR_HASH_SIZE; ++i)
    {
        for (TrackedAlloc* t = AllocHashMap[i]; t; t = t->pNext)
        {
            measuredLeakCount++;

            if (!leakReportBuffer) // Lazy allocate this, as it wouldn't be needed unless we had a leak, which we aim to be an unusual case.
            {
                leakReportBuffer = static_cast<char*>(SafeMMapAlloc(leakReportBufferSize));
                if (!leakReportBuffer)
                    break;
            }

            char line[2048];
            OVR_snprintf(line, OVR_ARRAY_COUNT(line), "\n[Leak] ** Detected leaked allocation at %p (size = %u) (%d frames)\n", t->pAlloc, (unsigned)t->Size, (unsigned)t->FrameCount);
            OVR_strlcat(leakReportBuffer, line, leakReportBufferSize);

            if (t->FrameCount == 0)
            {
                OVR_snprintf(line, OVR_ARRAY_COUNT(line), "(backtrace unavailable)\n");
                OVR_strlcat(leakReportBuffer, line, leakReportBufferSize);
            }
            else
            {
                for (size_t j = 0; j < t->FrameCount; ++j)
                {
                    SymbolInfo symbolInfo;

                    if (symbolLookupAvailable && Symbols.LookupSymbol((uint64_t)t->Callstack[j], symbolInfo) && (symbolInfo.filePath[0] || symbolInfo.function[0]))
                    {
                        if (symbolInfo.filePath[0])
                            OVR_snprintf(line, OVR_ARRAY_COUNT(line), "%s(%d): %s\n", symbolInfo.filePath, symbolInfo.fileLineNumber, symbolInfo.function[0] ? symbolInfo.function : "(unknown function)");
                        else
                            OVR_snprintf(line, OVR_ARRAY_COUNT(line), "%p (unknown source file): %s\n", t->Callstack[j], symbolInfo.function);
                    }
                    else
                    {
                        OVR_snprintf(line, OVR_ARRAY_COUNT(line), "%p (symbols unavailable)\n", t->Callstack[j]);
                    }

                    OVR_strlcat(leakReportBuffer, line, leakReportBufferSize);
                }

                // There are some leaks that aren't real because they are allocated by the Standard Library at runtime but 
                // aren't freed until shutdown. We don't want to report those, and so we filter them out here.
                const char* ignoredPhrases[] = { "Concurrency::details" /*add any additional strings here*/ };

                for(size_t j = 0; j < OVR_ARRAY_COUNT(ignoredPhrases); ++j)
                {
                    if (strstr(leakReportBuffer, ignoredPhrases[j])) // If we should ignore this leak...
                    {
                        leakReportBuffer[0] = '\0';
                    } 
                }
            }

            if (leakReportBuffer[0]) // If we are to report this as a bonafide leak...
            {
                ++reportedLeakCount;

                // We cannot use normal logging system here because it will allocate more memory!
                ::OutputDebugStringA(leakReportBuffer);
            }
        }
    }

    char summaryBuffer[128];
    OVR_snprintf(summaryBuffer, OVR_ARRAY_COUNT(summaryBuffer), "Measured leak count: %d, Reported leak count: %d\n", measuredLeakCount, reportedLeakCount);
    ::OutputDebugStringA(summaryBuffer);

    if (leakReportBuffer)
    {
        SafeMMapFree(leakReportBuffer, leakReportBufferSize);
        leakReportBuffer = nullptr;
    }

    if (lock)
        lock->Unlock();

    if(symbolLookupAvailable)
        SymbolLookup::Shutdown();

    return reportedLeakCount;
}




//------------------------------------------------------------------------
// ***** DebugPageAllocator

static size_t AlignSizeUp(size_t value, size_t alignment)
{
    return ((value + (alignment - 1)) & ~(alignment - 1));
}

static size_t AlignSizeDown(size_t value, size_t alignment)
{
    return (value & ~(alignment - 1));
}

template <typename Pointer>
Pointer AlignPointerUp(Pointer p, size_t alignment)
{
    return reinterpret_cast<Pointer>(((reinterpret_cast<size_t>(p) + (alignment - 1)) & ~(alignment - 1)));
}

template <typename Pointer>
Pointer AlignPointerDown(Pointer p, size_t alignment)
{
    return reinterpret_cast<Pointer>(reinterpret_cast<size_t>(p) & ~(alignment-1));
}


const size_t kFreedBlockArrayMaxSizeDefault = 16384;


#if defined(OVR_HUNT_UNTRACKED_ALLOCS)

static const char* WhiteList[] = {
    "OVR_Allocator.cpp",
    "OVR_Log.cpp",
    "crtw32", // Ignore CRT internal allocations
    nullptr
};

static int YourAllocHook(int, void *,
    size_t, int, long,
    const unsigned char *szFileName, int)
{
    if (!szFileName)
    {
        return TRUE;
    }

    for (int i = 0; WhiteList[i] != nullptr; ++i)
    {
        if (strstr((const char*)szFileName, WhiteList[i]) != 0)
        {
            return TRUE;
        }
    }

    OVR_ASSERT(false);
    return FALSE;
}

#endif // OVR_HUNT_UNTRACKED_ALLOCS


DebugPageAllocator::DebugPageAllocator()
  : FreedBlockArray(nullptr)
  , FreedBlockArrayMaxSize(0)
  , FreedBlockArraySize(0)
  , FreedBlockArrayOldest(0)
  , AllocationCount(0)
  , OverrunPageEnabled(true)
  #if defined(OVR_BUILD_DEBUG)
  , OverrunGuardBytesEnabled(true)
  #else
  , OverrunGuardBytesEnabled(false)
  #endif
  //PageSize(0)
  , Lock()
{
    #if defined(OVR_HUNT_UNTRACKED_ALLOCS)
        _CrtSetAllocHook(YourAllocHook);
    #endif // OVR_HUNT_UNTRACKED_ALLOCS

    #if defined(_WIN32)
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        PageSize = (size_t)systemInfo.dwPageSize;
    #else
        PageSize = 4096;
    #endif

    SetMaxDelayedFreeCount(kFreedBlockArrayMaxSizeDefault);
}


DebugPageAllocator::~DebugPageAllocator()
{
    Shutdown();
}


void DebugPageAllocator::Init()
{
    // Nothing to do.
}

void DebugPageAllocator::Shutdown()
{
    Lock::Locker autoLock(&Lock);

    for(size_t i = 0; i < FreedBlockArraySize; i++)
    {
        if(FreedBlockArray[i].BlockPtr)
        {
            FreePageMemory(FreedBlockArray[i].BlockPtr, FreedBlockArray[i].BlockSize);
            FreedBlockArray[i].Clear();
        }
    }

    SetMaxDelayedFreeCount(0);
    FreedBlockArraySize = 0;
    FreedBlockArrayOldest = 0;
}


void DebugPageAllocator::EnableOverrunDetection(bool enableOverrunDetection, bool enableOverrunGuardBytes)
{
    // Assert that no allocations have been made, which is a requirement for changing these properties. 
    // Otherwise future deallocations of these allocations can fail to work properly because these 
    // settings have changed behind their back.
    OVR_ASSERT_M(AllocationCount == 0, "DebugPageAllocator::EnableOverrunDetection called when DebugPageAllocator is not in a newly initialized state.");

    OverrunPageEnabled       = enableOverrunDetection;
    OverrunGuardBytesEnabled = (enableOverrunDetection && enableOverrunGuardBytes); // Set OverrunGuardBytesEnabled to false if enableOverrunDetection is false.
}


void DebugPageAllocator::SetMaxDelayedFreeCount(size_t maxDelayedFreeCount)
{
    if(FreedBlockArray)
    {
        SafeMMapFree(FreedBlockArray, FreedBlockArrayMaxSize * sizeof(Block));
        FreedBlockArrayMaxSize = 0;
    }

    if(maxDelayedFreeCount)
    {
        FreedBlockArray = (Block*)SafeMMapAlloc(maxDelayedFreeCount * sizeof(Block));
        OVR_ASSERT(FreedBlockArray);

        if(FreedBlockArray)
        {
            FreedBlockArrayMaxSize = maxDelayedFreeCount;
            #if defined(OVR_BUILD_DEBUG)
                memset(FreedBlockArray, 0, maxDelayedFreeCount * sizeof(Block));
            #endif
        }
    }
}


size_t DebugPageAllocator::GetMaxDelayedFreeCount() const
{
    return FreedBlockArrayMaxSize;
}


void* DebugPageAllocator::Alloc(size_t size)
{
    #if defined(_WIN32)
        return AllocAligned(size, DefaultAlignment);
    #else
        #ifdef OVR_USE_JEMALLOC
            void* p = je_malloc(size);
        #else // OVR_USE_JEMALLOC
            void* p = malloc(size);
        #endif // OVR_USE_JEMALLOC
        trackAlloc(p, size);
        return p;
    #endif
}


void* DebugPageAllocator::AllocAligned(size_t size, size_t align)
{
    #if defined(_WIN32)
        OVR_ASSERT(align <= PageSize);

        Lock::Locker autoLock(&Lock);

        if(align < DefaultAlignment)
            align = DefaultAlignment;

        // The actual needed size may be a little less than this, but it's hard to tell how the size and alignments will play out.
        size_t maxRequiredSize = AlignSizeUp(size, align) + SizeStorageSize;

        if(align > SizeStorageSize)
        {
            // Must do: more sophisticated fitting, as maxRequiredSize is potentially too small.
            OVR_ASSERT(SizeStorageSize <= align);
        }

        size_t blockSize = AlignSizeUp(maxRequiredSize, PageSize);

        if(OverrunPageEnabled)
            blockSize += PageSize; // We add another page which will be uncommitted, so any read or write with it will except.

        void* pBlockPtr;

        if((FreedBlockArraySize == FreedBlockArrayMaxSize) && FreedBlockArrayMaxSize &&     // If there is an old block we can recycle...
           (FreedBlockArray[FreedBlockArrayOldest].BlockSize == blockSize))                 // We require it to be the exact size, as there would be some headaches for us if it was over-sized.
        {
            pBlockPtr = EnablePageMemory(FreedBlockArray[FreedBlockArrayOldest].BlockPtr, blockSize);  // Convert this memory from PAGE_NOACCESS back to PAGE_READWRITE.
            FreedBlockArray[FreedBlockArrayOldest].Clear();
            
            if(++FreedBlockArrayOldest == FreedBlockArrayMaxSize)
                FreedBlockArrayOldest = 0;
        }
        else
        {
            pBlockPtr = AllocCommittedPageMemory(blockSize); // Allocate a new block of one or more pages (via VirtualAlloc).
        }

        if(pBlockPtr)
        {
            void*   pUserPtr = GetUserPosition(pBlockPtr, blockSize, size, align);
            size_t* pSizePos = GetSizePosition(pUserPtr);

            pSizePos[UserSizeIndex] = size;
            pSizePos[BlockSizeIndex] = blockSize;
            AllocationCount++;
            trackAlloc(pUserPtr, size);

            return pUserPtr;
        }

        return nullptr;
    #else
        OVR_ASSERT_AND_UNUSED(align <= DefaultAlignment, align);
        return DebugPageAllocator::Alloc(size);
    #endif
}


size_t DebugPageAllocator::GetUserSize(const void* p)
{
    #if defined(_WIN32)
        return GetSizePosition(p)[UserSizeIndex];
    #elif defined(__APPLE__)
        return malloc_size(p);
    #else
        return malloc_usable_size(const_cast<void*>(p));
    #endif
}


size_t DebugPageAllocator::GetBlockSize(const void* p)
{
    #if defined(_WIN32)
        return GetSizePosition(p)[BlockSizeIndex];
    #else
        OVR_UNUSED(p);
        return 0;
    #endif
}


size_t* DebugPageAllocator::GetSizePosition(const void* p)
{
    // No thread safety required as per our design, as we assume that anybody 
    // who owns a pointer returned by Alloc cannot have another thread take it away.

    // We assume the pointer is a valid pointer allocated by this allocator.
    // We store some size values into the memory returned to the user, a few bytes before it.
    size_t  value    = reinterpret_cast<size_t>(p);
    size_t  valuePos = (value - SizeStorageSize);
    size_t* pSize    = reinterpret_cast<size_t*>(valuePos);

    return pSize;
}


void* DebugPageAllocator::Realloc(void* p, size_t newSize)
{
    #if defined(_WIN32)
        return ReallocAligned(p, newSize, DefaultAlignment);
    #else
        void* newP = realloc(p, newSize);

        if(newP) // Need to check newP because realloc doesn't free p unless it returns a valid newP.
        {
            #if !defined(__clang_analyzer__) // The analyzer complains that we are using p after it was freed.
                untrackAlloc(p);
            #endif
        }
        trackAlloc(newP, newSize);
        return newP;
    #endif
}


void* DebugPageAllocator::ReallocAligned(void* p, size_t newSize, size_t newAlign)
{
    #if defined(_WIN32)
        // The ISO C99 standard states:
        //     The realloc function deallocates the old object pointed to by ptr and 
        //     returns a pointer to a new object that has the size specified by size. 
        //     The contents of the new object shall be the same as that of the old 
        //     object prior to deallocation, up to the lesser of the new and old sizes. 
        //     Any bytes in the new object beyond the size of the old object have
        //     indeterminate values.
        //
        //     If ptr is a null pointer, the realloc function behaves like the malloc 
        //     function for the specified size. Otherwise, if ptr does not match a 
        //     pointer earlier returned by the calloc, malloc, or realloc function, 
        //     or if the space has been deallocated by a call to the free or realloc 
        //     function, the behavior is undefined. If memory for the new object 
        //     cannot be allocated, the old object is not deallocated and its value 
        //     is unchanged.
        //     
        //     The realloc function returns a pointer to the new object (which may have 
        //     the same value as a pointer to the old object), or a null pointer if 
        //     the new object could not be allocated.

        // A mutex lock isn't required, as the functions below will handle it internally.
        // But having it here is a little more efficient because it woudl otherwise be 
        // locked and unlocked multiple times below, with possible context switches in between.
        Lock::Locker autoLock(&Lock);

        void* pReturn = nullptr;

        if(p)
        {
            if(newSize)
            {
                pReturn = AllocAligned(newSize, newAlign);

                if(pReturn)
                {
                    size_t prevSize = GetUserSize(p);

                    if(newSize > prevSize)
                        newSize = prevSize;

                    memcpy(pReturn, p, newSize);
                    Free(p);
                } // Else fall through, leaving p's memory unmodified and returning nullptr.
            }
            else
            {
                Free(p);
            }
        }
        else if(newSize)
        {
            pReturn = AllocAligned(newSize, newAlign);
        }

        return pReturn;
    #else
        OVR_ASSERT_AND_UNUSED(newAlign <= DefaultAlignment, newAlign);
        return DebugPageAllocator::Realloc(p, newSize);
    #endif
}


void DebugPageAllocator::Free(void *p)
{
    #if defined(_WIN32)
        if(p)
        {
            // Creating a scope for the lock
            {
                Lock::Locker autoLock(&Lock);

                if(FreedBlockArrayMaxSize)  // If we have a delayed free list...
                {
                    // We don't free the page(s) associated with this but rather put them in the FreedBlockArray in an inaccessible state for later freeing.
                    // We do this because we don't want those pages to be available again in the near future, so we can detect use-after-free misakes.
                    Block* pBlockNew;

                    if(FreedBlockArraySize == FreedBlockArrayMaxSize) // If we have reached freed block capacity... we can start purging old elements from it as a circular queue.
                    {
                        pBlockNew = &FreedBlockArray[FreedBlockArrayOldest];

                        // The oldest element in the container is FreedBlockArrayOldest.
                        if(pBlockNew->BlockPtr) // Currently this should always be true.
                        {
                            FreePageMemory(pBlockNew->BlockPtr, pBlockNew->BlockSize);
                            pBlockNew->Clear();
                        }

                        if(++FreedBlockArrayOldest == FreedBlockArrayMaxSize)
                            FreedBlockArrayOldest = 0;
                    }
                    else // Else we are still building the container and not yet treating it a circular.
                    {
                        pBlockNew = &FreedBlockArray[FreedBlockArraySize++];
                    }

                    pBlockNew->BlockPtr  = GetBlockPtr(p);
                    pBlockNew->BlockSize = GetBlockSize(p);

                    #if defined(OVR_BUILD_DEBUG)
                        if(OverrunGuardBytesEnabled) // If we have extra bytes at the end of the user's allocation between it and an inaccessible guard page...
                        {
                            const size_t   userSize = GetUserSize(p);
                            const uint8_t* pUserEnd = (static_cast<uint8_t*>(p) + userSize);
                            const uint8_t* pPageEnd = AlignPointerUp(pUserEnd, PageSize);

                            while(pUserEnd != pPageEnd)
                            {
                                if(*pUserEnd++ != GuardFillByte)
                                {
                                    OVR_FAIL();
                                    break;
                                }
                            }
                        }
                    #endif

                    DisablePageMemory(pBlockNew->BlockPtr, pBlockNew->BlockSize); // Make it so that future attempts to use this memory result in an exception.
                }
                else
                {
                    FreePageMemory(GetBlockPtr(p), GetBlockSize(p));
                }

                AllocationCount--;
            }
            untrackAlloc(p);
        }
    #else
        untrackAlloc(p);
        return free(p);
    #endif
}


void DebugPageAllocator::FreeAligned(void* p)
{
    return Free(p);
}


// Converts a user pointer to the beginning of its page.
void* DebugPageAllocator::GetBlockPtr(void* p)
{
    // We store size info before p in memory, and this will, by design, be always somewhere within 
    // the first page of a block of pages. So just align down to the beginning of its page.
    return AlignPointerDown(GetSizePosition(p), PageSize);
}


void* DebugPageAllocator::GetUserPosition(void* pPageMemory, size_t blockSize, size_t userSize, size_t userAlignment)
{
    uint8_t* pUserPosition;

    if(OverrunPageEnabled)
    {
        // We need to return the highest position within the page memory that fits the user size while being aligned to userAlignment.
        const size_t pageEnd      = reinterpret_cast<size_t>(pPageMemory) + (blockSize - PageSize); // pageEnd points to the beginning of the final guard page.
        const size_t userPosition = AlignSizeDown(pageEnd - userSize, userAlignment);
        pUserPosition = reinterpret_cast<uint8_t*>(userPosition);
        OVR_ASSERT((userPosition + userSize) <= pageEnd);

        // If userSize is not a multiple of userAlignment then there will be (userAlignment - userSize) bytes
        // of unused memory between the user allocated space and the end of the page. There is no way around having this.
        // For example, a user allocation of 3 bytes with 8 byte alignment will leave 5 unused bytes at the end of the page.
        // We optionally fill those unused bytes with a pattern and upon Free verify that the pattern is undisturbed.
        // This won't detect reads or writes in that area immediately as with reads or writes beyond that, but it will
        // at least detect them at some point (e.g. upon Free).
        #if defined(OVR_BUILD_DEBUG)
            if(OverrunGuardBytesEnabled)
            {
                uint8_t* const pUserEnd = (pUserPosition + userSize);
                const size_t remainingByteCount = (reinterpret_cast<uint8_t*>(pageEnd) - pUserEnd);
                if(remainingByteCount) // If there are any left-over bytes...
                    memset(pUserEnd, GuardFillByte, remainingByteCount);
            }
        #endif
    }
    else
    {
        // We need to return the first position in the first page after SizeStorageSize bytes which is aligned to userAlignment.
        const size_t lowestPossiblePos = reinterpret_cast<size_t>(pPageMemory) + SizeStorageSize;
        const size_t userPosition = AlignSizeUp(lowestPossiblePos, userAlignment);
        pUserPosition = reinterpret_cast<uint8_t*>(userPosition);
        OVR_ASSERT((userPosition + userSize) <= (reinterpret_cast<size_t>(pPageMemory) + blockSize));
    }

    // Assert that the returned user pointer (actually the size info before it) will be within the first page.
    // This is important because it verifieds that we haven't wasted memory and because 
    // our functionality for telling the start of the page block depends on it.
    OVR_ASSERT(AlignPointerDown(GetSizePosition(pUserPosition), PageSize) == pPageMemory);

    return pUserPosition;
}


void* DebugPageAllocator::AllocCommittedPageMemory(size_t blockSize)
{
    #if defined(_WIN32)
        void* p;
    
        if(OverrunPageEnabled)
        {
            // We need to make it so that last page is MEM_RESERVE and the previous pages are MEM_COMMIT + PAGE_READWRITE.
            OVR_ASSERT(blockSize > PageSize); // There should always be at least one extra page.

            // Reserve blockSize amount of pages.
            // We could possibly use PAGE_GUARD here for the last page. This differs from simply leaving it reserved
            // because the OS will generate a one-time-only gaurd page exception. We probabl don't want this, as it's
            // more useful for maintaining your own stack than for catching unintended overruns.
            p = VirtualAlloc(nullptr, blockSize, MEM_RESERVE, PAGE_READWRITE);

            if(p)
            {
                // Commit all but the last page. Leave the last page as merely reserved so that reads from or writes
                // to it result in an immediate exception.
                p = VirtualAlloc(p, blockSize - PageSize, MEM_COMMIT, PAGE_READWRITE);
            }
        }
        else
        {
            // We need to make it so that all pages are MEM_COMMIT + PAGE_READWRITE.
            p = VirtualAlloc(nullptr, blockSize, MEM_COMMIT, PAGE_READWRITE);
        }

        #if defined(OVR_BUILD_DEBUG)
            if(!p)
            {
                // To consider: Make a generic OVRKernel function for formatting system errors. We could move 
                // the OVRError GetSysErrorCodeString from LibOVR/OVRError.h to LibOVRKernel/OVR_DebugHelp.h
                DWORD dwLastError = GetLastError();
                WCHAR osError[256];
                DWORD osErrorBufferCapacity = OVR_ARRAY_COUNT(osError);
                CHAR  reportedError[384];
                DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, (DWORD)dwLastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), osError, osErrorBufferCapacity, nullptr);
        
                if (length)
                {
                    std::string errorBuff = UCSStringToUTF8String(osError, length + 1);
                    OVR_snprintf(reportedError, OVR_ARRAY_COUNT(reportedError), "DebugPageAllocator: VirtualAlloc failed with error: %s", errorBuff);
                }
                else
                {
                    OVR_snprintf(reportedError, OVR_ARRAY_COUNT(reportedError), "DebugPageAllocator: VirtualAlloc failed with error: %d.", dwLastError);
                }

                //LogError("%s", reportedError); Disabled because this call turns around and allocates memory, yet we may be in a broken or exhausted memory situation.
                OVR_FAIL_M(reportedError);
            }
        #endif

        return p;
    #else
        OVR_UNUSED2(blockSize, OverrunPageEnabled);
        return nullptr;
    #endif
}


// We convert disabled page memory (see DisablePageMemory) to enabled page memory. The output is the same 
// as with AllocPageMemory.
void* DebugPageAllocator::EnablePageMemory(void* pPageMemory, size_t blockSize)
{
    #if defined(_WIN32)
        // Make sure the entire range of memory is of type PAGE_READWRITE.
        DWORD dwPrevAccess = 0;
        BOOL result = VirtualProtect(pPageMemory, OverrunPageEnabled ? (blockSize - PageSize) : blockSize, PAGE_READWRITE, &dwPrevAccess);
        OVR_ASSERT_AND_UNUSED(result, result);
    #else
        OVR_UNUSED3(pPageMemory, blockSize, OverrunPageEnabled);
    #endif

    return pPageMemory;
}


void DebugPageAllocator::DisablePageMemory(void* pPageMemory, size_t blockSize)
{
    #if defined(_WIN32)
        // Disable access to the page(s). It's faster for us to change the page access than it is to decommit or free the pages.
        // However, this results in more committed physical memory usage than we would need if we instead decommitted the memory.
        DWORD dwPrevAccesss = 0;
        BOOL result = VirtualProtect(pPageMemory, OverrunPageEnabled ?  (blockSize - PageSize) : blockSize, PAGE_NOACCESS, &dwPrevAccesss);
        OVR_ASSERT_AND_UNUSED(result, result);
    #else
        OVR_UNUSED2(pPageMemory, blockSize);
    #endif
}


void DebugPageAllocator::FreePageMemory(void* pPageMemory, size_t /*blockSize*/)
{
    #if defined(_WIN32)    
        BOOL result = VirtualFree(pPageMemory, 0, MEM_RELEASE);
        OVR_ASSERT_AND_UNUSED(result, result);
    #else
        OVR_UNUSED(pPageMemory);
    #endif
}



} // namespace OVR
