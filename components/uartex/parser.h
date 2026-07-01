#pragma once
#include <cstdint>
#include <vector>

class Parser
{
public:
    Parser();
    ~Parser();
    bool add_header(const unsigned char header);
    bool add_headers(const std::vector<unsigned char>& header);
    bool add_header_mask(const unsigned char mask);
    bool add_header_masks(const std::vector<unsigned char>& mask);
    bool add_header_candidate(const std::vector<unsigned char>& header, const std::vector<unsigned char>& mask = {});
    bool add_footer(const unsigned char footer);
    bool add_footers(const std::vector<unsigned char>& footer);
    bool parse_byte(const unsigned char byte);
    bool verify_checksum(const std::vector<unsigned char>& checksums);
    bool has_header();
    bool has_footer();
    void clear();
    const std::vector<unsigned char> header();
    const std::vector<unsigned char> data(const std::vector<unsigned char>& mask = {});
    const std::vector<unsigned char> buffer();
    const std::vector<unsigned char> apply_mask(const std::vector<unsigned char>& data, const std::vector<unsigned char>& mask) const;
    bool parse_header();
    bool parse_footer();
    bool parse_length();
    bool available();
    void set_checksum_len(size_t len);
    void set_total_len(size_t len);
    void set_buffer_len(size_t len);
    void set_data_length(uint8_t offset, uint8_t length, bool big_endian, int8_t adjust);
private:
    struct header_candidate_t
    {
        std::vector<unsigned char> data;
        std::vector<unsigned char> mask;
    };
    bool header_prefix_matches(const std::vector<unsigned char>& buffer, const header_candidate_t& candidate) const;
    bool calculate_dynamic_length();
    std::vector<header_candidate_t> header_candidates_;
    std::vector<unsigned char> active_header_;
    int matched_header_index_{-1};
    std::vector<unsigned char> footer_;
    std::vector<unsigned char> buffer_;
    size_t checksum_len_;
    size_t total_len_;
    size_t buffer_len_;
    bool has_data_length_{false};
    uint8_t data_length_offset_{0};
    uint8_t data_length_size_{1};
    bool data_length_big_endian_{true};
    int8_t data_length_adjust_{0};
    size_t dynamic_total_len_{0};
};
