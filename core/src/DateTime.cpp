#include "DateTime.hpp"
#include <ctime>
#include <cstring>

class DateTimeReference
{
public:
	struct tm timeinfo;
};

DateTime::DateTime()
{
	_TimeReference = new DateTimeReference();
	time_t rawtime;
	time(&rawtime);
	_TimeReference->timeinfo = *localtime(&rawtime);
}

DateTime::DateTime(long long tinfo)
{
	_TimeReference->timeinfo = *localtime((const time_t*)&tinfo);
}

DateTime::DateTime(const DateTime& other)
{
	_TimeReference->timeinfo = other._TimeReference->timeinfo;
}


DateTime::DateTime(const std::string &str, const std::string &format)
{
	time_t rawtime;
	time(&rawtime);
	_TimeReference->timeinfo = *localtime(&rawtime);

	if (str.length() != 14)
	{
		if (str.length() != 12)
		{
			return;
		}
	}

	size_t pos;
	pos = format.find("yyyy");
	if (pos == std::string::npos)
	{
		pos = format.find("yy");
		if (pos != std::string::npos)
		{
			_TimeReference->timeinfo.tm_year = std::stoi(str.substr(pos, 2)) + 100;
		}
	}
	else
	{
		_TimeReference->timeinfo.tm_year = std::stoi(str.substr(pos, 4)) - 1900;
	}

	pos = format.find("MM");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_mon = std::stoi(str.substr(pos, 2)) - 1;
	}

	pos = format.find("dd");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_mday = std::stoi(str.substr(pos, 2));
	}

	pos = format.find("hh");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_hour = std::stoi(str.substr(pos, 2));
	}

	pos = format.find("mm");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_min = std::stoi(str.substr(pos, 2));
	}

	pos = format.find("ss");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_sec = std::stoi(str.substr(pos, 2));
	}
}

void DateTime::buildFromTime(long long tinfo)
{
	_TimeReference->timeinfo = *localtime((const time_t*)&tinfo);
}

void DateTime::buildFromString(const std::string &str, const std::string &format)
{
	time_t rawtime;
	time(&rawtime);
	_TimeReference->timeinfo = *localtime(&rawtime);

	if (str.length() != 14)
	{
		if (str.length() != 12)
		{
			return;
		}
	}

	size_t pos;
	pos = format.find("yyyy");
	if (pos == std::string::npos)
	{
		pos = format.find("yy");
		if (pos != std::string::npos)
		{
			_TimeReference->timeinfo.tm_year = std::stoi(str.substr(pos, 2)) + 100;
		}
	}
	else
	{
		_TimeReference->timeinfo.tm_year = std::stoi(str.substr(pos, 4)) - 1900;
	}

	pos = format.find("MM");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_mon = std::stoi(str.substr(pos, 2)) - 1;
	}

	pos = format.find("dd");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_mday = std::stoi(str.substr(pos, 2));
	}

	pos = format.find("hh");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_hour = std::stoi(str.substr(pos, 2));
	}

	pos = format.find("mm");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_min = std::stoi(str.substr(pos, 2));
	}

	pos = format.find("ss");
	if (pos != std::string::npos)
	{
		_TimeReference->timeinfo.tm_sec = std::stoi(str.substr(pos, 2));
	}
}

DateTime::~DateTime()
{
	delete _TimeReference;
}

void DateTime::operator=(const DateTime& other)
{
	_TimeReference->timeinfo = other._TimeReference->timeinfo;
	return;
}

bool DateTime::operator!=(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	if (t1 != t2)
	{
		return true;
	}
	return false;
}

bool DateTime::operator==(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	if (t1 == t2)
	{
		return true;
	}
	return false;
}

bool DateTime::operator>=(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	if (t1 >= t2)
	{
		return true;
	}
	return false;
}

bool DateTime::operator<=(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	if (t1 <= t2)
	{
		return true;
	}
	return false;
}

bool DateTime::operator>(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	if (t1 > t2)
	{
		return true;
	}
	return false;
}

bool DateTime::operator<(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	if (t1 < t2)
	{
		return true;
	}
	return false;
}

DateTime& DateTime::operator+=(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	t1 = t1 + t2;

	_TimeReference->timeinfo = *localtime(&t1);

	return *this;
}

DateTime DateTime::operator+(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	time_t t3 = t1 + t2;

	return DateTime(t3);
}

DateTime& DateTime::operator-=(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	t1 = t1 - t2;

	_TimeReference->timeinfo = *localtime(&t1);

	return *this;
}

DateTime DateTime::operator-(const DateTime& other)
{
	time_t t1 = mktime(&_TimeReference->timeinfo);
	time_t t2 = mktime((tm*)&other._TimeReference->timeinfo);

	time_t t3 = t1 - t2;

	return DateTime(t3);
}

long long DateTime::getTime() const
{
	time_t t = mktime(&_TimeReference->timeinfo);
	return (long long)t;
}


