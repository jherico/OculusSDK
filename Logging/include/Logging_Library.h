/************************************************************************************

Filename    :   Logging.h
Content     :   Logging system
Created     :   Oct 26, 2015
Authors     :   Chris Taylor

Copyright   :   Copyright 2015-2016 Oculus VR, LLC All Rights reserved.

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

#ifndef Logging_h
#define Logging_h

#pragma warning(push)
#pragma warning(disable: 4530) // C++ exception handler used, but unwind semantics are not enabled

#include <string>
#include <sstream>
#include <vector>
#include <queue>
#include <memory>
#include <set>

#pragma warning(push)

#include "Logging_Tools.h"

namespace ovrlog {

struct LogStringBuffer;
class OutputWorker;
class Channel;
class Configurator;


//-----------------------------------------------------------------------------
// Log Level
//
// Log message priority is indicated by its level.  The level can inform how
// prominently it is displayed on the console window or whether or not a
// message is displayed at all.

enum class Level
{

    // Trace message.  This is a message that can potentially happen once per
    // camera/HMD frame and are probably being reviewed after they are recorded
    // since they will scroll by too fast otherwise.
    Trace,

    // Debug message.  This is a verbose log level that can be selectively
    // turned on by the user in the case of perceived problems to help
    // root-cause the problems.  This log level is intended to be used for
    // messages that happen less often than once per camera/HMD frame,
    // such that they can be human readable.
    Debug,

    // Info messages, which should be the default message type for infrequent
    // messages used during subsystem initialization and/or shutdown.  This
    // log level is fairly visible so should be used sparingly.  Expect users to
    // have these turned on, so avoid printing anything here that would obscure
    // Warning and Error messages.
    Info,

    // Warning message, which is also displayed almost everywhere.  For most
    // purposes it is as visible as an Error message, so it should also be used
    // very selectively.  The main difference from Error level is informational
    // as it is just as visible.
    Warning,

    // Highest level of logging.  If any logging is going on it will include this
    // message.  For this reason, please avoid using the Error level unless the
    // message should be displayed absolutely everywhere.
    Error,

    // Number of error levels
    Count, // Used to static assert where updates must happen in code.
};


//-----------------------------------------------------------------------------
// Line of Code
//
// C++11 Trait that can be used to insert the file and line number into a log
// message.  Often times it is a good idea to put these at the start of a log
// message so that the user can click on them to have the code editor IDE jump
// to that line of code.

// Log Line of Code FileLineInfo object.
#if defined(LOGGING_DEBUG)
    #define LOGGING_FILE_LINE_STRING_ __FILE__ "(" LOGGING_STRINGIZE(__LINE__) ")"
    #define LOGGING_LOC LOGGING_FILE_LINE_STRING_
#else
    #define LOGGING_LOC "(no LOC)"
#endif


//-----------------------------------------------------------------------------
// LogStringBuffer
//
// Thread-local buffer for constructing a log message.

struct LogStringBuffer
{
    // Raw pointer to subsystem name
    const char* SubsystemName;

    // Message log level
    Level MessageLogLevel;

    // Buffer containing string as it is constructed
    std::stringstream Stream;
    // TBD: We can optimize this better than std::string
    // TBD: We can remember the last log string size to avoid extra allocations.

    // Flag indicating that the message is being relogged.
    // This is useful to prevent double-logging messages.
    bool Relogged;

    // Ctor
    LogStringBuffer(const char* subsystem, Level level) :
        SubsystemName(subsystem),
        MessageLogLevel(level),
        Stream(),
        Relogged(false)
    {
    }
};


//-----------------------------------------------------------------------------
// LogStringize Override
//
// This is the function that user code can override to control how special types
// are serialized into the log messages.

template<typename T>
LOGGING_INLINE void LogStringize(LogStringBuffer& buffer, const T& first)
{
    buffer.Stream << first;
}

// Overrides for various types we want to handle specially:

template<>
LOGGING_INLINE void LogStringize(LogStringBuffer& buffer, const bool& first)
{
    buffer.Stream << (first ? "true" : "false");
}

template<>
void LogStringize(LogStringBuffer& buffer, wchar_t const * const & first);

template<int N>
LOGGING_INLINE void LogStringize(LogStringBuffer& buffer, const wchar_t(&first)[N])
{
    const wchar_t* str = first;
    LogStringize(buffer, str);
}


//-----------------------------------------------------------------------------
// Log Output Worker Thread
//
// Worker thread that produces the output.
// Call AddPlugin() to register an output plugin.

// User-defined output plugin
class OutputPlugin
{
public:
    virtual ~OutputPlugin() {}

    // Return a unique string naming this output plugin.
    virtual const char* GetUniquePluginName() = 0;

    // Write data to output.
    virtual void Write(Level level, const char* subsystem, const char* header, const char* utf8msg) = 0;
};

// Log Output Worker Thread
class OutputWorker
{
    OutputWorker(); // Use GetInstance() to get the singleton instance.

public:
    // Get singleton instance for logging output
    static OutputWorker* GetInstance();

    ~OutputWorker();

    void InstallDefaultOutputPlugins();

    // Start/stop logging output (started automatically)
    void Start();
    void Stop();

    // Blocks until all log messages before this function call are completed.
    void Flush();

    enum class WriteOption
    {
        // Default log write
        Default,

        // Dangerously ignore the queue limit
        DangerouslyIgnoreQueueLimit
    };

    // Write a log buffer to the output
    void Write(LogStringBuffer& buffer, WriteOption option = WriteOption::Default);

    // Plugin management
    void AddPlugin(std::shared_ptr<OutputPlugin> plugin);
    void RemovePlugin(std::shared_ptr<OutputPlugin> plugin);

    // Disable all output
    void DisableAllPlugins();

private:
    // Is the logger running in a debugger?
    bool IsInDebugger;

    // Plugins
    Lock PluginsLock;
    std::set< std::shared_ptr<OutputPlugin> > Plugins;

    // Worker Log Buffer
    struct QueuedLogMessage
    {
        Level             MessageLogLevel;
        const char*       SubsystemName;
        std::string       Buffer;
        QueuedLogMessage* Next;
        HANDLE            FlushEvent;

        QueuedLogMessage(LogStringBuffer& buffer)
        {
            MessageLogLevel = buffer.MessageLogLevel;
            SubsystemName = buffer.SubsystemName;
            Buffer = buffer.Stream.str();
            Next = nullptr;
            FlushEvent = nullptr;
        }
    };

    // Maximum number of logs that we allow in the queue at a time.
    // If we go beyond this limit, we keep a count of additional logs that were lost.
    static const int WorkQueueLimit = 1000;

    AutoHandle        WorkerWakeEvent;      // Event letting the worker thread know the queue is not empty
    Lock              WorkQueueLock;        // Lock guarding the work queue
    QueuedLogMessage* WorkQueueHead;        // Head of linked list of work that is queued
    QueuedLogMessage* WorkQueueTail;        // Tail of linked list of work that is queued
    int               WorkQueueSize;        // Size of the linked list of queued work
    int               WorkQueueOverrun;     // Number of log messages that exceeded the limit
    // The work queue size is used to avoid overwhelming the logging thread, since it takes 1-2 milliseconds
    // to log out each message it can easily fall behind a large amount of logs.  Lost log messages are added
    // to the WorkQueueOverrun count so that they can be reported as "X logs were lost".

    inline void WorkQueueAdd(QueuedLogMessage* msg)
    {
        if (WorkQueueTail)
        {
            WorkQueueTail->Next = msg;
        }
        else
        {
            WorkQueueHead = msg;
        }
        WorkQueueTail = msg;
        ++WorkQueueSize;
    }

    static DWORD WINAPI WorkerThreadEntrypoint_(void* worker);
    void WorkerThreadEntrypoint();

    Lock StartStopLock;
    Terminator WorkerTerminator;
    AutoHandle LoggingThread;
    volatile bool LoggingFromWorkerThread;

    // Append level and subsystem name to timestamp buffer
    // The buffer should point to the ending null terminator of
    // the timstamp string.
    static void AppendHeader(char* buffer, size_t bufferBytes,
                             Level level, const char* subsystemName);

    void ProcessQueuedMessages();

    void FlushMessageImmediately(LogStringBuffer& buffer);
    void FlushDbgViewLogImmediately(LogStringBuffer& buffer);
};


//-----------------------------------------------------------------------------
// ErrorSilencer
//
// This will demote errors to warnings in the log until it goes out of scope.
// Helper class that allows error silencing to be done several function calls
// up the stack and checked down the stack.
class ErrorSilencer
{
public:
    // Returns true if errors are currently squelched and should not be loud.
    // This can be called from anywhere, even outside of an ErrorSquelch block.
    static bool IsSilenced();

    // Start silencing errors.
    ErrorSilencer(bool initiallySilenced = true);

    // Stop silencing errors.
    ~ErrorSilencer();

public:
    // Start silencing errors.  This is done automatically be the constructor.
    // This function is provided to manually control the silencing behavior.
    void Silence();

    // Stop silencing errors.  This is done automatically be the deconstructor.
    // This function is provided to manually control the silencing behavior.
    void Unsilence();

private:
    // Haas this object requested silencing errors?
    bool ThisObjectCurrentlySilenced = false;
};


//-----------------------------------------------------------------------------
// Channel
//
// One named logging channel.

class Channel
{
public:
    // This name string pointer must not go out of scope for the entire lifetime
    // of the logging output worker.
    // We recommend that the name string must be a string literal not a string
    // allocated on the heap at runtime.
    Channel(const char* nameString);
    ~Channel();

    // Add an extra prefix to all log messages generated by the channel.
    // This function is *not* thread-safe.  Logging from another thread while changing
    // the prefix can cause crashes.
    std::string GetPrefix() const;
    void SetPrefix(const std::string& prefix);

    // Set the minimum output level permitted from this channel.
    void SetMinimumOutputLevel(Level newLevel);

    // Set the output level temporarily for this session without remembering that setting.
    void SetMinimumOutputLevelNoSave(Level newLevel);

    const char* GetName() const;
    Level GetMinimumOutputLevel() const;

    LOGGING_INLINE bool Active(Level level) const
    {
        return MinimumOutputLevel <= level;
    }

    template<typename... Args>
    LOGGING_INLINE void Log(Level level, Args&&... args) const
    {
        if (Active(level))
        {
            // If the log message is at error level and errors are silenced,
            if (level == Level::Error && ErrorSilencer::IsSilenced())
            {
                // Demote to warning
                level = Level::Warning;
            }
            doLog(level, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogError(Args&&... args) const
    {
        if (Active(Level::Error))
        {
            // Demote to warning if errors are silenced
            const Level level = ErrorSilencer::IsSilenced() ?
                Level::Warning : Level::Error;

            doLog(level, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogWarning(Args&&... args) const
    {
        if (Active(Level::Warning))
        {
            doLog(Level::Warning, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogInfo(Args&&... args) const
    {
        if (Active(Level::Info))
        {
            doLog(Level::Info, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogDebug(Args&&... args) const
    {
        if (Active(Level::Debug))
        {
            doLog(Level::Debug, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogTrace(Args&&... args) const
    {
        if (Active(Level::Trace))
        {
            doLog(Level::Trace, std::forward<Args>(args)...);
        }
    }

    // printf style log functions
    template<typename... Args>
    LOGGING_INLINE void LogF(Level level, Args&&... args) const
    {
        if (Active(level))
        {
            // If the log message is at error level and errors are silenced,
            if (level == Level::Error && ErrorSilencer::IsSilenced())
            {
                // Demote to warning
                level = Level::Warning;
            }
            doLogF(level, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogErrorF(Args&&... args) const
    {
        if (Active(Level::Error))
        {
            // Demote to warning if errors are silenced
            const Level level = ErrorSilencer::IsSilenced() ?
                Level::Warning : Level::Error;

            doLogF(level, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogWarningF(Args&&... args) const
    {
        if (Active(Level::Warning))
        {
            doLogF(Level::Warning, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogInfoF(Args&&... args) const
    {
        if (Active(Level::Info))
        {
            doLogF(Level::Info, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogDebugF(Args&&... args) const
    {
        if (Active(Level::Debug))
        {
            doLogF(Level::Debug, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    LOGGING_INLINE void LogTraceF(Args&&... args) const
    {
        if (Active(Level::Trace))
        {
            doLogF(Level::Trace, std::forward<Args>(args)...);
        }
    }

    // DANGER DANGER DANGER
    // This function forces a log message to be recorded even if the log queue is full.
    // This is dangerous because the caller can run far ahead of the output writer thread
    // and cause a large amount of memory to be allocated and logging tasks can take many
    // minutes to flush afterwards.  It should only be used when the data is critical.
    template<typename... Args>
    LOGGING_INLINE void DangerousForceLog(Level level, Args&&... args) const
    {
        if (Active(level))
        {
            // If the log message is at error level and errors are silenced,
            if (level == Level::Error && ErrorSilencer::IsSilenced())
            {
                // Demote to warning
                level = Level::Warning;
            }

            LogStringBuffer buffer(SubsystemName, level);

            writeLogBuffer(buffer, Prefix, args...);

            // Submit buffer to logging subsystem
            OutputWorker::GetInstance()->Write(buffer, OutputWorker::WriteOption::DangerouslyIgnoreQueueLimit);
        }
    }
    // DANGER DANGER DANGER

private:
    //-------------------------------------------------------------------------
    // Internal Implementation

    // Level at which this channel will log.
    Level MinimumOutputLevel;

    // Channel name string
    const char* SubsystemName;

    // Optional prefix
    std::string Prefix;

    template<typename T>
    LOGGING_INLINE void writeLogBuffer(LogStringBuffer& buffer, T&& arg) const
    {
        LogStringize(buffer, arg);
    }

    template<typename T, typename... Args>
    LOGGING_INLINE void writeLogBuffer(LogStringBuffer& buffer, T&& arg, Args&&... args) const
    {
        writeLogBuffer(buffer, arg);
        writeLogBuffer(buffer, args...);
    }

    // Unroll arguments
    template<typename... Args>
    LOGGING_INLINE void doLog(Level level, Args&&... args) const
    {
        LogStringBuffer buffer(SubsystemName, level);

        writeLogBuffer(buffer, Prefix, args...);

        // Submit buffer to logging subsystem
        OutputWorker::GetInstance()->Write(buffer);
    }

    // Returns the buffer capacity required to printf the given format+arguments.
    // Returns -1 if the format is invalid.
    static int GetPrintfLengthV(const char* format, va_list argList)
    {
        int size;

    #if defined(_MSC_VER) // Microsoft doesn't support C99-Standard vsnprintf, so need to use _vscprintf.
        size = _vscprintf(format, argList); // Returns the required strlen, or -1 if format error.
    #else
        size = vsnprintf(nullptr, 0, format, argList); // Returns the required strlen, or negative if format error.
    #endif

        if (size > 0) // If we can 0-terminate the output...
            ++size; // Add one to account for terminating null.
        else
            size = -1;

        return size;
    }


    static int GetPrintfLength(const char* format, ...)
    {
        va_list argList;
        va_start(argList, format);
        int size = GetPrintfLengthV(format, argList);
        va_end(argList);
        return size;
    }


    template<typename... Args>
    LOGGING_INLINE void doLogF(Level level, Args&&... args) const
    {
        LogStringBuffer buffer(SubsystemName, level);

        char  logCharsLocal[1024];
        char* logChars = logCharsLocal;
        char* logCharsAllocated = nullptr;

#if defined(_MSC_VER)
        int result = _snprintf_s(logCharsLocal, sizeof(logCharsLocal), _TRUNCATE, args...);
#else
        int result = snprintf(logCharsLocal, sizeof(logCharsLocal), args...);
#endif

        if ((result < 0) || (result >= sizeof(logCharsLocal)))
        {
            int requiredSize = GetPrintfLength(args...);

            if ((requiredSize < 0) || (requiredSize > (1024 * 1024)))
            {
                LOGGING_DEBUG_BREAK(); // This call should be converted to the new log system.
                return;
            }

            logCharsAllocated = new char[requiredSize];
            logChars = logCharsAllocated;

#if defined(_MSC_VER)
            _snprintf_s(logChars, (size_t)requiredSize, _TRUNCATE, args...);
#else
            snprintf(logChars, (size_t)requiredSize, args...);
#endif
        }

        writeLogBuffer(buffer, Prefix, logChars);

        // Submit buffer to logging subsystem
        OutputWorker::GetInstance()->Write(buffer);

        delete[] logCharsAllocated;
    }
};


//-----------------------------------------------------------------------------
// Log Configurator
//
// Centralized object that can configure and enumerate all the channels.

class ConfiguratorPlugin
{
public:
    ConfiguratorPlugin();
    virtual ~ConfiguratorPlugin();

    // Modify the channel level if it is set, otherwise leave it as-is.
    virtual void RestoreChannelLevel(const char* name, Level& level) = 0;

    // Sets the channel level
    virtual void SaveChannelLevel(const char* name, Level level) = 0;
};

class Configurator
{
    friend class Channel;
    Configurator(); // Call GetInstance() to get the singleton instance.

public:
    // Get singleton instance for logging configurator
    static Configurator* GetInstance();

    ~Configurator();

    void SetGlobalMinimumLogLevel(Level level);

    inline void SilenceLogging()
    {
        // Set the minimum logging level higher than any actual message.
        SetGlobalMinimumLogLevel(Level::Count);
    }

    void SetPlugin(std::shared_ptr<ConfiguratorPlugin> plugin);

private:
    void Register(Channel* channel);
    void Unregister(Channel* channel);
    void OnChannelLevelChange(Channel* channel);

    // List of registered channels
    Lock ChannelsLock;
    Level GlobalMinimumLogLevel;
    std::set<Channel*> Channels;
    std::shared_ptr<ConfiguratorPlugin> Plugin;

    void RestoreChannelLogLevel(Channel* channel);
};


} // namespace ovrlog

#endif // Logging_h
