#pragma once

#include <fstream>
#include <string>
#include <sstream>
#include <random>
#include <vector>

std::string RunCmd(std::string cmd) {
    std::string data;
    FILE *stream;
    const int max_buffer = 2048;
    char buffer[max_buffer];
    cmd.append(" 2>&1");

    stream = popen(cmd.c_str(), "r");
    if (stream) {
        while (!feof(stream))
        if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
        pclose(stream);
    }
    return data;
}

std::string RandomString(const std::string::size_type length) {
	using namespace std;
	static string chars = "0123456789"
	                      "abcdefghijklmnopqrstuvwxyz"
	                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	static mt19937 gen{random_device{}()};
	static uniform_int_distribution<string::size_type> dis(0, chars.length() - 1);

	string s(length, '*');

	for (auto &c : s)
		c = chars[dis(gen)];

	return s;
}

std::vector<std::string> split(const std::string &text, char delim)
{
    size_t pos = text.find(delim);
    size_t initial_pos = 0;
    auto strs = std::vector<std::string>{};

    while(pos != -1) {
        strs.push_back(text.substr(initial_pos, pos - initial_pos));
        initial_pos = pos + 1;

        pos = text.find(delim, initial_pos);
    }

    strs.push_back(text.substr(initial_pos, std::min(pos, text.size()) - initial_pos + 1));

    return strs;
}

std::vector<int> convert_to_int(const std::vector<std::string> strings) {
    auto ints = std::vector<int>{};
    ints.reserve(strings.size());
    for (auto const &str : strings) {
        if (str.size() == 0) continue;
        ints.push_back(stoi(str));
    }

    return ints;
}
