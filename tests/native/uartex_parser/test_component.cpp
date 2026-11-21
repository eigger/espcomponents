#include "uartex.h"
#include <cassert>
using namespace esphome::uartex;
class TestComponent : public UARTExComponent {
 public:
  bool feed(uint8_t byte) { bytes.push_back(byte); return parse_bytes(); }
  ERROR validate() { return validate_data(); }
};
int main() {
  for (bool footer : {false, true}) for (bool checksum : {false, true}) {
    TestComponent c;
    c.set_rx_data_length(0, 1, ENDIAN_BIG, 0);
    if (footer) c.set_rx_footer({0x0D});
    if (checksum) c.set_rx_checksum(CHECKSUM_ADD);
    c.setup();
    assert(!c.feed(2));
    // The prefix {2,2} has a valid ADD checksum, but is incomplete.
    assert(!c.feed(2));
    bool complete = c.feed(3);
    if (checksum) { assert(!complete); complete = c.feed(7); }
    if (footer) { assert(!complete); complete = c.feed(0x0D); }
    assert(complete && c.validate() == ERROR_NONE);
  }
  TestComponent invalid_footer;
  invalid_footer.set_rx_data_length(0, 1, ENDIAN_BIG, 0);
  invalid_footer.set_rx_footer({0x0D}); invalid_footer.setup();
  assert(!invalid_footer.feed(1)); assert(!invalid_footer.feed(4));
  assert(invalid_footer.feed(0xFF)); assert(invalid_footer.validate() == ERROR_FOOTER);
  TestComponent invalid_checksum;
  invalid_checksum.set_rx_data_length(0, 1, ENDIAN_BIG, 0);
  invalid_checksum.set_rx_checksum(CHECKSUM_ADD); invalid_checksum.setup();
  assert(!invalid_checksum.feed(1)); assert(!invalid_checksum.feed(4));
  assert(invalid_checksum.feed(0)); assert(invalid_checksum.validate() == ERROR_CHECKSUM);
  TestComponent recovery;
  recovery.add_rx_header({{0xA0}, {}});
  recovery.set_rx_data_length(0, 4, ENDIAN_BIG, 1); recovery.setup();
  for (uint8_t b : {0xA0, 0xFF, 0xFF, 0xFF, 0xFF}) assert(!recovery.feed(b));
  for (uint8_t b : {0xA0, 0, 0, 0, 1, 0x11}) assert(!recovery.feed(b));
  assert(recovery.feed(0x22)); assert(recovery.validate() == ERROR_NONE);
  TestComponent shared_header;
  shared_header.add_rx_header({{0xA0, 0x10}, {0xF0, 0xF0}});
  shared_header.add_rx_header({{0xA0, 0x20}, {0xF0, 0xF0}});
  shared_header.set_rx_footer({0xEE}); shared_header.set_rx_checksum(CHECKSUM_ADD); shared_header.setup();
  for (uint8_t b : {0xA1, 0xA2, 0x2F, 0x33, 0x04}) assert(!shared_header.feed(b));
  assert(shared_header.feed(0xEE)); assert(shared_header.validate() == ERROR_NONE);
  // Two different headers in consecutive frames must both reach the component
  // callback, even when their bytes are already queued in the UART buffer.
  for (auto priority : {PRIORITY_DATA, PRIORITY_LOOP}) {
    TestComponent consecutive;
    consecutive.add_rx_header({{0xA0}, {}});
    consecutive.add_rx_header({{0xB0}, {0xF0}});
    consecutive.set_rx_length(2); consecutive.set_rx_priority(priority); consecutive.setup();
    std::vector<std::vector<uint8_t>> received;
    consecutive.add_on_read_callback([&](const uint8_t *p, uint16_t n) { received.emplace_back(p, p + n); });
    for (uint8_t b : {0xA0, 1, 0xB2, 2}) consecutive.bytes.push_back(b);
    consecutive.loop(); consecutive.loop();
    assert((received == std::vector<std::vector<uint8_t>>{{0xA0, 1}, {0xB2, 2}}));
  }
  TestComponent short_fixed;
  short_fixed.add_rx_header({{0xAA, 0xBB, 0xCC, 0xDD}, {}});
  short_fixed.add_rx_header({{0xAA}, {}}); short_fixed.set_rx_length(3); short_fixed.setup();
  assert(!short_fixed.feed(0xAA)); assert(!short_fixed.feed(0xBB));
  assert(short_fixed.feed(0xCC)); assert(short_fixed.validate() == ERROR_NONE);
  TestComponent exact_long_header;
  exact_long_header.add_rx_header({{0xAA, 0xBB, 0xCC}, {}});
  exact_long_header.add_rx_header({{0xAA}, {}}); exact_long_header.set_rx_length(3); exact_long_header.setup();
  assert(!exact_long_header.feed(0xAA)); assert(!exact_long_header.feed(0xBB));
  assert(exact_long_header.feed(0xCC)); assert(exact_long_header.validate() == ERROR_NONE);
  TestComponent no_trailer_room;
  no_trailer_room.add_rx_header({{0xAA, 0xBB}, {}});
  no_trailer_room.add_rx_header({{0xAA}, {}}); no_trailer_room.set_rx_length(4);
  no_trailer_room.set_rx_footer({0xEE}); no_trailer_room.set_rx_checksum(CHECKSUM_ADD); no_trailer_room.setup();
  assert(!no_trailer_room.feed(0xAA)); assert(!no_trailer_room.feed(0xBB));
  assert(!no_trailer_room.feed(0x65)); assert(no_trailer_room.feed(0xEE));
  assert(no_trailer_room.validate() == ERROR_NONE);
  TestComponent short_footer;
  short_footer.add_rx_header({{0xAA, 1, 0xEE, 0xFF}, {}});
  short_footer.add_rx_header({{0xAA}, {}}); short_footer.set_rx_footer({0xEE}); short_footer.setup();
  assert(!short_footer.feed(0xAA)); assert(!short_footer.feed(1));
  assert(short_footer.feed(0xEE)); assert(short_footer.validate() == ERROR_NONE);
  TestComponent short_dynamic;
  short_dynamic.add_rx_header({{0xAA, 1, 0xEE, 0xFF}, {}});
  short_dynamic.add_rx_header({{0xAA}, {}}); short_dynamic.set_rx_data_length(0, 1, ENDIAN_BIG, 0); short_dynamic.setup();
  assert(!short_dynamic.feed(0xAA)); assert(!short_dynamic.feed(1));
  assert(short_dynamic.feed(0xEE)); assert(short_dynamic.validate() == ERROR_NONE);
  TestComponent short_checksum;
  short_checksum.add_rx_header({{0xAA, 1, 0xAB, 0xFF}, {}});
  short_checksum.add_rx_header({{0xAA}, {}}); short_checksum.set_rx_checksum(CHECKSUM_ADD); short_checksum.setup();
  assert(!short_checksum.feed(0xAA)); assert(!short_checksum.feed(1));
  assert(short_checksum.feed(0xAB)); assert(short_checksum.validate() == ERROR_NONE);
  TestComponent longer_checksum;
  longer_checksum.add_rx_header({{0xAA, 1, 0xFF, 0xDD}, {}});
  longer_checksum.add_rx_header({{0xAA}, {}}); longer_checksum.set_rx_checksum(CHECKSUM_ADD); longer_checksum.setup();
  for (uint8_t b : {0xAA, 1, 0xFF, 0xDD, 2}) assert(!longer_checksum.feed(b));
  assert(longer_checksum.feed(0x89)); assert(longer_checksum.validate() == ERROR_NONE);
  TestComponent adjust_positive;
  adjust_positive.set_rx_data_length(0, 1, ENDIAN_BIG, 128); adjust_positive.setup();
  assert(!adjust_positive.feed(0));
  for (int i = 0; i < 127; ++i) assert(!adjust_positive.feed(0x55));
  assert(adjust_positive.feed(0x55)); assert(adjust_positive.validate() == ERROR_NONE);
  TestComponent adjust_negative;
  adjust_negative.set_rx_data_length(0, 1, ENDIAN_BIG, -128); adjust_negative.setup();
  assert(adjust_negative.feed(128)); assert(adjust_negative.validate() == ERROR_NONE);
  TestComponent fixed;
  fixed.set_rx_length(2); fixed.setup(); assert(!fixed.feed(1)); assert(fixed.feed(4)); assert(fixed.validate() == ERROR_NONE);
  TestComponent checksum_only;
  checksum_only.set_rx_checksum(CHECKSUM_ADD); checksum_only.setup(); assert(!checksum_only.feed(2)); assert(checksum_only.feed(2)); assert(checksum_only.validate() == ERROR_NONE);
}
