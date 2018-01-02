
#include "Nonce.h"

enum colour { DARKBLUE = 1, DARKGREEN, DARKTEAL, DARKRED, DARKPINK, DARKYELLOW, GRAY, DARKGRAY, BLUE, GREEN, TEAL, RED, PINK, YELLOW, WHITE };
std::array <char*, HASH_CAP * sizeof(char*)> cache;
std::array <char*, HASH_CAP * sizeof(char*)> cache_write;

HANDLE hConsole = nullptr;
HANDLE hHeap = nullptr;
HANDLE ofile = nullptr;
HANDLE ofile_stream = nullptr;
std::vector<size_t> worker_status;
unsigned long long written_scoops = 0;


void printColouredMessage(std::string message, WORD colour) {
	SetConsoleTextAttribute(hConsole, colour);
	std::printf("%s", message.c_str());
	SetConsoleTextAttribute(hConsole, colour::GRAY);
}

void printLastError(std::string message) {
	SetConsoleTextAttribute(hConsole, colour::RED);
	std::printf("%s (code = %u) \n", message.c_str(), GetLastError());
	SetConsoleTextAttribute(hConsole, colour::GRAY);
}

BOOL SetPrivilege(void)
{
	LUID luid;
	if (!LookupPrivilegeValue(
		NULL,					// lookup privilege on local system
		SE_MANAGE_VOLUME_NAME,  // privilege to lookup 
		&luid))					// receives LUID of privilege
	{
		printLastError("LookupPrivilegeValue error:");
		return FALSE;
	}

	HANDLE hToken;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		printLastError("OpenProcessToken error:");
		return FALSE;
	}

	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	
	// Enable the privilege or disable all privileges.

	if (!AdjustTokenPrivileges(	hToken,	FALSE,	&tp, sizeof(TOKEN_PRIVILEGES),	(PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL))
	{
		printLastError("AdjustTokenPrivileges error:");
		return FALSE;
	}

	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
	{
		printColouredMessage("The token does not have the specified privilege.\nFor faster writing you should restart plotter with Administrative rights.\n", RED);
		return FALSE;
	}
	return TRUE;
}

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
	LARGE_INTEGER start_time, end_time;
	DWORD dwBytesWritten;
	double PCFreq = 0.0;
	
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	PCFreq = double(li.QuadPart);
	
	written_scoops = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&start_time);
	for (size_t scoop = 0; scoop < HASH_CAP; scoop++)
	{
		liDistanceToMove.QuadPart = (scoop*glob_nonces + offset) * SCOOP_SIZE;
		if (!SetFilePointerEx(ofile, liDistanceToMove, nullptr, FILE_BEGIN))
		{
			printLastError(" error SetFilePointerEx");
			exit(-1);
		}
		if (!WriteFile(ofile, &cache_write[scoop][0], DWORD(SCOOP_SIZE * nonces_to_write), &dwBytesWritten, nullptr))
		{
			printLastError(" Failed WriteFile");
			exit(-1);
		}
		written_scoops = scoop+1;
	}
	QueryPerformanceCounter((LARGE_INTEGER*)&end_time);
	
	//printf("\n%.3f\n", double(end_time.QuadPart - start_time.QuadPart) / PCFreq);
	write_to_stream(offset+nonces_to_write);
	return;
}

bool write_to_stream(const unsigned long long data)
{
	LARGE_INTEGER liDistanceToMove;
	DWORD dwBytesWritten;
	liDistanceToMove.QuadPart = 0;
	unsigned long long buf = data;
	if (!SetFilePointerEx(ofile_stream, liDistanceToMove, nullptr, FILE_BEGIN))
	{
		printLastError(" error stream SetFilePointerEx");
		return false;
	}
	if (!WriteFile(ofile_stream, &buf, DWORD(sizeof(buf)), &dwBytesWritten, nullptr))
	{
		printLastError(" Failed stream WriteFile");
		return false;
	}
	if (SetEndOfFile(ofile_stream) == 0)
	{
		printLastError(" Failed stream SetEndOfFile");
		CloseHandle(ofile_stream);
		return false;
		exit(-1);
	}
	FlushFileBuffers(ofile_stream);
	return true;
}

