#include <algorithm>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <tlhelp32.h>


int refreshRateMs = 5000;
std::unordered_map<DWORD, ULONG64> elapsedTime;
std::vector<HANDLE> threadHandles;

bool UtilComparator(HANDLE& lhs, HANDLE& rhs)
{
	ULONG64 CPUTime_l, CPUTime_r;
	DWORD TID_l, TID_r;
	QueryThreadCycleTime(lhs, &CPUTime_l);
	QueryThreadCycleTime(rhs, &CPUTime_r);
	TID_l = GetThreadId(lhs);
	TID_r = GetThreadId(rhs);

	if ((TID_l == 0) || (!elapsedTime.contains(TID_l)))
		return false;
	else if ((TID_r == 0) || (!elapsedTime.contains(TID_r)))
		return true;
	else
		return ((CPUTime_l - elapsedTime[TID_l]) > (CPUTime_r - elapsedTime[TID_r]));
}

int main()
{
	DWORD processorCount = GetMaximumProcessorCount(ALL_PROCESSOR_GROUPS);
	DWORD processId;
	HANDLE threadSnapHandle;
	THREADENTRY32 curThreadEntry;
	curThreadEntry.dwSize = sizeof(THREADENTRY32);

	printf_s("Logical Core Count: %d\nPID for game to optimize: ", processorCount);
	scanf_s("%ul", &processId);

	while (true)
	{
		// Stage 1: Take a snapshot 
		threadSnapHandle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if ((threadSnapHandle == INVALID_HANDLE_VALUE) || !(Thread32First(threadSnapHandle, &curThreadEntry)))
		{
			printf_s("[E] Error when snapshoting threads.\n");
			exit(1);
		}
		do
		{
			if (curThreadEntry.th32OwnerProcessID == processId)
			{
				HANDLE curThreadHandle = OpenThread(THREAD_ALL_ACCESS, true, curThreadEntry.th32ThreadID);
				if (curThreadHandle != NULL)
				{
					threadHandles.push_back(curThreadHandle);
				}
					
			}
		} while (Thread32Next(threadSnapHandle, &curThreadEntry));

		// Stage 2: Sort and set Ideal Processor for those handles
		std::sort(threadHandles.begin(), threadHandles.end(), UtilComparator);

		for (int pos = 0; pos < threadHandles.size(); pos++)
		{
			DWORD unrIdx = pos % processorCount;
			if (unrIdx < (processorCount / 2))
				SetThreadIdealProcessor(threadHandles[pos], unrIdx * 2 + 1);
			else
				SetThreadIdealProcessor(threadHandles[pos], (2 * processorCount - 2 - 2 * unrIdx));
		}

		// Stage 3: Update the database
		elapsedTime.clear();
		for (const HANDLE& elem : threadHandles)
		{
			DWORD tid = GetThreadId(elem);
			ULONG64 cycleTime;
			QueryThreadCycleTime(elem, &cycleTime);

			if (tid != 0 && cycleTime != 0)
				elapsedTime[tid] = cycleTime;
		}

		// Stage 4: Cleanup
		for (const HANDLE& elem : threadHandles)
		{
			CloseHandle(elem);
		}
		threadHandles.clear();
		CloseHandle(threadSnapHandle);
		Sleep(refreshRateMs);
	}
}