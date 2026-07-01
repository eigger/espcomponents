#include "parser.h"

Parser::Parser()
{
    checksum_len_ = 0;
    total_len_ = 0;
    buffer_len_ = 256;
}

Parser::~Parser()
{
}

bool Parser::add_header(const unsigned char header)
{
    return this->add_header_candidate({header});
}

bool Parser::add_headers(const std::vector<unsigned char>& header)
{
    return this->add_header_candidate(header);
}

bool Parser::add_header_mask(const unsigned char mask)
{
    if (this->header_candidates_.empty())
        this->header_candidates_.push_back({});
    this->header_candidates_.back().mask.push_back(mask);
    return true;
}

bool Parser::add_header_masks(const std::vector<unsigned char>& mask)
{
    if (this->header_candidates_.empty())
        this->header_candidates_.push_back({});
    auto& candidate = this->header_candidates_.back();
    candidate.mask.insert(candidate.mask.end(), mask.begin(), mask.end());
    return true;
}

bool Parser::add_header_candidate(const std::vector<unsigned char>& header, const std::vector<unsigned char>& mask)
{
    if (header.empty()) return false;
    this->header_candidates_.push_back({header, mask});
    return true;
}

bool Parser::add_footer(const unsigned char footer)
{
    footer_.push_back(footer);
    return true;
}

bool Parser::add_footers(const std::vector<unsigned char>& footer)
{
    if (footer.empty()) return false;
    footer_.insert(footer_.end(), footer.begin(), footer.end());
    return true;
}

bool Parser::header_prefix_matches(const std::vector<unsigned char>& buffer, const header_candidate_t& candidate) const
{
    if (candidate.data.empty() || buffer.empty()) return false;
    std::vector<unsigned char> masked_buffer = apply_mask(buffer, candidate.mask);
    size_t size = masked_buffer.size() < candidate.data.size() ? masked_buffer.size() : candidate.data.size();
    return std::equal(masked_buffer.begin(), masked_buffer.begin() + size, candidate.data.begin());
}

bool Parser::parse_byte(const unsigned char byte)
{
    buffer_.push_back(byte);
    if ((total_len_ > 0 && buffer_.size() > total_len_) || buffer_.size() > buffer_len_)
    {
        buffer_.erase(buffer_.begin());
    }
    if (parse_header() == false)
    {
        buffer_.clear();
        matched_header_index_ = -1;
        active_header_.clear();
        return false;
    }
    if (parse_footer() == true) return true;
    if (parse_length() == true) return true;
    return false;
}

bool Parser::verify_checksum(const std::vector<unsigned char>& checksum)
{
    if (checksum_len_ != checksum.size()) return false;
    if (buffer_.size() < checksum.size() + footer_.size()) return false;
    return std::equal(buffer_.end() - checksum.size() - footer_.size(), buffer_.end() - footer_.size(), checksum.begin());
}

bool Parser::has_header()
{
    return !header_candidates_.empty();
}

bool Parser::has_footer()
{
    return !footer_.empty();
}

void Parser::clear()
{
    buffer_.clear();
    dynamic_total_len_ = 0;
    matched_header_index_ = -1;
    active_header_.clear();
}

bool Parser::parse_header()
{
    if (header_candidates_.empty()) return true;

    if (matched_header_index_ >= 0)
    {
        return this->header_prefix_matches(buffer_, header_candidates_[matched_header_index_]);
    }

    for (size_t i = 0; i < header_candidates_.size(); i++)
    {
        if (this->header_prefix_matches(buffer_, header_candidates_[i]))
        {
            matched_header_index_ = (int) i;
            active_header_ = header_candidates_[i].data;
            return true;
        }
    }
    return false;
}

const std::vector<unsigned char> Parser::apply_mask(const std::vector<unsigned char>& data, const std::vector<unsigned char>& mask) const
{
    if (mask.empty()) return data;
    std::vector<unsigned char> masked_data = data;
    for (size_t i = 0, j = 0; i < data.size() && j < mask.size(); i++, j++)
    {
        masked_data[i] &= mask[j];
    }
    return masked_data;
}

bool Parser::parse_footer()
{
    if (footer_.empty()) return false;
    if (buffer_.size() < footer_.size()) return false;
    if (total_len_ > 0 && buffer_.size() != total_len_) return false;

    if (has_data_length_)
    {
        if (!calculate_dynamic_length()) return false;
        if (buffer_.size() < dynamic_total_len_) return false;
    }

    return std::equal(buffer_.end() - footer_.size(), buffer_.end(), footer_.begin());
}

bool Parser::parse_length()
{
    if (total_len_ > 0)
    {
        if (footer_.size() > 0) return false;
        if (buffer_.size() != total_len_) return false;
        if (checksum_len_ > 0) return false;
        return true;
    }

    if (!has_data_length_) return false;
    if (!calculate_dynamic_length()) return false;
    if (buffer_.size() < dynamic_total_len_) return false;

    return true;
}

bool Parser::calculate_dynamic_length()
{
    size_t min_size = active_header_.size() + data_length_offset_ + data_length_size_;
    if (buffer_.size() < min_size) return false;

    if (dynamic_total_len_ == 0)
    {
        size_t length_pos = active_header_.size() + data_length_offset_;
        uint32_t data_len = 0;

        if (data_length_big_endian_)
        {
            for (uint8_t i = 0; i < data_length_size_; i++)
            {
                data_len = (data_len << 8) | buffer_[length_pos + i];
            }
        }
        else
        {
            for (int8_t i = data_length_size_ - 1; i >= 0; i--)
            {
                data_len = (data_len << 8) | buffer_[length_pos + i];
            }
        }

        dynamic_total_len_ = active_header_.size() + data_length_offset_ + data_length_size_ + data_len + data_length_adjust_ + checksum_len_ + footer_.size();
    }

    return true;
}

bool Parser::available()
{
    return !buffer_.empty();
}

const std::vector<unsigned char> Parser::header()
{
    if (active_header_.empty()) return {};
    size_t header_size = active_header_.size();
    if (buffer_.size() < header_size) header_size = buffer_.size();
    return std::vector<unsigned char>(buffer_.begin(), buffer_.begin() + header_size);
}

const std::vector<unsigned char> Parser::data(const std::vector<unsigned char>& mask)
{
    size_t offset = checksum_len_;
    if (buffer_.size() < active_header_.size() + footer_.size() + offset) return {};
    return apply_mask(std::vector<unsigned char>(buffer_.begin() + active_header_.size(), buffer_.end() - footer_.size() - offset), mask);
}

const std::vector<unsigned char> Parser::buffer()
{
    return buffer_;
}

void Parser::set_checksum_len(size_t len)
{
    checksum_len_ = len;
}

void Parser::set_total_len(size_t len)
{
    total_len_ = len;
}

void Parser::set_buffer_len(size_t len)
{
    buffer_len_ = len;
    buffer_.reserve(len + 1);
}

void Parser::set_data_length(uint8_t offset, uint8_t length, bool big_endian, int8_t adjust)
{
    has_data_length_ = true;
    data_length_offset_ = offset;
    data_length_size_ = length;
    data_length_big_endian_ = big_endian;
    data_length_adjust_ = adjust;
}
