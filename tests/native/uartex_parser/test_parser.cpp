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

void test_shared_headers_and_overlap()
{
    Parser shared;
    shared.add_header_candidate({0xAA, 0x10});
    shared.add_header_candidate({0xAA, 0x20});
    shared.set_total_len(3);
    require(!shared.parse_byte(0xAA), "shared header: first byte does not complete");
    require(shared.header().empty(), "shared header: partial candidate not committed");
    require(!shared.parse_byte(0x20), "shared header: second candidate selected");
    require(shared.parse_byte(0x42), "shared header: second frame completes");
    require_eq(shared.header(), {0xAA, 0x20}, "shared header: second header retained");
    require_eq(shared.data(), {0x42}, "shared header: second payload");
    shared.clear();
    require(feed(shared, {0xAA, 0x10, 0x43}), "shared header: first candidate completes");

    Parser overlap;
    overlap.add_header_candidate({0xAA, 0x55});
    overlap.set_total_len(3);
    require(feed(overlap, {0xAA, 0xAA, 0x55, 0x77}), "overlap: suffix recovers header");
    require_eq(overlap.buffer(), {0xAA, 0x55, 0x77}, "overlap: discard only invalid prefix");

    Parser masked;
    masked.add_header_candidate({0xA0, 0x10}, {0xF0, 0xF0});
    masked.add_header_candidate({0xA0, 0x20}, {0xF0, 0xF0});
    masked.set_checksum_len(1);
    masked.add_footer(0xEE);
    require(feed(masked, {0xAB, 0xAC, 0x2D, 0x33, 0x44, 0xEE}), "masked overlap: second candidate completes");
    require_eq(masked.header(), {0xAC, 0x2D}, "masked overlap: actual header bytes retained");
    require_eq(masked.data(), {0x33}, "masked overlap: payload excludes complete header");
    require(masked.verify_checksum({0x44}), "masked overlap: checksum position preserved");
}

void test_prefix_header_priority()
{
    Parser short_first;
    short_first.add_header_candidate({0xAA});
    short_first.add_header_candidate({0xAA, 0x55});
    short_first.set_total_len(3);
    require(feed(short_first, {0xAA, 0x55, 0x44}), "short priority: completes");
    require_eq(short_first.header(), {0xAA}, "short priority: first candidate wins");
    require_eq(short_first.data(), {0x55, 0x44}, "short priority: second byte is data");

    Parser long_first;
    long_first.add_header_candidate({0xAA, 0x55});
    long_first.add_header_candidate({0xAA});
    long_first.set_total_len(3);
    require(!long_first.parse_byte(0xAA), "long priority: first byte is not a frame");
    require(long_first.header_provisional(), "long priority: shorter candidate is provisional");
    require(feed(long_first, {0x55, 0x44}), "long priority: long frame completes");
    require_eq(long_first.header(), {0xAA, 0x55}, "long priority: earlier completed candidate wins");
    long_first.clear();
    require(feed(long_first, {0xAA, 0x44, 0x33}), "long priority: short candidate after mismatch");
    require_eq(long_first.header(), {0xAA}, "long priority: short candidate selected");

    Parser boundary;
    boundary.add_header_candidate({0xAA, 0xBB, 0xCC, 0xDD});
    boundary.add_header_candidate({0xAA});
    boundary.set_total_len(3);
    require(feed(boundary, {0xAA, 0xBB, 0xCC}), "long priority: short frame completes at fixed boundary");
    require_eq(boundary.header(), {0xAA}, "long priority: short header retained at boundary");

    Parser tied;
    tied.add_header_candidate({0xA0}, {0xF0});
    tied.add_header_candidate({0xAB});
    tied.set_total_len(2);
    require(feed(tied, {0xAB, 0x44}), "same-length match: completes");
    require_eq(tied.header(), {0xAB}, "same-length match: actual header retained");

}

void test_header_frame_capacity()
{
    Parser header_only;
    header_only.add_header_candidate({0xAA, 0xBB});
    header_only.set_total_len(2);
    require(feed(header_only, {0xAA, 0xBB}), "capacity: standalone parser supports header-only frames");

    Parser dynamic;
    dynamic.add_header_candidate({0xAA, 0xBB, 0xCC});
    dynamic.add_header_candidate({0xAA});
    dynamic.set_buffer_len(4);
    dynamic.set_data_length(0, 1, true, 0);
    dynamic.set_checksum_len(1);
    require(!feed(dynamic, {0xAA, 1, 0xCC}), "capacity: short dynamic frame awaits checksum");
    require(dynamic.parse_byte(0xDD), "capacity: dynamic length and checksum fit after short header");
    require_eq(dynamic.header(), {0xAA}, "capacity: header without room for dynamic field and checksum excluded");
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
    require(parser.buffer().size() == 7, "dynamic length LE: total size");
}

