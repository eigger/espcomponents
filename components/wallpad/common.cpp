#include "common.h"



namespace esphome {
namespace wallpad {

std::string hexencode(const uint8_t *raw_data, num_t len)
{
    char buf[20];
    std::string res;
    for (num_t i = 0; i < len; i++)
    {
        sprintf(buf, "0x%02X ", raw_data[i]);
        res += buf;
    }
    sprintf(buf, "(%d byte)", len);
    res += buf;
    return res;
}

bool compare(const uint8_t *data1, const num_t len1, const uint8_t *data2, const num_t len2, const num_t offset)
{
    if (len1 - offset < len2) return false;
    //ESP_LOGD(TAG, "compare(0x%02X, 0x%02X, %d)=> %d", data1[offset], data2[0], len2, memcmp(&data1[offset], &data2[0], len2));
    return memcmp(&data1[offset], &data2[0], len2) == 0;
}

bool compare(const uint8_t *data1, const num_t len1, const hex_t *data2)
{
    if (!data2->and_operator) return compare(data1, len1, &data2->data[0], data2->data.size(), data2->offset) ? !data2->inverted : data2->inverted;
    else if (len1 - data2->offset > 0 && data2->data.size() > 0)
    {
        uint8_t val = data1[data2->offset] & (data2->data[0]);
        if (data2->data.size() == 1) return val ? !data2->inverted : data2->inverted;
        else
        {
            bool ret = false;
            for (num_t i = 1; i < data2->data.size(); i++)
            {
                if (val == data2->data[i])
                {
                    ret = true;
                    break;
                }
            }
            return ret ? !data2->inverted : data2->inverted;
        }
    }
    else return false;
}

float hex_to_float(const uint8_t *data, const num_t len, const num_t precision)
{
    unsigned int val = 0;
    for (num_t i = 0; i < len; i++)
    {
        val = (val << 8) | data[i];
    }
    return val / powf(10, precision);
}



}
}
