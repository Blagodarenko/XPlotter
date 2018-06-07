#pragma once

#include <windows.h>
#include <vector>
#include <array>
#include <thread>
#include <string>

#define HASH_SIZE	32
#define HASH_CAP	4096
#define SCOOP_SIZE	64
#define PLOT_SIZE	(HASH_CAP * SCOOP_SIZE) // 4096*64

extern std::vector<size_t> worker_status;
extern std::array <char*, HASH_CAP * sizeof(char*)> cache;
bool write_to_stream(const unsigned long long data);


namespace AVX1
{
	void work_i(const size_t local_num, const unsigned long long loc_addr, const unsigned long long local_startnonce, const unsigned long long local_nonces);
}
namespace AVX2
{
	void work_i(const size_t local_num, const unsigned long long loc_addr, const unsigned long long local_startnonce, const unsigned long long local_nonces);
}
namespace SSE4
{
	void work_i(const size_t local_num, const unsigned long long loc_addr, const unsigned long long local_startnonce, const unsigned long long local_nonces);
}