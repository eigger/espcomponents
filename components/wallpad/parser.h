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
	bool add_header(const unsigned char header);
	bool add_footer(const unsigned char footer);
	int get_header_count(void);
	unsigned char get_header(const int index);
	int get_footer_count(void);
	unsigned char get_footer(const int index);

	bool parse_byte(const unsigned char byte);
	bool validate_data(std::vector<unsigned char> checksums);
private:

	void clear_buffer(void);
	bool parse_footer(void);
	bool get_datas_from_buffer(void);

private:
	std::vector<unsigned char> headers;
	std::vector<unsigned char> footers;

	std::vector<unsigned char> buffers;
	std::vector<unsigned char> datas;

	int header_index;
};

