#include "sip_message.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using esphome::sip_client::apply_route_set;
using esphome::sip_client::extract_angle_uri;
using esphome::sip_client::is_loose_route;
using esphome::sip_client::parse_sip_message;
using esphome::sip_client::SipMessage;
using esphome::sip_client::split_header_values;
using esphome::sip_client::via_branch;

namespace {

int g_failures = 0;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    g_failures++;
  }
}

void require_eq_str(const std::string &actual, const std::string &expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got \"" << actual << "\", expected \"" << expected
              << "\")\n";
    g_failures++;
  }
}

// An INVITE as it arrives through a 3CX SBC: two Via hops plus a
// Record-Route for the proxy.
const char *const PROXIED_INVITE =
    "INVITE sip:16@192.168.1.50:5060 SIP/2.0\r\n"
    "Via: SIP/2.0/UDP 192.168.1.194:5060;branch=z9hG4bK-sbc-1;rport\r\n"
    "Via: SIP/2.0/UDP 10.0.0.5:5060;branch=z9hG4bK-pbx-2\r\n"
    "Record-Route: <sip:192.168.1.194:5060;lr>\r\n"
    "Max-Forwards: 69\r\n"
    "From: \"Alice\" <sip:100@pbx.local>;tag=abc\r\n"
    "To: <sip:16@pbx.local>\r\n"
    "Call-ID: call-1@10.0.0.5\r\n"
    "CSeq: 1 INVITE\r\n"
    "Contact: <sip:100@10.0.0.5:5060>\r\n"
    "Content-Length: 0\r\n\r\n";

// ---------------- repeated list headers ----------------

void test_multiple_via_kept_in_order() {
  SipMessage m = parse_sip_message(PROXIED_INVITE);
  std::vector<std::string> vias = split_header_values(m.header("Via"));
  require(vias.size() == 2, "both Via hops are kept");
  if (vias.size() == 2) {
    require_eq_str(vias[0], "SIP/2.0/UDP 192.168.1.194:5060;branch=z9hG4bK-sbc-1;rport",
                   "topmost Via first");
    require_eq_str(vias[1], "SIP/2.0/UDP 10.0.0.5:5060;branch=z9hG4bK-pbx-2", "second Via next");
  }
  require_eq_str(via_branch(m.header("Via")), "z9hG4bK-sbc-1", "branch comes from the top Via");
}

void test_record_route_preserved() {
  SipMessage m = parse_sip_message(PROXIED_INVITE);
  std::vector<std::string> routes = split_header_values(m.header("Record-Route"));
  require(routes.size() == 1, "Record-Route is kept");
  if (!routes.empty()) {
    require_eq_str(routes[0], "<sip:192.168.1.194:5060;lr>", "Record-Route value");
    require(is_loose_route(routes[0]), "3CX record-route is a loose router");
  }
}

void test_repeated_record_route_rows_and_comma_list_are_equivalent() {
  const char *rows =
      "INVITE sip:a@b SIP/2.0\r\n"
      "Record-Route: <sip:p1;lr>\r\n"
      "Record-Route: <sip:p2;lr>\r\n"
      "Call-ID: x\r\n\r\n";
  const char *comma =
      "INVITE sip:a@b SIP/2.0\r\n"
      "Record-Route: <sip:p1;lr>, <sip:p2;lr>\r\n"
      "Call-ID: x\r\n\r\n";
  std::vector<std::string> a = split_header_values(parse_sip_message(rows).header("Record-Route"));
  std::vector<std::string> b = split_header_values(parse_sip_message(comma).header("Record-Route"));
  require(a.size() == 2 && b.size() == 2, "two route hops either way");
  require(a == b, "repeated rows and comma list parse identically");
  if (a.size() == 2) {
    require_eq_str(a[0], "<sip:p1;lr>", "first hop first");
    require_eq_str(a[1], "<sip:p2;lr>", "second hop second");
  }
}

void test_non_list_header_keeps_first_value() {
  const char *raw =
      "SIP/2.0 200 OK\r\n"
      "Contact: <sip:first@1.2.3.4>\r\n"
      "Contact: <sip:second@1.2.3.4>\r\n"
      "Call-ID: x\r\n\r\n";
  SipMessage m = parse_sip_message(raw);
  require_eq_str(m.header("Contact"), "<sip:first@1.2.3.4>", "Contact keeps first occurrence");
}

