#include "Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>

namespace
{
std::mutex& GetLoggerMutex()
{
    static std::mutex loggerMutex;
    return loggerMutex;
}

std::string GetTimestampString()
{
    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};

#if defined(_WIN32) || defined(WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y.%m.%d-%H.%M.%S", &localTime);
    return std::string(buffer);
}

std::string GetLevelString(LogLevel level)
{
    switch (level)
    {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Critical:
            return "CRITICAL";
        default:
            return "INFO";
    }
}

std::string GetShortFunctionName(const std::string& functionName)
{
    std::string result = functionName;
    const std::string::size_type begin = result.find_last_of(':');
    if (begin != std::string::npos)
    {
        result = result.substr(begin + 1);
    }

    const std::string::size_type paren = result.find('(');
    if (paren != std::string::npos)
    {
        result = result.substr(0, paren);
    }

    return result;
}
}

Logger objLogger;

Logger* Logger::GetInstance()
{
    return &objLogger;
}

Logger::Logger()
{
    _LogDirectory = "";
    _LogFileSize = 1024;
    _ModuleName = "devicevirtual";
    _FileMode = LogFileMode::Append;

    _LogLevelMap.clear();
    _LogLevelMap[LogLevel::Info] = "INFO";
    _LogLevelMap[LogLevel::Warning] = "WARN";
    _LogLevelMap[LogLevel::Error] = "ERROR";
    _LogLevelMap[LogLevel::Critical] = "CRITICAL";
}

Logger::~Logger()
{
    stopLogging();
}

void Logger::stopLogging()
{
    if (_LogFile.is_open())
    {
        _LogFile.flush();
        _LogFile.close();
    }
}

void Logger::createBackupFileName(std::string& str)
{
    const std::string timestamp = GetTimestampString();
    str = _ModuleName + "_" + timestamp + ".log";
}

void Logger::startLogging(LogFileMode fmode)
{
    _FileMode = fmode;

    std::filesystem::path logDirectory = std::filesystem::current_path();
    if (_LogDirectory.empty())
    {
        logDirectory /= "logs";
    }
    else
    {
        logDirectory = std::filesystem::path(_LogDirectory);
    }

    std::filesystem::create_directories(logDirectory);

    _LogDirectory = logDirectory.string();
    _LogFilename = (_LogDirectory + "/" + _ModuleName + ".log");

    _LogFile.open(_LogFilename, std::ios::out | (fmode == LogFileMode::Append ? std::ios::app : std::ios::trunc));
}

void Logger::write(std::string logEntry, LogLevel llevel, const char* func, const char* file, int line)
{
    if (!_LogFile.is_open())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(GetLoggerMutex());

    const long long fileSize = static_cast<long long>(_LogFile.tellp());
    if (fileSize >= (_LogFileSize * 1024LL))
    {
        std::string backupName;
        createBackupFileName(backupName);

        const std::filesystem::path source(_LogFilename);
        const std::filesystem::path backup = source.parent_path() / (_LogBackupDirectory.empty() ? std::string("backup") : _LogBackupDirectory) / backupName;

        std::filesystem::create_directories(backup.parent_path());
        if (std::filesystem::exists(source))
        {
            std::filesystem::rename(source, backup);
        }

        _LogFile.close();
        startLogging(_FileMode);
    }

    std::string filename = file ? std::filesystem::path(file).filename().string() : "unknown";
    const std::string timestamp = GetTimestampString();
    std::string functionName = func ? func : "unknown";
    functionName = GetShortFunctionName(functionName);

    std::ostringstream stream;
    stream << timestamp << "|" << GetLevelString(llevel) << "|" << line << "|" << functionName
           << "|" << filename << "| " << logEntry << '\n';

    _LogFile << stream.str();
    _LogFile.flush();
}

void Logger::setModuleName(std::string mname)
{
    if (mname.empty())
    {
        _ModuleName = "devicevirtual";
        return;
    }

    const std::filesystem::path modulePath(mname);
    _ModuleName = modulePath.filename().string();
    const std::string::size_type extensionPos = _ModuleName.find_last_of('.');
    if (extensionPos != std::string::npos && extensionPos > 0)
    {
        const std::string extension = _ModuleName.substr(extensionPos);
        if (extension == ".exe" || extension == ".EXE")
        {
            _ModuleName = _ModuleName.substr(0, extensionPos);
        }
    }
}

void Logger::setLogFileSize(int flsz)
{
    _LogFileSize = std::max(1, flsz);
}

void Logger::setLogDirectory(std::string dirpath)
{
    _LogDirectory = dirpath.empty() ? std::filesystem::current_path().string() : dirpath;
    std::filesystem::path logPath(_LogDirectory);
    if (!logPath.has_extension() || logPath.extension() == ".")
    {
        std::filesystem::create_directories(logPath);
    }

    _LogBackupDirectory = (logPath / "backup").string();
    std::filesystem::create_directories(_LogBackupDirectory);
}

void Logger::writeExtended(LogLevel llevel, const char* func, const char* file, int line, const char* format, ...)
{
    if (format == nullptr)
    {
        return;
    }

    va_list args;
    va_start(args, format);

    char buffer[2048] = {};
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    write(std::string(buffer), llevel, func, file, line);
}

void Logger::setRemotePort(int remotePort)
{
    (void)remotePort;
}

void Logger::setRemoteHost(std::string remoteHost)
{
    (void)remoteHost;
}


