#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>

size_t get_max_rss_bytes() {
	PROCESS_MEMORY_COUNTERS pmc{};
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
		return pmc.PeakWorkingSetSize;  // max resident set
	}
	return 0;
}

double get_cpu_percent(double wall_seconds) {
	FILETIME creation_time, exit_time, kernel_time, user_time;

	// Get cumulative CPU time (User + Kernel) for all threads in this process
	if (!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time)) {
		return 0.0;
	}

	// Convert FILETIME (100-nanosecond units) to a 64-bit integer
	auto ft_to_uint64 = [](FILETIME ft) {
		ULARGE_INTEGER li;
		li.LowPart = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		return li.QuadPart;
	};

	unsigned __int64 total_counts = ft_to_uint64(kernel_time) + ft_to_uint64(user_time);

	// Convert counts to seconds: 1 second = 10,000,000 (10^7) units of 100ns
	double cpu_seconds = static_cast<double>(total_counts) / 10000000.0;

	// This matches the Linux /usr/bin/time formula: (CPU_TIME / WALL_TIME) * 100
	return (cpu_seconds / wall_seconds) * 100.0;
}


#elif defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>

size_t get_max_rss_bytes() {
	struct rusage r {};
	getrusage(RUSAGE_SELF, &r);
#if defined(__APPLE__)
	return r.ru_maxrss;   // bytes
#else
	return r.ru_maxrss * 1024; // Linux: ru_maxrss in KB
#endif
}

double get_cpu_percent(double wall_seconds) {
	struct rusage r {};
	getrusage(RUSAGE_SELF, &r);

	// user + system time in seconds
	double cpu_seconds = r.ru_utime.tv_sec + r.ru_utime.tv_usec / 1e6
		+ r.ru_stime.tv_sec + r.ru_stime.tv_usec / 1e6;

	return (cpu_seconds / wall_seconds) * 100.0;
}

#else
size_t get_max_rss_bytes() {
	return 0; // fallback
}
double get_cpu_percent(double wall_seconds) {
	return 0; // fallback
}
#endif

struct TimePrinter {
	std::chrono::high_resolution_clock::time_point start_time;
	TimePrinter()
	{
		start_time = std::chrono::high_resolution_clock::now();
	}
	~TimePrinter() {
		auto wall = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time).count();
		std::cerr << "Program execution time: " << wall << " s\n";
		std::cerr << "RSS: " << get_max_rss_bytes() / 1024.0 / 1024 << " MiB\n";
		std::cerr << "CPU: " << get_cpu_percent(wall) << " %\n";
	}
};

int main()
{
	TimePrinter time_printer;
	uint32_t num_threads = 8;
	uint32_t each_allocates = 1ull << 30;

	using std::vector;
	using std::jthread;

	vector<jthread> threads;
	for (uint32_t i = 0; i < num_threads; ++i)
		threads.emplace_back([&] {
		auto start = std::chrono::high_resolution_clock::now();
		vector<uint8_t> data(each_allocates);
		while (true)
		{
			auto x = std::chrono::high_resolution_clock::now();
			auto end = std::chrono::high_resolution_clock::now();
			auto dur = std::chrono::duration<double>(end - start).count();
			if (dur >= 1.0)
				break;
		}
	});
}