void test_compact_via_is_merged() {
  const char *raw =
      "INVITE sip:a@b SIP/2.0\r\n"
      "v: SIP/2.0/UDP h1;branch=z9hG4bK1\r\n"
      "Via: SIP/2.0/UDP h2;branch=z9hG4bK2\r\n"
      "i: x\r\n\r\n";
  SipMessage m = parse_sip_message(raw);
  require(split_header_values(m.header("Via")).size() == 2, "compact v: merges with Via");
  require_eq_str(m.header("Call-ID"), "x", "compact i: resolves to Call-ID");
}

void test_header_folding() {
  const char *raw =
      "INVITE sip:a@b SIP/2.0\r\n"
      "Via: SIP/2.0/UDP h1;branch=z9hG4bK1;\r\n"
      "  rport\r\n"
      "Call-ID: x\r\n\r\n";
  SipMessage m = parse_sip_message(raw);
  require_eq_str(m.header("Via"), "SIP/2.0/UDP h1;branch=z9hG4bK1; rport", "folded line joined");
}

// ---------------- split_header_values ----------------

void test_split_ignores_nested_commas() {
  std::vector<std::string> v = split_header_values(
      "\"Doe, John\" <sip:john@x;p=a,b>;q=1, <sip:p2;lr>,,  <sip:p3>");
  require(v.size() == 3, "quoted and angle-nested commas are not separators");
  if (v.size() == 3) {
    require_eq_str(v[0], "\"Doe, John\" <sip:john@x;p=a,b>;q=1", "display-name comma kept");
    require_eq_str(v[1], "<sip:p2;lr>", "second value trimmed");
    require_eq_str(v[2], "<sip:p3>", "empty items dropped");
  }
}

void test_split_empty() {
  require(split_header_values("").empty(), "empty header value -> no items");
  require(split_header_values(" , ").empty(), "only separators -> no items");
}

// ---------------- via_branch ----------------

void test_via_branch_variants() {
  require_eq_str(via_branch("SIP/2.0/UDP 1.2.3.4:5060;rport;branch=z9hG4bKabc"), "z9hG4bKabc",
                 "branch after other params");
  require_eq_str(via_branch("SIP/2.0/UDP 1.2.3.4;BRANCH=z9hG4bKup;rport"), "z9hG4bKup",
                 "branch name is case-insensitive");
  require_eq_str(via_branch("SIP/2.0/UDP a;branch=z9hG4bK1, SIP/2.0/UDP b;branch=z9hG4bK2"),
                 "z9hG4bK1", "comma-joined chain -> top Via only");
  require_eq_str(via_branch("SIP/2.0/UDP a;rport"), "", "no branch -> empty");
  require_eq_str(via_branch(""), "", "empty Via -> empty");
}

// ---------------- is_loose_route ----------------

void test_loose_route_detection() {
  require(is_loose_route("<sip:p;lr>"), ";lr at end");
  require(is_loose_route("<sip:p;lr;transport=udp>"), ";lr followed by param");
  require(is_loose_route("<sip:p;lr=on>"), ";lr=on (Asterisk style)");
  require(is_loose_route("<sip:p;LR>"), "case-insensitive");
  require(is_loose_route("sip:p;lr"), "bare URI");
  require(!is_loose_route("<sip:p>"), "no lr -> strict");
  require(!is_loose_route("<sip:p;lrx>"), ";lrx is not ;lr");
  require(!is_loose_route("<sip:user;lr@p>"), "lr inside the user part is not a URI param");
  require(!is_loose_route("<sip:user;lr=on@p>"), "lr=on inside the user part is not a URI param");
  require(!is_loose_route("<sip:user;lr;x@p>"), "lr;x inside the user part is not a URI param");
  require(is_loose_route("<sip:user;lr@p;lr>"), "userinfo lr does not hide a real ;lr param");
  require(!is_loose_route("<sip:p?Subject=;lr>"), "lr in the headers part is not a URI param");
  require(!is_loose_route(""), "empty -> strict");
}

// ---------------- apply_route_set ----------------

