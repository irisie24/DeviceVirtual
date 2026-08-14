#include "Variant.hpp"
#include <cstring>

Variant::Variant()
{
    dataType = Void;
    std::ranges::fill(rawBuffer, 0);
    dataSize = 0;
}

Variant::Variant(const Variant& other)
{
	// Copy constructor 
	// Note: This is private to prevent copying
}

Variant::~Variant()
{
}

Variant::Variant(const char val)
{
    dataType = Char;
    rawBuffer[0] = val;
    rawBuffer[1] = 0;
    dataSize = sizeof(char);
}

Variant::Variant(const unsigned char val)
{
    dataType = UnsignedChar;
    rawBuffer[0] = val;
    rawBuffer[1] = 0;
    dataSize = sizeof(unsigned char);
}

Variant::Variant(const char *val)
{
    int sz = strlen(val);
    if(sz > 255)
    {
        sz = 255;
    }

    dataType = AsciiString;
    std::ranges::fill(rawBuffer, 0);
    strncpy((char*)&rawBuffer[0], val, sz);
    dataSize = sz;
}

Variant::Variant(const unsigned char* val, unsigned int &sz)
{
    if(sz > 255)
    {
        sz = 255;
    }
    dataType = Raw;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, val, sz);
    dataSize = sz;
}

Variant::Variant(const bool val)
{
    dataType = Boolean;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(bool));
    dataSize = sizeof(bool);
}

Variant::Variant(const long val)
{
    dataType = Number;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(long));
    dataSize = sizeof(long);
}

Variant::Variant(const unsigned long val)
{
    dataType = UnsignedNumber;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(unsigned long));
    dataSize = sizeof(unsigned long);
}

Variant::Variant(const double val)
{
    dataType = Decimal;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(double));
    dataSize = sizeof(double);
}

Variant::Variant(const tm val)
{
    dataType = TimeStamp;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(struct tm));
    dataSize = sizeof(struct tm);
}

VariantType Variant::GetType()
{
    return dataType;
}

void Variant::SetType(VariantType vtype)
{
    dataType = vtype;
}

unsigned int Variant::GetSize()
{
    return dataSize;
}

const void *Variant::GetData()
{
    return &rawBuffer[0];
}

void Variant::SetData(const char val)
{
    dataType = Char;
    rawBuffer[0] = val;
    rawBuffer[1] = 0;
    dataSize = sizeof(char);
}

void Variant::SetData(const unsigned char val)
{
    dataType = UnsignedChar;
    rawBuffer[0] = val;
    rawBuffer[1] = 0;
    dataSize = sizeof(unsigned char);
}

void Variant::SetData(const char *val, bool trim)
{
    int sz = strlen(val);
    if(sz > 255)
    {
        sz = 255;
    }

    dataType = AsciiString;
    std::ranges::fill(rawBuffer, 0);
    strncpy((char*)&rawBuffer[0], val, sz);
	dataSize = sz;

    if(trim)
    {
        dataSize = strlen((char*)&rawBuffer[0]);

        if(dataSize<1)
            return;

        for(int i = dataSize-1;  ; i--)
        {
            if(i < 0)
            {
                break;
            }

            if(rawBuffer[i]>32 && rawBuffer[i]<127)
            {
                break;
            }

            rawBuffer[i] = '\0';
        }

        dataSize = strlen((char*)&rawBuffer[0]);
    }
}

void Variant::SetData(const unsigned char* val, int &sz)
{
    if(sz > 255)
    {
        sz = 255;
    }
    dataType = Raw;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, val, sz);
    dataSize = sz;
}

void Variant::SetData(const bool val)
{
    dataType = Boolean;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(bool));
    dataSize = sizeof(bool);
}

void Variant::SetData(const long val)
{
    dataType = Number;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(long));
    dataSize = sizeof(long);
}

void Variant::SetData(const unsigned long val)
{
    dataType = UnsignedNumber;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(unsigned long));
    dataSize = sizeof(unsigned long);
}

void Variant::SetData(const double val)
{
    dataType = Decimal;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(double));
    dataSize = sizeof(double);
}

void Variant::SetData(const tm val)
{
    dataType = TimeStamp;
    std::ranges::fill(rawBuffer, 0);
    memcpy(rawBuffer, &val, sizeof(struct tm));
    dataSize = sizeof(struct tm);
}

std::string Variant::GetString()
{
    std::string str;
    str.clear();

    char ptr[255];
    std::ranges::fill(rawBuffer, 0);

    switch(dataType)
    {
        case Char:
        {
            sprintf(ptr,"%c", rawBuffer[0]);
            str = ptr;
            break;
        }
        case UnsignedChar:
        {
            sprintf(ptr,"%c",(char)rawBuffer[0]);
            str = ptr;
            break;
        }
        case AsciiString:
        {
            str = (char*)&rawBuffer[0];
            break;
        }
        case Boolean:
        {
            if(GetBoolean())
            {
                str = "true";
            }
            else
            {
                str = "false";
            }
            break;
        }
        case Number:
        {
            sprintf(ptr,"%ld", GetSignedNumber());
            str = (char*)&ptr[0];
            break;
        }
        case UnsignedNumber:
        {
            sprintf(ptr,"%lu", GetUnsignedNumber());
            str = ptr;
            break;
        }
        case Decimal:
        {
            sprintf(ptr,"%10.6lf", GetReal());
            str = ptr;
            break;
        }
        case TimeStamp:
        {
            std::chrono::system_clock::time_point dt = GetTimestamp();
			str = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(dt.time_since_epoch()).count());
            break;
        }
        case Raw:
        {
            str = (char*)&rawBuffer[0];
            break;
        }
        case Void:
        {
            str = "";
            break;
        }
        default:
        {
            str = "";
            break;
        }
    }

	return str;
}

long Variant::GetSignedNumber()
{
    if(dataType == Number)
    {
        long temp;
        memcpy((void*)&temp, (void*)&rawBuffer[0], sizeof(long));
        return temp;
    }

    return 0;
}

unsigned long Variant::GetUnsignedNumber()
{
    if(dataType == UnsignedNumber)
    {
        unsigned long temp;
        memcpy((void*)&temp, (void*)&rawBuffer[0], sizeof(unsigned long));
        return temp;
    }

    return 0;
}

double Variant::GetReal()
{
    if(dataType == Decimal)
    {
        double temp;
        memcpy((void*)&temp, (void*)&rawBuffer[0], sizeof(double));
        return temp;
    }

    return (double)0.0;
}

bool Variant::GetBoolean()
{
    if(dataType == Boolean)
    {
        bool temp;
        memcpy((void*)&temp, (void*)&rawBuffer[0], sizeof(bool));
        return temp;
    }
    return false;
}


std::chrono::system_clock::time_point Variant::GetTimestamp()
{
    if(dataType != TimeStamp)
    {
        auto ts = std::chrono::system_clock::now();
        return ts;
    }

    struct tm temp;
    memcpy((void*)&temp, (void*)&rawBuffer[0], sizeof(struct tm));

	// Convert struct tm to time_point
	std::time_t timeT = std::mktime(&temp);
	return std::chrono::system_clock::from_time_t(timeT);
}

char Variant::GetSignedChar()
{
    if(dataType == Char || dataType == AsciiString)
    {
        return rawBuffer[0];
    }

    return '\0';
}
