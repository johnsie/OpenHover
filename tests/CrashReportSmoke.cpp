// SPDX-License-Identifier: MIT OR Apache-2.0
#include "CrashReport.h"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
// Runs pCrash in a child process with the crash report installed, and returns the report it left.
std::string CrashAndRead(const std::string& pPath, void (*pCrash)())
{
    std::remove(pPath.c_str());
    const pid_t child = fork();
    if (child == 0)
    {
        InstallCrashReport(pPath, "9.9.9-test");
        pCrash();
        _exit(0);
    }
    int status = 0;
    waitpid(child, &status, 0);
    return TakePreviousCrashReport(pPath);
}

void SegmentationFault()
{
    volatile int* nothing = nullptr;
    *nothing = 1;
}

void UncaughtException()
{
    throw std::runtime_error("boom from the test");
}

void CallAbort()
{
    std::abort();
}
}

int main()
{
    bool ok = true;
    const auto expect = [&](bool pCondition, const char* pMessage)
    {
        if (!pCondition)
        {
            std::cerr << pMessage << '\n';
            ok = false;
        }
    };
    char pattern[] = "/tmp/openhover-crashreport-XXXXXX";
    const int descriptor = mkstemp(pattern);
    if (descriptor < 0)
        return 1;
    close(descriptor);
    const std::string path = pattern;

    expect(TakePreviousCrashReport(path).empty(), "no report to take before any crash");

    const std::string segv = CrashAndRead(path, SegmentationFault);
    expect(segv.find("OpenHover 9.9.9-test") == 0, "the report starts with the program and version");
    expect(segv.find("SIGSEGV") != std::string::npos, "a segmentation fault names its signal");
    expect(segv.find("stack trace") != std::string::npos || segv.find("crash:") != std::string::npos,
           "the report says what happened");

    const std::string exception = CrashAndRead(path, UncaughtException);
    expect(exception.find("boom from the test") != std::string::npos, "an uncaught exception's message is kept");
    expect(exception.find("9.9.9-test") != std::string::npos, "the version is in the exception report");

    const std::string aborted = CrashAndRead(path, CallAbort);
    expect(aborted.find("SIGABRT") != std::string::npos, "an abort names its signal");

    // A report is handed over once and then removed.
    expect(TakePreviousCrashReport(path).empty(), "a report is only taken once");
    expect(!InstallCrashReport("", "x"), "an empty log path is refused");
    std::remove(path.c_str());
    return ok ? 0 : 1;
}
