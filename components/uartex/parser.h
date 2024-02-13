#pragma once
#include <queue>
//#include <mutex>
#include <vector>
#include <string>
#include <memory>

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
	void set_checksum_len(size_t len);
	const std::vector<unsigned char> get_checksum();
	
private:
	std::vector<unsigned char> header_;
	std::vector<unsigned char> footer_;
	std::vector<unsigned char> buffer_;
	size_t checksum_len_;
};

