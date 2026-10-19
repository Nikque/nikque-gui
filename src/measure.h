// Timings of the start and of the frames, for the measuring build only (cmake -DNG_MEASURE=ON).
// In the default build every function here is empty.
#pragma once

#include <string>

#ifdef NG_MEASURE
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <vector>
#endif

namespace ng::measure {

#ifdef NG_MEASURE

struct State {
	LARGE_INTEGER freq{}, start{};
	double first_frame_ms = -1;
	std::vector<double> full, anim;
	double first_answer_ms[2] = {-1, -1};
};
inline State &state()
{
	static State s;
	return s;
}
inline double ms_since(const LARGE_INTEGER &t)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (now.QuadPart - t.QuadPart) * 1000.0 / state().freq.QuadPart;
}
inline void start()
{
	QueryPerformanceFrequency(&state().freq);
	QueryPerformanceCounter(&state().start);
}
inline void first_frame()
{
	if (state().first_frame_ms < 0) state().first_frame_ms = ms_since(state().start);
}
inline void engine_answer(int role)
{
	if (state().first_answer_ms[role] < 0) state().first_answer_ms[role] = ms_since(state().start);
}
class Frame {
public:
	explicit Frame(bool full) : full_(full) { QueryPerformanceCounter(&t_); }
	~Frame() { (full_ ? state().full : state().anim).push_back(ms_since(t_)); }

private:
	bool full_;
	LARGE_INTEGER t_;
};
inline void summary(FILE *f, const char *name, std::vector<double> v)
{
	if (v.empty()) {
		std::fprintf(f, "%s: none\n", name);
		return;
	}
	std::sort(v.begin(), v.end());
	double sum = 0;
	for (double x : v) sum += x;
	std::fprintf(f, "%s: n=%zu mean=%.3f ms median=%.3f ms p95=%.3f ms max=%.3f ms\n", name, v.size(), sum / v.size(),
	             v[v.size() / 2], v[v.size() * 95 / 100], v.back());
}
inline void finish(const std::wstring &dir)
{
	FILE *f = _wfopen((dir + L"\\measure.log").c_str(), L"a");
	if (!f) return;
	std::fprintf(f, "first frame: %.1f ms\n", state().first_frame_ms);
	std::fprintf(f, "first answer: main %.1f ms, search %.1f ms\n", state().first_answer_ms[0], state().first_answer_ms[1]);
	summary(f, "full frames", state().full);
	summary(f, "effect frames", state().anim);
	std::fprintf(f, "----\n");
	std::fclose(f);
}

#else

inline void start() {}
inline void first_frame() {}
inline void engine_answer(int) {}
class Frame {
public:
	explicit Frame(bool) {}
};
inline void finish(const std::wstring &) {}

#endif

}  // namespace ng::measure
