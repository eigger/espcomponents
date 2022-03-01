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
	bool add_headers(const std::vector<unsigned char>& header);
	bool add_footer(const unsigned char footer);
	bool add_footers(const std::vector<unsigned char>& footer);

	bool parse_byte(const unsigned char byte);
	bool validate_data(const std::vector<unsigned char>& checksums);
	void clear(void);
	const std::vector<unsigned char> get_data(void);
	const std::vector<unsigned char> get_buffer(void);
	bool parse_header(void);
	bool parse_footer(void);
private:

	


private:
	std::vector<unsigned char> header_;
	std::vector<unsigned char> footer_;

	std::vector<unsigned char> buffer_;
};

