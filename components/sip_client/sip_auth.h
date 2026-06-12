#pragma once
#include <string>
#include "esphome/components/md5/md5.h"

namespace esphome {
namespace sip_client {

// MD5 hex digest of a string, using ESPHome's md5 wrapper (avoids depending on
// a particular mbedtls API version).
static inline std::string md5_hex(const std::string &s) {
  md5::MD5Digest h;
  h.init();
  h.add(reinterpret_cast<const uint8_t *>(s.data()), s.size());
  h.calculate();
  char out[33];
  h.get_hex(out);
  out[32] = '\0';
  return std::string(out, 32);
}

// RFC 2617 HTTP Digest response. When qop is non-empty, the qop=auth variant
// (with nc/cnonce) is used; otherwise the legacy form.
static inline std::string digest_response(const std::string &username, const std::string &password,
                                          const std::string &realm, const std::string &method,
                                          const std::string &uri, const std::string &nonce,
                                          const std::string &qop, const std::string &nc,
                                          const std::string &cnonce) {
  std::string ha1 = md5_hex(username + ":" + realm + ":" + password);
  std::string ha2 = md5_hex(method + ":" + uri);
  if (!qop.empty()) {
    return md5_hex(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
  }
  return md5_hex(ha1 + ":" + nonce + ":" + ha2);
}

}  // namespace sip_client
}  // namespace esphome
