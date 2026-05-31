#ifndef EXCEPTION_DETECTOR_H
#define EXCEPTION_DETECTOR_H

#include "DataModels.h"
#include <vector>
#include <string>
#include <regex>

namespace LogAnalyzer {

// Detects and groups exception / stack-trace blocks in log entries.
// Works on individual lines; consecutive stack-frame lines following
// an exception header are attached to the same group.
class ExceptionDetector {
public:
    ExceptionDetector();

    std::vector<LogException> detect(
        const std::vector<LogEntry>& logs,
        const std::vector<int>&      indices
    );

private:
    bool        isExceptionHeader(const std::string& msg) const;
    bool        isStackFrame    (const std::string& msg) const;
    std::string extractType     (const std::string& msg) const;
    std::string extractMessage  (const std::string& msg) const;

    std::regex m_rexJavaException;
    std::regex m_rexDotNetException;
    std::regex m_rexPythonException;
    std::regex m_rexJavaFrame;
    std::regex m_rexDotNetFrame;
};

} // namespace LogAnalyzer

#endif // EXCEPTION_DETECTOR_H

// Made with Bob
