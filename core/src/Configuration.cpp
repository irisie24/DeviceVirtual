#include "Configuration.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


Configuration::Configuration()
{
	_ConfigFileName = "";
}

Configuration::~Configuration()
{
}

void Configuration::setFileName(std::string fname)
{
	_ConfigFileName = fname;
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

    std::string resolvedPath = resolveConfigPath();

    std::cout << resolvedPath;

    if (!resolvedPath.empty() && std::filesystem::exists(resolvedPath) && std::filesystem::is_regular_file(resolvedPath))
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

    std::string (*trim)(std::string) = [](std::string value) -> std::string {
        const std::string::size_type begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
        {
            return std::string{};
        }

        const std::string::size_type end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    };

    _ConfigurationMap.clear();

    std::string currentSection;
    std::map<std::string, std::string> currentValues;
    std::string line;

    while (std::getline(input, line))
    {
        std::string trimmed = trim(line);
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

        std::string key = trim(trimmed.substr(0, delimiterPos));
        std::string value = trim(trimmed.substr(delimiterPos + 1));

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

    #ifdef _WIN32
        const char* user = std::getenv("USERNAME");
        const char* localAppData = std::getenv("LOCALAPPDATA");

        if (localAppData && user) 
        {
            return std::filesystem::path(localAppData).generic_string() + "/" + user + "/" + _AppName;
        }
        return "C:/Users/Public/AppData/Local/"

    #else // POSIX
        uid_t uid = geteuid();
        const char* user = std::getenv("USER");
        const char* home = std::getenv("HOME");

        char exePath[4097] = {0};
        ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath)-1);
        if (len != -1)
        {
            exePath[len] = '\0';
            _AppName = std::filesystem::path(exePath).stem().string();
        }

        if (uid == 0)
        { // root
            if (std::filesystem::exists("/etc/" ))
                return "/etc/" ;

            if (std::filesystem::exists("/usr/local/etc/" ))
                return "/usr/local/etc/" ;

            if (std::filesystem::exists("/usr/etc/"))
                return "/usr/etc/" ;
        }
        else
        { // user
            if (home && user)
            {
                std::filesystem::path userPath = std::filesystem::path(home) / ".config" / "applications" / user / _AppName;
                if (std::filesystem::exists(userPath))
                {
                    return userPath.generic_string();
                }
            }
            if (home)
            {
                std::filesystem::path xdgPath = std::filesystem::path(home) / ".config" / _AppName;
                if (std::filesystem::exists(xdgPath)) 
                {
                    return xdgPath.generic_string();
                }
            }
        }
        return "";
    #endif
}
