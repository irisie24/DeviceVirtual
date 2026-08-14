#ifndef CORE_BASE64_HPP
#define CORE_BASE64_HPP

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>


class Base64
{
public:
	Base64() = default;
	~Base64() = default;

	static std::string Encode(const std::string& input);
	static std::string Encode(const std::vector<std::uint8_t>& input);
	static std::string Decode(const std::string& input);
	static std::vector<std::uint8_t> DecodeBinary(const std::string& input);

	void encodeBase64(const unsigned char* data, unsigned long inputlength, char* encodedString, unsigned long& outputlength);
	void decodeBase64(const char* encodedString, unsigned long inputlength, unsigned char* decodedData, unsigned long& outputlength);

private:
	static std::string EncodeInternal(const std::vector<std::uint8_t>& input);
	static std::vector<std::uint8_t> DecodeInternal(const std::string& input);
};

#endif
