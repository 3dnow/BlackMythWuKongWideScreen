
#include <iostream>
#include "windows.h"
#include "shlwapi.h"
#include "tlhelp32.h"
#pragma  comment(lib ,"shlwapi.lib")
#include <string>

std::wstring GetProcessPath(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) return L"";

    WCHAR path[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        CloseHandle(hProcess);
        return std::wstring(path);
    }

    CloseHandle(hProcess);
    return L"";
}



DWORD GetProcessIdByName(const std::wstring& processName) {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(snapshot, &entry)) {
        do {
            if (entry.th32ParentProcessID != GetCurrentProcessId() &&
                processName == entry.szExeFile &&
                entry.cntThreads != 0 ) 
            {
                CloseHandle(snapshot);
                
                return entry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return 0;
}


PROCESS_INFORMATION pi;

#define SEARCH_CHUNK_SIZE 4096

BOOL SearchAndModifyRemoteData(HANDLE hProcess, DWORD_PTR moduleBase, const char* sectionName,
    const BYTE * searchPattern, DWORD patternSize,
    const BYTE * newData, DWORD newDataSize) {
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS ntHeaders;
   

    // 读取 DOS 头
    if (!ReadProcessMemory(hProcess, (LPCVOID)moduleBase, &dosHeader, sizeof(dosHeader), NULL)) {
        printf("Failed to read DOS header. Error %u \n", GetLastError());

        return FALSE;
    }

    // 读取 NT 头
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + dosHeader.e_lfanew), &ntHeaders, sizeof(ntHeaders), NULL)) {
        printf("Failed to read NT headers.\n");
        return FALSE;
    }

    // 读取节表
    DWORD sectionTableOffset = dosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS);
    IMAGE_SECTION_HEADER sectionHeader;
    BOOL sectionFound = FALSE;

    for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; i++) {
        if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + sectionTableOffset + i * sizeof(IMAGE_SECTION_HEADER)),
            &sectionHeader, sizeof(sectionHeader), NULL)) {
            printf("Failed to read section header.\n");
            return FALSE;
        }

        if (strncmp((char*)sectionHeader.Name, sectionName, 8) == 0) {
            sectionFound = TRUE;
            break;
        }
    }

    if (!sectionFound) {
        printf("Section %s not found.\n", sectionName);
        return FALSE;
    }

    // 搜索段中的数据
    DWORD_PTR sectionStart = moduleBase + sectionHeader.VirtualAddress;
    DWORD_PTR sectionEnd = sectionStart + sectionHeader.Misc.VirtualSize;
    BYTE* buffer = (BYTE*)malloc(SEARCH_CHUNK_SIZE);
    if (!buffer) {
        printf("Failed to allocate memory.\n");
        return FALSE;
    }

    for (DWORD_PTR currentAddress = sectionStart; currentAddress < sectionEnd; currentAddress += SEARCH_CHUNK_SIZE - patternSize + 1) {
        SIZE_T bytesRead;
        if (!ReadProcessMemory(hProcess, (LPCVOID)currentAddress, buffer, SEARCH_CHUNK_SIZE, &bytesRead)) {
            printf("Failed to read memory at 0x%p\n", (void*)currentAddress);
            free(buffer);
            return FALSE;
        }

        for (SIZE_T i = 0; i < bytesRead - patternSize + 1; i++) {
            if (memcmp(buffer + i, searchPattern, patternSize) == 0) {
                // 找到匹配的模式
                DWORD_PTR matchAddress = currentAddress + i;
                printf("Pattern found at address: 0x%p\n", (void*)matchAddress);
                // 修改内存保护
                DWORD oldProtect;
                if (!VirtualProtectEx(hProcess, (LPVOID)matchAddress, newDataSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    printf("Failed to change memory protection. Error: %d\n", GetLastError());
                    free(buffer);
                    return FALSE;
                }
                // 修改数据
                if (!WriteProcessMemory(hProcess, (LPVOID)matchAddress, newData, newDataSize, NULL)) {
                    printf("Failed to write new data.Error %u\n" , GetLastError());
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
	DWORD dwret; 
	HKEY hkey; 
	WCHAR FinalPath[MAX_PATH];
	WCHAR InstallPath[MAX_PATH];
    ULONG dwcb = sizeof(InstallPath);
    BOOL bFoundPath = FALSE; 

    printf("BlackMythWuKong WideScreen Modifier v0.1\nby MJ0011\n");



	dwret = RegOpenKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 2358720", &hkey);

	if (dwret != ERROR_SUCCESS)
	{
		printf("Can not locate BlackMythWuKong...You can just start the game manually\n");
        goto waitgame;
	}

    bFoundPath = TRUE; 

  	DWORD RegType;

	dwret = RegQueryValueEx(hkey, L"InstallLocation", 0, &RegType, (LPBYTE) InstallPath, &dwcb);

	if (dwret != ERROR_SUCCESS)
	{
		printf("Query InstallLocation failed %u\n", dwret);
		RegCloseKey(hkey);
		return 0; 
	}

	RegCloseKey(hkey);

	PathCombine(FinalPath, InstallPath, L"b1\\Binaries\\Win64\\b1-Win64-Shipping.exe");

	if (PathFileExists(FinalPath) == FALSE)
	{
		printf("Can not find BlackMythWuKong file...Make sure you have installed it\n");
		return 0;
	}

	STARTUPINFO si; 

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	memset(&pi, 0, sizeof(pi));

	printf("Starting BlackMythWuKong...\n");


	if (CreateProcess(FinalPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == FALSE)
	{
		printf("Can not start BlackMythWuKong, failed %u\n", GetLastError());

		return 0; 
	}
  waitgame:


    std::wstring targetProcessName = L"b1-Win64-Shipping.exe"; 
    DWORD processId = 0;
    HANDLE hProcess = NULL;
    HMODULE hlib;

    while (true) {
        processId = GetProcessIdByName(targetProcessName);

        if (processId != 0) {
            std::wcout << L"Process found! Process ID: " << processId << std::endl;

            hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
            if (hProcess != NULL) {

                if (bFoundPath)
                {
                    //No ASLR in current version but we still locate it for futher versions

                    hlib = LoadLibraryEx(FinalPath, 0, DONT_RESOLVE_DLL_REFERENCES);

                    FreeLibrary(hlib);
                }
                else
                {
                    hlib = LoadLibraryEx(GetProcessPath(processId).c_str(), 0, DONT_RESOLVE_DLL_REFERENCES);

                    FreeLibrary(hlib);
                }

                
                std::wcout << L"Process handle opened successfully." << std::endl;
                // 要搜索的模式（4个字节）
                BYTE searchPattern[] = { 0x8e, 0xe3, 0x18, 0x40 };
                // 要写入的新数据（4个字节）
                BYTE newData[] = { 0x39, 0x8e, 0x63, 0x40 };


                 //在 .sdata 段中搜索并修改数据
                if (SearchAndModifyRemoteData(hProcess, (DWORD_PTR)hlib, ".rdata", searchPattern, sizeof(searchPattern), newData, sizeof(newData))) {
                    printf("Data in .rdata section has been modified.\n");
                }
                else
                {
                    if (SearchAndModifyRemoteData(hProcess, (DWORD_PTR)hlib, ".sdata", searchPattern, sizeof(searchPattern), newData, sizeof(newData))) {
                        printf("Data in .sdata section has been modified.\n");
                    }
                    else
                    {
                        printf("Failed to modify to widescreen, exit\n");
                        return 0;
                    }
                }



                break;  // 找到并打开进程后退出循环
            }
            else {
                std::wcout << L"Failed to open process handle. Error code: " << GetLastError() << std::endl;
            }
        }

        Sleep(1000);  // 每秒检查一次
    }

    printf("\nSuccessfully changed it to widescreen, WuKong will start in seconds...\n");

	return 0;


}
