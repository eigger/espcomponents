#include "parser.h"

Parser::Parser()
{
	clear_buffer();
}


Parser::~Parser()
{
}


bool Parser::add_header(const unsigned char header)
{
	headers.push_back(header);
	return true;
}


bool Parser::add_footer(const unsigned char footer)
{
	footers.push_back(footer);
	return true;
}

int Parser::get_header_count(void)
{
	return headers.size();
}

unsigned char Parser::get_header(const int index)
{
	if (index < 0 || index >= headers.size()) return 0;
	return headers.at(index);
}

int Parser::get_footer_count(void)
{
	return footers.size();
}

unsigned char Parser::get_footer(const int index)
{
	if (index < 0 || index >= footers.size()) return 0;
	return footers.at(index);
}

bool Parser::parse_byte(const unsigned char byte)
{
	if (header_index < headers.size())
	{
		if (headers[header_index] == byte)
		{
			buffers.push_back(byte);
			header_index++;
		}
	}
	else if (footers.size() == 0)
	{
		buffers.push_back(byte);
		datas.push_back(byte);
	}
	else
	{
		buffers.push_back(byte);
		if (parse_footer() == true) return true;
	}
	return false;
}

bool Parser::validate_data(std::vector<unsigned char> checksums)
{
	//if (headers.size() > 0)
	//{
	//	if (std::equal(datas.begin(), datas.begin() + headers.size(), headers.begin()) == false) return false;
	//}
	//if (footers.size() > 0)
	//{
	//	std::equal(datas.end() - footers.size(), datas.end(), footers.begin());
	//}
	if (datas.size() < checksums.size()) return false;
	return std::equal(datas.end() - checksums.size(), datas.end(), checksums.begin());
}

void Parser::clear_buffer(void)
{
	buffers.clear();
	datas.clear();
	header_index = 0;
}

bool Parser::parse_footer(void)
{
	if (buffers.size() < footers.size()) return false;
	return std::equal(buffers.end() - footers.size(), buffers.end(), footers.begin());
}

bool Parser::get_datas_from_buffer(void)
{
	unsigned int data_count = buffers.size() - headers.size() - footers.size();
	for (unsigned int i = headers.size(); i < headers.size() + data_count; i++)
	{
		datas.push_back(buffers.at(i));
	}
	return true;
}
