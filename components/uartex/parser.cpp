#include "Parser.h"

Parser::Parser()
{
	InitializeBuffer();
}


Parser::~Parser()
{
}


bool Parser::AddHeader(const unsigned char header)
{
	for (int i = 0; i < m_headers.size(); i++) {
		if (m_headers.at(i) == header) return false;
	}
	m_headers.push_back(header);
	return true;
}


bool Parser::AddFooter(const unsigned char footer)
{
	for (int i = 0; i < m_footers.size(); i++) {
		if (m_footers.at(i) == footer) return false;
	}
	m_footers.push_back(footer);
	return true;
}

int Parser::GetHeaderCount(void)
{
	return m_headers.size();
}

unsigned char Parser::GetHeader(const int index)
{
	if (index < 0 || index >= m_headers.size()) return 0;
	return m_headers.at(index);
}

int Parser::GetFooterCount(void)
{
	return m_footers.size();
}

unsigned char Parser::GetFooter(const int index)
{
	if (index < 0 || index >= m_footers.size()) return 0;
	return m_footers.at(index);
}

bool Parser::ParseByte(const unsigned char byte)
{
	bool result = false;
	//m_mutex.lock();

	if (m_header_index < m_headers.size()) {
		if (m_headers.at(m_header_index) == byte) {
			m_buffer.push_back(byte);
			m_header_index++;
			result = true;
		}
	}
	else if (m_footers.size() == 0) {
		m_buffer.push_back(byte);
		m_data.push_back(byte);
		result = true;
	}
	else {
		result = true;
		m_buffer.push_back(byte);
		if (ParseFooter() == true) {
			SetParsingDataFromBuffer();
			result = PushParsingData();
			InitializeBuffer();
		}
	}
	//m_mutex.unlock();
	return result;
}

bool Parser::ParseBytes(const unsigned char bytes[], int size)
{
	bool result = true;
	for (int len = 0; len < size; len++) {
		if (ParseByte(bytes[len]) == false) result = false;
	}
	return result;
}

void Parser::InitializeBuffer(void)
{
	m_buffer.clear();
	m_data.clear();
	m_header_index = 0;
}

bool Parser::ParseFooter(void)
{
	bool result = true;
	if (m_buffer.size() < m_footers.size()) return false;
	for (int i = 0; i < m_footers.size(); i++) {
		unsigned char footer = m_footers.at(m_footers.size() - i - 1);
		unsigned char buffer = m_buffer.at(m_buffer.size() - i - 1);
		if (footer != buffer) {
			result = false;
			break;
		}
	}
	return result;
}

bool Parser::SetParsingDataFromBuffer(void)
{
	int data_count = m_buffer.size() - m_headers.size() - m_footers.size();
	for (int i = m_headers.size(); i < m_headers.size() + data_count; i++) {
		m_data.push_back(m_buffer.at(i));
	}
	return true;
}

bool Parser::PushParsingData(void)
{
	bool result = true;
	//m_mutex.lock();
	if (m_queue.size() < MAX_QUEUE_COUNT) {
		if (m_data.size() > 0) {
			m_queue.push(m_data);
		}
		result = true;
	}
	else {
		result = false;
	}
	//m_mutex.unlock();
	return result;
}

std::vector<unsigned char> Parser::PopParsingData(void)
{
	int count = 0;
	std::vector<unsigned char> out_data;
	//m_mutex.lock();
	if (m_footers.size() == 0) {
		out_data = m_data;
		InitializeBuffer();
	}
	else if (!m_queue.empty()) {
		std::vector<unsigned char> queue = m_queue.front();
		m_queue.pop();
		out_data = queue;
	}
	//m_mutex.unlock();
	return out_data;
}

std::string Parser::PopParsingDataToString(void)
{
	std::vector<unsigned char> data;
	std::string str_result;
	data = PopParsingData();
	if (data.size() == 0) return "";
	for (unsigned char ch : data) {
		str_result += (char)ch;
	}	
	return str_result;
}


std::string Parser::Encode(std::string msg)
{
	std::string str;
	for (unsigned char ch : m_headers) {
		str += (char)ch;
	}	
	str += msg;
	for (unsigned char ch : m_footers) {
		str += (char)ch;
	}	
	return str;
}
