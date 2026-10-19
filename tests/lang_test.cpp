// The language files: every file has the keys of ja.txt (no more, no fewer, none twice), and every value has the
// same places to fill ({0}, {1}, ...) as the Japanese one. Argument: the lang folder.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

static int failures = 0;

static std::string trim(const std::string &s)
{
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
	return s.substr(a, b - a);
}

static std::map<std::string, std::string> load(const std::string &path)
{
	std::map<std::string, std::string> out;
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		std::printf("FAIL cannot open %s\n", path.c_str());
		++failures;
		return out;
	}
	std::string line;
	bool first = true;
	while (std::getline(in, line)) {
		if (first && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
		first = false;
		line = trim(line);
		if (line.empty() || line[0] == '#') continue;
		const size_t eq = line.find('=');
		if (eq == std::string::npos) {  // a value broken by a real new line (\n is written as two letters)
			std::printf("FAIL %s: a line without a key: %s\n", path.c_str(), line.c_str());
			++failures;
			continue;
		}
		const std::string k = trim(line.substr(0, eq));
		if (out.count(k)) {
			std::printf("FAIL %s: %s twice\n", path.c_str(), k.c_str());
			++failures;
		}
		out[k] = trim(line.substr(eq + 1));
	}
	return out;
}

static std::multiset<std::string> places(const std::string &v)
{
	std::multiset<std::string> out;
	for (size_t i = 0; i + 2 < v.size(); ++i)
		if (v[i] == '{' && v[i + 1] >= '0' && v[i + 1] <= '9' && v[i + 2] == '}') out.insert(v.substr(i, 3));
	return out;
}

int main(int argc, char **argv)
{
	if (argc < 2) return 1;
	const std::string dir = argv[1];
	const auto ja = load(dir + "/ja.txt");
	for (const char *code : {"en", "zh-Hans", "zh-Hant"}) {
		const auto o = load(dir + "/" + code + ".txt");
		for (const auto &[k, v] : ja) {
			const auto it = o.find(k);
			if (it == o.end()) {
				std::printf("FAIL %s: %s missing\n", code, k.c_str());
				++failures;
			} else if (places(v) != places(it->second)) {
				std::printf("FAIL %s: %s has other places to fill\n", code, k.c_str());
				++failures;
			} else if (it->second.empty() && !v.empty()) {
				std::printf("FAIL %s: %s is empty\n", code, k.c_str());
				++failures;
			}
		}
		for (const auto &[k, v] : o)
			if (!ja.count(k)) {
				std::printf("FAIL %s: %s is not in ja.txt\n", code, k.c_str());
				++failures;
			}
		std::printf("%s: %zu keys\n", code, o.size());
	}
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed (%zu keys)\n", ja.size());
	return 0;
}
