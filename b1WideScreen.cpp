#include <memory>
#include <iostream>
#include <string>

#ifndef UNICODE
#define UNICODE
#endif // UNICODE

#include <windows.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

using namespace std;

typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> unique_handle;
typedef std::unique_ptr<std::remove_pointer<HKEY>::type, decltype(&::RegCloseKey)> unique_hkey;
typedef std::unique_ptr<std::remove_pointer<HMODULE>::type, decltype(&::FreeLibrary)> unique_lib;

std::wstring GetProcessPath(DWORD processId)
{
    auto hProcess = unique_handle(::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId), ::CloseHandle);
    if (hProcess.get() == NULL)
        return L"";

    WCHAR path[MAX_PATH];
    DWORD size = MAX_PATH;
    if (::QueryFullProcessImageName(hProcess.get(), 0, path, &size))
    {
        return std::wstring(path);
    }
    return L"";
}

DWORD GetProcessIdByName(const std::wstring &processName)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    auto hSnapshot = unique_handle(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0), ::CloseHandle);
    if (::Process32First(hSnapshot.get(), &entry))
    {
        do
        {
            if (entry.th32ParentProcessID != ::GetCurrentProcessId() &&
                processName == entry.szExeFile &&
                entry.cntThreads != 0)
            {
                return entry.th32ProcessID;
            }
        } while (Process32Next(hSnapshot.get(), &entry));
    }
    return 0;
}

#define SEARCH_CHUNK_SIZE 4096

BOOL SearchAndModifyRemoteData(HANDLE hProcess, DWORD_PTR moduleBase, const char *sectionName,
                               const BYTE *searchPattern, DWORD patternSize,
                               const BYTE *newData, DWORD newDataSize)
{
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS ntHeaders;

    // 读取 DOS 头
    if (!ReadProcessMemory(hProcess, (LPCVOID)moduleBase, &dosHeader, sizeof(dosHeader), NULL))
    {
        printf("Failed to read DOS header. Error %u \n", GetLastError());
        return FALSE;
    }

    // 读取 NT 头
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + dosHeader.e_lfanew), &ntHeaders, sizeof(ntHeaders), NULL))
    {
        printf("Failed to read NT headers.\n");
        return FALSE;
    }

    // 读取节表
    DWORD sectionTableOffset = dosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS);
    IMAGE_SECTION_HEADER sectionHeader;
    BOOL sectionFound = FALSE;

    for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; i++)
    {
        if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + sectionTableOffset + i * sizeof(IMAGE_SECTION_HEADER)),
                               &sectionHeader, sizeof(sectionHeader), NULL))
        {
            printf("Failed to read section header.\n");
            return FALSE;
        }

        if (strncmp((char *)sectionHeader.Name, sectionName, 8) == 0)
        {
            sectionFound = TRUE;
            break;
        }
    }

    if (!sectionFound)
    {
        printf("Section %s not found.\n", sectionName);
        return FALSE;
    }

    // 搜索段中的数据
    DWORD_PTR sectionStart = moduleBase + sectionHeader.VirtualAddress;
    DWORD_PTR sectionEnd = sectionStart + sectionHeader.Misc.VirtualSize;
    BYTE *buffer = (BYTE *)malloc(SEARCH_CHUNK_SIZE);
    if (!buffer)
    {
        printf("Failed to allocate memory.\n");
        return FALSE;
    }

    for (DWORD_PTR currentAddress = sectionStart; currentAddress < sectionEnd; currentAddress += SEARCH_CHUNK_SIZE - patternSize + 1)
    {
        SIZE_T bytesRead;
        if (!ReadProcessMemory(hProcess, (LPCVOID)currentAddress, buffer, SEARCH_CHUNK_SIZE, &bytesRead))
        {
            printf("Failed to read memory at 0x%p\n", (void *)currentAddress);
            free(buffer);
            return FALSE;
        }

        for (SIZE_T i = 0; i < bytesRead - patternSize + 1; i++)
        {
            if (memcmp(buffer + i, searchPattern, patternSize) == 0)
            {
                // 找到匹配的模式
                DWORD_PTR matchAddress = currentAddress + i;
                printf("Pattern found at address: 0x%p\n", (void *)matchAddress);
                // 修改内存保护
                DWORD oldProtect;
                if (!VirtualProtectEx(hProcess, (LPVOID)matchAddress, newDataSize, PAGE_EXECUTE_READWRITE, &oldProtect))
                {
                    printf("Failed to change memory protection. Error: %d\n", GetLastError());
                    free(buffer);
                    return FALSE;
                }
                // 修改数据
                if (!WriteProcessMemory(hProcess, (LPVOID)matchAddress, newData, newDataSize, NULL))
                {
                    printf("Failed to write new data.Error %u\n", GetLastError());
                    free(buffer);
                    return FALSE;
                }

                printf("Data successfully modified.\n");
                free(buffer);
                return TRUE;
            }
        }
    }

    printf("Pattern not found in section %s.\n", sectionName);
    free(buffer);
    return FALSE;
}

