#include "Application.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

Configuration appConfig; // Loads Application.ini
Configuration devConfig; // Loads Device.ini
Configuration snsConfig; // Loads Sensing.ini
Configuration actConfig; // Loads Actuation.ini

Device device;
Sensing sensing;
Actuation actuation;

MqttClient sensingClient;
MqttClient actuationClient;
SignalHandler signalHandler;
Logger* loggerRefPtr = nullptr;

std::string brokerAddress;
int port;
std::string clientId;
std::string username;
std::string password;
std::string mqttPublishTopic;
std::string mqttSubscribeTopic;

std::string resolveDataPath()
{
    std::string _AppName = "";
    char exePath[4097] = { 0 };
    std::string dataPath = "";

#ifdef _WIN32
    const char* user = std::getenv("USERNAME");
    const char* localAppData = std::getenv("LOCALAPPDATA");
    GetModuleFileNameA(NULL, exePath, sizeof(exePath));
    _AppName = std::filesystem::path(exePath).stem().string();

    if (localAppData && user)
    {
        dataPath = std::filesystem::path(localAppData).generic_string() + "/" + _AppName + "/data";
    }
    else
    {
        dataPath = (std::filesystem::path("C:/Users/") / user / "AppData/Local" / _AppName).generic_string() + "/data";
    }

#else // POSIX
    uid_t uid = geteuid();
    const char* user = std::getenv("USER");
    const char* home = std::getenv("HOME");

    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1)
    {
        exePath[len] = '\0';
        _AppName = std::filesystem::path(exePath).stem().string();
    }

    if (uid == 0)
    { // root
        dataPath = "/root/data/";
    }
    else
    { // user
        if (home && user)
        {
            std::filesystem::path userPath = std::filesystem::path(home) / ".local" / "applications" / _AppName;
            if (!std::filesystem::exists(userPath))
            {
                std::filesystem::create_directories(userPath);
            }
            dataPath = userPath.generic_string();
        }
        else
        {
            if (home && !user)
            {
                std::filesystem::path xdgPath = std::filesystem::path(home) / ".local" / _AppName;
                if (!std::filesystem::exists(xdgPath))
                {
                    std::filesystem::create_directories(xdgPath);
                }
                dataPath = xdgPath.generic_string();
            }
        }
    }

#endif
    return dataPath;
}
