#include "Base64.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <algorithm>

constexpr std::array<char, 64> kEncodingTable = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

std::array<int, 256> BuildDecodingTable()
{
    std::array<int, 256> table{};
    for (std::size_t i = 0; i < kEncodingTable.size(); ++i)
    {
        table[static_cast<unsigned char>(kEncodingTable[i])] = static_cast<int>(i);
    }
    return table;
}

const std::array<int, 256> kDecodingTable = BuildDecodingTable();

std::string TrimWhitespace(const std::string& input)
{
    std::string cleaned;
    cleaned.reserve(input.size());
    for (const char ch : input)
    {
        if (!std::isspace(static_cast<unsigned char>(ch)))
        {
            cleaned.push_back(ch);
        }
    }
    return cleaned;
}

std::string Base64::Encode(const std::string& input)
{
    return EncodeInternal(std::vector<std::uint8_t>(input.begin(), input.end()));
}

std::string Base64::Encode(const std::vector<std::uint8_t>& input)
{
    return EncodeInternal(input);
}

std::string Base64::Decode(const std::string& input)
{
    const auto decoded = DecodeInternal(input);
    return std::string(decoded.begin(), decoded.end());
}

std::vector<std::uint8_t> Base64::DecodeBinary(const std::string& input)
{
    return DecodeInternal(input);
}

std::string Base64::EncodeInternal(const std::vector<std::uint8_t>& input)
{
    if (input.empty())
    {
        return {};
    }

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    for (std::size_t index = 0; index < input.size(); index += 3)
    {
        const std::uint32_t octetA = static_cast<std::uint32_t>(input[index]);
        const std::uint32_t octetB = index + 1 < input.size() ? static_cast<std::uint32_t>(input[index + 1]) : 0u;
        const std::uint32_t octetC = index + 2 < input.size() ? static_cast<std::uint32_t>(input[index + 2]) : 0u;

        const std::uint32_t triple = (octetA << 16) | (octetB << 8) | octetC;

        output.push_back(kEncodingTable[(triple >> 18) & 0x3F]);
        output.push_back(kEncodingTable[(triple >> 12) & 0x3F]);
        output.push_back(index + 1 < input.size() ? kEncodingTable[(triple >> 6) & 0x3F] : '=');
        output.push_back(index + 2 < input.size() ? kEncodingTable[triple & 0x3F] : '=');
    }

    return output;
}

std::vector<std::uint8_t> Base64::DecodeInternal(const std::string& input)
{
    const std::string cleaned = TrimWhitespace(input);
    if (cleaned.empty())
    {
        return {};
    }

    if (cleaned.size() % 4 != 0)
    {
        throw std::invalid_argument("Base64 input length must be a multiple of 4.");
    }

    std::vector<std::uint8_t> output;
    output.reserve((cleaned.size() / 4) * 3);

    std::size_t index = 0;
    while (index < cleaned.size())
    {
        const std::uint8_t charA = static_cast<std::uint8_t>(cleaned[index++]);
        const std::uint8_t charB = static_cast<std::uint8_t>(cleaned[index++]);
        const std::uint8_t charC = static_cast<std::uint8_t>(cleaned[index++]);
        const std::uint8_t charD = static_cast<std::uint8_t>(cleaned[index++]);

        const std::uint32_t valueA = (charA == '=') ? 0u : static_cast<std::uint32_t>(kDecodingTable[charA]);
        const std::uint32_t valueB = (charB == '=') ? 0u : static_cast<std::uint32_t>(kDecodingTable[charB]);
        const std::uint32_t valueC = (charC == '=') ? 0u : static_cast<std::uint32_t>(kDecodingTable[charC]);
        const std::uint32_t valueD = (charD == '=') ? 0u : static_cast<std::uint32_t>(kDecodingTable[charD]);

        const std::uint32_t triple = (valueA << 18) | (valueB << 12) | (valueC << 6) | valueD;

        if (charA != '=')
        {
            output.push_back(static_cast<std::uint8_t>((triple >> 16) & 0xFF));
        }
        if (charB != '=')
        {
            output.push_back(static_cast<std::uint8_t>((triple >> 8) & 0xFF));
        }
        if (charC != '=')
        {
            output.push_back(static_cast<std::uint8_t>(triple & 0xFF));
        }
    }

    const std::size_t paddingChars = std::count(cleaned.end() - 2, cleaned.end(), '=');
    if (paddingChars > 0)
    {
        output.resize(output.size() - paddingChars);
    }

    return output;
}

void Base64::encodeBase64(const unsigned char* data, unsigned long inputlength, char* encodedString, unsigned long& outputlength)
{
    if (data == nullptr && inputlength != 0)
    {
        outputlength = 0;
        return;
    }

    const std::vector<std::uint8_t> input(data, data + inputlength);
    const std::string encoded = EncodeInternal(input);
    outputlength = static_cast<unsigned long>(encoded.size());

    if (encodedString != nullptr)
    {
        std::memcpy(encodedString, encoded.c_str(), outputlength);
        encodedString[outputlength] = '\0';
    }
}

void Base64::decodeBase64(const char* encodedString, unsigned long inputlength, unsigned char* decodedData, unsigned long& outputlength)
{
    if (encodedString == nullptr)
    {
        outputlength = 0;
        return;
    }

    const std::string encoded(encodedString, encodedString + inputlength);
    const auto decoded = DecodeInternal(encoded);
    outputlength = static_cast<unsigned long>(decoded.size());

    if (decodedData != nullptr)
    {
        std::memcpy(decodedData, decoded.data(), decoded.size());
    }
}

