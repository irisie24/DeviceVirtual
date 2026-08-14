#ifndef STRING_EXTENSIONS_H
#define STRING_EXTENSIONS_H

#include <string>
#include <vector>

class StringExtensions
{
public:
    StringExtensions();
    ~StringExtensions();

    static void Split(const std::string &str, std::vector<std::string> &tokens, const std::string &delimiters = " ");
    static void Split(const std::string &str, std::vector<std::string> &tokens, char delim=' ');
    static void Split(const std::string &str, const std::string &delim, std::string &keystr, std::string &valuestr);

    static void Replace(std::string &srcstr, const std::string &oldpattern, const std::string &newpattern);
	static void Replace(std::string &srcstr, const std::string &oldpattern, const char newchar);
    static void	Replace(std::string &srcstr, const char oldchar, const char newchar);
    static void Replace(std::string &srcstr, const std::string &oldpattern, const float npattern);
    static void Replace(std::string &srcstr, const std::string &oldpattern, const long npattern);
    static void Replace(std::string &srcstr, const std::string &oldpattern, const double npattern);
    static void Replace(std::string &srcstr, const std::string &oldpattern, const int npattern);

    static const char* LeftTrim(const std::string str);
    static void AllTrim(std::string &str);
    static void AllTrim(char* str);
    static int CharacterCount(const char *str, char ch);
    static bool IsSpace(int in);
    static int SubStringPosition(const char* str,const char* substr);
    static int CharacterPosition(const char* str,const char ch);

    static void RealToString(std::string &str, const double val);
};

#endif
