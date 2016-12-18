
#include "Nonce.h"

enum colour { DARKBLUE = 1, DARKGREEN, DARKTEAL, DARKRED, DARKPINK, DARKYELLOW, GRAY, DARKGRAY, BLUE, GREEN, TEAL, RED, PINK, YELLOW, WHITE };
//char **cache_write = nullptr;
//char **cache = nullptr;
//char **cache_tmp = nullptr;
std::array <char*, HASH_CAP * sizeof(char*)> cache;
std::array <char*, HASH_CAP * sizeof(char*)> cache_write;

HANDLE hConsole = nullptr;
HANDLE hHeap = nullptr;
HANDLE ofile = nullptr;

std::vector<size_t> worker_status;
unsigned long long written_scoops = 0;
//double written_speed = 0;

unsigned long long getFreeSpace(const char* path)
{
	ULARGE_INTEGER lpFreeBytesAvailable;
	ULARGE_INTEGER lpTotalNumberOfBytes;
	ULARGE_INTEGER lpTotalNumberOfFreeBytes;

	GetDiskFreeSpaceExA(path, &lpFreeBytesAvailable, &lpTotalNumberOfBytes, &lpTotalNumberOfFreeBytes);
	
	return lpFreeBytesAvailable.QuadPart;
}

unsigned long long getTotalSystemMemory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullAvailPhys;
}

void writer_i(const unsigned long long offset, const unsigned long long nonces_to_write, const unsigned long long glob_nonces)
{
	LARGE_INTEGER liDistanceToMove;
	__int64 start_time, end_time;
	DWORD dwBytesWritten;
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	double const pcFreq = double(li.QuadPart);

	//written_speed = 0;
	for (size_t scoop = 0; scoop < HASH_CAP; scoop++)
	{
		QueryPerformanceCounter((LARGE_INTEGER*)&start_time);
		liDistanceToMove.QuadPart = (scoop*glob_nonces + offset) * SCOOP_SIZE;
		if (!SetFilePointerEx(ofile, liDistanceToMove, nullptr, FILE_BEGIN))
		{
			printf("error SetFilePointerEx. code = %d\n", GetLastError());
			exit(-1);
		}
		if (!WriteFile(ofile, &cache_write[scoop][0], DWORD(SCOOP_SIZE * nonces_to_write), &dwBytesWritten, nullptr))
			{
				printf("Failed WriteFile (%d).\n", GetLastError());
				exit(-1);
			}
		QueryPerformanceCounter((LARGE_INTEGER*)&end_time);
		written_scoops = scoop+1;
		//written_speed = (written_speed + (double)(SCOOP_SIZE * nonces_to_write) / ((double)(end_time - start_time) / pcFreq)) / (double)written_scoops;
	}
	written_scoops = 0;
	return;
}

bool is_number(const std::string& s)
{
	return(strspn(s.c_str(), "0123456789") == s.size());
}

/*
unsigned long long GCD(unsigned long long a, unsigned long long b)  //наибольший общий делитель
{    
	return b ? GCD(b, a%b) : a;
}

unsigned long long LCM(unsigned long long a, unsigned long long b) //наименьшее общее кратное
{
	return a / GCD(a, b) * b;
}
*/