std::string DateTime::getDateString(const char *format)
{
	std::string str = format;
	size_t pos = 0;
	bool ap = false;

	char buffer[256];
	memset((char*)&buffer[0], 0, 256);

	pos = str.find("ss");
	if (pos != std::string::npos)
	{
		str.replace(pos, 2, "%S");
	}

	pos = str.find("mm");
	if (pos != std::string::npos)
	{
		str.replace(pos, 2, "%M");
	}

	pos = str.find("hh");
	if (pos != std::string::npos)
	{
		str.replace(pos, 2, "%H");
	}
	else
	{
		pos = str.find("h");
		if (pos != std::string::npos)
		{
			str.replace(pos, 1, "%I");
			ap = true;
		}
	}

	pos = str.find("dd");
	if (pos != std::string::npos)
	{
		str.replace(pos, 2, "%d");
	}

	pos = str.find("MMMM");
	if (pos != std::string::npos)
	{
		str.replace(pos, 4, "%B");
	}
	else
	{
		pos = str.find("MM");
		if (pos != std::string::npos)
		{
			str.replace(pos, 2, "%m");
		}
	}

	pos = str.find("yyyy");
	if (pos != std::string::npos)
	{
		str.replace(pos, 4, "%Y");
	}
	else
	{
		pos = str.find("yy");
		if (pos != std::string::npos)
		{
			str.replace(pos, 2, "%y");
		}
	}

	if (ap)
	{
		str += "%p";

	}

	if (_TimeReference->timeinfo.tm_year < 100)
	{
		_TimeReference->timeinfo.tm_year += 100;
	}

	strftime(buffer, 256, str.c_str(), &_TimeReference->timeinfo);

	return buffer;
}

std::string DateTime::getDateString()
{
	std::string str = "yyyy/MM/dd hh:mm:ss";
	size_t pos = 0;
	bool ap = false;

	char buffer[256];
	memset((char*)&buffer[0], 0, 256);

	pos = str.find("ss");
	if (pos != std::string::npos)
	{
		str.replace(pos, 2, "%S");
	}

	pos = str.find("mm");
	if (pos != std::string::npos)
	{
		str.replace(pos, 2, "%M");
	}

	pos = str.find("hh");
	if (pos != std::string::npos)
	{
		str.replace(pos, 2, "%H");
	}
	else
	{
		pos = str.find("h");
		if (pos != std::string::npos)
		{
			str.replace(pos, 1, "%I");
			ap = true;
		}
	}

	pos = str.find("dd");
	if (pos != std::string::npos)
	{
		str.replace(pos, 2, "%d");
	}

	pos = str.find("MMMM");
	if (pos != std::string::npos)
	{
		str.replace(pos, 4, "%B");
	}
	else
	{
		pos = str.find("MM");
		if (pos != std::string::npos)
		{
			str.replace(pos, 2, "%m");
		}
	}

	pos = str.find("yyyy");
	if (pos != std::string::npos)
	{
		str.replace(pos, 4, "%Y");
	}
	else
	{
		pos = str.find("yy");
		if (pos != std::string::npos)
		{
			str.replace(pos, 2, "%y");
		}
	}

	if (ap)
	{
		str += "%p";

	}

	if (_TimeReference->timeinfo.tm_year < 100)
	{
		_TimeReference->timeinfo.tm_year += 100;
	}

	strftime(buffer, 256, str.c_str(), &_TimeReference->timeinfo);

	return buffer;
}


void DateTime::addDays(int val)
{
	addSeconds(val * 60 * 60 * 24);
}

void DateTime::addHours(int val)
{
	addSeconds(val * 60 * 60);
}

void DateTime::addMinutes(int val)
{
	addSeconds(val * 60);
}

void DateTime::addSeconds(int val)
{
	// Commented due to GCC non POSIX behaviour
	//time_t t = mktime(&timeinfo);
	//t = t + val;
	//timeinfo = *localtime(&t);

	_TimeReference->timeinfo.tm_sec = _TimeReference->timeinfo.tm_sec + val;
	time_t t = mktime(&_TimeReference->timeinfo);
	_TimeReference->timeinfo = *localtime(&t);
}

int DateTime::getDays()
{
	return _TimeReference->timeinfo.tm_mday;
}

int DateTime::getMonths()
{
	return _TimeReference->timeinfo.tm_mon + 1;
}

int DateTime::getYears()
{
	return _TimeReference->timeinfo.tm_year + 1900;
}

int DateTime::getHours()
{
	return _TimeReference->timeinfo.tm_hour;
}

int DateTime::getMinutes()
{
	return _TimeReference->timeinfo.tm_min;
}

int DateTime::getSeconds()
{
	return _TimeReference->timeinfo.tm_sec;
}

void DateTime::setDay(int val)
{
	_TimeReference->timeinfo.tm_mday = val;
}

void DateTime::setMonth(int val)
{
	_TimeReference->timeinfo.tm_mon = val - 1;
}

void DateTime::setYear(int val)
{
	_TimeReference->timeinfo.tm_year = val - 1900;
}

void DateTime::setHour(int val)
{
	_TimeReference->timeinfo.tm_hour = val;
}

void DateTime::setMinute(int val)
{
	_TimeReference->timeinfo.tm_min = val;
}

void DateTime::setSecond(int val)
{
	_TimeReference->timeinfo.tm_sec = val;
}
