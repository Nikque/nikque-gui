// Names given to the engines: a folder and a file with spaces and Japanese letters become a name without spaces
// in the code page of the system (the short 8.3 form) that opens the same file; a file still to be made gets the
// short form of its folder. Arguments: folders to try in (for example the temporary folder and one on D:).
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <string>

#include "../src/paths.h"

using namespace ng;

static int failures = 0;
// the longest folder Windows makes without the long names of Windows 10 (MAX_PATH - 12: room for an 8.3 name)
constexpr size_t kMaxDir = MAX_PATH - 12 - 1;
#define CHECK(c)                                                              \
	do {                                                                      \
		if (!(c)) {                                                           \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);          \
			++failures;                                                       \
		}                                                                     \
	} while (0)

// opens a name as the engine does: its bytes read in the code page of the system (the engine has no manifest of UTF-8;
// this program has the one of the GUI)
static FILE *engine_fopen(const std::string &name, const wchar_t *mode)
{
	const unsigned cp = engine_code_page();
	const int n = MultiByteToWideChar(cp, 0, name.c_str(), -1, nullptr, 0);
	if (n <= 0) return nullptr;
	std::wstring w(static_cast<size_t>(n - 1), L'\0');
	MultiByteToWideChar(cp, 0, name.c_str(), -1, w.data(), n);
	FILE *f = nullptr;
	_wfopen_s(&f, w.c_str(), mode);
	return f;
}

static void try_in(const std::filesystem::path &base)
{
	namespace fs = std::filesystem;
	const fs::path dir = base / L"ng path test" / L"日本語 の dir";  // "日本語 の dir"
	if (dir.wstring().size() > kMaxDir) {
		std::printf("%ls: the folder is too long for this test (%zu letters; not tried)\n", base.c_str(), base.wstring().size());
		return;
	}
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
		FILE *f = engine_fopen(*p, L"rb");  // the way the engine opens it
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
		FILE *f = engine_fopen(*q, L"wb");
		CHECK(f);
		if (f) std::fclose(f);
		CHECK(fs::exists(dir / L"new.txt"));
	}
	CHECK(!engine_path((dir / L"new file.txt").wstring(), false));
	fs::remove_all(base / L"ng path test", ec);
}

// the conversion to a code page: UTF-8 takes every letter (the flags that tell a lost letter in the other code pages
// are not allowed with it), code page 1252 has no Japanese letters
static void test_codepage()
{
	const auto u = to_codepage(L"C:\\日本語 の dir\\a b.txt", CP_UTF8);
	CHECK(u && *u == "C:\\\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xE3\x81\xAE dir\\a b.txt");
	const auto p = to_codepage(L"plain.txt", 1252);
	CHECK(p && *p == "plain.txt");
	CHECK(!to_codepage(L"\x65E5\x672C.txt", 1252));
}

// The commands for the engines are in the code page of the system (typed as UTF-8): an empty command stays empty, a
// text that is no UTF-8 is refused; the lines of the engines are read in that code page.
static void test_engine_text()
{
	CHECK(engine_command("") && engine_command("")->empty());
	CHECK(engine_command("book save") == std::optional<std::string>("book save"));
	CHECK(engine_command("book store \xE6\x97\xA5\xE6\x9C\xAC.txt") == acp(L"book store \x65E5\x672C.txt"));
	CHECK(!engine_command("book \xFF\xFE"));
	CHECK(engine_text("") == L"" && engine_text("Edax version 4.5.5") == L"Edax version 4.5.5");
	if (const auto j = acp(L"\x65E5\x672C \x2460")) CHECK(engine_text(*j) == L"\x65E5\x672C \x2460");
}

