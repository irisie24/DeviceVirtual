#include "Configuration.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace
{
std::string trimWhitespace(const std::string &value)
{
    const std::string::size_type begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
    {
        return std::string();
    }

    const std::string::size_type end = value.find_last_not_of(" \t\r\n");
    if (end == std::string::npos)
    {
        return std::string();
    }

    return value.substr(begin, end - begin + 1);
}
}

Configuration::Configuration()
{
	_ConfigFileName = "";
}

Configuration::~Configuration()
{
}

void Configuration::setConfigDirectory(const std::string &configDir)
{
	_ConfigDirectory = configDir;
}

void Configuration::setConfigFileName(const std::string &fname)
{
	_ConfigFileName = fname;
}

std::string Configuration::getConfigFileName()
{
	return _ConfigFileName;
}


std::string Configuration::getConfigDirectory()
{
    // Do an auto resolution only if _ConfigDirectory is not set
    if (!_ConfigDirectory.empty())
    {
        return _ConfigDirectory;
    }
    return resolveConfigPath();
}

bool Configuration::isSection(const std::string &section)
{
	std::map<std::string, std::string> *subsection = nullptr;
    std::map<std::string, std::map<std::string, std::string>>::iterator it = _ConfigurationMap.find(section);
	if (it != _ConfigurationMap.end())
	{
		subsection = &it->second;
	}

	if (subsection == nullptr)
	{
		return false;
	}
	else
	{
		return true;
	}
}

std::string Configuration::getValue(const std::string &section, const std::string &settingKey, const std::string defval)
{
	std::string str;

	std::map<std::string, std::string> *subsection = nullptr;
    std::map<std::string, std::map<std::string, std::string>>::iterator it = _ConfigurationMap.find(section);
	if (it != _ConfigurationMap.end())
	{
		subsection = &it->second;
	}

	if (subsection == nullptr)
	{
		return defval;
	}
	else
	{
		std::string *value = nullptr;
        std::map<std::string, std::string>::iterator vit = subsection->find(settingKey);
		if (vit != subsection->end())
		{
			value = &vit->second;
		}

		if (value == nullptr)
		{
			return defval;
		}
		str = *value;
	}
	return str;
}

bool Configuration::loadCustomConfiguration(const std::string &configFile)
{
	return loadConfiguration(configFile);
}

bool Configuration::loadConfiguration()
{
    if (_ConfigFileName.empty())
    {
        return false;
    }

    // Use auto resolution only if _ConfigDirectory is not set
    if (!_ConfigDirectory.empty())
    {
        std::string resolvedConfigFile = _ConfigDirectory + "/" + _ConfigFileName;
        if (std::filesystem::exists(resolvedConfigFile) && std::filesystem::is_regular_file(resolvedConfigFile))
        {
            return loadConfiguration(resolvedConfigFile);
        }
    }

    std::string resolvedPath = resolveConfigPath();

    if (!resolvedPath.empty() && std::filesystem::exists(resolvedPath))
    {
        std::string resolvedConfigFile = resolvedPath + "/" + _ConfigFileName;

        if (std::filesystem::exists(resolvedConfigFile) && std::filesystem::is_regular_file(resolvedConfigFile))
        {
            return loadConfiguration(resolvedConfigFile);
        }
    }

    return false;
}

bool Configuration::loadConfiguration(const std::string &configFile)
{
    std::ifstream input(configFile);
    if (!input.is_open())
    {
        return false;
    }

    _ConfigurationMap.clear();

    std::string currentSection;
    std::map<std::string, std::string> currentValues;
    std::string line;

    while (std::getline(input, line))
    {
        std::string trimmed = trimWhitespace(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
        {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']')
        {
            if (!currentSection.empty())
            {
                addSection(currentSection, currentValues);
            }

            currentSection = trimmed.substr(1, trimmed.size() - 2);
            currentValues.clear();
            continue;
        }

        const std::string::size_type delimiterPos = trimmed.find('=');
        if (delimiterPos == std::string::npos)
        {
            continue;
        }

        std::string key = trimWhitespace(trimmed.substr(0, delimiterPos));
        std::string value = trimWhitespace(trimmed.substr(delimiterPos + 1));

        if (!currentSection.empty() && !key.empty())
        {
            currentValues[key] = value;
        }
    }

    if (!currentSection.empty())
    {
        addSection(currentSection, currentValues);
    }

    return true;
}

void Configuration::addSection(const std::string &str, const std::map<std::string, std::string> &list)
{
    std::string section = str;

    const std::string::size_type begin = section.find_first_not_of(" \t\r\n[");
    if (begin == std::string::npos)
    {
        return;
    }

    section.erase(0, begin);
    const std::string::size_type end = section.find_last_not_of(" \t\r\n]");
    if (end != std::string::npos)
    {
        section.erase(end + 1);
    }

    if (!section.empty())
    {
        _ConfigurationMap[section] = list;
    }
}

std::string Configuration::resolveConfigPath() 
{
    std::string _AppName = "";
    char exePath[4097] = {0};
    std::string configPath = "";

    #ifdef _WIN32
        const char* user = std::getenv("USERNAME");
        const char* localAppData = std::getenv("LOCALAPPDATA");
        GetModuleFileNameA(NULL, exePath, sizeof(exePath));
        _AppName = std::filesystem::path(exePath).stem().string();

        if (localAppData && user) 
        {
            configPath = std::filesystem::path(localAppData).generic_string() + "/"  + _AppName;
        }
        else
        {
            configPath = (std::filesystem::path("C:/Users/") / user / "AppData/Local" / _AppName).generic_string();
        }

    #else // POSIX
        uid_t uid = geteuid();
        const char* user = std::getenv("USER");
        const char* home = std::getenv("HOME");

        ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath)-1);
        if (len != -1)
        {
            exePath[len] = '\0';
            _AppName = std::filesystem::path(exePath).stem().string();
        }

        if (uid == 0)
        { // root
            if (std::filesystem::exists("/etc/" ))
                configPath = "/etc/" ;

            if (std::filesystem::exists("/usr/local/etc/" ))
                configPath = "/usr/local/etc/" ;

            if (std::filesystem::exists("/usr/etc/"))
                configPath = "/usr/etc/" ;
        }
        else
        { // user
            if (home && user)
            {
                std::filesystem::path userPath = std::filesystem::path(home) / ".config" / "applications" / _AppName;
                if (!std::filesystem::exists(userPath))
                {
                    std::filesystem::create_directories(userPath);
                }
                configPath = userPath.generic_string();
            }
            else
            {
                if (home && !user)
                {
                    std::filesystem::path xdgPath = std::filesystem::path(home) / ".config" / _AppName;
                    if (!std::filesystem::exists(xdgPath)) 
                    {
                        std::filesystem::create_directories(xdgPath);
                    }
                    configPath = xdgPath.generic_string();
                }
            }
        }
      
    #endif
    return configPath;
}

std::list<std::string> Configuration::getSections()
{
    std::list<std::string> sections;
    for (const auto &pair : _ConfigurationMap)
    {
        sections.push_back(pair.first);
    }
    return sections;
}

std::list<std::string> Configuration::getKeys(const std::string &section)
{
    std::list<std::string> keys;
    if (_ConfigurationMap.find(section) != _ConfigurationMap.end())
    {
        for (const auto &pair : _ConfigurationMap[section])
        {
            keys.push_back(pair.first);
        }
    }
    return keys;
}