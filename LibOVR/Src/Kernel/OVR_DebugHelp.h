/************************************************************************************

Filename    :   OVR_DebugHelp.h
Content     :   Platform-independent exception handling interface
Created     :   October 6, 2014

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_ExceptionHandler_h
#define OVR_ExceptionHandler_h


#include "OVR_Types.h"
#include "OVR_String.h"
#include "OVR_Threads.h"
#include "OVR_Atomic.h"
#include "OVR_Nullptr.h"
#include <stdio.h>
#include <time.h>

#if defined(OVR_OS_WIN32) || defined(OVR_OS_WIN64)
    #include <Windows.h>

#elif defined(OVR_OS_APPLE)
    #include <pthread.h>
    #include <mach/thread_status.h>
    #include <mach/mach_types.h>

    extern "C" void* MachHandlerThreadFunctionStatic(void*);
    extern "C" int   catch_mach_exception_raise_state_identity_OVR(mach_port_t, mach_port_t, mach_port_t, exception_type_t, mach_exception_data_type_t*,
                                       mach_msg_type_number_t, int*, thread_state_t, mach_msg_type_number_t, thread_state_t, mach_msg_type_number_t*);
#elif defined(OVR_OS_LINUX)
    #include <pthread.h>
#endif


OVR_DISABLE_MSVC_WARNING(4351) // new behavior: elements of array will be default initialized


namespace OVR { 

    // Thread identifiers
    //typedef void*     ThreadHandle;  // Already defined by OVR Threads. Same as Windows thread handle, Unix pthread_t.
    //typedef void*     ThreadId;      // Already defined by OVR Threads. Used by Windows as DWORD thread id, by Unix as pthread_t. 
    typedef uintptr_t   ThreadSysId;   // System thread identifier. Used by Windows the same as ThreadId (DWORD), thread_act_t on Mac/BSD, lwp id on Linux.

    // Thread constants
    // To do: Move to OVR Threads
    #define OVR_THREADHANDLE_INVALID ((ThreadHandle*)nullptr)
    #define OVR_THREADID_INVALID     ((ThreadId*)nullptr)
    #define OVR_THREADSYSID_INVALID  ((uintptr_t)0)

    OVR::ThreadSysId  ConvertThreadHandleToThreadSysId(OVR::ThreadHandle threadHandle);
    OVR::ThreadHandle ConvertThreadSysIdToThreadHandle(OVR::ThreadSysId threadSysId);   // The returned handle must be freed with FreeThreadHandle.
    void              FreeThreadHandle(OVR::ThreadHandle threadHandle);                 // Frees the handle returned by ConvertThreadSysIdToThreadHandle.
    OVR::ThreadSysId  GetCurrentThreadSysId();

    // CPUContext
    #if defined(OVR_OS_MS)
        typedef CONTEXT CPUContext; 
    #elif defined(OVR_OS_MAC)
        struct CPUContext
        {
            x86_thread_state_t  threadState; // This works for both x86 and x64.
            x86_float_state_t   floatState;
            x86_debug_state_t   debugState;
            x86_avx_state_t     avxState;
            x86_exception_state exceptionState;
            
            CPUContext() { memset(this, 0, sizeof(CPUContext)); }
        };
    #elif defined(OVR_OS_LINUX)
        typedef int CPUContext; // To do.
    #endif


    // Tells if the current process appears to be running under a debugger. Does not attempt to 
    // detect the case of sleath debuggers (malware-related for example).
    bool OVRIsDebuggerPresent();

    // Exits the process with the given exit code.
    #if !defined(OVR_NORETURN)
        #if defined(OVR_CC_MSVC)
            #define OVR_NORETURN __declspec(noreturn)
        #else
            #define OVR_NORETURN __attribute__((noreturn))
        #endif
    #endif
    OVR_NORETURN void ExitProcess(intptr_t processReturnValue);

    // Returns the instruction pointer of the caller for the position right after the call.
    OVR_NO_INLINE void GetInstructionPointer(void*& pInstruction);

    // Returns the stack base and limit addresses for the given thread, or for the current thread if the threadHandle is default.
    // The stack limit is a value less than the stack base on most platforms, as stacks usually grow downward.
    // Some platforms (e.g. Microsoft) have dynamically resizing stacks, in which case the stack limit reflects the current limit.
    void GetThreadStackBounds(void*& pStackBase, void*& pStackLimit, ThreadHandle threadHandle = OVR_THREADHANDLE_INVALID);


    // Equates to VirtualAlloc/VirtualFree on Windows, mmap/munmap on Unix.
    // These are useful for when you need system-supplied memory pages. 
    // These are also useful for when you need to allocate memory in a way 
    // that doesn't affect the application heap.
    void* SafeMMapAlloc(size_t size);
    void  SafeMMapFree(const void* memory, size_t size);


    // OVR_MAX_PATH
    // Max file path length (for most uses).
    // To do: move this to OVR_File.
    #if defined(OVR_OS_MS)         // http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx
        #define OVR_MAX_PATH  260  // Windows can use paths longer than this in some cases (network paths, UNC paths).
    #else
        #define OVR_MAX_PATH 1024  // This isn't a strict limit on all Unix-based platforms.
    #endif


    // ModuleHandle
    #if defined(OVR_OS_MS)
        typedef void* ModuleHandle;  // from LoadLibrary()
    #elif defined(OVR_OS_APPLE) || defined(OVR_OS_UNIX)
        typedef void* ModuleHandle;  // from dlopen()
    #endif

    #define OVR_MODULEHANDLE_INVALID ((ModuleHandle*)nullptr)



    // Module info constants
    static const ModuleHandle kMIHandleInvalid          = OVR_MODULEHANDLE_INVALID;
    static const uint64_t     kMIAddressInvalid         = 0xffffffffffffffffull;
    static const uint64_t     kMISizeInvalid            = 0xffffffffffffffffull;
    static const int32_t      kMILineNumberInvalid      = -1;
    static const int32_t      kMIFunctionOffsetInvalid  = -1;
    static const uint64_t     kMIBaseAddressInvalid     = 0xffffffffffffffffull;
    static const uint64_t     kMIBaseAddressUnspecified = 0xffffffffffffffffull;

    struct ModuleInfo
    {
        ModuleHandle handle;
        uint64_t     baseAddress;           // The actual runtime base address of the module. May be different from the base address specified in the debug symbol file.
        uint64_t     size;
        char         filePath[OVR_MAX_PATH];
        char         name[32];
        char         type[8];               // Unix-specific. e.g. __TEXT
        char         permissions[8];        // Unix specific. e.g. "drwxr-xr-x"

        ModuleInfo() : handle(kMIHandleInvalid), baseAddress(kMIBaseAddressInvalid), size(0), filePath(), name(){}
    };


    // Refers to symbol info for an instruction address. 
    // Info includes function name, source code file/line, and source code itself.
    struct SymbolInfo
    {
        uint64_t          address;
        uint64_t          size;
        const ModuleInfo* pModuleInfo;
        char              filePath[OVR_MAX_PATH];
        int32_t           fileLineNumber;
        char              function[128];            // This is a fixed size because we need to use it during application exceptions.
        int32_t           functionOffset;
        char              sourceCode[1024];         // This is a string representing the code itself and not a file path to the code.

        SymbolInfo() : address(kMIAddressInvalid), size(kMISizeInvalid), pModuleInfo(nullptr), filePath(), 
                        fileLineNumber(kMILineNumberInvalid), function(), functionOffset(kMIFunctionOffsetInvalid), sourceCode() {}
    };


    // Implements support for reading thread lists, module lists, backtraces, and backtrace symbols.
    class SymbolLookup
    {
    public:
        SymbolLookup();
       ~SymbolLookup();

        void AddSourceCodeDirectory(const char* pDirectory);

        bool Initialize();
        void Shutdown();

        // Should be disabled when within an exception handler.
        void EnableMemoryAllocation(bool enabled);
        
        // Retrieves the backtrace (call stack) of the given thread. There may be some per-platform restrictions on this.
        // Returns the number written, which will be <= addressArrayCapacity.
        // This may not work on some platforms unless stack frames are enabled.
        // For Microsoft platforms the platformThreadContext is CONTEXT*.
        // For Apple platforms the platformThreadContext is x86_thread_state_t* or arm_thread_state_t*.
        // If threadSysIdHelp is non-zero, it may be used by the implementation to help produce a better backtrace.
        size_t GetBacktrace(void* addressArray[], size_t addressArrayCapacity, size_t skipCount = 0, void* platformThreadContext = nullptr, OVR::ThreadSysId threadSysIdHelp = OVR_THREADSYSID_INVALID);

        // Retrieves the backtrace for the given ThreadHandle.
        // Returns the number written, which will be <= addressArrayCapacity.
        size_t GetBacktraceFromThreadHandle(void* addressArray[], size_t addressArrayCapacity, size_t skipCount = 0, OVR::ThreadHandle threadHandle = OVR_THREADHANDLE_INVALID);

        // Retrieves the backtrace for the given ThreadSysId.
        // Returns the number written, which will be <= addressArrayCapacity.
        size_t GetBacktraceFromThreadSysId(void* addressArray[], size_t addressArrayCapacity, size_t skipCount = 0, OVR::ThreadSysId threadSysId = OVR_THREADSYSID_INVALID);

        // Gets a list of the modules (e.g. DLLs) present in the current process.
        // Writes as many ModuleInfos as possible to pModuleInfoArray.
        // Returns the required count of ModuleInfos, which will be > moduleInfoArrayCapacity if the capacity needs to be larger.
        size_t GetModuleInfoArray(ModuleInfo* pModuleInfoArray, size_t moduleInfoArrayCapacity);

        // Retrieves a list of the current threads. Unless the process is paused the list is volatile.
        // Returns the required capacity, which may be larger than threadArrayCapacity.
        // Either array can be NULL to specify that it's not written to.
        // For Windows the caller needs to CloseHandle the returned ThreadHandles. This can be done by calling DoneThreadList.
        size_t GetThreadList(ThreadHandle* threadHandleArray, ThreadSysId* threadSysIdArray, size_t threadArrayCapacity);

        // Frees any references to thread handles or ids returned by GetThreadList;
        void DoneThreadList(ThreadHandle* threadHandleArray, ThreadSysId* threadSysIdArray, size_t threadArrayCount);

        // Writes a given thread's callstack with symbols to the given output.
        // It may not be safe to call this from an exception handler, as sOutput allocates memory.
        bool ReportThreadCallstack(OVR::String& sOutput, size_t skipCount = 0, ThreadSysId threadSysId = OVR_THREADSYSID_INVALID);

        // Writes all thread's callstacks with symbols to the given output.
        // It may not be safe to call this from an exception handler, as sOutput allocates memory.
        bool ReportThreadCallstacks(OVR::String& sOutput, size_t skipCount = 0);

        // Retrieves symbol info for the given address. 
        bool LookupSymbol(uint64_t address, SymbolInfo& symbolInfo);
        bool LookupSymbols(uint64_t* addressArray, SymbolInfo* pSymbolInfoArray, size_t arraySize);

        const ModuleInfo* GetModuleInfoForAddress(uint64_t address);  // The returned ModuleInfo points to an internal structure.

    protected:
        bool RefreshModuleList();

    protected:
        bool       initialized;
        bool       allowMemoryAllocation;   // True by default. If true then we allow allocating memory (and as a result provide less information). This is useful for when in an exception handler.
        bool       moduleListUpdated;
        ModuleInfo moduleInfoArray[96];     // Cached list of modules we use. This is a fixed size because we need to use it during application exceptions.
        size_t     moduleInfoArraySize;
    };



    // ExceptionInfo
    // We need to be careful to avoid data types that can allocate memory while we are 
    // handling an exception, as the memory system may be corrupted at that point in time.
    struct ExceptionInfo
    {
        tm            time;                             // GM time.
        time_t        timeVal;                          // GM time_t (seconds since 1970).
        void*         backtrace[64];
        size_t        backtraceCount;
        ThreadHandle  threadHandle;                     // 
        ThreadSysId   threadSysId;                      // 
        char          threadName[32];                   // Cannot be an allocating String object.
        void*         pExceptionInstructionAddress;
        void*         pExceptionMemoryAddress;
        CPUContext    cpuContext;
        char          exceptionDescription[1024];       // Cannot be an allocating String object.
        SymbolInfo    symbolInfo;                       // SymbolInfo for the exception location.

        #if defined(OVR_OS_MS)
            EXCEPTION_RECORD exceptionRecord;           // This is a Windows SDK struct.
        #elif defined(OVR_OS_APPLE)
            uint64_t         exceptionType;             // e.g. EXC_BAD_INSTRUCTION, EXC_BAD_ACCESS, etc.
            uint32_t         cpuExceptionId;            // The x86/x64 CPU trap id.
            uint32_t         cpuExceptionIdError;       // The x86/x64 CPU trap id extra info.
            int64_t          machExceptionDetail[4];    // Kernel exception code info.
            int              machExceptionDetailCount;  // Count of valid entries.
        #endif

        ExceptionInfo();
    };


    // Implments support for asynchronous exception handling and basic exception report generation.
    // If you are implementing exception handling for a commercial application and want auto-uploading
    // functionality you may want to consider using something like Google Breakpad. This exception handler
    // is for in-application debug/diagnostic services, though it can write a report that has similar
    // information to Breakpad or OS-provided reports such as Apple .crash files.
    //
    // Example usage:
    //     ExceptionHandler exceptionHandler;
    //
    //     int main(int, char**)
    //     {
    //          exceptionHandler.Enable(true);
    //          exceptionHandler.SetExceptionListener(pSomeListener, 0);  // Optional listener hook.
    //     }
    // 
    class ExceptionHandler
    {
    public:
        ExceptionHandler();
       ~ExceptionHandler();

        bool Enable(bool enable);
        
        // Some report info can be considered private information of the user, such as the current process list,
        // computer name, IP address or other identifying info, etc. We should not report this information for
        // external users unless they agree to this.
        void EnableReportPrivacy(bool enable);

        struct ExceptionListener
        {
            virtual ~ExceptionListener(){}
            virtual int HandleException(uintptr_t userValue, ExceptionHandler* pExceptionHandler, ExceptionInfo* pExceptionInfo, const char* reportFilePath) = 0;
        };

        void SetExceptionListener(ExceptionListener* pExceptionListener, uintptr_t userValue);

        // What we do after handling the exception.
        enum ExceptionResponse
        {
            kERContinue,    // Continue execution. Will result in the exception being re-generated unless the application has fixed the cause. Similar to Windows EXCEPTION_CONTINUE_EXECUTION.
            kERHandle,      // Causes the OS to handle the exception as it normally would. Similar to Windows EXCEPTION_EXECUTE_HANDLER.
            kERTerminate,   // Exit the application.
            kERThrow,       // Re-throw the exception. Other handlers may catch it. Similar to Windows EXCEPTION_CONTINUE_SEARCH.
            kERDefault      // Usually set to kERTerminate.
        };

        void SetExceptionResponse(ExceptionResponse er)
            { exceptionResponse = er; }

        // Allws you to add an arbitrary description of the current application, which will be added to exception reports.
        void SetAppDescription(const char* appDescription);

        // If the report path has a "%s" in its name, then assume the path is a sprintf format and write it 
        // with the %s specified as a date/time string.
        // The report path can be "default" to signify that you want to use the default user location.
        // Example usage:
        //     handler.SetExceptionPaths("/Users/Current/Exceptions/Exception %s.txt");
        void SetExceptionPaths(const char* exceptionReportPath, const char* exceptionMinidumpPath = nullptr);

        // Allows you to specify base directories for code paths, which can be used to associate exception addresses to lines 
        // of code as opposed to just file names and line numbers, or function names plus binary offsets.
        void SetCodeBaseDirectoryPaths(const char* codeBaseDirectoryPathArray[], size_t codeBaseDirectoryPathCount);

		// Given an exception report at a given file path, returns a string suitable for displaying in a message
		// box or similar user interface during the handling of an exception. The returned string must be passed
		// to FreeMessageBoxText when complete.
		static const char* GetExceptionUIText(const char* exceptionReportPath);
		static void FreeExceptionUIText(const char* messageBoxText);

    protected:
        void WriteExceptionDescription();
        void WriteReport();
        void WriteReportLine(const char* pLine);
        void WriteReportLineF(const char* format, ...);
        void WriteThreadCallstack(ThreadHandle threadHandle, ThreadSysId threadSysId, const char* additionalInfo);
        void WriteMiniDump();

        // Runtime constants
        bool               enabled;
        bool               reportPrivacyEnabled;        // Defaults to true.
        ExceptionResponse  exceptionResponse;           // Defaults to kERHandle
        ExceptionListener* exceptionListener;
        uintptr_t          exceptionListenerUserValue;
        String             appDescription;
        String             codeBasePathArray[6];        // 6 is arbitrary.
        char               reportFilePath[OVR_MAX_PATH];// May be an encoded path, in that it has "%s" in it or is named "default". See reporFiletPathActual for the runtime actual report path.
        int                miniDumpFlags;
        char               miniDumpFilePath[OVR_MAX_PATH];
        FILE*              file;                        // Can/should we use OVR Files for this?
        char               scratchBuffer[4096];
        SymbolLookup       symbolLookup;

        // Runtime variables
        bool                     exceptionOccurred;
        OVR::AtomicInt<uint32_t> handlingBusy;
        char                     reportFilePathActual[OVR_MAX_PATH];
        char                     minidumpFilePathActual[OVR_MAX_PATH];
        int                      terminateReturnValue;
        ExceptionInfo            exceptionInfo;

        #if defined(OVR_OS_MS)
            void*                        vectoredHandle;
            LPTOP_LEVEL_EXCEPTION_FILTER previousFilter;
            LPEXCEPTION_POINTERS         pExceptionPointers;

            friend LONG WINAPI Win32ExceptionFilter(LPEXCEPTION_POINTERS pExceptionPointers);
            LONG ExceptionFilter(LPEXCEPTION_POINTERS pExceptionPointers);

        #elif defined(OVR_OS_APPLE)
            struct SavedExceptionPorts
            {
                SavedExceptionPorts() : count(0) { memset(this, 0, sizeof(SavedExceptionPorts)); }

                mach_msg_type_number_t count;
                exception_mask_t       masks[6];
                exception_handler_t    ports[6];
                exception_behavior_t   behaviors[6];
                thread_state_flavor_t  flavors[6];
            };

            friend void* ::MachHandlerThreadFunctionStatic(void*);
            friend int ::catch_mach_exception_raise_state_identity_OVR(mach_port_t, mach_port_t, mach_port_t, exception_type_t,
                                        mach_exception_data_type_t*, mach_msg_type_number_t, int*, thread_state_t,
                                        mach_msg_type_number_t, thread_state_t, mach_msg_type_number_t*);
        
            bool          InitMachExceptionHandler();
            void          ShutdownMachExceptionHandler();
            void*         MachHandlerThreadFunction();
            kern_return_t HandleMachException(mach_port_t port, mach_port_t thread, mach_port_t task, exception_type_t exceptionType,
                                              mach_exception_data_type_t* pExceptionDetail, mach_msg_type_number_t exceptionDetailCount, 
                                              int* pFlavor, thread_state_t pOldState, mach_msg_type_number_t oldStateCount, thread_state_t pNewState,
                                              mach_msg_type_number_t* pNewStateCount);
            kern_return_t ForwardMachException(mach_port_t thread, mach_port_t task, exception_type_t exceptionType,
                                               mach_exception_data_t pExceptionDetail, mach_msg_type_number_t exceptionDetailCount);

            bool                machHandlerInitialized;
            mach_port_t         machExceptionPort;
            SavedExceptionPorts machExceptionPortsSaved;
            volatile bool       machThreadShouldContinue;
            volatile bool       machThreadExecuting;
            pthread_t           machThread;

        #elif defined(OVR_OS_LINUX)
            // To do.
        #endif
    };


    // Identifies basic exception types for the CreateException function.
    enum CreateExceptionType
    {
        kCETAccessViolation,      // Read or write to inaccessable memory.
        kCETAlignment,            // Misaligned read or write.
        kCETDivideByZero,         // Integer divide by zero.
        kCETFPU,                  // Floating point / VPU exception.
        kCETIllegalInstruction,   // Illegal opcode.
        kCETStackCorruption,      // Stack frame was corrupted.
        kCETStackOverflow,        // Stack ran out of space, often due to infinite recursion.
        kCETTrap                  // System/OS trap (system call).
    };


    // Creates an exception of the given type, primarily for testing.
    void CreateException(CreateExceptionType exceptionType);



} // namespace OVR


OVR_RESTORE_MSVC_WARNING()


#endif // Header include guard
