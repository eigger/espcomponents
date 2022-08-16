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
	bool validate(const std::vector<unsigned char>& checksums);
	void clear();
	const std::vector<unsigned char> data();
	const std::vector<unsigned char> buffer();
	bool parse_header();
	bool parse_footer();
	void use_checksum(size_t len);
	const std::vector<unsigned char> checksum();
	
private:
	std::vector<unsigned char> header_;
	std::vector<unsigned char> footer_;
	std::vector<unsigned char> buffer_;
	size_t checksum_len_;
};

