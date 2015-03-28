/************************************************************************************

Filename    :   OVR_Allocator.cpp
Content     :   Installable memory allocator implementation
Created     :   September 19, 2012
Notes       : 

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "OVR_Allocator.h"
#include "OVR_DebugHelp.h"
#include <stdlib.h>

#ifdef OVR_OS_MAC
 #include <stdlib.h>
 #include <malloc/malloc.h>
#else
 #include <malloc.h>
#endif

#if defined(OVR_OS_MS)
 #include "OVR_Win32_IncludeWindows.h"
#elif defined(OVR_OS_MAC) || defined(OVR_OS_UNIX)
 #include <unistd.h>
 #include <sys/mman.h>
#endif

namespace OVR {


//-----------------------------------------------------------------------------------
// ***** Allocator

Allocator* Allocator::pInstance = 0;

// Default AlignedAlloc implementation will delegate to Alloc/Free after doing rounding.
void* Allocator::AllocAligned(size_t size, size_t align)
{
    OVR_ASSERT((align & (align-1)) == 0);
    align = (align > sizeof(size_t)) ? align : sizeof(size_t);
    size_t p = (size_t)Alloc(size+align);
    size_t aligned = 0;
    if (p)
    {
        aligned = (size_t(p) + align-1) & ~(align-1);
        if (aligned == p) 
            aligned += align;
        *(((size_t*)aligned)-1) = aligned-p;
    }

    trackAlloc((void*)aligned, size);

    return (void*)aligned;
}

void Allocator::FreeAligned(void* p)
{
    untrackAlloc((void*)p);

    size_t src = size_t(p) - *(((size_t*)p) - 1);
    Free((void*)src);
}


//------------------------------------------------------------------------
// ***** Default Allocator

// This allocator is created and used if no other allocator is installed.
// Default allocator delegates to system malloc.

void* DefaultAllocator::Alloc(size_t size)
{
    void* p = malloc(size);
    trackAlloc(p, size);
    return p;
}
void* DefaultAllocator::AllocDebug(size_t size, const char* file, unsigned line)
{
	OVR_UNUSED2(file, line); // should be here for debugopt config
    void* p;
#if defined(OVR_CC_MSVC) && defined(_CRTDBG_MAP_ALLOC)
    p = _malloc_dbg(size, _NORMAL_BLOCK, file, line);
#else
    p = malloc(size);
#endif
    trackAlloc(p, size);
    return p;
}

void* DefaultAllocator::Realloc(void* p, size_t newSize)
{
    void* newP = realloc(p, newSize);

    // This used to more efficiently check if (newp != p) but static analyzers were erroneously flagging this.
    if(newP) // Need to check newP because realloc doesn't free p unless it returns a valid newP.
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
    return free(p);
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

static TrackedAlloc* TrackHead = nullptr;
static SymbolLookup Symbols;
static bool IsLeakTracking = false;

void Allocator::SetLeakTracking(bool enabled)
{
#if defined(OVR_OS_WIN32) && !defined(OVR_OS_WIN64)
    // HACK: Currently 32-bit leak tracing is too slow to run in real-time on Windows.
    // Note: We can possibly fix this by making a custom Win32 backtrace function which 
    // takes advantage of the fact that we have enabled stack frames in all builds.
    enabled = false;
#endif

    IsLeakTracking = enabled;
}

bool Allocator::IsTrackingLeaks()
{
    return IsLeakTracking;
}

void Allocator::trackAlloc(void* p, size_t size)
{
    if (!p || !IsLeakTracking)
        return;

    Lock::Locker locker(&TrackLock);

    TrackedAlloc* tracked = (TrackedAlloc*)malloc(sizeof(TrackedAlloc));
    if (tracked)
    {
        memset(tracked, 0, sizeof(TrackedAlloc));

        tracked->pAlloc = p;
        tracked->pPrev = nullptr;
        tracked->FrameCount = (uint32_t)Symbols.GetBacktrace(tracked->Callstack, OVR_ARRAY_COUNT(tracked->Callstack), 2);
        tracked->Size = (uint32_t)size;

        tracked->pNext = TrackHead;
        if (TrackHead)
        {
            TrackHead->pPrev = tracked;
        }
        TrackHead = tracked;
    }
}

void Allocator::untrackAlloc(void* p)
{
    if (!p || !IsLeakTracking)
        return;

    Lock::Locker locker(&TrackLock);

    for (TrackedAlloc* t = TrackHead; t; t = t->pNext)
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
            if (TrackHead == t)
            {
                TrackHead = t->pNext;
            }
            free(t);

            break;
        }
    }
}

int DumpMemory()
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

    int leakCount = 0;

    for (TrackedAlloc* t = TrackHead; t; t = t->pNext)
    {
        LogError("[Leak] ** Detected leaked allocation at %p (size = %u) (%d frames)", t->pAlloc, (unsigned)t->Size, (unsigned)t->FrameCount);

        for (size_t i = 0; i < t->FrameCount; ++i)
        {
            SymbolInfo symbolInfo;

            if (symbolLookupAvailable && Symbols.LookupSymbol((uint64_t)t->Callstack[i], symbolInfo) && (symbolInfo.filePath[0] || symbolInfo.function[0]))
            {
                if(symbolInfo.filePath[0])
                    LogText("%s(%d): %s\n", symbolInfo.filePath, symbolInfo.fileLineNumber, symbolInfo.function[0] ? symbolInfo.function : "(unknown function)");
                else
                    LogText("%p (unknown source file): %s\n", t->Callstack[i], symbolInfo.function);
            }
            else
            {
                LogText("%p (symbols unavailable)\n", t->Callstack[i]);
            }
        }

        ++leakCount;
    }

    if (lock)
        lock->Unlock();

    if(symbolLookupAvailable)
        SymbolLookup::Shutdown();

    return leakCount;
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
    #if defined(_WIN32)
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        PageSize = (size_t)systemInfo.dwPageSize;
    #else
        PageSize = 4096;
    #endif

    SetDelayedFreeCount(kFreedBlockArrayMaxSizeDefault);
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

    SetDelayedFreeCount(0);
    FreedBlockArraySize = 0;
    FreedBlockArrayOldest = 0;
}


void DebugPageAllocator::EnableOverrunDetection(bool enableOverrunDetection, bool enableOverrunGuardBytes)
{
    OVR_ASSERT(AllocationCount == 0);

    OverrunPageEnabled       = enableOverrunDetection;
    OverrunGuardBytesEnabled = (enableOverrunDetection && enableOverrunGuardBytes); // Set OverrunGuardBytesEnabled to false if enableOverrunDetection is false.
}


void DebugPageAllocator::SetDelayedFreeCount(size_t delayedFreeCount)
{
    OVR_ASSERT(AllocationCount == 0);

    if(FreedBlockArray)
    {
        SafeMMapFree(FreedBlockArray, FreedBlockArrayMaxSize * sizeof(Block));
        FreedBlockArrayMaxSize = 0;
    }

    if(delayedFreeCount)
    {
        FreedBlockArray = (Block*)SafeMMapAlloc(delayedFreeCount * sizeof(Block));
        OVR_ASSERT(FreedBlockArray);

        if(FreedBlockArray)
        {
            FreedBlockArrayMaxSize = delayedFreeCount;
            #if defined(OVR_BUILD_DEBUG)
                memset(FreedBlockArray, 0, delayedFreeCount * sizeof(Block));
            #endif
        }
    }
}


size_t DebugPageAllocator::GetDelayedFreeCount() const
{
    return FreedBlockArrayMaxSize;
}


void* DebugPageAllocator::Alloc(size_t size)
{
    #if defined(_WIN32)
        return AllocAligned(size, DefaultAlignment);
    #else
        void* p = malloc(size);
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

            untrackAlloc(p);
            AllocationCount--;
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
            OVR_ASSERT(p);

            if(p)
            {
                // Commit all but the last page. Leave the last page as merely reserved so that reads from or writes
                // to it result in an immediate exception.
                p = VirtualAlloc(p, blockSize - PageSize, MEM_COMMIT, PAGE_READWRITE);
                OVR_ASSERT(p);
            }
        }
        else
        {
            // We need to make it so that all pages are MEM_COMMIT + PAGE_READWRITE.

            p = VirtualAlloc(nullptr, blockSize, MEM_COMMIT, PAGE_READWRITE);
        }
    
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
