#include <windows.h>
#include <stdio.h>

typedef DWORD (WINAPI *t_GetSize)(LPCWSTR, LPDWORD);
typedef BOOL  (WINAPI *t_GetInfo)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL  (WINAPI *t_QueryValue)(LPVOID, LPCWSTR, LPVOID*, PUINT);

int main(void)
{
    int fail = 0;
    wchar_t self[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, self, MAX_PATH);

    printf("================ version.dll proxy test ================\n");
    printf("test exe : %ws\n", self);

    HMODULE h = LoadLibraryW(L"version.dll");
    printf("LoadLibraryW(\"version.dll\") = %p   err=%lu\n", (void*)h, GetLastError());
    if (!h) { printf("[FAIL] 无法加载 version.dll\n"); return 1; }

    wchar_t mod[MAX_PATH] = {0};
    GetModuleFileNameW(h, mod, MAX_PATH);
    printf("实际加载 : %ws\n", mod);
    if (wcsstr(mod, L"System32")) {
        printf("[WARN] 加载到的是系统 version.dll, 代理未被使用!\n");
        fail++;
    } else {
        printf("[ OK ] 加载到的是本地代理\n");
    }

    t_GetSize pSize = (t_GetSize)GetProcAddress(h, "GetFileVersionInfoSizeW");
    t_GetInfo pInfo = (t_GetInfo)GetProcAddress(h, "GetFileVersionInfoW");
    t_QueryValue pQuery = (t_QueryValue)GetProcAddress(h, "VerQueryValueW");
    printf("\n导出解析:\n");
    printf("  GetFileVersionInfoSizeW = %p\n", (void*)pSize);
    printf("  GetFileVersionInfoW     = %p\n", (void*)pInfo);
    printf("  VerQueryValueW          = %p\n", (void*)pQuery);
    if (!pSize || !pInfo || !pQuery) { printf("[FAIL] 导出缺失\n"); return 1; }

    LPCWSTR tgt = L"C:\\Windows\\System32\\version.dll";
    DWORD dummy = 0;
    DWORD sz = pSize(tgt, &dummy);
    printf("\nGetFileVersionInfoSizeW(version.dll) = %lu\n", sz);
    if (!sz) { printf("[FAIL] 转发未生效 (返回 0)\n"); return 1; }
    printf("[ OK ] 转发生效\n");

    BYTE* buf = (BYTE*)malloc(sz);
    if (!buf) return 1;
    if (pInfo(tgt, 0, sz, buf)) {
        VS_FIXEDFILEINFO* fi = NULL;
        UINT len = 0;
        if (pQuery(buf, L"\\", (LPVOID*)&fi, &len) && fi) {
            printf("  文件版本 = %u.%u.%u.%u\n",
                   HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS),
                   HIWORD(fi->dwFileVersionLS), LOWORD(fi->dwFileVersionLS));
            printf("[ OK ] VerQueryValueW 也正常\n");
        } else {
            printf("[FAIL] VerQueryValueW 失败\n"); fail++;
        }
    } else {
        printf("[FAIL] GetFileVersionInfoW 失败\n"); fail++;
    }
    free(buf);

    printf("\n%s\n", fail ? "==== 结果: 有问题 ====" : "==== 结果: 代理工作正常, 可以放进游戏目录 ====");
    return fail ? 1 : 0;
}
