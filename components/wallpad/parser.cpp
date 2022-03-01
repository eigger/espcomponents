#include "parser.h"

Parser::Parser()
{
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
	header_.insert(header_.end(), header.begin(), header.end());
	return true;
}

bool Parser::add_footer(const unsigned char footer)
{
	footer_.push_back(footer);
	return true;
}

bool Parser::add_footers(const std::vector<unsigned char>& footer)
{
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
	return false;
}

bool Parser::validate_data(const std::vector<unsigned char>& checksum)
{
	if (buffer_.size() < checksum.size()) return false;
	return std::equal(buffer_.end() - checksum.size() - footer_.size(), buffer_.end() - footer_.size(), checksum.begin());
}

void Parser::clear(void)
{
	buffer_.clear();
}

bool Parser::parse_header(void)
{
	if (header_.size() == 0) return true;
	size_t size = buffer_.size() < header_.size() ? buffer_.size() : header_.size();
	return std::equal(buffer_.begin(), buffer_.begin() + size, header_.begin());
}

bool Parser::parse_footer(void)
{
	if (footer_.size() == 0) return false;
	if (buffer_.size() < footer_.size()) return false;
	return std::equal(buffer_.end() - footer_.size(), buffer_.end(), footer_.begin());
}

const std::vector<unsigned char> Parser::get_data(void)
{
	if (buffer_.size() < header_.size() + footer_.size()) return std::vector<unsigned char>();
	return std::vector<unsigned char>(buffer_.begin() + header_.size(), buffer_.end() - footer_.size());
}

const std::vector<unsigned char> Parser::get_buffer(void)
{
	return buffer_;
}