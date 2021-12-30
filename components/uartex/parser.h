#pragma once
#include <queue>
//#include <mutex>
#include <vector>
#include <string>
#include <memory>
#define MAX_QUEUE_COUNT 100

class Parser
{
public:
	Parser();
	~Parser();
	bool AddHeader(const unsigned char header);
	bool AddFooter(const unsigned char footer);
	int GetHeaderCount(void);
	unsigned char GetHeader(const int index);
	int GetFooterCount(void);
	unsigned char GetFooter(const int index);

	bool ParseByte(const unsigned char byte);
	bool ParseBytes(const unsigned char bytes[], int size);

	std::vector<unsigned char> PopParsingData(void);
	std::string PopParsingDataToString(void);
	std::string Encode(std::string msg);
	
private:
	bool PushParsingData(void);
	void InitializeBuffer(void);
	bool ParseFooter(void);
	bool SetParsingDataFromBuffer(void);

private:
	std::vector<unsigned char> m_headers;
	std::vector<unsigned char> m_footers;

	std::vector<unsigned char> m_buffer;
	std::vector<unsigned char> m_data;

	std::queue <std::vector<unsigned char>> m_queue;
	//std::recursive_mutex m_mutex;
	int m_header_index;
};

