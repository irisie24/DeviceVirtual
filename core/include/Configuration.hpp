#ifndef _CONFIGURATION
#define _CONFIGURATION

#include "StringExtensions.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <string>
#include <optional>
#include <list>

class Configuration
{
public:
	Configuration();
	~Configuration();
	void setConfigDirectory(const std::string &configDir);
	std::string getConfigDirectory();
	void setConfigFileName(const std::string &fname);
	std::string getConfigFileName();
	bool loadConfiguration();
	bool loadCustomConfiguration(const std::string &configFile);
	std::string getValue(const std::string &section, const std::string &settingKey, const std::string defval = "");
	bool isSection(const std::string &section);
	std::list<std::string> getSections();
	std::list<std::string> getKeys(const std::string &section);
private:
	bool loadConfiguration(const std::string &configFile);
	void addSection(const std::string &str, const std::map<std::string, std::string> &list);
	std::string resolveConfigPath();
	std::map<std::string, std::map<std::string, std::string>> _ConfigurationMap;
	std::string _ConfigFileName;
	std::string _ConfigDirectory;
};

#endif