int main(int argc, char* argv[])
{

	unsigned long long addr = 0;
	unsigned long long startnonce = 0;
	unsigned long long nonces = 0;
	unsigned long long threads = 1;
	unsigned long long nonces_per_thread = 0;
	std::string out_path = "";

	std::thread writer;
	std::vector<std::thread> workers;

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == NULL) {
		printf("Failed to retrieve handle of the process (%d).\n", GetLastError());
		exit(-1);
	}
	
	SetConsoleTextAttribute(hConsole, colour::GREEN);
	printf("\nXPlotter v0.6 for BURST\n");
	SetConsoleTextAttribute(hConsole, colour::DARKGREEN); 
	printf("\t\tprogrammers: Blago, Cerr Janror, DCCT\n\n");
	SetConsoleTextAttribute(hConsole, colour::GRAY);
	std::vector<std::string> args(argv, &argv[argc]);	//copy all parameters to args
	for (auto & it : args)								//make all parameters to upper case
		for (auto & c : it) c = tolower(c);

	for (size_t i = 1; i < args.size()-1; i++)
	{
		if (args[i] == "-id")
		{
			if (is_number(args[i + 1]))	addr = strtoull(args[i+1].c_str(), 0, 10);
			i++;
		}
		if (args[i] == "-sn")
		{
			if (is_number(args[i + 1]))	startnonce = strtoull(args[i + 1].c_str(), 0, 10);
			i++;
		}
		if (args[i] == "-n")
		{
			if (is_number(args[i + 1]))	nonces = strtoull(args[i + 1].c_str(), 0, 10);
			i++;
		}
		if (args[i] == "-t")
		{
			if (is_number(args[i + 1]))	threads = strtoull(args[i + 1].c_str(), 0, 10);
			i++;
		}
		if (args[i] == "-path")
		{
			out_path = args[i + 1];
			i++;
		}
	}

	if (out_path.empty())
	{
		char Buffer[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH-1, Buffer);
		out_path = Buffer;
		//printf("GetCurrentDirectory %s\n", out_path.c_str());
	}
	if (out_path.rfind("\\") < out_path.length() - 1) out_path += "\\";

	if (out_path.length() > 3)
	{
		printf("Checking directory...\n");
		if (!CreateDirectoryA(out_path.c_str(), nullptr) &&	ERROR_ALREADY_EXISTS != GetLastError())
		{
			printf("out_path.c_str() %s\n", out_path.c_str());
			printf("Can't create directory for plots (Error %d)\n", GetLastError());
			exit(-1);
		}
	}
	
	DWORD sectorsPerCluster;
	DWORD bytesPerSector;
	DWORD numberOfFreeClusters;
	DWORD totalNumberOfClusters;
	if (!GetDiskFreeSpaceA(out_path.c_str(), &sectorsPerCluster, &bytesPerSector, &numberOfFreeClusters, &totalNumberOfClusters))
	{
		SetConsoleTextAttribute(hConsole, colour::RED);
		printf("GetDiskFreeSpace failed\n");
		SetConsoleTextAttribute(hConsole, colour::GRAY);
		exit(-1);
	}
	
	// whole free space
	if (nonces == 0) 	nonces = getFreeSpace(out_path.c_str()) / PLOT_SIZE;
	
	// ajusting nonces 
	nonces = (nonces / (bytesPerSector / SCOOP_SIZE)) * (bytesPerSector / SCOOP_SIZE);

	char filename[MAX_PATH];
	sprintf_s(filename, "%llu_%llu_%llu_%llu", addr, startnonce, nonces, nonces);
	
	SetConsoleTextAttribute(hConsole, colour::DARKGRAY);
	out_path = out_path + filename;
	printf("Creating file: %s\n", out_path.c_str());
	ofile = CreateFileA(out_path.c_str(), GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, nullptr); //FILE_ATTRIBUTE_NORMAL   FILE_FLAG_WRITE_THROUGH | 
	if (ofile == INVALID_HANDLE_VALUE)
	{
		SetConsoleTextAttribute(hConsole, colour::RED);
		printf("Error creating file %s\n", filename);
		SetConsoleTextAttribute(hConsole, colour::GRAY);
		exit(-1);
	}

	// empty file, reserved free space for plot
	LARGE_INTEGER liDistanceToMove;
	liDistanceToMove.QuadPart = nonces * PLOT_SIZE;
	SetFilePointerEx(ofile, liDistanceToMove, nullptr, FILE_BEGIN);
	if (SetEndOfFile(ofile) == 0)
	{
		SetConsoleTextAttribute(hConsole, colour::RED);
		printf("Not enough free space for %llu nonces, reduce \"nonces\"\n", nonces);
		CloseHandle(ofile);
		DeleteFileA(out_path.c_str());
		SetConsoleTextAttribute(hConsole, colour::GRAY);
		exit(-1);
	}
	liDistanceToMove.QuadPart = 0;
	SetFilePointerEx(ofile, liDistanceToMove, nullptr, FILE_BEGIN);
	
	
	unsigned long long freeRAM = getTotalSystemMemory();
	
	nonces_per_thread = (bytesPerSector / SCOOP_SIZE) * 1024 / threads;
	if (nonces < nonces_per_thread * threads) 	nonces_per_thread = nonces / threads;

	// check free RAM
	if (freeRAM < nonces_per_thread * threads * PLOT_SIZE * 2) nonces_per_thread = freeRAM / threads / PLOT_SIZE / 2;

	//ajusting
	nonces_per_thread = (nonces_per_thread / (bytesPerSector / SCOOP_SIZE)) * (bytesPerSector / SCOOP_SIZE);
	
	SetConsoleTextAttribute(hConsole, colour::BLUE);
	printf("ID:  %llu\n", addr);
	printf("Start_nonce:  %llu\n", startnonce);
	printf("Nonces: %llu\n", nonces);
	printf("Nonces per thread:  %llu\n", nonces_per_thread);
	
	SetConsoleTextAttribute(hConsole, colour::TEAL);
	printf("Uses %llu Mb of %llu Mb free RAM\n", nonces_per_thread * threads * 2 * PLOT_SIZE / 1024 / 1024, freeRAM / 1024 / 1024);
	
	SetConsoleTextAttribute(hConsole, colour::DARKGRAY);
	printf("Allocating memory for nonces... ");

	cache.fill(nullptr);
	cache_write.fill(nullptr);
	for (size_t i = 0; i < HASH_CAP; i++)
	{
		//cache[i] = new char[threads * nonces_per_thread * SCOOP_SIZE];
		//cache_write[i] = new char[threads * nonces_per_thread * SCOOP_SIZE];
		cache[i] = (char *)VirtualAlloc(nullptr, threads * nonces_per_thread * SCOOP_SIZE, MEM_COMMIT, PAGE_READWRITE);
		cache_write[i] = (char *)VirtualAlloc(nullptr, threads * nonces_per_thread * SCOOP_SIZE, MEM_COMMIT, PAGE_READWRITE);
		if ((cache[i] == nullptr) || (cache_write[i] == nullptr))
		{
			SetConsoleTextAttribute(hConsole, colour::RED);
			printf(" Error allocating memory\n");
			SetConsoleTextAttribute(hConsole, colour::GRAY);
			exit(-1);
		}
	}
	printf(" OK\n");
	SetConsoleTextAttribute(hConsole, colour::GRAY);


	unsigned long long start_timer = GetTickCount64();
	unsigned long long t_timer;
	unsigned long long x = 0;
	unsigned long long nonces_done = 0;
	unsigned long long leftover = 0;
	unsigned long long nonces_in_work = 0;
	do 
	{
		t_timer = GetTickCount64();
		for (size_t i = 0; i < threads; i++)
		{
			#ifdef __AVX__
			std::thread th(std::thread(AVX1::work_i, i, addr, nonces_done + i*nonces_per_thread, nonces_per_thread));
			#else
			std::thread th(std::thread(SSE4::work_i, i, addr, nonces_done + i*nonces_per_thread, nonces_per_thread));
			#endif
			workers.push_back(move(th));
			worker_status.push_back(0);
		}
		nonces_in_work = threads*nonces_per_thread;
		SetConsoleTextAttribute(hConsole, colour::WHITE);
		printf("\rGenerating nonces from %llu to %llu\t\t\t\t\t\n", startnonce + nonces_done, startnonce + nonces_done + nonces_in_work);
		SetConsoleTextAttribute(hConsole, colour::YELLOW);

		do
		{
			Sleep(100);
			x = 0;
			for (auto it = worker_status.begin(); it != worker_status.end(); ++it) x += *it;
			printf("\rCPU: %llu nonces done, (%llu nonces/min)", nonces_done + x, x * 60000 / (GetTickCount64() - t_timer));
			if (written_scoops != 0) printf("\tHDD: Writing scoops: %llu%%", written_scoops * 100 / HASH_CAP);
		} while (x < nonces_in_work);
		SetConsoleTextAttribute(hConsole, colour::GRAY);
		
		for (auto it = workers.begin(); it != workers.end(); ++it)	if (it->joinable()) it->join();
		for (auto it = worker_status.begin(); it != worker_status.end(); ++it) *it = 0;

		while ((written_scoops != 0) && (written_scoops < HASH_CAP))
		{
			Sleep(100);
			printf("\rCPU: %llu nonces done                   ", nonces_done + x);
			printf("\tHDD: Writing scoops: %llu%%", written_scoops * 100 / HASH_CAP);
		}
		if (writer.joinable())	writer.join();

		//swap buffers
		cache_write.swap(cache);

		writer = std::thread(writer_i, nonces_done, nonces_in_work, nonces);
		
		nonces_done += nonces_in_work;
		
		leftover = nonces - nonces_done;
		if (leftover / (nonces_per_thread*threads) == 0)
		{
			if (leftover >= threads*(bytesPerSector / SCOOP_SIZE))
			{
				nonces_per_thread = leftover / threads;
				//ajusting
				nonces_per_thread = (nonces_per_thread / (bytesPerSector / SCOOP_SIZE)) * (bytesPerSector / SCOOP_SIZE);
			}
			else
			{
				threads = 1;
				nonces_per_thread = leftover;
			}
		}
		
	} while (nonces_done < nonces);

	printf("\rClosing file...\t\t\t\t\t\t\t\t \n");
	
	if (writer.joinable()) writer.join();
	//FlushFileBuffers(ofile);
	printf("\rAll done. %llu seconds\n", (GetTickCount64() - start_timer) / 1000);
	CloseHandle(ofile);

	// freeing memory
	SetConsoleTextAttribute(hConsole, colour::DARKGRAY);
	printf("Releasing memory... ");
	for (size_t i = 0; i < HASH_CAP; i++)
	{
		//delete[] cache_write[i];
		VirtualFree(cache[i], 0, MEM_RELEASE);
		VirtualFree(cache_write[i], 0, MEM_RELEASE);
		//delete[] cache[i];
	}
	printf(" OK\n");
	SetConsoleTextAttribute(hConsole, colour::GRAY);
	return 0;
}

