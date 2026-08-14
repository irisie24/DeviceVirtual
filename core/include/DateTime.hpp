#ifndef _TIME_STAMP
#define _TIME_STAMP

#include "StringExtensions.hpp"

typedef enum DateTimeAttribute
{
	Days = 0,
	Months = 1,
	Years = 2,
	Hours = 3,
	Minutes = 4,
	Seconds = 5,
	Milliseconds

}DateTimeAttribute;

class DateTimeReference;

class DateTime
{
public:
	DateTime();
	DateTime(const DateTime& other);
	DateTime(const std::string &str, const std::string &format);
	DateTime(long long tinfo);
	~DateTime();

	void operator=(const DateTime& other);
	bool operator!=(const DateTime& other);
	bool operator==(const DateTime& other);
	bool operator>=(const DateTime& other);
	bool operator<=(const DateTime& other);
	bool operator>(const DateTime& other);
	bool operator<(const DateTime& other);
	DateTime& operator+=(const DateTime& other);
	DateTime operator+(const DateTime& other);
	DateTime& operator-=(const DateTime& other);
	DateTime operator-(const DateTime& other);

	void buildFromString(const std::string &str, const std::string &format);
	void buildFromTime(long long tinfo);

	std::string getDateString(const char *format);
	std::string getDateString();
	long long getTime() const;

	void addDays(int val);
	void addHours(int val);
	void addMinutes(int val);
	void addSeconds(int val);

	int getDays();
	int getMonths();
	int getYears();
	int getHours();
	int getMinutes();
	int getSeconds();

	void setDay(int val);
	void setMonth(int val);
	void setYear(int val);
	void setHour(int val);
	void setMinute(int val);
	void setSecond(int val);

private:
	DateTimeReference* _TimeReference;
};

#endif

