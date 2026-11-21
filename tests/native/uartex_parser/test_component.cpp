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
  TestComponent fixed;
  fixed.set_rx_length(2); fixed.setup(); assert(!fixed.feed(1)); assert(fixed.feed(4)); assert(fixed.validate() == ERROR_NONE);
  TestComponent checksum_only;
  checksum_only.set_rx_checksum(CHECKSUM_ADD); checksum_only.setup(); assert(!checksum_only.feed(2)); assert(checksum_only.feed(2)); assert(checksum_only.validate() == ERROR_NONE);
}
