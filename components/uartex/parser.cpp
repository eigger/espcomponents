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

bool Parser::header_candidate_fits(const header_candidate_t& candidate) const
{
    size_t remaining = total_len_ > 0 ? std::min(total_len_, buffer_len_) : buffer_len_;
    const size_t dynamic_min = has_data_length_ ? static_cast<size_t>(data_length_offset_) + data_length_size_ : 0;
    const size_t parts[] = {candidate.data.size(), std::max(min_data_len_, dynamic_min), checksum_len_, footer_.size()};
    for (size_t part : parts)
    {
        if (part > remaining) return false;
        remaining -= part;
    }
    return true;
}

bool Parser::header_prefix_matches(const std::vector<unsigned char>& buffer, const header_candidate_t& candidate) const
{
    if (candidate.data.empty() || buffer.empty()) return false;
    const size_t size = std::min(buffer.size(), candidate.data.size());
    for (size_t i = 0; i < size; ++i)
    {
        const unsigned char byte = i < candidate.mask.size() ? buffer[i] & candidate.mask[i] : buffer[i];
        if (byte != candidate.data[i]) return false;
    }
    return true;
}

bool Parser::parse_byte(const unsigned char byte)
{
    buffer_.push_back(byte);
    if ((total_len_ > 0 && buffer_.size() > total_len_) || buffer_.size() > buffer_len_)
    {
        buffer_.erase(buffer_.begin());
        dynamic_total_len_ = 0;
        invalid_dynamic_length_ = false;
        matched_header_index_ = -1;
        header_provisional_ = false;
        active_header_.clear();
    }
    if (!header_candidates_.empty())
    {
        while (!buffer_.empty() && !parse_header())
        {
            // A matching prefix is not yet a complete header: wait for more
            // bytes before choosing a candidate or attempting frame parsing.
            bool prefix = false;
            for (const auto &candidate : header_candidates_)
            {
                if (header_candidate_fits(candidate) && header_prefix_matches(buffer_, candidate)) { prefix = true; break; }
            }
            if (prefix) return false;

            // Keep the longest suffix that could start a header. This also
            // handles repeated start bytes, e.g. AA AA 55 for header AA 55.
            buffer_.erase(buffer_.begin());
            dynamic_total_len_ = 0;
            invalid_dynamic_length_ = false;
            matched_header_index_ = -1;
            header_provisional_ = false;
            active_header_.clear();
        }
        if (buffer_.empty()) return false;
    }
    if (parse_footer() == true) return true;
    if (parse_length() == true) return true;
    // A length outside the receive buffer can never form a valid frame.
    // Discard it now so the next header can start a new frame.
    if (invalid_dynamic_length_ && !header_provisional_) clear();
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
    invalid_dynamic_length_ = false;
    matched_header_index_ = -1;
    header_provisional_ = false;
    active_header_.clear();
}

bool Parser::parse_header()
{
    if (header_candidates_.empty()) return true;
    if (matched_header_index_ >= 0 && !header_provisional_)
        return this->header_prefix_matches(buffer_, header_candidates_[matched_header_index_]);

    bool earlier_incomplete = false;
    for (size_t i = 0; i < header_candidates_.size(); i++)
    {
        const auto &candidate = header_candidates_[i];
        if (!header_candidate_fits(candidate) || !this->header_prefix_matches(buffer_, candidate)) continue;
        if (buffer_.size() < candidate.data.size())
        {
            earlier_incomplete = true;
            continue;
        }
        if (matched_header_index_ != static_cast<int>(i))
        {
            dynamic_total_len_ = 0;
            invalid_dynamic_length_ = false;
        }
        matched_header_index_ = static_cast<int>(i);
        header_provisional_ = earlier_incomplete;
        active_header_ = candidate.data;
        return true;
    }
    if (matched_header_index_ >= 0)
    {
        matched_header_index_ = -1;
        header_provisional_ = false;
        active_header_.clear();
        dynamic_total_len_ = 0;
        invalid_dynamic_length_ = false;
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
        if (buffer_.size() != dynamic_total_len_) return false;
    }

    return std::equal(buffer_.end() - footer_.size(), buffer_.end(), footer_.begin());
}

bool Parser::parse_length()
{
    // A dynamic length defines the frame boundary even when a footer or a
    // checksum is present. Validation of those fields happens at that boundary.
    if (has_data_length_)
    {
        if (!calculate_dynamic_length()) return false;
        return buffer_.size() == dynamic_total_len_;
    }

    if (total_len_ > 0)
    {
        if (footer_.size() > 0) return false;
        if (buffer_.size() != total_len_) return false;
        if (checksum_len_ > 0) return false;
        return true;
    }
    return false;
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

        const int64_t adjusted_payload = static_cast<int64_t>(data_len) + data_length_adjust_;
        // Subtract each part from the cap before adding, avoiding overflow on
        // 32-bit targets and rejecting frames that can never fit in the buffer.
        uint64_t remaining = buffer_len_;
        const uint64_t parts[] = {active_header_.size(), data_length_offset_, data_length_size_,
                                  checksum_len_, footer_.size()};
        if (adjusted_payload < 0)
        {
            invalid_dynamic_length_ = true;
            return false;
        }
        for (uint64_t part : parts)
        {
            if (part > remaining)
            {
                invalid_dynamic_length_ = true;
                return false;
            }
            remaining -= part;
        }
        if (static_cast<uint64_t>(adjusted_payload) > remaining)
        {
            invalid_dynamic_length_ = true;
            return false;
        }
        dynamic_total_len_ = buffer_len_ - remaining + static_cast<size_t>(adjusted_payload);
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

void Parser::set_data_length(uint8_t offset, uint8_t length, bool big_endian, int16_t adjust)
{
    has_data_length_ = true;
    data_length_offset_ = offset;
    data_length_size_ = length;
    data_length_big_endian_ = big_endian;
    data_length_adjust_ = adjust;
}
