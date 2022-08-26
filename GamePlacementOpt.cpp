#include <algorithm>
#include <unordered_map>

#include <windows.h>
#include <tlhelp32.h>


int refreshRateMs = 5000;
std::unordered_map<DWORD, ULONG64> elapsedTime;
std::vector<std::tuple<DWORD, HANDLE, ULONG64>> threadHandles;

template <typename T>
bool UtilComparator(T& lhs, T& rhs)
{
    if ((std::get<2>(lhs) == 0) || (!elapsedTime.contains(std::get<0>(lhs))))
        return false;
    else if ((std::get<2>(rhs) == 0) || (!elapsedTime.contains(std::get<0>(rhs))))
        return true;
    else
        return ((std::get<2>(lhs) - elapsedTime[std::get<0>(lhs)]) > (std::get<2>(rhs) - elapsedTime[std::get<0>(rhs)]));
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
                    ULONG64 CPUTime;
                    QueryThreadCycleTime(curThreadHandle, &CPUTime);
                    threadHandles.emplace_back(curThreadEntry.th32ThreadID, curThreadHandle, CPUTime);
                }
            }
        } while (Thread32Next(threadSnapHandle, &curThreadEntry));

        // Stage 2: Sort and set Ideal Processor for those handles
        std::sort(threadHandles.begin(), threadHandles.end(), UtilComparator<std::tuple<DWORD, HANDLE, ULONG64>>);

        for (int pos = 0; pos < threadHandles.size(); pos++)
        {
            DWORD unrIdx = pos % processorCount;
            if (unrIdx < (processorCount / 2))
                SetThreadIdealProcessor(std::get<1>(threadHandles[pos]), unrIdx * 2 + 1);
            else
                SetThreadIdealProcessor(std::get<1>(threadHandles[pos]), (2 * processorCount - 2 - 2 * unrIdx));
        }

        // Stage 3: Update the database and cleanup
        elapsedTime.clear();
        for (const auto& elem : threadHandles)
        {
            if (std::get<2>(elem) != 0)
                elapsedTime[std::get<0>(elem)] = std::get<2>(elem);
            CloseHandle(std::get<1>(elem));
        }
        threadHandles.clear();
        CloseHandle(threadSnapHandle);
        Sleep(refreshRateMs);
    }
}