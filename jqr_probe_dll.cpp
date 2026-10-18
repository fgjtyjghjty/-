#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

static wchar_t g_logPath[MAX_PATH];
static CRITICAL_SECTION g_cs;
static BOOL g_csReady = FALSE;

static void LogInit(void)
{
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    lstrcpynW(g_logPath, tmp, MAX_PATH - 32);
    lstrcatW(g_logPath, L"jqrmod.log");
    InitializeCriticalSection(&g_cs);
    g_csReady = TRUE;
    FILE* f = NULL;
    if (_wfopen_s(&f, g_logPath, L"w") == 0 && f) fclose(f);
}

static void Log(const char* fmt, ...)
{
    if (!g_csReady) return;
    EnterCriticalSection(&g_cs);
    FILE* f = NULL;
    if (_wfopen_s(&f, g_logPath, L"a") == 0 && f) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fputs("\r\n", f);
        fclose(f);
    }
    LeaveCriticalSection(&g_cs);
}

typedef struct _MonoObject MonoObject;
typedef struct _MonoClass MonoClass;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoImage MonoImage;
typedef struct _MonoType MonoType;
typedef struct _MonoDomain MonoDomain;
typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoMethodSignature MonoMethodSignature;
typedef struct _MonoMethodDesc MonoMethodDesc;

typedef MonoDomain*         (*t_get_root_domain)(void);
typedef MonoDomain*         (*t_domain_get)(void);
typedef void*               (*t_thread_attach)(void*);
typedef void                (*t_assembly_foreach)(void (*)(MonoAssembly*, void*), void*);
typedef MonoImage*          (*t_assembly_get_image)(MonoAssembly*);
typedef const char*         (*t_image_get_name)(MonoImage*);
typedef MonoClass*          (*t_class_from_name)(MonoImage*, const char*, const char*);
typedef MonoMethod*         (*t_class_get_method_from_name)(MonoClass*, const char*, int);
typedef MonoClass*          (*t_class_get_parent)(MonoClass*);
typedef MonoMethod*         (*t_class_get_methods)(MonoClass*, void**);
typedef MonoMethodSignature*(*t_method_signature)(MonoMethod*);
typedef int                 (*t_signature_get_param_count)(MonoMethodSignature*);
typedef MonoType**          (*t_signature_get_params)(MonoMethodSignature*);
typedef const char*         (*t_type_get_name)(MonoType*);
typedef const char*         (*t_method_get_name)(MonoMethod*);
typedef const char*         (*t_class_get_name)(MonoClass*);
typedef const char*         (*t_class_get_namespace)(MonoClass*);
typedef MonoObject*         (*t_runtime_invoke)(MonoMethod*, void*, void**, MonoObject**);
typedef MonoClass*          (*t_object_get_class)(MonoObject*);
typedef void*               (*t_object_unbox)(MonoObject*);
typedef MonoType*           (*t_class_get_type)(MonoClass*);
typedef MonoObject*         (*t_type_get_object)(MonoDomain*, MonoType*);
typedef void                (*t_free)(void*);
typedef MonoMethodDesc*     (*t_method_desc_new)(const char*, int);
typedef MonoMethod*         (*t_method_desc_search_in_image)(MonoMethodDesc*, MonoImage*);
typedef void                (*t_method_desc_free)(MonoMethodDesc*);
typedef void                (*t_class_get_method_from_name_flags_dummy)(void);

#define DECL(n) static t_##n p_##n = NULL
DECL(get_root_domain);       DECL(domain_get);            DECL(thread_attach);        DECL(assembly_foreach);
DECL(assembly_get_image);    DECL(image_get_name);       DECL(class_from_name);
DECL(class_get_method_from_name); DECL(class_get_parent); DECL(class_get_methods);
DECL(method_signature);      DECL(signature_get_param_count); DECL(signature_get_params);
DECL(type_get_name);         DECL(method_get_name);      DECL(class_get_name);
DECL(class_get_namespace);   DECL(runtime_invoke);       DECL(object_get_class);
DECL(object_unbox);          DECL(class_get_type);       DECL(type_get_object);
DECL(free);                  DECL(method_desc_new);      DECL(method_desc_search_in_image);
DECL(method_desc_free);

