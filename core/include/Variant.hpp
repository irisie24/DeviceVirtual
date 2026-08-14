#ifndef VARIANT_H
#define VARIANT_H

#include "StringExtensions.hpp"
#include <chrono>

typedef enum VariantType
{
    Void =0,
    Char =1,
    UnsignedChar =2,
    AsciiString=3,
    Boolean=4,
    Number=5,
    UnsignedNumber=6,
    Decimal=7,
    TimeStamp=8,
    Raw=9
}VariantType;

class Variant
{
public:
    Variant();
    ~Variant();
    Variant(const char val);
    Variant(const unsigned char val);
    Variant(const char* val);
    Variant(const unsigned char* val, unsigned int &sz);
    Variant(const bool val);
    Variant(const long val);
    Variant(const unsigned long val);
    Variant(const double val);
    Variant(const struct tm val);

    const void* GetData();
    std::string GetString();
    long GetSignedNumber();
    unsigned long GetUnsignedNumber();
    double GetReal();
    bool GetBoolean();
    std::chrono::system_clock::time_point GetTimestamp();
    char GetSignedChar();

    VariantType GetType();
    void SetType(VariantType vtype);
    unsigned int GetSize();

    void SetData(const char val);
    void SetData(const unsigned char val);
    void SetData(const char* val, bool trim=false);
    void SetData(const unsigned char* val, int &sz);
    void SetData(const bool val);
    void SetData(const long val);
    void SetData(const unsigned long val);
    void SetData(const double val);
    void SetData(const struct tm val);

private:
    // Prevent copying - by design
    Variant(const Variant& other);
    Variant& operator=(const Variant& other);
        
    VariantType dataType;
    unsigned char rawBuffer[256];
    unsigned int dataSize;
};

#endif

