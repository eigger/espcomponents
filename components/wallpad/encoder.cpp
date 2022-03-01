#include "encoder.h"

std::string to_hex_string(const std::vector<unsigned char>& data)
{
    std::string str;
	for (unsigned char hex : data)
    {
        str += std::format("0x%02X ", hex);
    }
	str += std::format("(%d byte)", data.size());
	return str;
}