static HMODULE g_hMono = NULL;
static MonoImage* g_image = NULL;
static MonoDomain* g_domain = NULL;
static HWND  g_hwnd = NULL;
static WNDPROC g_oldProc = NULL;
static volatile LONG g_ready = 0;

typedef struct { float x, y, z; } Vec3;

#define GETSYM(h, n) do { p_##n = (t_##n)GetProcAddress(h, "mono_" #n); if (!p_##n) { Log("  [MISS] mono_%s", #n); miss++; } } while (0)

static MonoClass* FindClassIn(MonoImage* img, const char* ns, const char* name)
{
    if (!img) return NULL;
    return p_class_from_name(img, ns, name);
}

static MonoMethod* FindMethod(MonoClass* k, const char* name, int np)
{
    int depth = 0;
    MonoClass* c = k;
    while (c && depth < 10) {
        MonoMethod* m = p_class_get_method_from_name(c, name, np);
        if (m) return m;
        c = p_class_get_parent(c);
        depth++;
    }
    return NULL;
}

static void DumpMethods(MonoClass* k, const char* onlyFilter)
{
    void* iter = NULL;
    MonoMethod* m = NULL;
    int n = 0;
    Log("  --- methods of %s ---", p_class_get_name(k));
    while ((m = p_class_get_methods(k, &iter)) != NULL) {
        const char* nm = p_method_get_name(m);
        if (onlyFilter && !strstr(nm, onlyFilter)) continue;
        MonoMethodSignature* sig = p_method_signature(m);
        int np = sig ? p_signature_get_param_count(sig) : -1;
        char buf[512];
        int off = _snprintf_s(buf, sizeof(buf), _TRUNCATE, "    %-28s (", nm);
        if (sig) {
            MonoType** ps = p_signature_get_params(sig);
            for (int i = 0; i < np && off < (int)sizeof(buf) - 64; i++) {
                const char* tn = p_type_get_name(ps[i]);
                off += _snprintf_s(buf + off, sizeof(buf) - off, _TRUNCATE, "%s%s",
                                   i ? ", " : "", tn ? tn : "?");
            }
        }
        _snprintf_s(buf + off, sizeof(buf) - off, _TRUNCATE, ")");
        Log("%s", buf);
        n++;
        if (n > 200) { Log("    ... (truncated)"); break; }
    }
}

