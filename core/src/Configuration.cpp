#include "Configuration.hpp"

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
	auto it = _ConfigurationMap.find(section);
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
	auto it = _ConfigurationMap.find(section);
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
		auto vit = subsection->find(settingKey);
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

    const std::filesystem::path current = std::filesystem::current_path();
    const std::vector<std::filesystem::path> candidatePaths = {
        current / "etc" / _ConfigFileName,
        current / "config" / _ConfigFileName,
        current.parent_path() / "etc" / _ConfigFileName,
        current.parent_path() / "config" / _ConfigFileName,
        std::filesystem::path("/etc") / _ConfigFileName,
        std::filesystem::path("/etc") / "config" / _ConfigFileName
    };

    for (const auto& candidate : candidatePaths)
    {
        if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate))
        {
            return loadConfiguration(candidate.string());
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

    auto trim = [](std::string value) {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
        {
            return std::string{};
        }

        const auto end = value.find_last_not_of(" \t\r\n");
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

        const auto delimiterPos = trimmed.find('=');
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

    const auto begin = section.find_first_not_of(" \t\r\n[");
    if (begin == std::string::npos)
    {
        return;
    }

    section.erase(0, begin);
    const auto end = section.find_last_not_of(" \t\r\n]");
    if (end != std::string::npos)
    {
        section.erase(end + 1);
    }

    if (!section.empty())
    {
        _ConfigurationMap[section] = list;
    }
}