// WideCharToMultiByte as its documentation tells for UTF-8 (the code page of the system when Windows is set to use
// UTF-8): the flags of the other code pages make it fail (some versions of Windows take them)
static int __stdcall documented_convert(unsigned cp, unsigned long flags, const wchar_t *w, int wn, char *s, int sn,
                                        const char *def, int *used)
{
	if (cp == CP_UTF8 && ((flags & ~static_cast<unsigned long>(WC_ERR_INVALID_CHARS)) || def || used)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	return WideCharToMultiByte(cp, flags, w, wn, s, sn, def, used);
}

// a name in UTF-8: every letter is taken, a broken one (half of a pair) is not
static void test_codepage_utf8()
{
	const auto u = to_codepage(L"C:\\\x65E5\x672C \x2460\\\xD83D\xDE00.txt", CP_UTF8, documented_convert);
	CHECK(u && *u == "C:\\\xE6\x97\xA5\xE6\x9C\xAC \xE2\x91\xA0\\\xF0\x9F\x98\x80.txt");
	CHECK(!to_codepage(L"C:\\\xD83D.txt", CP_UTF8, documented_convert));
	CHECK(!to_codepage(L"C:\\\xD83D.txt", CP_UTF8));
}

// A name for the command line of an engine: spaces are allowed (it is quoted); a name longer than the engine keeps
// (252 bytes in the code page of the system) becomes its short form when the drive keeps short names, else none.
static void test_arg_path(const std::filesystem::path &base)
{
	namespace fs = std::filesystem;
	// a name short enough stays as it is, spaces too
	const std::wstring plain = (base / L"a b" / L"book.dat").wstring();
	const auto same = engine_arg_path(plain, EngineName::Book);
	CHECK(same && *same == plain);
	// The long folder: 120 Japanese letters, fewer when the folder of the test is long, so that the folder stays within
	// kMaxDir letters and the name of a file in it within MAX_PATH - 1. The name of the file must still be longer than
	// 259 bytes (each letter is 2 bytes or more in a code page that has it); a folder too long for that is said and not
	// tried.
	const size_t outer = (base / L"ng arg test" / L"a b").wstring().size() + 1;  // (+1: the separator of the letters)
	const size_t letters = outer < kMaxDir ? std::min<size_t>(120, kMaxDir - outer) : 0;
	if (letters == 0 || outer + std::wcslen(L"\\eval.dat") + 2 * letters <= 259) {
		std::printf("%ls: the folder is too long for this test (%zu letters; not tried)\n", base.c_str(), base.wstring().size());
		return;
	}
	const fs::path dir = base / L"ng arg test" / std::wstring(letters, L'\x3042') / L"a b";
	std::error_code ec;
	fs::create_directories(dir, ec);
	CHECK(!ec);
	const fs::path file = dir / L"book.dat";
	{
		FILE *f = nullptr;
		_wfopen_s(&f, file.c_str(), L"wb");
		CHECK(f);
		if (f) {
			std::fputs("x", f);
			std::fclose(f);
		}
	}
	const std::wstring s = file.wstring();
	const auto a = acp(s);
	if (!a) {
		std::printf("%ls: the letters are not in the code page of the system\n", base.c_str());
	} else {
		CHECK(a->size() > 252);
		const auto p = engine_arg_path(s, EngineName::Book);
		if (!p) {
			std::printf("%ls: no short names on this drive (the GUI says so and does not start the engine)\n", base.c_str());
		} else {
			const auto b = acp(*p);
			CHECK(b && b->size() <= 252);
			FILE *f = nullptr;
			_wfopen_s(&f, p->c_str(), L"rb");
			CHECK(f);
			if (f) {
				CHECK(std::fgetc(f) == 'x');
				std::fclose(f);
			}
			std::printf("%ls: %ls\n", base.c_str(), p->c_str());
		}
		// a file still to be made (the empty book of an engine without the book): the short form of its folder
		const auto q = engine_arg_path((dir / L"nobook.dat").wstring(), EngineName::Book);
		if (q) {
			const auto b = acp(*q);
			CHECK(b && b->size() <= 252);
		}
	}
	// The other files (the eval file) are opened with the functions of the code page, which take MAX_PATH - 1 letters
	// whatever their bytes: such a name stays as it is.
	{
		const fs::path eval = dir / L"eval.dat";
		std::filesystem::copy_file(file, eval, fs::copy_options::overwrite_existing, ec);
		const std::wstring e = eval.wstring();
		const auto b = acp(e);
		if (b) {
			CHECK(b->size() > 259 && e.size() <= 259);
			FILE *f = engine_fopen(*b, L"rb");  // the way the engine opens it
			CHECK(f);
			if (f) std::fclose(f);
			const auto p = engine_arg_path(e, EngineName::File);
			CHECK(p && *p == e);
		}
	}
	// A book named by its short form makes the engine save its files on the way (".store", ".dev2", ...) with that
	// short name: the folder alone is shortened when that is enough, so the book keeps its own name.
	{
		const fs::path book = dir / L"long book name.dat";
		std::filesystem::copy_file(file, book, fs::copy_options::overwrite_existing, ec);
		if (const auto p = engine_arg_path(book.wstring(), EngineName::Book)) CHECK(p->ends_with(L"\\long book name.dat"));
	}
	// The engine saves a book by writing another file and renaming it to the name it was given: a book given by the
	// short form of its own name would come back under that short name, and the name the GUI keeps would be gone. A
	// book whose own name cannot be given (a letter not in the code page) is not given at all.
	{
		const fs::path book = dir / L"book\xD55C.dat";  // a Hangul letter
		std::filesystem::copy_file(file, book, fs::copy_options::overwrite_existing, ec);
		CHECK(!engine_arg_path(book.wstring(), EngineName::Book));
	}
	fs::remove_all(base / L"ng arg test", ec);
}

int main(int argc, char **argv)
{
	// run as the GUI: its manifest makes CP_ACP UTF-8, while the engines read names in the code page of the system
	CHECK(GetACP() == CP_UTF8);
	std::printf("code page of the engines: %u\n", engine_code_page());
	test_codepage();
	test_codepage_utf8();
	test_engine_text();
	// in a folder of this run in each folder (runs side by side, and the files an earlier run left, do not meet)
	const auto in = [](const std::filesystem::path &base) {
		const std::filesystem::path run = base / (L"ng-path-test-" + std::to_wstring(GetCurrentProcessId()));
		std::error_code ec;
		std::filesystem::remove_all(run, ec);
		std::filesystem::create_directories(run, ec);
		CHECK(!ec);
		try_in(run);
		test_arg_path(run);
		std::filesystem::remove_all(run, ec);
	};
	in(std::filesystem::temp_directory_path());
	for (int i = 1; i < argc; ++i) in(std::filesystem::path(argv[i]));
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