static void ProbeOnMainThread(void)
{
    Log("=== [main-thread probe] ===");
    __try {
        MonoObject* exc = NULL;

        MonoClass* kTracker = FindClassIn(g_image, "", "CharacterTracker");
        Log("CharacterTracker class = %p", (void*)kTracker);
        if (!kTracker) return;

        MonoMethod* mInst = FindMethod(kTracker, "get_Instance", 0);
        Log("CharacterTracker.get_Instance = %p", (void*)mInst);
        if (!mInst) return;

        MonoObject* tracker = NULL;
        p_runtime_invoke(mInst, NULL, NULL, &exc);
        if (exc) { Log("  invoke get_Instance threw exception"); return; }

        tracker = NULL;
        {
            void* args0[1] = { NULL };
            MonoObject* r = p_runtime_invoke(mInst, NULL, args0, &exc);
            if (r) tracker = r;
        }
        Log("  tracker instance = %p", (void*)tracker);
        if (!tracker) { Log("  !! tracker is NULL (not in game?)"); return; }

        MonoMethod* mGetTf = FindMethod(kTracker, "GetPlayerTransform", 0);
        Log("CharacterTracker.GetPlayerTransform = %p", (void*)mGetTf);
        if (!mGetTf) return;

        MonoObject* tf = p_runtime_invoke(mGetTf, tracker, NULL, &exc);
        if (exc) { Log("  GetPlayerTransform threw"); return; }
        Log("  player Transform = %p", (void*)tf);
        if (!tf) { Log("  !! player transform NULL"); return; }

        MonoClass* kTf = p_object_get_class(tf);
        MonoMethod* mPos = FindMethod(kTf, "get_position", 0);
        Log("Transform.get_position = %p", (void*)mPos);
        if (mPos) {
            MonoObject* boxed = p_runtime_invoke(mPos, tf, NULL, &exc);
            if (!exc && boxed) {
                Vec3* v = (Vec3*)p_object_unbox(boxed);
                if (v) Log("  PLAYER POSITION = (%.2f, %.2f, %.2f)", v->x, v->y, v->z);
            } else {
                Log("  get_position failed");
            }
        }

        MonoClass* kFactory = FindClassIn(g_image, "", "EnemyFactory");
        Log("EnemyFactory class = %p", (void*)kFactory);
        if (kFactory) {
            MonoMethod* mFI = FindMethod(kFactory, "get_Instance", 0);
            Log("EnemyFactory.get_Instance = %p", (void*)mFI);
            if (mFI) {
                MonoObject* fac = p_runtime_invoke(mFI, NULL, NULL, &exc);
                if (!exc && fac) Log("  EnemyFactory instance = %p", (void*)fac);
                else Log("  EnemyFactory instance NULL/exc");
            }
            DumpMethods(kFactory, "SpawnEnemy");
        }

        MonoMethodDesc* md = p_method_desc_new(
            "EnemyFactory:SpawnEnemy(EnemyType,UnityEngine.Vector3,UnityEngine.Vector3,CharacterModel)", 0);
        Log("method_desc = %p", (void*)md);
        if (md) {
            MonoMethod* mm = p_method_desc_search_in_image(md, g_image);
            Log("  desc search SpawnEnemy(EnemyType,...) = %p", (void*)mm);
            p_method_desc_free(md);
        }

        MonoClass* kChar = FindClassIn(g_image, "", "Character");
        if (kChar) {
            MonoMethod* mSet = FindMethod(kChar, "set_IsPlayerTeam", 1);
            MonoMethod* mGet = FindMethod(kChar, "get_IsPlayerTeam", 0);
            Log("Character.set_IsPlayerTeam = %p   get = %p", (void*)mSet, (void*)mGet);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("!! SEH exception in main-thread probe (code %08X)", GetExceptionCode());
    }
    Log("=== [probe done] ===");
}

#define WM_JQR_PROBE (WM_APP + 101)

static LRESULT CALLBACK JqrProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_JQR_PROBE) {
        ProbeOnMainThread();
        return 0;
    }
    return CallWindowProcW(g_oldProc, h, msg, w, l);
}

typedef struct { HWND best; DWORD pid; } FindWndCtx;

static BOOL CALLBACK EnumWinProc(HWND h, LPARAM lp)
{
    FindWndCtx* ctx = (FindWndCtx*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != ctx->pid) return TRUE;
    if (!IsWindowVisible(h)) return TRUE;
    if (GetWindow(h, GW_OWNER) != NULL) return TRUE;

    wchar_t cls[128] = {0};
    GetClassNameW(h, cls, 128);
    wchar_t title[256] = {0};
    GetWindowTextW(h, title, 256);

    if (wcscmp(cls, L"UnityWndClass") == 0) {
        ctx->best = h;
        return FALSE;
    }
    if (!ctx->best) ctx->best = h;
    return TRUE;
}

static void AsmEnumCb(MonoAssembly* asm_, void* user)
{
    MonoImage* img = p_assembly_get_image(asm_);
    const char* nm = img ? p_image_get_name(img) : "?";
    Log("  asm: %s  image=%p", nm ? nm : "?", (void*)img);
    if (img && nm && !g_image && strstr(nm, "Assembly-CSharp.dll")) {
        g_image = img;
    }
}

