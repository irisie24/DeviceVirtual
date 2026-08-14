#include "StringExtensions.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <cstring>
#include <sstream>

StringExtensions::StringExtensions()
{
}

StringExtensions::~StringExtensions()
{
}

int StringExtensions::SubStringPosition(const char* str, const char* substr)
{
    const char* pdest = std::strstr(str, substr);
    if (pdest == nullptr)
    {
        return -1;
    }
    return static_cast<int>(pdest - str);
}

void StringExtensions::Split(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters)
{
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos)
    {
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        lastPos = str.find_first_not_of(delimiters, pos);
        pos = str.find_first_of(delimiters, lastPos);
    }
}

void StringExtensions::Split(const std::string& str, std::vector<std::string>& tokens, char delim)
{
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delim))
    {
        tokens.push_back(item);
    }
}

void StringExtensions::Split(const std::string& str, const std::string& delim, std::string& keystr, std::string& valuestr)
{
    size_t pos = str.find(delim);

    if (pos == std::string::npos)
    {
        return;
    }

    keystr = str.substr(0, pos);
    valuestr = str.substr(pos + delim.length());
}

void StringExtensions::Replace(std::string& srcstr, const char oldchar, const char newchar)
{
    std::replace(srcstr.begin(), srcstr.end(), oldchar, newchar);
}

void StringExtensions::Replace(std::string& srcstr, const std::string& oldpattern, const std::string& newpattern)
{
    if (oldpattern.empty() || srcstr.empty())
    {
        return;
    }

    size_t idx = 0;

    while ((idx = srcstr.find(oldpattern, idx)) != std::string::npos)
    {
        srcstr.replace(idx, oldpattern.length(), newpattern);
        idx += newpattern.length();
    }
}

void StringExtensions::Replace(std::string& srcstr, const std::string& oldpattern, const double npattern)
{
    Replace(srcstr, oldpattern, static_cast<float>(npattern));
}

void StringExtensions::Replace(std::string& srcstr, const std::string& oldpattern, const int npattern)
{
    Replace(srcstr, oldpattern, static_cast<long>(npattern));
}

void StringExtensions::Replace(std::string& srcstr, const std::string& oldpattern, const char newchar)
{
    Replace(srcstr, oldpattern, std::string(1, newchar));
}

void StringExtensions::Replace(std::string& srcstr, const std::string& oldpattern, const float npattern)
{
    if (oldpattern.empty() || srcstr.empty())
    {
        return;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << npattern;
    std::string newpattern = oss.str();

    size_t idx = 0;

    while ((idx = srcstr.find(oldpattern, idx)) != std::string::npos)
    {
        srcstr.replace(idx, oldpattern.length(), newpattern);
        idx += newpattern.length();
    }
}

void StringExtensions::Replace(std::string& srcstr, const std::string& oldpattern, const long npattern)
{
    if (oldpattern.empty() || srcstr.empty())
    {
        return;
    }

    std::string newpattern = std::to_string(npattern);

    size_t idx = 0;

    while ((idx = srcstr.find(oldpattern, idx)) != std::string::npos)
    {
        srcstr.replace(idx, oldpattern.length(), newpattern);
        idx += newpattern.length();
    }
}

const char* StringExtensions::LeftTrim(const std::string str)
{
    const char* buf = str.c_str();

    while (*buf && std::isspace(static_cast<unsigned char>(*buf)))
    {
        buf++;
    }
    return buf;
}

void StringExtensions::AllTrim(std::string& str)
{
    if (str.empty())
    {
        return;
    }

    // Trim trailing whitespace
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    if (end != std::string::npos)
    {
        str.erase(end + 1);
    }
    else
    {
        str.clear();
        return;
    }

    // Trim leading whitespace
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start != std::string::npos)
    {
        str.erase(0, start);
    }
}

void StringExtensions::AllTrim(char* str)
{
    if (str == nullptr)
    {
        return;
    }

    size_t len = std::strlen(str);

    if (len < 1)
    {
        return;
    }

    // Trim trailing whitespace
    for (int i = static_cast<int>(len) - 1; i >= 0; i--)
    {
        if (!std::isspace(static_cast<unsigned char>(str[i])))
        {
            break;
        }
        str[i] = '\0';
    }

    len = std::strlen(str);

    if (len < 1)
    {
        return;
    }

    // Trim leading whitespace
    size_t start = 0;
    while (str[start] && std::isspace(static_cast<unsigned char>(str[start])))
    {
        start++;
    }

    if (start > 0)
    {
        std::memmove(str, str + start, len - start + 1);
    }
}

bool StringExtensions::IsSpace(int in)
{
    return (in == 0x20) || (in >= 0x09 && in <= 0x0D);
}

int StringExtensions::CharacterCount(const char* str, char ch)
{
    return static_cast<int>(std::count(str, str + std::strlen(str), ch));
}

int StringExtensions::CharacterPosition(const char* str, const char ch)
{
    const char* pos = std::strchr(str, ch);
    return pos ? static_cast<int>(pos - str) : -1;
}

void StringExtensions::RealToString(std::string& str, const double val)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << val;
    str = oss.str();
}