unsigned long long read_from_stream()
{
	LARGE_INTEGER liDistanceToMove;
	DWORD dwBytesRead;
	liDistanceToMove.QuadPart = 0;
	if (!SetFilePointerEx(ofile_stream, liDistanceToMove, nullptr, FILE_BEGIN))
	{
		printLastError(" error stream SetFilePointerEx");
		return 0;
	}
	unsigned long long buf = 0;
	if (!ReadFile(ofile_stream, &buf, DWORD(sizeof(buf)), &dwBytesRead, nullptr))
	{
		printLastError(" Failed stream ReadFile");
		return 0;
	}
	//printf(" read_from_stream = %llu\n", buf);
	return buf;
}



bool is_number(const std::string& s)
{
	return(strspn(s.c_str(), "0123456789") == s.size());
}

int drive_info(const std::string &path)
{
	std::string drive = path.substr(0, path.find_first_of("/\\")+1);
	char NameBuffer[MAX_PATH];
	char SysNameBuffer[MAX_PATH];
	DWORD VSNumber;
	DWORD MCLength;
	DWORD FileSF;
	if (GetVolumeInformationA(drive.c_str(), NameBuffer, sizeof(NameBuffer), &VSNumber, &MCLength, &FileSF, SysNameBuffer, sizeof(SysNameBuffer)))
	{
		printf("Drive %s info:\n\tName: %s\n\tFile system: %s\n\tSerial Number: %lu\n", drive.c_str(), NameBuffer, SysNameBuffer, VSNumber);
		if (FileSF & FILE_SUPPORTS_SPARSE_FILES)
		{
			printf("\tFILE_SUPPORTS_SPARSE_FILES: yes\n"); // File system supports sparse streams
			return  1;
		}
		else
		{
			printf("\tFILE_SUPPORTS_SPARSE_FILES: no\n"); // Sparse streams are not supported
			return 0;
		}
	}
	return -1;
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
	unsigned long long memory = 0;
	std::string out_path = "";

	std::thread writer;
	std::vector<std::thread> workers;
	unsigned long long start_timer = 0;

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == NULL) {
		printLastError("Failed to retrieve handle of the process");
		exit(-1);
	}

	if (argc < 2)
	{
		printf("Usage: %s -id <ID> -sn <start_nonce> [-n <nonces>] -t <threads> [-path <d:\\plots>] [-mem <8G>]\n", argv[0]);
		printf("         <ID> = your numeric acount id\n");
		printf("         <start_nonce> = where you want to start plotting.\n");
		printf("                         If this is your first HDD then set it to 0, other wise set it to your last hdd's <start_nonce> + <nonces>\n");
		printf("         <nonces> = how many nonces you want to plot - 200gb is about 800000 nonces.\n");
		printf("                         If not specified the number of nonces, or it is 0, the file is created on all the free disk space.\n");
		printf("         <threads> = how many CPU threads you want to utilise\n");
		printf("         -path = the place where plots will be written\n");
		printf("         -mem = how much memory you will use\n");
		printf("\nExample:\n %s -id 17930413153828766298 -sn 600000000 -n 1000000 -t 6 -path H:\\plots\n", argv[0]);
		exit(-1);
	}

	SetConsoleTextAttribute(hConsole, colour::GREEN);
	printf("\nXPlotter v1.0 for BURST\n");
	SetConsoleTextAttribute(hConsole, colour::DARKGREEN);
	printf("\t\tprogrammers: Blago, Cerr Janror, DCCT\n\n");
	SetConsoleTextAttribute(hConsole, colour::GRAY);
	std::vector<std::string> args(argv, &argv[(size_t)argc]);	//copy all parameters to args
	for (auto & it : args)								//make all parameters to lower case
		for (auto & c : it) c = tolower(c);

	for (size_t i = 1; i < args.size() - 1; i++)
	{
		if ((args[i] == "-id") && is_number(args[++i]))
			addr = strtoull(args[i].c_str(), 0, 10);
		if ((args[i] == "-sn") && is_number(args[++i]))
			startnonce = strtoull(args[i].c_str(), 0, 10);
		if ((args[i] == "-n") && is_number(args[++i]))
			nonces = strtoull(args[i].c_str(), 0, 10);
		if ((args[i] == "-t") && is_number(args[++i]))
			threads = strtoull(args[i].c_str(), 0, 10);
		if (args[i] == "-path")
			out_path = args[++i];
		if (args[i] == "-mem")
		{
				i++;
				memory = strtoull(args[i].substr(0, args[i].find_last_of("0123456789") + 1).c_str(), 0, 10);
				switch (args[i][args[i].length() - 1]) 
				{
				case 't':
				case 'T':
					memory *= 1024;
				case 'g':
				case 'G':
					memory *= 1024;
				}
		}
	}

	
	if (out_path.empty() || (out_path.find(":") == std::string::npos))
	{
		char Buffer[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH, Buffer);
		std::string _path = Buffer;
		out_path = _path + "\\" + out_path;
		//printf("GetCurrentDirectory %s\n", out_path.c_str());
	}
	if (out_path.rfind("\\") < out_path.length() - 1) out_path += "\\";

	printf("Checking directory...\n");
	//printf("GetCurrentDirectory %s\n", out_path.c_str());
	if (!CreateDirectoryA(out_path.c_str(), nullptr) && ERROR_ALREADY_EXISTS != GetLastError())
	{
		printLastError("Can't create directory " + out_path + " for plots");
		exit(-1);
	}
	
	drive_info(out_path);

	DWORD sectorsPerCluster;
	DWORD bytesPerSector;
	DWORD numberOfFreeClusters;
	DWORD totalNumberOfClusters;
	if (!GetDiskFreeSpaceA(out_path.c_str(), &sectorsPerCluster, &bytesPerSector, &numberOfFreeClusters, &totalNumberOfClusters))
	{
		printLastError("GetDiskFreeSpace failed");
		exit(-1);
	}
	printf("\tBytes per Sector: %u\n", bytesPerSector);
	printf("\tSectors per Cluster: %u\n", sectorsPerCluster);



	// whole free space
	if (nonces == 0) 	nonces = getFreeSpace(out_path.c_str()) / PLOT_SIZE;

	// ajusting nonces 
	nonces = (nonces / (bytesPerSector / SCOOP_SIZE)) * (bytesPerSector / SCOOP_SIZE);

	std::string filename = std::to_string(addr) + "_" + std::to_string(startnonce) + "_" + std::to_string(nonces) + "_" + std::to_string(nonces);
	
	BOOL granted = SetPrivilege();

	ofile_stream = CreateFileA((out_path + filename + ":stream").c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (ofile_stream == INVALID_HANDLE_VALUE)
	{
		printColouredMessage("Error creating stream for file " + out_path + filename + "\n", RED);
		exit(-1);
	}
	unsigned long long nonces_done = read_from_stream();
	if (nonces_done == nonces) // exit
	{
		printColouredMessage("File is already finished. Delete the existing file to start over\n", RED);
		CloseHandle(ofile_stream);
		exit(0);
	}
	if (nonces_done > 0)
	{
		SetConsoleTextAttribute(hConsole, colour::YELLOW);
		printf("Continuing with nonce %llu\n", nonces_done);
	}

	printColouredMessage("Creating file: " + out_path + filename + "\n", colour::DARKGRAY);
	ofile = CreateFileA((out_path + filename).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_ALWAYS, FILE_FLAG_NO_BUFFERING, nullptr); //FILE_ATTRIBUTE_NORMAL     FILE_FLAG_WRITE_THROUGH |
	if (ofile == INVALID_HANDLE_VALUE)
	{
		printColouredMessage("Error creating file " + out_path + filename + "\n", RED);
		CloseHandle(ofile_stream);
		exit(-1);
	}

	
	// reserve free space for plot
	LARGE_INTEGER liDistanceToMove;
	liDistanceToMove.QuadPart = nonces * PLOT_SIZE;
	SetFilePointerEx(ofile, liDistanceToMove, nullptr, FILE_BEGIN);
	if (SetEndOfFile(ofile) == 0)
	{
		SetConsoleTextAttribute(hConsole, colour::RED);
		printLastError("Not enough free space, reduce \"nonces\"...");
		CloseHandle(ofile);
		CloseHandle(ofile_stream);
		DeleteFileA((out_path + filename).c_str());
		exit(-1);
	}
	
	if (granted)
	{
		if (SetFileValidData(ofile, nonces * PLOT_SIZE) == 0)
		{
			printLastError("SetFileValidData error");
			CloseHandle(ofile);
			CloseHandle(ofile_stream);
			exit(-1);
		}
	}

	unsigned long long freeRAM = getTotalSystemMemory();

	if (memory) nonces_per_thread = memory * 2 / threads;
	else nonces_per_thread = 1024; //(bytesPerSector / SCOOP_SIZE) * 1024 / threads;
	
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
		cache[i] = (char *)VirtualAlloc(nullptr, threads * nonces_per_thread * SCOOP_SIZE, MEM_COMMIT, PAGE_READWRITE);
		cache_write[i] = (char *)VirtualAlloc(nullptr, threads * nonces_per_thread * SCOOP_SIZE, MEM_COMMIT, PAGE_READWRITE);
		if ((cache[i] == nullptr) || (cache_write[i] == nullptr))
		{
			printColouredMessage(" Error allocating memory", RED);
			CloseHandle(ofile);
			CloseHandle(ofile_stream);
			exit(-1);
		}
	}
	printf(" OK\n");
	SetConsoleTextAttribute(hConsole, colour::GRAY);



	unsigned long long t_timer;
	unsigned long long x = 0;
	unsigned long long leftover = 0;
	unsigned long long nonces_in_work = 0;
	start_timer = GetTickCount64();

	while (nonces_done < nonces)
	{
		t_timer = GetTickCount64();

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


		for (size_t i = 0; i < threads; i++)
		{
		#ifdef __AVX__
			std::thread th(std::thread(AVX1::work_i, i, addr, startnonce + nonces_done + i*nonces_per_thread, nonces_per_thread));
		#else
			std::thread th(std::thread(SSE4::work_i, i, addr, startnonce + nonces_done + i*nonces_per_thread, nonces_per_thread));
		#endif
			workers.push_back(move(th));
			worker_status.push_back(0);
		}
		nonces_in_work = threads*nonces_per_thread;
		SetConsoleTextAttribute(hConsole, colour::WHITE);
		printf("\r[%llu%%] Generating nonces from %llu to %llu\t\t\t\t\t\t\n", (nonces_done * 100) / nonces, startnonce + nonces_done, startnonce + nonces_done + nonces_in_work);
		SetConsoleTextAttribute(hConsole, colour::YELLOW);

		do
		{
			Sleep(100);
			x = 0;
			for (auto it = worker_status.begin(); it != worker_status.end(); ++it) x += *it;
			printf("\rCPU: %llu nonces done, (%llu nonces/min)", nonces_done + x, x * 60000 / (GetTickCount64() - t_timer));
			printf("\t\tHDD: Writing scoops: %.2f%%", (double)(written_scoops * 100) / (double)HASH_CAP);
		} while (x < nonces_in_work);
		SetConsoleTextAttribute(hConsole, colour::GRAY);

		for (auto it = workers.begin(); it != workers.end(); ++it)	if (it->joinable()) it->join();
		for (auto it = worker_status.begin(); it != worker_status.end(); ++it) *it = 0;

		while ((written_scoops != 0) && (written_scoops < HASH_CAP))
		{
			Sleep(100);
			printf("\rCPU: %llu nonces done                   ", nonces_done + x);
			printf("\t\tHDD: Writing scoops: %.2f%%", (double)(written_scoops * 100) / (double)HASH_CAP);
		}
		if (writer.joinable())	writer.join();

		//swap buffers
		cache_write.swap(cache);

		writer = std::thread(writer_i, nonces_done, nonces_in_work, nonces);

		nonces_done += nonces_in_work;

	}


	while ((written_scoops != 0) && (written_scoops < HASH_CAP))
	{
		Sleep(100);
		printf("\rCPU: %llu nonces done                   ", nonces_done + x);
		printf("\t\tHDD: Writing scoops: %.2f%%", (double)(written_scoops * 100) / (double)HASH_CAP);
	}
	if (writer.joinable()) writer.join();
	printf("\rClosing file...                                                                            \n");

	FlushFileBuffers(ofile);  //https://msdn.microsoft.com/en-en/library/windows/desktop/aa364218(v=vs.85).aspx
	CloseHandle(ofile_stream);
	CloseHandle(ofile);
	printf("\rAll done. %llu seconds\n", (GetTickCount64() - start_timer) / 1000);

	// freeing memory
	SetConsoleTextAttribute(hConsole, colour::DARKGRAY);
	printf("Releasing memory... ");
	for (size_t i = 0; i < HASH_CAP; i++)
	{
		VirtualFree(cache[i], 0, MEM_RELEASE);
		VirtualFree(cache_write[i], 0, MEM_RELEASE);
	}
	printf(" OK\n");
	SetConsoleTextAttribute(hConsole, colour::GRAY);
	return 0;
}
