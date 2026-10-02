// SPDX-License-Identifier: MIT OR Apache-2.0
#include "CrashReport.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#if defined(__unix__) || defined(__APPLE__)
#define OPENHOVER_CRASH_SIGNALS 1
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#define OPENHOVER_HAVE_BACKTRACE 1
#endif
#endif

namespace
{
std::string gLogPath;
std::string gHeader;

void WriteWholeFile(const std::string& pPath, const std::string& pText)
{
    if (FILE* file = std::fopen(pPath.c_str(), "wb"))
    {
        std::fwrite(pText.data(), 1, pText.size(), file);
        std::fclose(file);
    }
}

#ifdef OPENHOVER_CRASH_SIGNALS
const char* SignalName(int pSignal)
{
    switch (pSignal)
    {
    case SIGSEGV: return "SIGSEGV (invalid memory access)";
    case SIGABRT: return "SIGABRT (abort)";
    case SIGFPE: return "SIGFPE (arithmetic error)";
    case SIGILL: return "SIGILL (illegal instruction)";
    case SIGBUS: return "SIGBUS (bus error)";
    default: return "unknown signal";
    }
}

// Runs inside a crashing process, so it sticks to calls that are safe in a signal handler: write
// to a file descriptor and the backtrace helpers, with no allocation of our own.
void HandleCrashSignal(int pSignal)
{
    // Restore the default action first so that a fault inside this handler cannot loop.
    std::signal(pSignal, SIG_DFL);
    const int descriptor = ::open(gLogPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (descriptor >= 0)
    {
        const auto writeText = [descriptor](const char* pText)
        {
            const ssize_t written = ::write(descriptor, pText, std::strlen(pText));
            (void)written;
        };
        writeText(gHeader.c_str());
        writeText("crash: signal ");
        writeText(SignalName(pSignal));
        writeText("\n");
#ifdef OPENHOVER_HAVE_BACKTRACE
        void* frames[48];
        const int count = backtrace(frames, 48);
        writeText("stack trace:\n");
        backtrace_symbols_fd(frames, count, descriptor);
#endif
        ::close(descriptor);
    }
    std::raise(pSignal);
}
#endif

void HandleTerminate()
{
    std::string text = gHeader + "crash: uncaught exception or terminate\n";
    try
    {
        const std::exception_ptr current = std::current_exception();
        if (current)
            std::rethrow_exception(current);
    }
    catch (const std::exception& error)
    {
        text += std::string("what: ") + error.what() + "\n";
    }
    catch (...)
    {
        text += "what: non-standard exception\n";
    }
    WriteWholeFile(gLogPath, text);
#ifdef OPENHOVER_CRASH_SIGNALS
    // The abort below must not run our signal handler, which would overwrite this report.
    std::signal(SIGABRT, SIG_DFL);
#endif
    std::abort();
}
}

bool InstallCrashReport(const std::string& pLogPath, const std::string& pVersion)
{
    if (pLogPath.empty())
        return false;
    gLogPath = pLogPath;
    gHeader = "OpenHover " + pVersion + "\n";
    std::set_terminate(HandleTerminate);
#ifdef OPENHOVER_CRASH_SIGNALS
    for (int signal : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS})
        std::signal(signal, HandleCrashSignal);
#endif
    return true;
}

std::string TakePreviousCrashReport(const std::string& pLogPath)
{
    std::string text;
    if (FILE* file = std::fopen(pLogPath.c_str(), "rb"))
    {
        char chunk[1024];
        std::size_t read = 0;
        while ((read = std::fread(chunk, 1, sizeof(chunk), file)) > 0 && text.size() < 32768)
            text.append(chunk, read);
        std::fclose(file);
        std::remove(pLogPath.c_str());
    }
    return text;
}
