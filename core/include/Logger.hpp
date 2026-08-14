#ifndef CORE_LOGGER_HPP
#define CORE_LOGGER_HPP

#include <fstream>
#include <map>
#include <string>

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#define FUNCTIONNAME __FUNCTION__
#elif defined(__linux__) || defined(__gnu_linux__)
#define FUNCTIONNAME __PRETTY_FUNCTION__
#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
#define FUNCTIONNAME __func__
#else
#define FUNCTIONNAME "NoName"
#endif

enum class LogLevel
{
    Info = 0,
    Error = 1,
    Warning = 2,
    Critical = 3
};

enum class LogFileMode
{
    Append = 0,
    CreateNew = 1
};

using LogLevelStrings = std::map<LogLevel, std::string>;

class Logger
{
public:
    Logger();
    ~Logger();

    void startLogging(LogFileMode fmode);
    void stopLogging();
    void write(std::string logEntry, LogLevel llevel, const char* func, const char* file, int line);
    void writeExtended(LogLevel llevel, const char* func, const char* file, int line, const char* format, ...);
    void setRemotePort(int remotePort);
    void setRemoteHost(std::string remoteHost);
    void setLogFileSize(int flsz);
    void setLogDirectory(std::string dirpath);
    void setModuleName(std::string mname);
    static Logger* GetInstance();

private:
    void createBackupFileName(std::string& str);

    std::string _LogFilename;
    std::string _LogDirectory;
    std::string _LogBackupDirectory;
    int _LogFileSize;
    std::string _ModuleName;
    std::ofstream _LogFile;
    LogFileMode _FileMode;
    LogLevelStrings _LogLevelMap;
};

#define writeLog(str, level) Logger::GetInstance()->write(str, level, FUNCTIONNAME, __FILE__, __LINE__)
#define writeLogNormal(str) Logger::GetInstance()->write(str, LogLevel::Info, FUNCTIONNAME, __FILE__, __LINE__)

#endif

