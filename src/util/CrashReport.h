// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_CRASH_REPORT_H
#define OPENHOVER_CRASH_REPORT_H

#include <string>

// Arranges for a fatal error (a crash signal such as a segmentation fault, or an uncaught
// exception) to leave a small report in pLogPath before the program dies: the program name and
// version, what went wrong, and, where the platform supports it, a stack trace. The report is
// written plain text, overwriting the previous one, and contains nothing about the player.
// Returns false if the log file could not be prepared (the program still runs normally).
bool InstallCrashReport(const std::string& pLogPath, const std::string& pVersion);

// Reads the report left by a previous crash, if any, and removes it so it is only reported once.
// Returns an empty string when there is none.
std::string TakePreviousCrashReport(const std::string& pLogPath);

#endif
