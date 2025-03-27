#pragma once
#include <queue>
#include <vector>
#include <string>
#include <memory>

class Parser
{
public:
    Parser();
    ~Parser();
    bool add_header(const unsigned char header);
    bool add_headers(const std::vector<unsigned char>& header);
    bool add_header_mask(const unsigned char mask);
    bool add_header_masks(const std::vector<unsigned char>& mask);
    bool add_footer(const unsigned char footer);
    bool add_footers(const std::vector<unsigned char>& footer);
    bool parse_byte(const unsigned char byte);
    bool verify_checksum(const std::vector<unsigned char>& checksums);
    void clear();
    const std::vector<unsigned char> header();
    const std::vector<unsigned char> data(const std::vector<unsigned char>& mask = {});
    const std::vector<unsigned char> buffer();
    const std::vector<unsigned char> apply_mask(const std::vector<unsigned char>& data, const std::vector<unsigned char>& mask);
    bool parse_header();
    bool parse_footer();
    bool parse_length();
    bool available();
    void set_checksum_len(size_t len);
    void set_total_len(size_t len);
private:
    std::vector<unsigned char> header_;
    std::vector<unsigned char> header_mask_;
    std::vector<unsigned char> footer_;
    std::vector<unsigned char> buffer_;
    size_t checksum_len_;
    size_t total_len_;
};

