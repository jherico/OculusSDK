/************************************************************************************

Filename    :   Logging.cpp
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

#include "../include/Logging_Library.h"
#include "../include/Logging_OutputPlugins.h"

#pragma warning(push)
#pragma warning(disable: 4530) // C++ exception handler used, but unwind semantics are not enabled

#include <time.h>
#include <string.h>

#pragma warning(push)

namespace ovrlog {


//-----------------------------------------------------------------------------
// Log Output Worker Thread

OutputWorker* OutputWorker::GetInstance()
{
    static OutputWorker worker;
    return &worker;
}

OutputWorker::OutputWorker() :
    IsInDebugger(false),
    PluginsLock(),
    Plugins(),
    WorkerWakeEvent(),
    WorkQueueLock(),
    WorkQueueHead(nullptr),
    WorkQueueTail(nullptr),
    WorkQueueSize(0),
    WorkQueueOverrun(0),
    StartStopLock(),
    LoggingFromWorkerThread(false),
    WorkerTerminator(),
    LoggingThread()
{
    // Create a worker wake event
    WorkerWakeEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);

    IsInDebugger = IsDebuggerAttached();

    InstallDefaultOutputPlugins();

    Start();

    if (IsInDebugger)
    {
        LogStringBuffer buffer("Logging", Level::Warning);
        buffer.Stream << "Running from a debugger. Most log output will be written from a background thread. Only DbgView (MSVC Output Window) logs will be flushed immediately.";
        Write(buffer);
    }
}

OutputWorker::~OutputWorker()
{
    // If we were not already explicitly shutdown,
    if (LoggingThread.IsValid())
    {
        // Error in user code:
        // Before ending your main() function be sure to call
        //   ovrlog::OutputWorker::GetInstance()->Stop();
        // This allows the logging subsystem to finish writing before closing,
        // as otherwise the log will be truncated and data will be lost.
        LOGGING_DEBUG_BREAK();
    }

    Stop();
}

void OutputWorker::InstallDefaultOutputPlugins()
{
    // These are the default outputs for ALL applications:

    // If debugger is *not* attached,
    if (!IsInDebugger)
    {
        // Enable event log output.  This logger is fairly slow, taking about 1 millisecond per log,
        // and this is very expensive to flush after each log message for debugging.  Since we almost
        // never use the Event Log when debugging apps it is better to simply leave it out.
        AddPlugin(std::make_shared<OutputEventLog>());

        // Do not log to the DbgView output from the worker thread.  When a debugger is attached we
        // instead flush directly to the DbgView log so that the messages are available at breakpoints.
        AddPlugin(std::make_shared<OutputDbgView>());
    }

    // If there is a console window,
    if (GetConsoleWindow() != NULL)
    {
        // Enable the console.  This logger takes 3 milliseconds per message, so it is fairly
        // slow and should be avoided if it is not needed (ie. console is not shown).
        AddPlugin(std::make_shared<OutputConsole>());
    }
}

void OutputWorker::AddPlugin(std::shared_ptr<OutputPlugin> plugin)
{
    if (!plugin)
    {
        return;
    }

    Locker locker(PluginsLock);

    RemovePlugin(plugin);

    Plugins.insert(plugin);
}

void OutputWorker::RemovePlugin(std::shared_ptr<OutputPlugin> pluginToRemove)
{
    if (!pluginToRemove)
    {
        return;
    }

    const char* nameOfPluginToRemove = pluginToRemove->GetUniquePluginName();

    Locker locker(PluginsLock);

    for (auto& existingPlugin : Plugins)
    {
        const char* existingPluginName = existingPlugin->GetUniquePluginName();

        // If the names match exactly,
        if (0 == strcmp(nameOfPluginToRemove, existingPluginName))
        {
            Plugins.erase(existingPlugin);
            break;
        }
    }
}

void OutputWorker::DisableAllPlugins()
{
    Locker locker(PluginsLock);

    Plugins.clear();
}

void OutputWorker::Start()
{
    // Hold start-stop lock to prevent Start() and Stop() from being called at the same time.
    Locker startStopLocker(StartStopLock);

    // If already started,
    if (LoggingThread.IsValid())
    {
        return; // Nothing to do!
    }

    if (!WorkerTerminator.Initialize())
    {
        // Unable to create worker terminator event?
        LOGGING_DEBUG_BREAK();
        return;
    }

    LoggingThread = ::CreateThread(
        nullptr, // No thread security attributes
        0, // Default stack size
        &OutputWorker::WorkerThreadEntrypoint_, // Thread entrypoint
        this, // This parameter
        0, // No creation flags, start immediately
        nullptr); // Do not request thread id

    if (!LoggingThread.IsValid())
    {
        // Unable to create worker thread?
        LOGGING_DEBUG_BREAK();
        return;
    }

    // Scoped work queue lock:
    {
        // Set the flag to indicate the background thread should be used for log output.
        // Log messages that happen before this point will flush immediately and not get
        // lost so there is no race condition danger here.
        Locker workQueueLock(WorkQueueLock);
        LoggingFromWorkerThread = true;
    }
}

void OutputWorker::Stop()
{
    // Hold start-stop lock to prevent Start() and Stop() from being called at the same time.
    Locker startStopLocker(StartStopLock);

    if (LoggingThread.IsValid())
    {
        // Flag termination
        WorkerTerminator.Terminate();

        // Wait for thread to end
        ::WaitForSingleObject(
            LoggingThread.Get(), // Thread handle
            INFINITE); // Wait forever for thread to terminate

        LoggingThread.Clear();
    }

    // Hold scoped work queue lock:
    {
        // This ensures that logs are not printed out of order on Stop(), and that Flush()
        // can use the flag to check if a flush has already occurred.
        Locker workQueueLock(WorkQueueLock);

        // Set the flag here with the lock held as a sync point for calls into the logger.
        LoggingFromWorkerThread = false;

        // Finish the last set of queued messages to avoid losing any before Stop() returns.
        ProcessQueuedMessages();
    }
}

void OutputWorker::Flush()
{
    AutoHandle flushEvent;

    // Scoped work queue lock:
    {
        Locker workQueueLock(WorkQueueLock);

        // If we are already flushing immediately,
        if (!LoggingFromWorkerThread)
        {
            // Flush function does nothing in this case.
            return;
        }

        // Generate a flush event
        flushEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        LogStringBuffer buffer("Logging", ovrlog::Level::Info);
        QueuedLogMessage* queuedBuffer = new QueuedLogMessage(buffer);
        queuedBuffer->FlushEvent = flushEvent.Get();

        // Add queued buffer to the end of the work queue
        WorkQueueAdd(queuedBuffer);

        // Wake the worker thread
        ::SetEvent(WorkerWakeEvent.Get());
    }

    // Wait until the event signals.
    // Since we are guaranteed to never lose log messages, as late as Stop() being called,
    // this cannot cause a hang.
    ::WaitForSingleObject(flushEvent.Get(), INFINITE);
}

// Returns number of bytes written to buffer
// Precondition: Buffer is large enough to hold everything,
// so don't bother complaining there isn't enough length checking.
static int GetTimestamp(char* buffer, int bufferBytes)
{
    // Get timestamp string
    SYSTEMTIME time;
    ::GetLocalTime(&time);
    int writtenChars = ::GetTimeFormatA( // Intentionally using 'A' version.
        LOCALE_USER_DEFAULT, // User locale
        0, // Default flags
        &time, // Time
        "HH:mm:ss", // No format string
        buffer, // Output buffer
        bufferBytes); // Size of buffer in tchars

    if (writtenChars <= 0)
    {
        // Failure
        buffer[0] = '\0';
        return 0;
    }

    // Append milliseconds
    buffer[writtenChars - 1] = '.';
    buffer[writtenChars]     = ((time.wMilliseconds / 100) % 10) + '0';
    buffer[writtenChars + 1] = ((time.wMilliseconds / 10) % 10) + '0';
    buffer[writtenChars + 2] = (time.wMilliseconds % 10) + '0';

    return writtenChars + 3;
}

static void WriteAdvanceStrCpy(char*& buffer, size_t& bufferBytes, const char* str)
{
    // Get length of string to copy into buffer
    size_t slen = strlen(str);

    // If the resulting buffer cannot accommodate the string and a null terminator,
    if (bufferBytes < slen + 1)
    {
        // Do nothing
        return;
    }

    // Copy string to buffer
    memcpy(buffer, str, slen);

    // Advance buffer by number of bytes copied
    buffer += slen;
    bufferBytes -= slen;
}

void OutputWorker::AppendHeader(char* buffer, size_t bufferBytes, Level level, const char* subsystemName)
{
    // Writes <L> [SubSystem] to the provided buffer.

    // Based on message log level,
    const char* initial = "";
    switch (level)
    {
    case Level::Trace:   initial = " {TRACE}   ["; break;
    case Level::Debug:   initial = " {DEBUG}   ["; break;
    case Level::Info:    initial = " {INFO}    ["; break;
    case Level::Warning: initial = " {WARNING} ["; break;
    case Level::Error:   initial = " {!ERROR!} ["; break;
    default:             initial = " {???}     ["; break;
    }
    static_assert(Level::Count == static_cast<Level>(5), "Needs updating");

    WriteAdvanceStrCpy(buffer, bufferBytes, initial);
    WriteAdvanceStrCpy(buffer, bufferBytes, subsystemName);
    WriteAdvanceStrCpy(buffer, bufferBytes, "] ");
    buffer[0] = '\0';
}

DWORD WINAPI OutputWorker::WorkerThreadEntrypoint_(void* vworker)
{
    // Invoke thread entry-point
    OutputWorker* worker = reinterpret_cast<OutputWorker*>(vworker);
    if (worker)
    {
        worker->WorkerThreadEntrypoint();
    }
    return 0;
}

void OutputWorker::ProcessQueuedMessages()
{
    static const int TempBufferBytes = 1024; // 1 KiB
    char HeaderBuffer[TempBufferBytes];

    QueuedLogMessage* message = nullptr;

    // Pull messages off the queue
    int lostCount = 0;
    {
        Locker locker(WorkQueueLock);
        message = WorkQueueHead;
        WorkQueueHead = WorkQueueTail = nullptr;
        lostCount = WorkQueueOverrun;
        WorkQueueOverrun = 0;
        WorkQueueSize = 0;
    }

    if (message == nullptr)
    {
        // No data to process
        return;
    }

    // Get timestamp string
    int timestampLength = GetTimestamp(HeaderBuffer, TempBufferBytes);
    if (timestampLength <= 0)
    {
        LOGGING_DEBUG_BREAK(); return; // Maybe bug in timestamp code?
    }

    // Log output format:
    // TIMESTAMP <L> [SubSystem] Message

    // If some messages were lost,
    if (lostCount > 0)
    {
        LogStringBuffer buffer("Logging", Level::Error);
        buffer.Stream << "Lost " << lostCount << " log messages due to queue overrun; try to reduce the amount of logging";

        // Insert at the front of the list
        QueuedLogMessage* queuedMsg = new QueuedLogMessage(buffer);
        queuedMsg->Next = message;
        message = queuedMsg;
    }

    {
        Locker locker(PluginsLock);

        // For each log message,
        for (QueuedLogMessage* next; message; message = next)
        {
            // If the message is a flush event,
            if (message->FlushEvent != nullptr)
            {
                // Signal it to wake up the waiting Flush() call.
                ::SetEvent(message->FlushEvent);
            }
            else
            {
                // Construct header on top of timestamp buffer
                AppendHeader(HeaderBuffer + timestampLength, sizeof(HeaderBuffer) - timestampLength,
                    message->MessageLogLevel, message->SubsystemName);

                // For each plugin,
                for (auto& plugin : Plugins)
                {
                    plugin->Write(
                        message->MessageLogLevel,
                        message->SubsystemName,
                        HeaderBuffer,
                        message->Buffer.c_str());
                }
            }

            next = message->Next;
            delete message;
        }
    }
}

void OutputWorker::FlushMessageImmediately(LogStringBuffer& buffer)
{
    static const int TempBufferBytes = 1024; // 1 KiB
    char HeaderBuffer[TempBufferBytes];

    // Get timestamp string
    int timestampLength = GetTimestamp(HeaderBuffer, TempBufferBytes);
    if (timestampLength <= 0)
    {
        LOGGING_DEBUG_BREAK(); return; // Maybe bug in timestamp code?
    }

    // Construct log header on top of timestamp buffer
    AppendHeader(HeaderBuffer + timestampLength, sizeof(HeaderBuffer) - timestampLength,
                 buffer.MessageLogLevel, buffer.SubsystemName);

    std::string messageString = buffer.Stream.str();

    {
        Locker locker(PluginsLock);

        // For each plugin,
        for (auto& plugin : Plugins)
        {
            plugin->Write(
                buffer.MessageLogLevel,
                buffer.SubsystemName,
                HeaderBuffer,
                messageString.c_str());
        }
    }
}

void OutputWorker::FlushDbgViewLogImmediately(LogStringBuffer& buffer)
{
    static const int TempBufferBytes = 1024; // 1 KiB
    char HeaderBuffer[TempBufferBytes];

    // Get timestamp string
    int timestampLength = GetTimestamp(HeaderBuffer, TempBufferBytes);
    if (timestampLength <= 0)
    {
        LOGGING_DEBUG_BREAK(); return; // Maybe bug in timestamp code?
    }

    // Construct log header on top of timestamp buffer
    AppendHeader(HeaderBuffer + timestampLength, sizeof(HeaderBuffer) - timestampLength,
        buffer.MessageLogLevel, buffer.SubsystemName);

    // Build up a single string to send to OutputDebugStringA so it
    // all appears on the same line in DbgView.
    std::stringstream ss;
    ss << HeaderBuffer << buffer.Stream.str() << "\n";

    ::OutputDebugStringA(ss.str().c_str());
}

void OutputWorker::WorkerThreadEntrypoint()
{
    // Lower the priority for logging.
    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_LOWEST);

    while (!WorkerTerminator.IsTerminated())
    {
        if (WorkerTerminator.WaitOn(WorkerWakeEvent.Get()))
        {
            ProcessQueuedMessages();
        }
    }
}

void OutputWorker::Write(LogStringBuffer& buffer, WriteOption option)
{
    // If not logging from the worker thread,
    if (!LoggingFromWorkerThread)
    {
        FlushMessageImmediately(buffer);
        return;
    }

    QueuedLogMessage* queuedBuffer = new QueuedLogMessage(buffer);
    bool dropped = false; // Flag indicates if the message was dropped due to queue overrun
    bool needToWakeWorkerThread = false; // Flag indicates if we need to wake the worker thread

    // Add work to queue.
    {
        Locker locker(WorkQueueLock);

        // If not logging from worker thread,
        if (!LoggingFromWorkerThread)
        {
            // Immediately dequeue and log it.  Yes this is slow but it only happens in transient cases.
            FlushMessageImmediately(buffer);
            dropped = true;
        }
        // If no room in the queue,
        else if (option != WriteOption::DangerouslyIgnoreQueueLimit &&
                 WorkQueueSize >= WorkQueueLimit)
        {
            // Record drop
            WorkQueueOverrun++;
            dropped = true;
        }
        else
        {
            // Add queued buffer to the end of the work queue
            WorkQueueAdd(queuedBuffer);

            // Only need to wake the worker thread on the first message
            // The SetEvent() call takes 6 microseconds or so
            if (WorkQueueSize <= 1)
            {
                needToWakeWorkerThread = true;
            }
        }
    }

    if (dropped)
    {
        delete queuedBuffer;
    }
    else if (needToWakeWorkerThread)
    {
        // Wake the worker thread
        ::SetEvent(WorkerWakeEvent.Get());
    }

    // If this is the first time logging this message,
    if (!buffer.Relogged)
    {
        // If we are in a debugger,
        if (IsInDebugger)
        {
            FlushDbgViewLogImmediately(buffer);
        }
    }
}


//-----------------------------------------------------------------------------
// Channel

Channel::Channel(const char* nameString) :
    SubsystemName(nameString),
    MinimumOutputLevel(Level::Info)
{
    SubsystemName = nameString;

    Configurator::GetInstance()->Register(this);
    // We can get modified from other threads here...
}

Channel::~Channel()
{
    // ...We can get modified from other threads here.
    Configurator::GetInstance()->Unregister(this);
}

std::string Channel::GetPrefix() const
{
    return Prefix;
}

void Channel::SetPrefix(const std::string& prefix)
{
    Prefix = prefix;
}

void Channel::SetMinimumOutputLevel(Level newLevel)
{
    SetMinimumOutputLevelNoSave(newLevel);

    Configurator::GetInstance()->OnChannelLevelChange(this);
}

void Channel::SetMinimumOutputLevelNoSave(Level newLevel)
{
    MinimumOutputLevel = newLevel;
}

const char* Channel::GetName() const
{
    return SubsystemName;
}

Level Channel::GetMinimumOutputLevel() const
{
    return MinimumOutputLevel;
}


//-----------------------------------------------------------------------------
// Conversion functions

#ifdef _WIN32

template<>
void LogStringize(LogStringBuffer& buffer, const wchar_t* const & first)
{
    // Use Windows' optimized multi-byte UTF8 conversion function for performance.
    // Returns the number of bytes used by the conversion, including the null terminator
    // since -1 is passed in for the input string length.
    // Returns 0 on failure.
    int bytesUsed = ::WideCharToMultiByte(
        CP_ACP, // Default code page
        0, // Default flags
        first, // String to convert
        -1, // Unknown string length
        nullptr, // Null while checking length of buffer
        0, // 0 to request the buffer size required
        nullptr, // Default default character
        nullptr); // Ignore whether or not default character was used
    // Setting the last two arguments to null is documented to execute faster via MSDN.

    // If the function succeeded,
    if (bytesUsed > 0)
    {
        // Avoid allocating memory if the string is fairly small.
        char stackBuffer[128];
        char* dynamicBuffer = nullptr;
        char* convertedString = stackBuffer;
        if (bytesUsed > (int)sizeof(stackBuffer))
        {
            // (defensive coding) Add 8 bytes of slop in case of API bugs.
            dynamicBuffer = new char[bytesUsed + 8];
            convertedString = dynamicBuffer;
        }

        int charsWritten = ::WideCharToMultiByte(
            CP_ACP, // Default code page
            0, // Default flags
            first, // String to convert
            -1, // Unknown string length
            convertedString, // Output buffer
            bytesUsed, // Request the same number of bytes
            nullptr, // Default default character
            nullptr); // Ignore whether or not default character was used
        // Setting the last two arguments to null is documented to execute faster via MSDN.

        if (charsWritten > 0)
        {
            // Append the converted string.
            buffer.Stream << convertedString;
        }

        delete[] dynamicBuffer;
    }
}

#endif // _WIN32


//-----------------------------------------------------------------------------
// ConfiguratorPlugin

ConfiguratorPlugin::ConfiguratorPlugin()
{
}

ConfiguratorPlugin::~ConfiguratorPlugin()
{
}


//-----------------------------------------------------------------------------
// Log Configurator

Configurator::Configurator() :
    ChannelsLock(),
#ifdef LOGGING_DEBUG
    GlobalMinimumLogLevel(Level::Debug),
#else
    GlobalMinimumLogLevel(Level::Info),
#endif
    Channels(),
    Plugin(nullptr)
{
}

Configurator* Configurator::GetInstance()
{
    static Configurator configurator;
    return &configurator;
}

Configurator::~Configurator()
{
}

void Configurator::SetGlobalMinimumLogLevel(Level level)
{
    Locker locker(ChannelsLock);

    GlobalMinimumLogLevel = level;

    for (Channel* channel : Channels)
    {
        channel->SetMinimumOutputLevelNoSave(level);
    }
}

void Configurator::RestoreChannelLogLevel(Channel* channel)
{
    Level level = GlobalMinimumLogLevel;

    // Look up the log level for this channel if we can
    if (Plugin)
    {
        Plugin->RestoreChannelLevel(channel->GetName(), level);
    }

    channel->SetMinimumOutputLevelNoSave(level);
}

void Configurator::SetPlugin(std::shared_ptr<ConfiguratorPlugin> plugin)
{
    Locker locker(ChannelsLock);

    Plugin = plugin;

    for (Channel* channel : Channels)
    {
        RestoreChannelLogLevel(channel);
    }
}

void Configurator::Register(Channel* channel)
{
    Locker locker(ChannelsLock);

    Channels.erase(channel);
    Channels.insert(channel);

    RestoreChannelLogLevel(channel);
}

void Configurator::Unregister(Channel* channel)
{
    Locker locker(ChannelsLock);

    Channels.erase(channel);
}

void Configurator::OnChannelLevelChange(Channel* channel)
{
    Locker locker(ChannelsLock);

    if (Plugin)
    {
        // Save channel level
        Plugin->SaveChannelLevel(channel->GetName(), channel->GetMinimumOutputLevel());
    }
}


//-----------------------------------------------------------------------------
// ErrorSilencer

#if !defined(OVR_CC_MSVC) || (OVR_CC_MSVC < 1300)
    __declspec(thread) int ThreadErrorSilenced = 0;
#else
    #pragma data_seg(".tls$")
    __declspec(thread) int ThreadErrorSilenced = 0;
    #pragma data_seg(".rwdata")
#endif

bool ErrorSilencer::IsSilenced()
{
    return ThreadErrorSilenced > 0;
}

ErrorSilencer::ErrorSilencer(bool initiallySilenced) :
    ThisObjectCurrentlySilenced(false)
{
    if (initiallySilenced)
    {
        Silence();
    }
}

ErrorSilencer::~ErrorSilencer()
{
    Unsilence();
}

void ErrorSilencer::Silence()
{
    if (!ThisObjectCurrentlySilenced)
    {
        ThreadErrorSilenced++;
        ThisObjectCurrentlySilenced = true;
    }
}

void ErrorSilencer::Unsilence()
{
    if (ThisObjectCurrentlySilenced)
    {
        ThreadErrorSilenced--;
        ThisObjectCurrentlySilenced = false;
    }
}


} // namespace ovrlog

#ifdef OVR_STRINGIZE
#error "This code must remain independent of LibOVR"
#endif