void test_dynamic_length_boundaries()
{
    for (bool footer : {false, true})
    {
        for (bool checksum : {false, true})
        {
            Parser parser;
            parser.add_header_candidate({0xAA});
            if (footer) parser.add_footer(0x0D);
            if (checksum) parser.set_checksum_len(1);
            parser.set_data_length(0, 1, true, 0);
            require(!feed(parser, {0xAA, 0x02, 0x0D}), "dynamic boundary: embedded footer does not complete");
            require(!parser.parse_length(), "dynamic boundary: incomplete frame has not reached length");
            bool complete = parser.parse_byte(0x11);
            if (checksum)
            {
                require(!complete, "dynamic boundary: waits for checksum");
                complete = parser.parse_byte(0x20);
            }
            if (footer)
            {
                require(!complete, "dynamic boundary: waits for footer");
                complete = parser.parse_byte(0x0D);
                require(parser.parse_footer(), "dynamic boundary: footer matches at boundary");
            }
            require(complete && parser.parse_length(), "dynamic boundary: completes at exact length");
            if (checksum)
            {
                require(parser.verify_checksum({0x20}), "dynamic boundary: checksum accepted");
                require(!parser.verify_checksum({0x21}), "dynamic boundary: invalid checksum rejected");
            }
            parser.parse_byte(0xFF);
            require(!parser.parse_length(), "dynamic boundary: oversized frame rejected");
            if (footer) require(!parser.parse_footer(), "dynamic boundary: oversized footer rejected");
        }
    }

    Parser invalid_footer;
    invalid_footer.set_data_length(0, 1, true, 0);
    invalid_footer.add_footer(0x0D);
    require(feed(invalid_footer, {0x01, 0x33, 0xFF}), "dynamic boundary: invalid footer completes for validation");
    require(!invalid_footer.parse_footer(), "dynamic boundary: invalid footer rejected");
}

void test_dynamic_length_bounds_and_recovery()
{
    Parser huge;
    huge.add_header_candidate({0xA0});
    huge.set_buffer_len(16);
    huge.set_data_length(0, 4, true, 1);
    require(!feed(huge, {0xA0, 0xFF, 0xFF, 0xFF, 0xFF}), "dynamic bounds: 32-bit overflow rejected");
    require(!huge.available(), "dynamic bounds: oversized frame discarded");
    require(feed(huge, {0xA0, 0, 0, 0, 1, 0x11, 0x22}), "dynamic bounds: valid header recovers after oversized frame");

    Parser negative;
    negative.add_header_candidate({0xA1});
    negative.set_data_length(0, 1, true, -2);
    require(!feed(negative, {0xA1, 1}), "dynamic bounds: negative adjusted payload rejected");
    require(!negative.available(), "dynamic bounds: negative frame discarded");
    require(feed(negative, {0xA1, 2}), "dynamic bounds: zero adjusted payload accepted");

    Parser cap;
    cap.set_buffer_len(4);
    cap.set_data_length(0, 1, true, 0);
    require(feed(cap, {3, 0x11, 0x22, 0x33}), "dynamic bounds: exact buffer capacity accepted");
    cap.clear();
    require(!feed(cap, {4}), "dynamic bounds: above buffer capacity rejected");
    require(!cap.available(), "dynamic bounds: above capacity discarded");

    Parser shifted;
    shifted.add_header_candidate({0xA2});
    shifted.set_total_len(3);
    shifted.set_buffer_len(8);
    shifted.set_data_length(0, 1, true, 0);
    require(!feed(shifted, {0xA2, 2, 0x11, 0x22}), "dynamic bounds: fixed cap discards incomplete frame");
    require(feed(shifted, {0xA2, 1, 0x33}), "dynamic bounds: discarded frame does not retain cached length");
}

void test_adjust_extremes()
{
    Parser positive;
    positive.set_data_length(0, 1, true, 128);
    require(!positive.parse_byte(0), "adjust +128: length byte is not a complete frame");
    for (int i = 0; i < 127; ++i) require(!positive.parse_byte(0x55), "adjust +128: waits for all payload");
    require(positive.parse_byte(0x55), "adjust +128: completes after 128 extra bytes");
    Parser negative;
    negative.set_data_length(0, 1, true, -128);
    require(negative.parse_byte(128), "adjust -128: zero adjusted payload completes");
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
    test_shared_headers_and_overlap();
    test_prefix_header_priority();
    test_header_frame_capacity();
    test_header_mismatch_clears_buffer();
    test_header_with_mask();
    test_header_footer_checksum();
    test_fixed_length_with_footer();
    test_dynamic_data_length_big_endian();
    test_dynamic_data_length_little_endian();
    test_dynamic_length_boundaries();
    test_dynamic_length_bounds_and_recovery();
    test_adjust_extremes();
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
