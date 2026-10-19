// Names given to the engines: a folder and a file with spaces and Japanese letters become a name without spaces
// in the code page of the system (the short 8.3 form) that opens the same file; a file still to be made gets the
// short form of its folder. Arguments: folders to try in (for example the temporary folder and one on D:).
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <string>

#include "../src/paths.h"

using namespace ng;

static int failures = 0;
#define CHECK(c)                                                              \
	do {                                                                      \
		if (!(c)) {                                                           \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);          \
			++failures;                                                       \
		}                                                                     \
	} while (0)

static void try_in(const std::filesystem::path &base)
{
	namespace fs = std::filesystem;
	const fs::path dir = base / L"ng path test" / L"日本語 の dir";  // "日本語 の dir"
	std::error_code ec;
	fs::create_directories(dir, ec);
	CHECK(!ec);
	const fs::path file = dir / L"a b.obf";
	{
		FILE *f = nullptr;
		_wfopen_s(&f, file.c_str(), L"wb");
		CHECK(f);
		if (f) {
			std::fputs("x", f);
			std::fclose(f);
		}
	}
	// a plain name stays as it is
	const auto plain = engine_path(base.wstring() + L"\\plain.txt", false);
	CHECK(plain && plain->find(' ') == std::string::npos);

	const auto p = engine_path(file.wstring(), true);
	if (!p) {
		std::printf("%ls: no short names on this drive (the GUI refuses such names)\n", base.c_str());
	} else {
		CHECK(p->find(' ') == std::string::npos);
		FILE *f = std::fopen(p->c_str(), "rb");  // the way the engine opens it
		CHECK(f);
		if (f) {
			CHECK(std::fgetc(f) == 'x');
			std::fclose(f);
		}
		std::printf("%ls: %s\n", base.c_str(), p->c_str());
	}
	// a file to be made: the short folder and the name; a name with a space cannot be given
	const auto q = engine_path((dir / L"new.txt").wstring(), false);
	if (q) {
		CHECK(q->find(' ') == std::string::npos);
		FILE *f = std::fopen(q->c_str(), "wb");
		CHECK(f);
		if (f) std::fclose(f);
		CHECK(fs::exists(dir / L"new.txt"));
	}
	CHECK(!engine_path((dir / L"new file.txt").wstring(), false));
	fs::remove_all(base / L"ng path test", ec);
}

int main(int argc, char **argv)
{
	try_in(std::filesystem::temp_directory_path());
	for (int i = 1; i < argc; ++i) try_in(std::filesystem::path(argv[i]));
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