void test_route_set_empty_keeps_target() {
  std::string target = "sip:100@10.0.0.5:5060";
  std::string block = "stale";
  apply_route_set({}, target, block);
  require_eq_str(target, "sip:100@10.0.0.5:5060", "no route set: target untouched");
  require_eq_str(block, "", "no route set: no Route header");

  apply_route_set({"", "  "}, target, block);
  require_eq_str(block, "", "blank entries count as no route set");
}

void test_route_set_loose_router() {
  // 3CX / Kamailio style: the whole set travels as Route, one header per
  // hop, and the Request-URI stays the remote Contact.
  std::string target = "sip:100@10.0.0.5:5060";
  std::string block;
  apply_route_set({"<sip:192.168.1.194:5060;lr>", "<sip:10.0.0.1;lr>"}, target, block);
  require_eq_str(target, "sip:100@10.0.0.5:5060", "loose: Request-URI is the remote target");
  require_eq_str(block, "Route: <sip:192.168.1.194:5060;lr>\r\nRoute: <sip:10.0.0.1;lr>\r\n",
                 "loose: one Route line per hop, first hop first");
}

void test_route_set_strict_router() {
  // RFC 2543-style first hop: it takes the Request-URI and the remote target
  // is appended to the route set so it is not lost.
  std::string target = "sip:100@10.0.0.5:5060";
  std::string block;
  apply_route_set({"<sip:strict.example>", "<sip:10.0.0.1;lr>"}, target, block);
  require_eq_str(target, "sip:strict.example", "strict: first hop becomes the Request-URI");
  require_eq_str(block, "Route: <sip:10.0.0.1;lr>\r\nRoute: <sip:100@10.0.0.5:5060>\r\n",
                 "strict: remaining hops then the remote target");

  target = "sip:100@10.0.0.5:5060";
  apply_route_set({"<sip:strict.example>"}, target, block);
  require_eq_str(target, "sip:strict.example", "single strict hop takes the Request-URI");
  require_eq_str(block, "Route: <sip:100@10.0.0.5:5060>\r\n", "remote target still travels");

  target = "";
  apply_route_set({"sip:bare.strict"}, target, block);
  require_eq_str(target, "sip:bare.strict", "bare URI hop works without <>");
  require_eq_str(block, "", "no remote target -> nothing to append");
}

void test_route_set_from_reversed_2xx_record_route() {
  // UAC: Record-Route of the 2xx reversed is the route set (RFC 3261 §12.1.2).
  const char *raw =
      "SIP/2.0 200 OK\r\n"
      "Record-Route: <sip:outer;lr>\r\n"
      "Record-Route: <sip:inner;lr>\r\n"
      "Contact: <sip:bob@10.0.0.9>\r\n\r\n";
  std::vector<std::string> routes = split_header_values(parse_sip_message(raw).header("Record-Route"));
  std::vector<std::string> reversed(routes.rbegin(), routes.rend());
  std::string target = "sip:bob@10.0.0.9";
  std::string block;
  apply_route_set(reversed, target, block);
  require_eq_str(block, "Route: <sip:inner;lr>\r\nRoute: <sip:outer;lr>\r\n",
                 "nearest proxy (last Record-Route) is the first Route");
}

// ---------------- extract_angle_uri ----------------

void test_extract_angle_uri() {
  require_eq_str(extract_angle_uri("\"A\" <sip:a@b>;tag=1"), "sip:a@b", "angle URI");
  require_eq_str(extract_angle_uri("  sip:a@b "), "sip:a@b", "bare sip URI trimmed");
  require_eq_str(extract_angle_uri("nonsense"), "", "not a URI -> empty");
}

}  // namespace

int main() {
  test_multiple_via_kept_in_order();
  test_record_route_preserved();
  test_repeated_record_route_rows_and_comma_list_are_equivalent();
  test_non_list_header_keeps_first_value();
  test_compact_via_is_merged();
  test_header_folding();
  test_split_ignores_nested_commas();
  test_split_empty();
  test_via_branch_variants();
  test_loose_route_detection();
  test_extract_angle_uri();
  test_route_set_empty_keeps_target();
  test_route_set_loose_router();
  test_route_set_strict_router();
  test_route_set_from_reversed_2xx_record_route();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return EXIT_FAILURE;
  }
  std::cout << "sip_message tests passed\n";
  return EXIT_SUCCESS;
}
