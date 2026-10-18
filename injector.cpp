#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

static DWORD FindProcessByName(const wchar_t* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static BOOL InjectDll(DWORD pid, const wchar_t* dllPath)
{
    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                               PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                               FALSE, pid);
    if (!hProc) {
        wprintf(L"[!] OpenProcess failed, err=%lu (try running as admin)\n", GetLastError());
        return FALSE;
    }

    SIZE_T sz = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID remote = VirtualAllocEx(hProc, NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        wprintf(L"[!] VirtualAllocEx failed, err=%lu\n", GetLastError());
        CloseHandle(hProc);
        return FALSE;
    }
    if (!WriteProcessMemory(hProc, remote, dllPath, sz, NULL)) {
        wprintf(L"[!] WriteProcessMemory failed, err=%lu\n", GetLastError());
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }

    HMODULE hK = GetModuleHandleW(L"kernel32.dll");
    FARPROC fn = GetProcAddress(hK, "LoadLibraryW");
    HANDLE hT = CreateRemoteThread(hProc, NULL, 0,
                                   (LPTHREAD_START_ROUTINE)fn, remote, 0, NULL);
    if (!hT) {
        wprintf(L"[!] CreateRemoteThread failed, err=%lu\n", GetLastError());
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }
    WaitForSingleObject(hT, 10000);
    DWORD code = 0;
    GetExitCodeThread(hT, &code);
    wprintf(L"[+] LoadLibraryW returned 0x%08lX %s\n", code,
            (code == 0) ? L"(FAILED)" : L"(ok)");

    CloseHandle(hT);
    CloseHandle(hProc);
    return code != 0;
}

static void GetExeDir(wchar_t* out, DWORD cch)
{
    GetModuleFileNameW(NULL, out, cch);
    wchar_t* p = wcsrchr(out, L'\\');
    if (p) *(p + 1) = 0;
}

int main(void)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;

    wchar_t dllPath[MAX_PATH] = {0};
    const wchar_t* procName = L"Clone Drone in the Danger Zone.exe";
    DWORD pid = 0;

    if (argc >= 2) {
        lstrcpynW(dllPath, argv[1], MAX_PATH);

        BOOL isAbs = (dllPath[0] && dllPath[1] == L':')
                  || (dllPath[0] == L'\\' && dllPath[1] == L'\\');
        if (!isAbs) {
            wchar_t dir[MAX_PATH], tmp[MAX_PATH];
            GetExeDir(dir, MAX_PATH);
            lstrcpynW(tmp, dir, MAX_PATH);
            lstrcatW(tmp, dllPath);
            lstrcpynW(dllPath, tmp, MAX_PATH);
        }
    }
    if (argc >= 3) {
        procName = argv[2];

        if (argv[2][0] >= L'0' && argv[2][0] <= L'9') pid = _wtoi(argv[2]);
    }

    wprintf(L"========= JQRMod Injector =========\n");
    wprintf(L"DLL : %s\n", dllPath[0] ? dllPath : L"(none)");

    if (!dllPath[0]) {
        wprintf(L"[!] no DLL path given\n");
        LocalFree(argv);
        return 1;
    }
    if (GetFileAttributesW(dllPath) == INVALID_FILE_ATTRIBUTES) {
        wprintf(L"[!] DLL not found: %s\n", dllPath);
        LocalFree(argv);
        return 1;
    }

    if (!pid) pid = FindProcessByName(procName);
    if (!pid) {
        wprintf(L"[!] target process not running: %s\n", procName);
        wprintf(L"    start the game first, then run this injector again.\n");
        LocalFree(argv);
        return 1;
    }
    wprintf(L"target: %s  pid=%lu\n", procName, pid);

    BOOL ok = InjectDll(pid, dllPath);
    wprintf(L"%s\n", ok ? L"[+] injected. log: %TEMP%\\jqrmod.log" : L"[-] injection FAILED");
    LocalFree(argv);
    return ok ? 0 : 1;
}
