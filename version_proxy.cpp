#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#pragma comment(linker, "/export:GetFileVersionInfoA=C:\\Windows\\System32\\version.GetFileVersionInfoA")
#pragma comment(linker, "/export:GetFileVersionInfoByHandle=C:\\Windows\\System32\\version.GetFileVersionInfoByHandle")
#pragma comment(linker, "/export:GetFileVersionInfoExA=C:\\Windows\\System32\\version.GetFileVersionInfoExA")
#pragma comment(linker, "/export:GetFileVersionInfoExW=C:\\Windows\\System32\\version.GetFileVersionInfoExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=C:\\Windows\\System32\\version.GetFileVersionInfoSizeA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExA=C:\\Windows\\System32\\version.GetFileVersionInfoSizeExA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeW")
#pragma comment(linker, "/export:GetFileVersionInfoW=C:\\Windows\\System32\\version.GetFileVersionInfoW")
#pragma comment(linker, "/export:VerFindFileA=C:\\Windows\\System32\\version.VerFindFileA")
#pragma comment(linker, "/export:VerFindFileW=C:\\Windows\\System32\\version.VerFindFileW")
#pragma comment(linker, "/export:VerInstallFileA=C:\\Windows\\System32\\version.VerInstallFileA")
#pragma comment(linker, "/export:VerInstallFileW=C:\\Windows\\System32\\version.VerInstallFileW")
#pragma comment(linker, "/export:VerLanguageNameA=C:\\Windows\\System32\\version.VerLanguageNameA")
#pragma comment(linker, "/export:VerLanguageNameW=C:\\Windows\\System32\\version.VerLanguageNameW")
#pragma comment(linker, "/export:VerQueryValueA=C:\\Windows\\System32\\version.VerQueryValueA")
#pragma comment(linker, "/export:VerQueryValueW=C:\\Windows\\System32\\version.VerQueryValueW")

static HINSTANCE g_hInst = NULL;

static void PLog(const char* fmt, ...)
{
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"jqrproxy.log");

    FILE* f = NULL;
    if (_wfopen_s(&f, path, L"a") != 0 || !f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputs("\r\n", f);
    fclose(f);
}

static DWORD WINAPI LoaderThread(LPVOID p)
{
    (void)p;

    Sleep(1500);

    if (GetModuleHandleW(L"jqrmod.dll")) {
        PLog("[proxy] jqrmod.dll 已在进程内, 跳过本次加载 (防止重复初始化)");
        return 0;
    }

    wchar_t self[MAX_PATH] = {0};
    if (!g_hInst || !GetModuleFileNameW(g_hInst, self, MAX_PATH)) {
        PLog("[proxy] 取自身路径失败");
        return 1;
    }
    wchar_t* s = wcsrchr(self, L'\\');
    if (s) *s = 0;
    lstrcatW(self, L"\\JQRMod\\jqrmod.dll");

    if (GetFileAttributesW(self) == INVALID_FILE_ATTRIBUTES) {
        PLog("[proxy] 找不到 %S", self);
        return 1;
    }
    PLog("[proxy] 正在加载 %S", self);
    HMODULE h = LoadLibraryW(self);
    PLog("[proxy] LoadLibrary -> %p  err=%lu", (void*)h,
         h ? (DWORD)0 : GetLastError());
    return h ? 0 : 1;
}

static HANDLE g_mtx = NULL;

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        g_hInst = hInst;

        g_mtx = CreateMutexW(NULL, TRUE, L"Local\\JQRModVersionProxyOnce");
        if (g_mtx && GetLastError() == ERROR_ALREADY_EXISTS) return TRUE;

        HANDLE h = CreateThread(NULL, 0, LoaderThread, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
