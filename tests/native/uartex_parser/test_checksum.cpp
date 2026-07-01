#include "checksum.h"
#include "parser.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

using esphome::uartex::CHECKSUM;
using esphome::uartex::CHECKSUM_ADD;
using esphome::uartex::CHECKSUM_ADD_NO_HEADER;
using esphome::uartex::CHECKSUM_CUSTOM;
using esphome::uartex::CHECKSUM_NONE;
using esphome::uartex::CHECKSUM_XOR;
using esphome::uartex::CHECKSUM_XOR_ADD;
using esphome::uartex::CHECKSUM_XOR_NO_HEADER;
using esphome::uartex::compute_checksum;

namespace {

int g_failures = 0;

const std::vector<uint8_t> k_header = {0x02, 0x01};
const std::vector<uint8_t> k_data = {0x02, 0x03};

void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        g_failures++;
    }
}

void require_eq_u16(uint16_t actual, uint16_t expected, const char *message)
{
    if (actual != expected)
    {
        std::cerr << "FAIL: " << message << " (0x" << std::hex << actual << " vs 0x" << expected << std::dec << ")\n";
        g_failures++;
    }
}

bool feed(Parser &parser, const std::vector<uint8_t> &bytes)
{
    bool complete = false;
    for (uint8_t byte : bytes)
    {
        if (parser.parse_byte(byte))
            complete = true;
    }
    return complete;
}

void test_checksum_none_and_custom()
{
    require_eq_u16(compute_checksum(CHECKSUM_NONE, k_header, k_data), 0, "NONE returns 0");
    require_eq_u16(compute_checksum(CHECKSUM_CUSTOM, k_header, k_data), 0, "CUSTOM returns 0");
}

void test_checksum_add()
{
    require_eq_u16(compute_checksum(CHECKSUM_ADD, k_header, k_data), 0x08, "ADD with header");
    require_eq_u16(compute_checksum(CHECKSUM_ADD, {}, k_data), 0x05, "ADD empty header");
}

void test_checksum_xor()
{
    require_eq_u16(compute_checksum(CHECKSUM_XOR, k_header, k_data), 0x02, "XOR with header");
    require_eq_u16(compute_checksum(CHECKSUM_XOR, {}, {0xFF, 0x0F}), 0xF0, "XOR empty header");
}

void test_checksum_add_no_header()
{
    require_eq_u16(compute_checksum(CHECKSUM_ADD_NO_HEADER, k_header, k_data), 0x05, "ADD_NO_HEADER ignores header");
}

void test_checksum_xor_no_header()
{
    require_eq_u16(compute_checksum(CHECKSUM_XOR_NO_HEADER, k_header, k_data), 0x01, "XOR_NO_HEADER ignores header");
}

void test_checksum_xor_add()
{
    require_eq_u16(compute_checksum(CHECKSUM_XOR_ADD, k_header, k_data), 0x020A, "XOR_ADD 16-bit result");
}

void test_parser_integration_add_with_footer()
{
    Parser parser;
    parser.add_headers(k_header);
    parser.add_footers({0x0D, 0x0A});
    parser.set_checksum_len(1);

    uint8_t crc = static_cast<uint8_t>(compute_checksum(CHECKSUM_ADD, k_header, k_data) & 0xFF);
    std::vector<uint8_t> packet = k_header;
    packet.insert(packet.end(), k_data.begin(), k_data.end());
    packet.push_back(crc);
    packet.insert(packet.end(), {0x0D, 0x0A});

    require(feed(parser, packet), "integration ADD: frame completes");
    require(parser.verify_checksum({crc}), "integration ADD: checksum verifies");
}

void test_parser_integration_xor_no_header_fixed_length()
{
    Parser parser;
    parser.add_header_candidate({0xA0});
    parser.set_total_len(4);
    parser.set_checksum_len(1);

    std::vector<uint8_t> payload = {0x51, 0x00};
    uint8_t crc = static_cast<uint8_t>(compute_checksum(CHECKSUM_XOR_NO_HEADER, {}, payload) & 0xFF);
    require_eq_u16(crc, 0x51, "integration XOR_NO_HEADER: expected crc");

    require(!feed(parser, {0xA0, 0x51, 0x00, crc}), "integration XOR_NO_HEADER: completion deferred when checksum set");
    require(parser.buffer().size() == 4, "integration XOR_NO_HEADER: frame buffered");
    require(parser.verify_checksum({crc}), "integration XOR_NO_HEADER: checksum verifies");
}

void test_parser_integration_xor_add_two_byte_checksum()
{
    Parser parser;
    parser.add_headers(k_header);
    parser.add_footers({0x0D, 0x0A});
    parser.set_checksum_len(2);

    uint16_t crc16 = compute_checksum(CHECKSUM_XOR_ADD, k_header, k_data);
    std::vector<uint8_t> packet = k_header;
    packet.insert(packet.end(), k_data.begin(), k_data.end());
    packet.push_back(static_cast<uint8_t>((crc16 >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>(crc16 & 0xFF));
    packet.insert(packet.end(), {0x0D, 0x0A});

    require(feed(parser, packet), "integration XOR_ADD: frame completes");
    require(parser.verify_checksum({static_cast<uint8_t>((crc16 >> 8) & 0xFF), static_cast<uint8_t>(crc16 & 0xFF)}),
           "integration XOR_ADD: 2-byte checksum verifies");
}

}  // namespace

int main()
{
    test_checksum_none_and_custom();
    test_checksum_add();
    test_checksum_xor();
    test_checksum_add_no_header();
    test_checksum_xor_no_header();
    test_checksum_xor_add();
    test_parser_integration_add_with_footer();
    test_parser_integration_xor_no_header_fixed_length();
    test_parser_integration_xor_add_two_byte_checksum();

    if (g_failures > 0)
    {
        std::cerr << g_failures << " checksum test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All checksum tests passed.\n";
    return EXIT_SUCCESS;
}