int main()
{
    WCHAR szFinalPath[MAX_PATH];
    WCHAR dwInstallPath[MAX_PATH];
    BOOL bFoundPath(FALSE);

    PROCESS_INFORMATION pi{0};
    STARTUPINFO si{0};
    si.cb = sizeof(si);

    cout << "BlackMythWuKong WideScreen Modifier v0.1" << endl
         << "by MJ0011" << endl;

    auto hkey = unique_hkey(NULL, ::RegCloseKey);
    HKEY rawHKey;
    if (::RegOpenKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 2358720", &rawHKey) != ERROR_SUCCESS)
    {
        cout << "Can not locate BlackMythWuKong...You can just start the game manually." << endl;
        goto LOCATE_GAME_PROCESS;
    }
    hkey.reset(rawHKey);
    bFoundPath = TRUE;

    DWORD dwRegType;
    ULONG cbData(sizeof(dwInstallPath));
    if (::RegQueryValueEx(hkey.get(), L"InstallLocation", 0, &dwRegType, (LPBYTE)dwInstallPath, &cbData) != ERROR_SUCCESS)
    {
        cout << "Query InstallLocation failed. Error code: " << ::GetLastError() << endl;
        return 0;
    }

    ::PathCombine(szFinalPath, dwInstallPath, L"b1\\Binaries\\Win64\\b1-Win64-Shipping.exe");
    if (::PathFileExists(szFinalPath) == FALSE)
    {
        cout << "Can not find BlackMythWuKong file...Make sure you have installed it." << endl;
        return 0;
    }

    cout << "Starting BlackMythWuKong..." << endl;
    if (::CreateProcess(szFinalPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == FALSE)
    {
        cout << "Can not start BlackMythWuKong. Error code: " << ::GetLastError() << endl;
        return 0;
    }

LOCATE_GAME_PROCESS:
    while (true)
    {
        auto processId(GetProcessIdByName(L"b1-Win64-Shipping.exe"));
        if (processId == 0)
        {
            ::Sleep(1000);
            continue;
        }

        cout << "Process found! Process ID: " << processId << endl;
        auto hProcess = unique_handle(::OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId), ::CloseHandle);
        if (hProcess.get() == NULL)
        {
            cout << "Failed to open process handle. Error code: " << ::GetLastError() << endl;
        }
        HMODULE hlib(NULL);
        if (bFoundPath)
        {
            // No ASLR in current version but we still locate it for futher versions
            hlib = unique_lib(::LoadLibraryEx(szFinalPath, 0, DONT_RESOLVE_DLL_REFERENCES), ::FreeLibrary).get();
        }
        else
        {
            hlib = unique_lib(::LoadLibraryEx(GetProcessPath(processId).c_str(), 0, DONT_RESOLVE_DLL_REFERENCES), ::FreeLibrary).get();
        }

        cout << "Process handle opened successfully." << endl;
        // 要搜索的模式（4个字节）
        BYTE searchPattern[] = {0x8e, 0xe3, 0x18, 0x40};
        // 要写入的新数据（4个字节）
        BYTE newData[] = {0x39, 0x8e, 0x63, 0x40};

        // 在 .sdata 段中搜索并修改数据
        if (!SearchAndModifyRemoteData(hProcess.get(), (DWORD_PTR)hlib, ".sdata", searchPattern, sizeof(searchPattern), newData, sizeof(newData)))
        {
            cout << "Failed to modify to widescreen, exit..." << endl;
            return 0;
        }
        cout << "Data in .sdata section has been modified." << endl;
        break; // 找到并打开进程后退出循环
    }
    cout << "Successfully changed it to widescreen, WuKong will start in seconds..." << endl;
    return 0;
}