static DWORD WINAPI Worker(LPVOID param)
{
    LogInit();
    Log("");
    Log("========================================");
    Log(" JQRMod probe DLL  attached  pid=%lu", GetCurrentProcessId());
    Log("========================================");

    for (int i = 0; i < 240 && !g_hMono; i++) {
        g_hMono = GetModuleHandleW(L"mono-2.0-bdwgc.dll");
        if (!g_hMono) Sleep(500);
    }
    if (!g_hMono) { Log("!! mono-2.0-bdwgc.dll not found - abort"); return 1; }
    Log("mono module = %p", (void*)g_hMono);

    int miss = 0;
    GETSYM(g_hMono, get_root_domain);
    GETSYM(g_hMono, domain_get);
    GETSYM(g_hMono, thread_attach);
    GETSYM(g_hMono, assembly_foreach);
    GETSYM(g_hMono, assembly_get_image);
    GETSYM(g_hMono, image_get_name);
    GETSYM(g_hMono, class_from_name);
    GETSYM(g_hMono, class_get_method_from_name);
    GETSYM(g_hMono, class_get_parent);
    GETSYM(g_hMono, class_get_methods);
    GETSYM(g_hMono, method_signature);
    GETSYM(g_hMono, signature_get_param_count);
    GETSYM(g_hMono, signature_get_params);
    GETSYM(g_hMono, type_get_name);
    GETSYM(g_hMono, method_get_name);
    GETSYM(g_hMono, class_get_name);
    GETSYM(g_hMono, class_get_namespace);
    GETSYM(g_hMono, runtime_invoke);
    GETSYM(g_hMono, object_get_class);
    GETSYM(g_hMono, object_unbox);
    GETSYM(g_hMono, class_get_type);
    GETSYM(g_hMono, type_get_object);
    GETSYM(g_hMono, free);
    GETSYM(g_hMono, method_desc_new);
    GETSYM(g_hMono, method_desc_search_in_image);
    GETSYM(g_hMono, method_desc_free);
    Log("symbols resolved, missing=%d", miss);
    if (miss > 3) { Log("!! too many missing symbols - abort"); return 1; }

    g_domain = p_get_root_domain();
    if (!g_domain) { Log("!! get_root_domain returned NULL"); return 1; }
    p_thread_attach(g_domain);
    Log("attached to mono domain %p", (void*)g_domain);

    for (int i = 0; i < 120 && !g_image; i++) {
        p_assembly_foreach(AsmEnumCb, NULL);
        if (!g_image) Sleep(1000);
    }
    Log("");
    Log("=== loaded assemblies ===");
    p_assembly_foreach(AsmEnumCb, NULL);
    if (!g_image) { Log("!! Assembly-CSharp.dll not found - abort"); return 1; }
    Log("=> Assembly-CSharp image = %p", (void*)g_image);

    for (int i = 0; i < 120 && !g_hwnd; i++) {
        FindWndCtx ctx;
        ctx.best = NULL;
        ctx.pid = GetCurrentProcessId();
        EnumWindows(EnumWinProc, (LPARAM)&ctx);
        g_hwnd = ctx.best;
        if (!g_hwnd) Sleep(500);
    }
    if (!g_hwnd) { Log("!! game window not found"); return 1; }
    {
        wchar_t title[256] = {0};
        GetWindowTextW(g_hwnd, title, 256);
        g_oldProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)JqrProc);
        Log("game hwnd = %p title = %ls  oldProc = %p", (void*)g_hwnd, title, (void*)g_oldProc);
    }

    Sleep(3000);
    for (int round = 0; round < 3; round++) {
        Log(">> posting probe request #%d to main thread", round + 1);
        PostMessageW(g_hwnd, WM_JQR_PROBE, 0, 0);
        Sleep(5000);
    }

    Log("probe thread finished. (DLL stays resident, hotkeys not installed in probe build)");
    InterlockedExchange(&g_ready, 1);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        HANDLE h = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
