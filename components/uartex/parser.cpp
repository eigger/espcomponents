#include "parser.h"

Parser::Parser()
{
	checksum_len_ = 0;
	total_len_ = 0;
}


Parser::~Parser()
{
}


bool Parser::add_header(const unsigned char header)
{
	header_.push_back(header);
	return true;
}

bool Parser::add_headers(const std::vector<unsigned char>& header)
{
	if (header.size() == 0) return false;
	header_.insert(header_.end(), header.begin(), header.end());
	return true;
}

bool Parser::add_header_mask(const unsigned char mask)
{
	header_mask_.push_back(mask);
	return true;
}

bool Parser::add_header_masks(const std::vector<unsigned char>& mask)
{
	if (mask.size() == 0) return false;
	header_mask_.insert(header_.end(), mask.begin(), mask.end());
	return true;
}

bool Parser::add_footer(const unsigned char footer)
{
	footer_.push_back(footer);
	return true;
}

bool Parser::add_footers(const std::vector<unsigned char>& footer)
{
	if (footer.size() == 0) return false;
	footer_.insert(footer_.end(), footer.begin(), footer.end());
	return true;
}

bool Parser::parse_byte(const unsigned char byte)
{
	buffer_.push_back(byte);
	if (parse_header() == false)
	{
		buffer_.clear();
		return false;
	}
	if (parse_footer() == true) return true;
	if (parse_length() == true) return true;
	return false;
}

bool Parser::verify_checksum(const std::vector<unsigned char>& checksum)
{
	if (checksum_len_ != checksum.size()) return false;
	if (buffer_.size() < checksum.size()) return false;
	return std::equal(buffer_.end() - checksum.size() - footer_.size(), buffer_.end() - footer_.size(), checksum.begin());
}

void Parser::clear()
{
	buffer_.clear();
}

const std::vector<uint8_t> masked_data(const std::vector<uint8_t>& data, const state_t* state)
{
    std::vector<uint8_t> masked_data = data;
    for (size_t i = state->offset, j = 0; i < data.size() && j < state->mask.size(); i++, j++)
    {
        masked_data[i] &= state->mask[j];
    }
    return masked_data;
}


bool Parser::parse_header()
{
	if (header_.size() == 0) return true;
	std::vector<uint8_t> masked_header = header_;
	for (size_t i = 0, j = 0; i < header_.size() && j < header_mask_.size(); i++, j++)
    {
        masked_header[i] &= header_mask_[j];
    }
	size_t size = buffer_.size() < masked_header.size() ? buffer_.size() : masked_header.size();
	return std::equal(buffer_.begin(), buffer_.begin() + size, masked_header.begin());
}

bool Parser::parse_footer()
{
	if (footer_.size() == 0) return false;
	if (buffer_.size() < footer_.size()) return false;
	if (total_len_ > 0 && buffer_.size() != total_len_) return false;
	return std::equal(buffer_.end() - footer_.size(), buffer_.end(), footer_.begin());
}

bool Parser::parse_length()
{
	if (total_len_ == 0) return false;
	if (footer_.size() > 0) return false;
	if (buffer_.size() != total_len_) return false;
	return true;
}

bool Parser::available()
{
	if (buffer_.size() == 0) return false;
	return true;
}

const std::vector<unsigned char> Parser::data()
{
	size_t offset = checksum_len_;
	if (buffer_.size() < header_.size() + footer_.size() + offset) return {};
	return std::vector<unsigned char>(buffer_.begin() + header_.size(), buffer_.end() - footer_.size() - offset);
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