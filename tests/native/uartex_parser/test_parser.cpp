#include "parser.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        g_failures++;
    }
}

void require_eq(const std::vector<unsigned char> &actual, const std::vector<unsigned char> &expected, const char *message)
{
    if (actual != expected)
    {
        std::cerr << "FAIL: " << message << " (size " << actual.size() << " vs " << expected.size() << ")\n";
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

bool feed_byte(Parser &parser, uint8_t byte)
{
    return parser.parse_byte(byte);
}

void test_no_header_fixed_length()
{
    Parser parser;
    parser.set_total_len(4);
    require(feed(parser, {0x10, 0x20, 0x30, 0x40}), "no header: frame completes at fixed length");
    require_eq(parser.buffer(), {0x10, 0x20, 0x30, 0x40}, "no header: buffer");
    require_eq(parser.data(), {0x10, 0x20, 0x30, 0x40}, "no header: payload");
    require(parser.header().empty(), "no header: matched header empty");
}

void test_single_header_fixed_length()
{
    Parser parser;
    parser.add_header_candidate({0xA0});
    parser.set_total_len(3);
    require(feed(parser, {0xA0, 0x51, 0x00}), "single header: frame completes at fixed length");
    require_eq(parser.header(), {0xA0}, "single header: matched header");
    require_eq(parser.data(), {0x51, 0x00}, "single header: payload");
}

void test_single_header_with_checksum_no_auto_complete()
{
    Parser parser;
    parser.add_header_candidate({0xA0});
    parser.set_total_len(4);
    parser.set_checksum_len(1);
    require(!feed(parser, {0xA0, 0x51, 0x00, 0xF1}), "checksum+length: parser waits for external validation");
    require_eq(parser.header(), {0xA0}, "checksum+length: header still matched");
    require_eq(parser.data(), {0x51, 0x00}, "checksum+length: payload");
    require(parser.verify_checksum({0xF1}), "checksum+length: verify_checksum accepts trailing byte");
    require(!parser.verify_checksum({0x00}), "checksum+length: wrong checksum rejected");
}

void test_multi_header_candidates()
{
    Parser parser;
    parser.add_header_candidate({0xA0});
    parser.add_header_candidate({0xA1});
    parser.set_total_len(4);

    require(feed(parser, {0xA1, 0x52, 0x01, 0xAB}), "multi header: A1 frame completes");
    require_eq(parser.header(), {0xA1}, "multi header: second candidate matched");

    parser.clear();
    require(feed(parser, {0xA0, 0x51, 0x00, 0xCD}), "multi header: A0 frame completes");
    require_eq(parser.header(), {0xA0}, "multi header: first candidate matched");
}

void test_header_mismatch_clears_buffer()
{
    Parser parser;
    parser.add_header_candidate({0xA0});
    parser.set_total_len(4);
    require(!feed_byte(parser, 0xFF), "header mismatch: first byte rejected");
    require(!parser.available(), "header mismatch: buffer cleared");
    require(parser.header().empty(), "header mismatch: no active header");
}

void test_header_with_mask()
{
    Parser parser;
    parser.add_header_candidate({0x30, 0xB0}, {0xFF, 0xF0});
    parser.set_total_len(4);
    require(feed(parser, {0x30, 0xBC, 0x01, 0x02}), "masked header: accepts masked second byte");
    require_eq(parser.header(), {0x30, 0xBC}, "masked header: stores actual bytes");
}

void test_header_footer_checksum()
{
    Parser parser;
    parser.add_headers({0x02, 0x01});
    parser.add_footers({0x0D, 0x0A});
    parser.set_checksum_len(1);
    // ADD checksum over header+payload: 0x02+0x01+0x02+0x03 = 0x08
    require(feed(parser, {0x02, 0x01, 0x02, 0x03, 0x08, 0x0D, 0x0A}), "footer frame: completes on footer");
    require_eq(parser.data(), {0x02, 0x03}, "footer frame: payload");
    require(parser.verify_checksum({0x08}), "footer frame: checksum valid");
}

void test_fixed_length_with_footer()
{
    Parser parser;
    parser.add_headers({0x02, 0x01});
    parser.add_footers({0x0D, 0x0A});
    parser.set_total_len(7);
    require(feed(parser, {0x02, 0x01, 0xAA, 0xBB, 0xCC, 0x0D, 0x0A}), "fixed length + footer: completes");
    require_eq(parser.data(), {0xAA, 0xBB, 0xCC}, "fixed length + footer: payload");
}

void test_dynamic_data_length_big_endian()
{
    Parser parser;
    parser.add_headers({0xAA, 0x55});
    parser.add_footers({0x0D, 0x0A});
    parser.set_data_length(0, 1, true, 0);
    // header(2) + len(1) + payload(2) + footer(2) = 7 bytes
    require(feed(parser, {0xAA, 0x55, 0x02, 0xDD, 0xEE, 0x0D, 0x0A}), "dynamic length: frame completes");
    require_eq(parser.data(), {0x02, 0xDD, 0xEE}, "dynamic length: payload includes length field and body");
}

void test_dynamic_data_length_little_endian()
{
    Parser parser;
    parser.add_header_candidate({0xAB});
    parser.add_footers({0xEE});
    parser.set_data_length(0, 2, false, 0);
    // header(1) + len LE 0x0300 -> 0x0003 = 3 payload bytes + footer(1) = 9
    require(feed(parser, {0xAB, 0x03, 0x00, 0x11, 0x22, 0x33, 0xEE}), "dynamic length LE: completes");
    require_eq(parser.buffer().size(), static_cast<size_t>(7), "dynamic length LE: total size");
}

void test_no_footer_no_length_incomplete()
{
    Parser parser;
    parser.add_header_candidate({0xA0});
    require(!feed(parser, {0xA0, 0x01, 0x02}), "no footer/length: stays incomplete");
    require(parser.available(), "no footer/length: partial buffer kept");
    require_eq(parser.header(), {0xA0}, "no footer/length: header still matched");
}

void test_apply_mask()
{
    Parser parser;
    require_eq(parser.apply_mask({0x30, 0xBC}, {0xFF, 0xF0}), {0x30, 0xB0}, "apply_mask: masks bytes");
    require_eq(parser.apply_mask({0x30, 0xBC}, {}), {0x30, 0xBC}, "apply_mask: empty mask is identity");
}

void test_clear_resets_state()
{
    Parser parser;
    parser.add_header_candidate({0xA0});
    parser.set_total_len(4);
    require(feed(parser, {0xA0, 0x01, 0x02, 0x03}), "clear: initial frame");
    parser.clear();
    require(!parser.available(), "clear: buffer empty");
    require(parser.header().empty(), "clear: active header reset");
}

void test_buffer_len_trims_oldest()
{
    Parser parser;
    parser.set_buffer_len(4);
    parser.set_total_len(0);
    feed(parser, {0x01, 0x02, 0x03, 0x04, 0x05});
    require_eq(parser.buffer(), {0x02, 0x03, 0x04, 0x05}, "buffer_len: drops oldest byte");
}

}  // namespace

int main()
{
    test_no_header_fixed_length();
    test_single_header_fixed_length();
    test_single_header_with_checksum_no_auto_complete();
    test_multi_header_candidates();
    test_header_mismatch_clears_buffer();
    test_header_with_mask();
    test_header_footer_checksum();
    test_fixed_length_with_footer();
    test_dynamic_data_length_big_endian();
    test_dynamic_data_length_little_endian();
    test_no_footer_no_length_incomplete();
    test_apply_mask();
    test_clear_resets_state();
    test_buffer_len_trims_oldest();

    if (g_failures > 0)
    {
        std::cerr << g_failures << " parser test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All parser tests passed.\n";
    return EXIT_SUCCESS;
}
