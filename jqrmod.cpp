#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#pragma comment(lib, "advapi32.lib")
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <d3d11.h>
#include <dxgi.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "jqr_enemytypes.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static wchar_t g_logPath[MAX_PATH];
static CRITICAL_SECTION g_cs;
static BOOL g_csReady = FALSE;

static void Log(const char* fmt, ...);

static const char* NowStr(void)
{
    static char buf[32];
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%02d:%02d:%02d.%03d",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

static void LogInit(void)
{
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    lstrcpynW(g_logPath, tmp, MAX_PATH - 32);
    lstrcatW(g_logPath, L"jqrmod.log");
    InitializeCriticalSection(&g_cs);
    g_csReady = TRUE;

    Log("\n================ 新会话 ================");
}

static void Log(const char* fmt, ...)
{
    if (!g_csReady) return;
    EnterCriticalSection(&g_cs);
    FILE* f = NULL;
    if (_wfopen_s(&f, g_logPath, L"a") == 0 && f) {
        va_list ap;
        fprintf(f, "[%s] ", NowStr());
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
typedef struct _MonoClassField MonoClassField;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoImage MonoImage;
typedef struct _MonoType MonoType;
typedef struct _MonoDomain MonoDomain;
typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoMethodSignature MonoMethodSignature;

typedef MonoDomain*          (*t_get_root_domain)(void);
typedef MonoDomain*          (*t_domain_get)(void);
typedef void*                (*t_thread_attach)(void*);
typedef void                 (*t_assembly_foreach)(void (*)(MonoAssembly*, void*), void*);
typedef MonoImage*           (*t_assembly_get_image)(MonoAssembly*);
typedef const char*          (*t_image_get_name)(MonoImage*);
typedef MonoClass*           (*t_class_from_name)(MonoImage*, const char*, const char*);
typedef MonoMethod*          (*t_class_get_method_from_name)(MonoClass*, const char*, int);
typedef MonoClass*           (*t_class_get_parent)(MonoClass*);
typedef MonoMethod*          (*t_class_get_methods)(MonoClass*, void**);
typedef MonoMethodSignature* (*t_method_signature)(MonoMethod*);
typedef int                  (*t_signature_get_param_count)(MonoMethodSignature*);
typedef MonoType**           (*t_signature_get_params)(MonoMethodSignature*);
typedef const char*          (*t_type_get_name)(MonoType*);
typedef const char*          (*t_method_get_name)(MonoMethod*);
typedef const char*          (*t_class_get_name)(MonoClass*);
typedef MonoObject*          (*t_runtime_invoke)(MonoMethod*, void*, void**, MonoObject**);
typedef MonoClass*           (*t_object_get_class)(MonoObject*);
typedef void*                (*t_object_unbox)(MonoObject*);
typedef MonoType*            (*t_class_get_type)(MonoClass*);
typedef MonoObject*          (*t_type_get_object)(MonoDomain*, MonoType*);
typedef MonoImage*           (*t_class_get_image)(MonoClass*);
typedef MonoObject*          (*t_string_new)(MonoDomain*, const char*);
typedef void                 (*t_free)(void*);
typedef MonoAssembly*        (*t_domain_assembly_open)(MonoDomain*, const char*);
typedef MonoImage*           (*t_image_loaded)(const char*);
typedef const char*          (*t_image_get_filename)(MonoImage*);
typedef MonoClassField*      (*t_class_get_field_from_name)(MonoClass*, const char*);
typedef void                 (*t_field_set_value)(MonoObject*, MonoClassField*, void*);
typedef void                 (*t_field_get_value)(MonoObject*, MonoClassField*, void*);

typedef const void*          (*t_image_get_table_info)(MonoImage*, int);
typedef int                  (*t_table_info_get_rows)(const void*);
typedef MonoClass*           (*t_class_get)(MonoImage*, unsigned int);
typedef const char*          (*t_class_get_namespace)(MonoClass*);

#define DECL(n) static t_##n p_##n = NULL
DECL(get_root_domain); DECL(domain_get);   DECL(thread_attach);  DECL(assembly_foreach); DECL(assembly_get_image);
DECL(image_get_name); DECL(class_from_name); DECL(class_get_method_from_name); DECL(class_get_parent);
DECL(class_get_methods); DECL(method_signature); DECL(signature_get_param_count);
DECL(signature_get_params); DECL(type_get_name); DECL(method_get_name); DECL(class_get_name);
DECL(runtime_invoke); DECL(object_get_class); DECL(object_unbox); DECL(class_get_type);
DECL(type_get_object); DECL(class_get_image); DECL(string_new); DECL(free);
DECL(domain_assembly_open); DECL(image_loaded); DECL(image_get_filename);
DECL(class_get_field_from_name); DECL(field_set_value); DECL(field_get_value);
DECL(image_get_table_info); DECL(table_info_get_rows); DECL(class_get); DECL(class_get_namespace);

typedef uintptr_t (*t_array_length)(void*);
typedef char*     (*t_array_addr_with_size)(void*, int, uintptr_t);
typedef char*     (*t_string_to_utf8)(void*);
DECL(array_length); DECL(array_addr_with_size); DECL(string_to_utf8);

static HMODULE g_hMono = NULL;
static MonoImage* g_image = NULL;
static MonoDomain* g_domain = NULL;
static HWND    g_hwnd = NULL;
static WNDPROC g_oldProc = NULL;
static BOOL    g_inited = FALSE;
static HINSTANCE g_hInst = NULL;

typedef struct { float x, y, z; } Vec3;

static BOOL g_apiMissing = FALSE;
#define GETSYM(h, n) do { \
        p_##n = (t_##n)GetProcAddress(h, "mono_" #n); \
        if (!p_##n) { Log("  [MISS] mono_" #n); g_apiMissing = TRUE; } \
    } while (0)

static MonoClass* Cls(const char* ns, const char* name)
{
    return g_image ? p_class_from_name(g_image, ns, name) : NULL;
}

static MonoMethod* FindMethod(MonoClass* k, const char* name, int np)
{
    int depth = 0;
    MonoClass* c = k;
    while (c && depth < 12) {
        MonoMethod* m = p_class_get_method_from_name(c, name, np);
        if (m) return m;
        c = p_class_get_parent(c);
        depth++;
    }
    return NULL;
}

static MonoMethod* FindMethodP(MonoClass* k, const char* name, int np,
                               int idx, const char* typeSub)
{
    int depth = 0;
    MonoClass* c = k;
    while (c && depth < 12) {
        void* iter = NULL;
        MonoMethod* m = NULL;
        while ((m = p_class_get_methods(c, &iter)) != NULL) {
            const char* nm = p_method_get_name(m);
            if (!nm || strcmp(nm, name) != 0) continue;
            MonoMethodSignature* sig = p_method_signature(m);
            if (!sig) continue;
            if (p_signature_get_param_count(sig) != np) continue;
            if (idx < 0 || typeSub == NULL) return m;

            BOOL hit = FALSE;
            __try {
                MonoType** ps = p_signature_get_params(sig);
                if (ps) {
                    MonoType* t0 = ps[idx];
                    if (t0) {
                        const char* tn = p_type_get_name(t0);
                        if (tn && strstr(tn, typeSub)) hit = TRUE;
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                hit = FALSE;
            }
            if (hit) return m;
        }
        c = p_class_get_parent(c);
        depth++;
    }
    return NULL;
}

static MonoMethod* FindMethodSub(MonoClass* k, const char* sub, int np)
{
    int depth = 0;
    MonoClass* c = k;
    while (c && depth < 12) {
        void* it = NULL;
        MonoMethod* m = NULL;
        while ((m = p_class_get_methods(c, &it)) != NULL) {
            const char* nm = p_method_get_name(m);
            if (!nm || !strstr(nm, sub)) continue;
            MonoMethodSignature* sg = p_method_signature(m);
            if (sg && p_signature_get_param_count(sg) == np) return m;
        }
        c = p_class_get_parent(c);
        depth++;
    }
    return NULL;
}

static MonoClassField* FindField(MonoClass* k, const char* name)
{
    int depth = 0;
    MonoClass* c = k;
    while (c && depth < 12) {
        MonoClassField* f = p_class_get_field_from_name(c, name);
        if (f) return f;
        c = p_class_get_parent(c);
        depth++;
    }
    return NULL;
}

static void LogExc(const char* tag, MonoObject* exc)
{

    if (tag && strstr(tag, "Character.Kill")) {
        static int s_n = 0;
        if ((++s_n % 20) != 1) return;
        Log("  (Character.Kill 的 onDeath 异常累计 %d 次 —— 游戏自身缺陷, 角色已照常击杀)", s_n);
        return;
    }

    const char* tn = "?";
    if (exc) {
        MonoClass* k = p_object_get_class(exc);
        if (k) tn = p_class_get_name(k);
    }

    char msg[300];
    msg[0] = 0;
    if (exc) {
        MonoMethod* gm = FindMethod(p_object_get_class(exc), "get_Message", 0);
        if (gm) {
            MonoObject* s = p_runtime_invoke(gm, exc, NULL, NULL);
            char* u = (s && p_string_to_utf8) ? p_string_to_utf8(s) : NULL;
            if (u) {
                int w = 0;
                for (int i = 0; u[i] && w < 280; i++) {
                    char ch = u[i];
                    if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
                    msg[w++] = ch;
                }
                msg[w] = 0;
                p_free(u);
            }
        }
    }
    Log("  !! %s 抛托管异常: %s%s%s", tag ? tag : "?", tn,
        msg[0] ? " | " : "", msg);
}

static MonoObject* Invoke(MonoMethod* m, void* obj, void** args, const char* tag)
{
    MonoObject* exc = NULL;
    MonoObject* r = NULL;
    if (!m) { Log("  !! %s: method is NULL", tag ? tag : "?"); return NULL; }
    r = p_runtime_invoke(m, obj, args, &exc);
    if (exc) { LogExc(tag, exc); return NULL; }
    return r;
}

static MonoObject* SafeCall(MonoObject* obj, const char* name, int np,
                            void** args, const char* tag)
{
    MonoObject* ret = NULL;
    DWORD code = 0;
    __try {
        MonoMethod* m = obj ? FindMethod(p_object_get_class(obj), name, np) : NULL;
        if (!m) {
            code = 1;
        } else {
            ret = p_runtime_invoke(m, obj, args, NULL);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        code = GetExceptionCode();
        ret = NULL;
    }
    if (code == 1)      Log("  (跳过 %s: 该方法不存在)", tag);
    else if (code)      Log("  !! %s 抛硬件异常 code=%08X (已单句隔离, 游戏继续)", tag, code);
    return ret;
}

static MonoMethod*     m_SetNotMain   = NULL;
static MonoMethod*     m_SetMain      = NULL;
static MonoMethod*     m_OnMTF        = NULL;
static MonoMethod*     m_EnableAI     = NULL;
static MonoMethod*     m_EnableInput  = NULL;
static MonoMethod*     m_MatchSpeed   = NULL;
static MonoMethod*     m_TrackerSetPl = NULL;

static const char* TypeNameCN(int id);

static int  g_cnEditor = 0;
static void DoCnEditorTick(void);

static volatile LONG g_reachOn  = 0;
static float         g_reachMul = 3.0f;
static void DoWeaponReachTick(void);
static void DoEditorListTick(void);
static void ReachRestore(void);
static void ActEditorEnemyFilter(int reset);
static void ActEditorPlaceEnemy(int idx);

static const char* PickNameOf(MonoObject* c);

static volatile LONG g_epLowGrav = 0;
static float         g_epKickMul = 3.0f;
static float         g_epRunMul  = 3.0f;

static volatile LONG g_epSuperKick = 0;
static volatile LONG g_epNoFall    = 0;
static void DoEpTick(void);
static void ActEpExplodeSel(MonoObject* c);
static void ActEpFlingSel(MonoObject* c);
static void ActEpStunSel(MonoObject* c);
static void ActEpExplodeAll(void);
static void ActEpFlingAll(void);
static void ActEpStunAll(float sec);
static void ActEpFallAll(void);
static void ActEpSuperKick(int on, float mul);
static void ActEpNoFall(int on);
static void ActEpRunAnim(float mul);
static void ActEpBow(void);
static void ActEpCine(float deg);
static void ActEpCineOff(void);

static MonoClassField* f_DbgHideUI   = NULL;
static MonoClassField* f_DbgSeams    = NULL;
static MonoClassField* f_DbgNoSmooth = NULL;
static void ActRecruitAll(void);
static void ActScaleAll(int percent);
static void ActForceWeaponAll(int wt);
static void ActKillNearest(void);
static void ActSuicide(void);
static void ActMaxSkill(void);
static void ActTakeAllWeapon(void);
static void ActDbgFlag(MonoClassField** cache, const char* field, int on);

static void ActCamTilt(int on);
static void ActFoeSight(int mode);
static void ActBurnAll(void);
static void ActStunAll(float sec);
static void ActGodAll(int on);

static void ActRbFormation(float radius);
static void ActRbPrep(int weapon);
static void ActRbBet(int team);
static void ActRbRandomWeapon(void);
static void DoRbMatchTick(void);

static int CallBool0(MonoObject* o, const char* name);

static volatile LONG g_laserMove = 0;
static volatile LONG g_wlenOn    = 0;
static float         g_wlenMul   = 2.0f;

static volatile LONG   g_wlenAxisSel  = -1;
static MonoClassField* f_MiaCollider  = NULL;
static MonoMethod*     m_ColGetBounds = NULL;
static int             s_wlDiag       = 0;

static volatile LONG g_arrowBig   = 0;
static float         g_arrowMul   = 5.0f;
static MonoClass*    c_ArrowProj  = NULL;
static int           s_arrowTried = 0;
static DWORD         s_arrowNext  = 0;
static void LaserMoveSweep(void);
static void DoLaserMoveTick(void);
static void WLenRestore(void);
static void DoWeaponLenTick(void);

static volatile LONG g_aimOn     = 0;
static volatile LONG g_autoAtkOn = 0;
static volatile LONG g_tsOn      = 0;
static float         g_tsValue   = 1.0f;
static volatile LONG g_noCountOn = 0;
static void DoAimTick(void);
static void DoAutoAtkTick(void);
static void ActGameSpeed(int percent);
static void ActBlink(void);
static void ActEmpAll(void);
static void ActSuperJump(void);
static void ActNoCount(int on);
static void ActTeleportAll(void);

static int         HfCount(void);
static const char* HfName(int i);
static int         HfBindOf(int i);
static const char* HfKeyName(int i, char* buf, int cap);
static int         HfCapturing(void);
static void        HfBeginCapture(int i);
static void        HfSetBind(int i, int vk);
static void        HfResetAll(void);
static void        HfClearAll(void);

static int         HfCustomAddEx(int typeId, int ally, int autoCapture);
static int         HfCustomAdd(int typeId, int ally);
static void        HfCustomDel(int idx);
static BOOL        HfIsCustom(int i);
static int         HfCustomTypeId(int i);
static const char* HfCustomTypeName(int i);
static int         HfCustomAlly(int i);
static int         HfCustomCount(void);

static MonoObject* TryGetPlayer(void);
static void        DoAIOffTick(void);
static void        ActAiSmart(int on);
static void        DoAiSmartTick(void);

static void        DoCloneAllyTick(void);
static void        DoFenceOffTick(void);
static void        DoArrowBigTick(void);

static void        DoPoseTick(void);
static void        ActSelfMorph(int typeId);
static void        ActPoseSet(int which);
static void        PoseLoadNames(void);
static void        PoseStop(void);
static int         PoseCount(void);
static const char* PoseNameAt(int i);
static void        ActPoseDumpBones(void);
static int         MorphForbidden(int id);
static void*       FindAllOfType(MonoClass* kind, uintptr_t* outLen);
static int         UnboxBool(MonoObject* o, int dflt);

static int         HfCustomCount(void);
static int         HfCustomAlly(int i);

static MonoMethod*     m_GetGO        = NULL;
static MonoMethod*     m_ObjDestroy   = NULL;
static MonoMethod*     m_GetEnemyCfg  = NULL;
static MonoClassField* f_EnemyPrefab  = NULL;

static MonoClassField* f_CMPrefab     = NULL;

static MonoObject* GetSingleton(MonoClass* k, const char** errOut)
{
    MonoMethod* m = FindMethod(k, "get_Instance", 0);
    if (!m) { *errOut = "get_Instance not found"; return NULL; }
    MonoObject* exc = NULL;
    MonoObject* r = p_runtime_invoke(m, NULL, NULL, &exc);
    if (exc) { *errOut = "get_Instance threw"; return NULL; }
    if (!r) { *errOut = "Instance is null (game not in level?)"; return NULL; }
    return r;
}

static MonoClass*  c_Tracker = NULL;
static MonoClass*  c_Factory = NULL;
static MonoClass*  c_Character = NULL;
static MonoClass*  c_CharModel = NULL;
static MonoClass*  c_AI = NULL;
static MonoClass*  c_CloneMgr = NULL;
static MonoClass*  c_GameData = NULL;
static MonoClass*  c_Time = NULL;
static MonoMethod* m_GetPlayerTransform = NULL;
static MonoMethod* m_SpawnEnemy = NULL;
static MonoMethod* m_GetEuler = NULL;
static MonoMethod* m_GetTransform = NULL;

static MonoMethod* m_GetPosition = NULL;
static MonoMethod* m_GetComponent = NULL;
static MonoMethod* m_SetIsPlayerTeam = NULL;
static MonoClassField* f_IsPlayerTeam = NULL;
static MonoClassField* f_IsNeutral = NULL;
static MonoClassField* f_CharType = NULL;
static MonoClassField* f_LivingList = NULL;

static MonoClass*      c_EnergySource       = NULL;
static MonoMethod*     m_TrackerGetPlayer   = NULL;
static MonoClassField* f_EnergySource       = NULL;
static MonoClassField* f_HasInfiniteEnergy  = NULL;
static MonoClassField* f_MaxEnergy          = NULL;
static MonoClassField* f_CurrentLevel       = NULL;
static MonoMethod*     m_GDM_SetTransfers   = NULL;
static MonoMethod*     m_GDM_GetTransfers   = NULL;

static BOOL InitRefs(void)
{
    if (g_inited) return TRUE;
    if (!g_image) return FALSE;

    c_Tracker   = Cls("", "CharacterTracker");
    c_Factory   = Cls("", "EnemyFactory");
    c_Character = Cls("", "Character");
    c_AI        = Cls("", "AIController");
    c_CloneMgr  = Cls("", "CloneManager");
    c_GameData  = Cls("", "GameDataManager");
    c_EnergySource = Cls("", "EnergySource");
    Log("InitRefs: Tracker=%p Factory=%p Character=%p AI=%p CloneMgr=%p GameData=%p",
        (void*)c_Tracker, (void*)c_Factory, (void*)c_Character,
        (void*)c_AI, (void*)c_CloneMgr, (void*)c_GameData);
    if (!c_Tracker || !c_Factory || !c_Character) return FALSE;

    f_IsPlayerTeam = FindField(c_Character, "IsPlayerTeam");
    f_IsNeutral    = FindField(c_Character, "IsNeutral");
    f_CharType     = FindField(c_Character, "EnemyType");
    if (!f_CharType) f_CharType = FindField(c_Character, "CharacterType");
    Log("fields: IsPlayerTeam=%p IsNeutral=%p CharacterType=%p",
        (void*)f_IsPlayerTeam, (void*)f_IsNeutral, (void*)f_CharType);

    f_LivingList = FindField(c_Tracker, "_livingCharacters");
    Log("field: CharacterTracker._livingCharacters = %p", (void*)f_LivingList);

    __try {
        m_SpawnEnemy = FindMethodP(c_Factory, "SpawnEnemy", 4, 0, "EnemyType");
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("!! FindMethodP 异常 code=%08X -> 降级", GetExceptionCode());
        m_SpawnEnemy = NULL;
    }
    if (!m_SpawnEnemy) {
        __try {
            m_SpawnEnemy = p_class_get_method_from_name(c_Factory, "SpawnEnemy", 4);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            m_SpawnEnemy = NULL;
        }
    }
    if (!m_SpawnEnemy) { Log("!! SpawnEnemy(EnemyType,..) 没找到"); return FALSE; }

    __try {
        m_GetPlayerTransform = FindMethod(c_Tracker, "GetPlayerTransform", 0);
        m_SetIsPlayerTeam    = FindMethod(c_Character, "set_IsPlayerTeam", 1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("!! 其余方法查找异常 code=%08X", GetExceptionCode());
    }
    Log("refs: SpawnEnemy=%p GetPlayerTransform=%p set_IsPlayerTeam=%p(预期空)",
        (void*)m_SpawnEnemy, (void*)m_GetPlayerTransform, (void*)m_SetIsPlayerTeam);

    g_inited = TRUE;
    return (m_SpawnEnemy && m_GetPlayerTransform) ? TRUE : FALSE;
}

static int IsUnityDead(MonoObject* o);

static MonoObject* GetTracker(void)
{
    static int s_last = -1;
    const char* err = "?";
    MonoObject* tr = GetSingleton(c_Tracker, &err);
    int now = tr ? 1 : 0;
    if (now != s_last) {
        if (now) Log("CharacterTracker: 就绪 (已进入关卡)");
        else     Log("  !! CharacterTracker: %s", err);
        s_last = now;
    }
    return tr;
}

static MonoObject* GetPlayerCharacter(void)
{
    MonoObject* tr = GetTracker();
    if (!tr) return NULL;
    MonoMethod* m = FindMethod(p_object_get_class(tr), "GetPlayer", 0);
    MonoObject* r = Invoke(m, tr, NULL, "GetPlayer");

    if (r && IsUnityDead(r)) return NULL;
    return r;
}

static MonoObject* GetLivingList(void)
{
    MonoObject* tr = GetTracker();
    if (!tr || !f_LivingList) return NULL;
    MonoObject* list = NULL;
    p_field_get_value(tr, f_LivingList, &list);
    return list;
}

static int ListCount(MonoObject* list)
{
    if (!list) return 0;
    MonoMethod* m = FindMethod(p_object_get_class(list), "get_Count", 0);
    MonoObject* b = Invoke(m, list, NULL, "List.get_Count");
    if (!b) return 0;
    int* p = (int*)p_object_unbox(b);
    return p ? *p : 0;
}

static MonoObject* ListItem(MonoObject* list, int i)
{
    if (!list) return NULL;
    MonoMethod* m = FindMethod(p_object_get_class(list), "get_Item", 1);
    if (!m) return NULL;
    void* a[1]; a[0] = &i;
    return Invoke(m, list, a, "List.get_Item");
}

static int CharIsPlayerTeam(MonoObject* ch)
{
    if (!ch || !f_IsPlayerTeam) return 0;
    int v = 0;
    p_field_get_value(ch, f_IsPlayerTeam, &v);
    return v & 0xFF;
}

static volatile LONG g_freeAll = 0;

static int SkipAlly(MonoObject* c)
{
    return (!g_freeAll && c && CharIsPlayerTeam(c)) ? 1 : 0;
}

static int CharTypeId(MonoObject* ch)
{
    if (!ch || !f_CharType) return -1;
    int v = -1;
    p_field_get_value(ch, f_CharType, &v);
    return v;
}

static void CharSetImmune(MonoObject* ch, int on)
{
    if (!ch) return;
    const char* nm = on ? "MakeImmuneToDamage" : "MakeNotImmuneToDamage";
    MonoMethod* m = FindMethod(p_object_get_class(ch), nm, 0);
    Invoke(m, ch, NULL, nm);
}

static void CharDestroy(MonoObject* ch)
{
    if (!ch) return;
    MonoObject* go = SafeCall(ch, "get_gameObject", 0, NULL, "get_gameObject");
    if (!go) return;

    if (!m_ObjDestroy) m_ObjDestroy = FindMethod(p_object_get_class(go), "Destroy", 1);
    if (!m_ObjDestroy) {
        Log("!! 销毁: 找不到 UnityEngine.Object.Destroy, 放弃 (不走 Bolt 那条会卡的路)");
        return;
    }

    DWORD code = 0;
    void* a[1]; a[0] = &go;
    __try { p_runtime_invoke(m_ObjDestroy, NULL, a, NULL); }
    __except (EXCEPTION_EXECUTE_HANDLER) { code = GetExceptionCode(); }
    if (code) Log("!! 销毁: Object.Destroy 异常 %08X", code);
}

static MonoMethod*     m_CharKill = NULL;
static MonoClassField* f_IsAlive  = NULL;
static MonoMethod*     m_OnCharDied = NULL;

static int InLiving(MonoObject* c);

static int CharKill(MonoObject* ch)
{
    if (!ch) return 0;
    if (!c_Character) c_Character = Cls("", "Character");
    if (!c_Character) return 0;

    if (!f_IsAlive) f_IsAlive = FindField(c_Character, "_isAlive");
    if (f_IsAlive) {
        unsigned char alive = 0;
        p_field_get_value(ch, f_IsAlive, &alive);
        if (!alive) return 0;
    }

    if (!m_CharKill) m_CharKill = FindMethod(c_Character, "Kill", 3);
    if (!m_CharKill) { Log("!! 击杀: Character.Kill 没找到"); return 0; }

    MonoObject* killer = NULL;
    int src  = 0;
    int part = -1;
    void* a[3]; a[0] = &killer; a[1] = &src; a[2] = &part;
    Invoke(m_CharKill, ch, a, "Character.Kill");

    if (InLiving(ch)) {
        MonoObject* tr = GetTracker();
        if (tr) {
            if (!m_OnCharDied) m_OnCharDied = FindMethod(p_object_get_class(tr), "OnCharacterDied", 1);
            if (m_OnCharDied) {
                void* b[1]; b[0] = &ch;
                Invoke(m_OnCharDied, tr, b, "OnCharacterDied(兜底)");
            }
        }
    }
    return 1;
}

static MonoObject* GetComponentT(MonoObject* obj, MonoClass* want, const char* fn)
{
    if (!obj || !want) return NULL;
    MonoObject* tobj = p_type_get_object(g_domain, p_class_get_type(want));
    if (!tobj) return NULL;
    MonoMethod* m = FindMethod(p_object_get_class(obj), fn, 1);
    if (!m) return NULL;
    void* a[1]; a[0] = tobj;
    return Invoke(m, obj, a, fn);
}

static void SetBehaviourEnabled(MonoObject* comp, int on)
{
    if (!comp) return;
    MonoMethod* m = FindMethod(p_object_get_class(comp), "set_enabled", 1);
    int v = on ? 1 : 0;
    void* a[1]; a[0] = &v;
    Invoke(m, comp, a, "set_enabled");
}

static MonoClass* GetUnityTimeClass(void)
{
    if (c_Time) return c_Time;
    MonoClass* c = c_Character;
    for (int i = 0; c && i < 14; i++) {
        const char* nm = p_class_get_name(c);
        if (nm && (strcmp(nm, "MonoBehaviour") == 0 || strcmp(nm, "Behaviour") == 0 ||
                   strcmp(nm, "Component") == 0 || strcmp(nm, "Object") == 0)) {
            MonoImage* img = p_class_get_image(c);
            const char* inm = img ? p_image_get_name(img) : "?";
            c_Time = p_class_from_name(img, "UnityEngine", "Time");
            Log("  unity image = %s   Time = %p", inm ? inm : "?", (void*)c_Time);
            return c_Time;
        }
        c = p_class_get_parent(c);
    }
    Log("  !! 未能定位 UnityEngine.Time");
    return NULL;
}

static BOOL GetPlayerPosition(Vec3* out)
{
    MonoObject* tracker = GetTracker();
    if (!tracker) return FALSE;
    MonoObject* tf = Invoke(m_GetPlayerTransform, tracker, NULL, "GetPlayerTransform");
    if (!tf) return FALSE;

    MonoClass* kTf = p_object_get_class(tf);
    if (!m_GetPosition) m_GetPosition = FindMethod(kTf, "get_position", 0);
    if (!m_GetPosition) return FALSE;

    MonoObject* boxed = Invoke(m_GetPosition, tf, NULL, "Transform.get_position");
    if (!boxed) return FALSE;
    Vec3* v = (Vec3*)p_object_unbox(boxed);
    if (!v) return FALSE;
    *out = *v;
    return TRUE;
}

static void ActSummon(int enemyType, int makeAlly)
{
    Log(">>> 召唤 type=%d ally=%d", enemyType, makeAlly);
    if (!InitRefs()) { Log("!! InitRefs failed"); return; }

    const char* err = "?";
    MonoObject* factory = GetSingleton(c_Factory, &err);
    if (!factory) { Log("!! EnemyFactory: %s", err); return; }

    Vec3 pos;
    if (!GetPlayerPosition(&pos)) { Log("!! 拿不到玩家位置 (是不是没进关卡?)"); return; }

    Vec3 spawn, vel;
    spawn.x = pos.x + 2.0f; spawn.y = pos.y + 0.5f; spawn.z = pos.z + 2.0f;
    vel.x = vel.y = vel.z = 0.0f;

    void* args[4];
    args[0] = &enemyType;
    args[1] = &spawn;
    args[2] = &vel;
    args[3] = NULL;

    MonoObject* ret = Invoke(m_SpawnEnemy, factory, args, "SpawnEnemy");
    if (!ret) { Log("  (SpawnEnemy 返回空, 该类型可能在当前关卡不可用)"); return; }

    if (!makeAlly) { Log("  完成: 敌方"); return; }

    MonoObject* comp = GetComponentT(ret, c_Character, "GetComponent");
    if (!comp) comp = GetComponentT(ret, c_Character, "GetComponentInChildren");
    if (!comp) { Log("  !! 拿不到 Character 组件"); return; }
    if (!f_IsPlayerTeam) f_IsPlayerTeam = FindField(c_Character, "IsPlayerTeam");
    if (!f_IsPlayerTeam) { Log("  !! IsPlayerTeam 字段找不到"); return; }
    int one = 1;
    p_field_set_value(comp, f_IsPlayerTeam, &one);
    Log("  -> 友军标记已写入 (IsPlayerTeam=1)");
}

static void ActPlayerImmune(int on)
{
    MonoObject* p = GetPlayerCharacter();
    if (!p) { Log("!! 无敌: 找不到玩家角色"); return; }
    CharSetImmune(p, on);
    Log("玩家无敌 = %d", on);
}

static void ActAlliesImmune(int on)
{
    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || !CharIsPlayerTeam(c)) continue;
        CharSetImmune(c, on);
        cnt++;
    }
    Log("友军无敌 = %d  (共 %d 个)", on, cnt);
}

static void ActCruiserImmune(int on)
{
    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || !CharIsPlayerTeam(c)) continue;
        int t = CharTypeId(c);
        for (unsigned k = 0; k < JQR_WARSHIP_COUNT; k++) {
            if (t == g_warshipIds[k]) { CharSetImmune(c, on); cnt++; break; }
        }
    }
    Log("战舰无敌 = %d  (命中 %d 艘)", on, cnt);
}

static void ActClonePlus(void)
{
    if (!c_GameData) c_GameData = Cls("", "GameDataManager");
    if (!c_CloneMgr) c_CloneMgr = Cls("", "CloneManager");
    int curBefore = -1;

    if (c_GameData) {
        const char* derr = "?";
        MonoObject* gd0 = GetSingleton(c_GameData, &derr);
        if (gd0) {
            MonoMethod* g0 = FindMethod(p_object_get_class(gd0), "GetNumClones", 0);
            if (g0) { MonoObject* b0 = Invoke(g0, gd0, NULL, "GetNumClones");
                      if (b0) curBefore = *(int*)p_object_unbox(b0); }
        }
    }

    BOOL spawned = FALSE;
    if (c_CloneMgr) {
        const char* cerr = "?";
        MonoObject* cm = GetSingleton(c_CloneMgr, &cerr);
        if (!cm) {
            Log("!! CloneManager 单例拿不到: %s (退回纯 +1)", cerr);
        } else {

            MonoObject* pl = TryGetPlayer();
            MonoObject* pTf = NULL;
            if (pl) {
                if (!m_GetTransform) m_GetTransform = FindMethod(p_object_get_class(pl), "get_transform", 0);
                if (m_GetTransform) pTf = Invoke(m_GetTransform, pl, NULL, "get_transform");
            }
            if (!pTf) {
                Log("  (拿不到玩家 transform, 造实体跳过)");
            } else {
                MonoMethod* sp = FindMethod(p_object_get_class(cm), "SpawnClone", 2);
                if (!sp) {
                    Log("  !! CloneManager.SpawnClone(Transform,Vector3) 找不到");
                } else {

                    Vec3 rot; rot.x = 0; rot.y = 0; rot.z = 0;
                    MonoObject* plRot = NULL;
                    if (pl) {
                        if (!m_GetEuler) m_GetEuler = FindMethod(p_object_get_class(pl), "get_eulerAngles", 0);
                        if (m_GetEuler) plRot = Invoke(m_GetEuler, pl, NULL, "get_eulerAngles");
                    }
                    if (plRot) { Vec3* rv = (Vec3*)p_object_unbox(plRot); if (rv) rot = *rv; }

                    void* sa[2];
                    sa[0] = pTf;
                    sa[1] = &rot;
                    MonoObject* mover = Invoke(sp, cm, sa, "SpawnClone");
                    if (mover) {
                        spawned = TRUE;
                        Log("  -> 克隆实体已生成 (SpawnClone 返回 %p)", (void*)mover);
                    } else {
                        Log("  (SpawnClone 返回空 —— 可能要先进关卡 / 没有克隆区)");
                    }
                }
            }
        }
    }

    if (!c_GameData) { Log("!! GameDataManager 类找不到"); return; }
    const char* err = "?";
    MonoObject* gd = GetSingleton(c_GameData, &err);
    if (!gd) { Log("!! GameDataManager: %s", err); return; }
    MonoMethod* m = FindMethod(p_object_get_class(gd), "IncreaseNumClones", 1);
    if (!m) { Log("!! IncreaseNumClones 找不到"); return; }
    int one = 1;
    void* a[1]; a[0] = &one;
    Invoke(m, gd, a, "IncreaseNumClones");

    MonoMethod* g = FindMethod(p_object_get_class(gd), "GetNumClones", 0);
    MonoObject* b = Invoke(g, gd, NULL, "GetNumClones");
    int cur = b ? *(int*)p_object_unbox(b) : -1;
    Log("克隆 +1  -> 实体=%s  当前克隆数 %d -> %d",
        spawned ? "已生成" : "未生成(仅数字)", curBefore, cur);
}

static volatile LONG g_slowOn      = 0;
static volatile LONG g_infEnergy   = 0;
static volatile LONG g_infTransfer = 0;

static volatile LONG g_flyOn       = 0;
static volatile LONG g_wallOn      = 0;
static float         g_slowValue   = 0.15f;

static MonoMethod* m_TimeSetScale = NULL;

static volatile LONG g_aiOffOn    = 0;
static DWORD         g_aiOffTick  = 0;
#define AIOFF_RESCAN_MS 250

#define AIOFF_CACHE_MAX 512
static void*         g_aiOffCache[AIOFF_CACHE_MAX];
static int           g_aiOffCacheN = 0;

static volatile LONG g_cloneAllyOn = 0;
static volatile LONG g_fenceOffOn  = 0;
static DWORD         g_cloneAllyTick = 0;
static DWORD         g_fenceOffTick  = 0;
static volatile LONG g_cloneAllyHit  = 0;
static volatile LONG g_fenceOffHit   = 0;
#define CLONEALLY_RESCAN_MS 250
#define FENCEOFF_RESCAN_MS  500

static void ActTimeScaleEx(float ts, BOOL verbose)
{
    MonoClass* k = GetUnityTimeClass();
    if (!k) return;
    if (!m_TimeSetScale) m_TimeSetScale = p_class_get_method_from_name(k, "set_timeScale", 1);
    MonoMethod* m = m_TimeSetScale;
    if (!m) { if (verbose) Log("!! set_timeScale 找不到"); return; }
    void* a[1]; a[0] = &ts;
    MonoObject* exc = NULL;
    p_runtime_invoke(m, NULL, a, &exc);
    if (verbose) Log("子弹时间 timeScale = %.3f%s", ts, exc ? "  (抛异常)" : "");
}

static void ActTimeScale(float ts) { ActTimeScaleEx(ts, TRUE); }

static MonoObject* TryGetPlayer(void)
{
    if (!c_Character || !c_Tracker) return NULL;
    if (!m_TrackerGetPlayer) m_TrackerGetPlayer = FindMethod(c_Tracker, "GetPlayer", 0);
    if (!m_TrackerGetPlayer) return NULL;
    const char* err = "?";
    MonoObject* tr = GetSingleton(c_Tracker, &err);
    if (!tr) return NULL;
    MonoObject* exc = NULL;
    MonoObject* pl = p_runtime_invoke(m_TrackerGetPlayer, tr, NULL, &exc);
    if (exc || !pl) return NULL;
    return pl;
}

static MonoObject* TryGetEnergySource(MonoObject* player)
{
    if (!player) return NULL;
    if (!f_EnergySource) f_EnergySource = FindField(c_Character, "_energySource");
    if (!f_EnergySource) return NULL;
    MonoObject* es = NULL;
    p_field_get_value(player, f_EnergySource, &es);
    return es;
}

static BOOL CacheEnergyFields(MonoObject* es)
{
    if (!es) return FALSE;
    MonoClass* k = p_object_get_class(es);
    if (!f_HasInfiniteEnergy) f_HasInfiniteEnergy = FindField(k, "HasInfiniteEnergy");
    if (!f_MaxEnergy)         f_MaxEnergy         = FindField(k, "_maxEnergy");
    if (!f_CurrentLevel)      f_CurrentLevel      = FindField(k, "_currentLevel");
    return f_HasInfiniteEnergy ? TRUE : FALSE;
}

static float GetEnergyMax(MonoObject* es)
{
    float mx = 0.0f;
    if (f_MaxEnergy) p_field_get_value(es, f_MaxEnergy, &mx);
    return (mx > 1.0f) ? mx : 100.0f;
}

static void ActInfEnergyEx(MonoObject* player, int on, BOOL verbose)
{
    MonoObject* es = TryGetEnergySource(player);
    if (!es) { if (verbose) Log("!! 无限能量: 拿不到玩家能量源 (是不是没进关卡?)"); return; }
    if (!CacheEnergyFields(es)) {
        if (verbose) Log("!! EnergySource.HasInfiniteEnergy 字段没找到");
        return;
    }
    int v = on ? 1 : 0;
    p_field_set_value(es, f_HasInfiniteEnergy, &v);

    if (on && f_CurrentLevel) {

        float cur = 0.0f;
        p_field_get_value(es, f_CurrentLevel, &cur);
        float target = GetEnergyMax(es);
        if (cur < target) p_field_set_value(es, f_CurrentLevel, &target);
    }
    if (verbose) Log("无限能量 = %d  (上限 %.1f)", on, GetEnergyMax(es));
}

static MonoObject* TryGetGameData(void)
{
    if (!c_GameData) c_GameData = Cls("", "GameDataManager");
    if (!c_GameData) return NULL;
    const char* err = "?";
    return GetSingleton(c_GameData, &err);
}

static int ReadTransfersLeft(MonoObject* gd)
{
    if (!gd) return -1;
    if (!m_GDM_GetTransfers)
        m_GDM_GetTransfers = FindMethod(c_GameData, "GetNumConsciousnessTransfersLeft", 0);
    if (!m_GDM_GetTransfers) return -1;
    MonoObject* b = Invoke(m_GDM_GetTransfers, gd, NULL, "GetNumTransfers");
    if (!b) return -1;
    int* p = (int*)p_object_unbox(b);
    return p ? *p : -1;
}

static int WriteTransfersLeft(MonoObject* gd, int v)
{
    if (!gd) return 0;
    if (!m_GDM_SetTransfers)
        m_GDM_SetTransfers = FindMethod(c_GameData, "SetNumConsciousnessTransfersLeft", 1);
    if (!m_GDM_SetTransfers) return 0;
    void* a[1]; a[0] = &v;
    MonoObject* exc = NULL;
    p_runtime_invoke(m_GDM_SetTransfers, gd, a, &exc);
    return exc ? 0 : 1;
}

#define JQR_TRANSFER_FULL 999
#define JQR_TRANSFER_KEEP 500

static int g_lastTransfers = -2;

static void ActInfTransferEx(int on, BOOL verbose)
{
    MonoObject* gd = TryGetGameData();
    if (!gd) { if (verbose) Log("!! 无限意识转移: GameDataManager 拿不到"); return; }

    int cur = ReadTransfersLeft(gd);
    if (verbose) Log("意识转移次数: 当前 = %d", cur);

    if (!on) { g_lastTransfers = -2; if (verbose) Log("无限意识转移 = 0 (不再补充)"); return; }

    if (cur < 0) { if (verbose) Log("!! 读不到转移次数"); return; }
    if (cur >= JQR_TRANSFER_KEEP) {
        if (verbose) Log("无限意识转移 = 1  (已有 %d 次, 无需补充)", cur);
        return;
    }
    if (WriteTransfersLeft(gd, JQR_TRANSFER_FULL))
        Log("无限意识转移: 补充 %d -> %d", cur, JQR_TRANSFER_FULL);
    else if (verbose)
        Log("!! SetNumConsciousnessTransfersLeft 调用失败");
}

static void DoFlyTick(void);
static void DoWallTick(void);

static void DoRosterTick(void);
static void DoAiTargetTick(void);
static void AiTgtCacheReset(void);

static void DoPossessWatchdog(void);

static void DoCutsceneTick(void);

static void DoFreeAllTick(void);
static void ActFreeAll(int on);

static MonoClass* ClsAny(const char* ns, const char* name);

static MonoClass* ClsScan(const char* ns, const char* name);
static MonoClass* ClsHard(const char* name);
static MonoClass* FindUnityObjectClass(void);

static void DoCrashCleanTick(void);
static void ActCrashClean(int on);

static volatile LONG g_cutFreeOn = 0;
static volatile LONG g_freeCamOn = 0;
static DWORD         g_cutTick   = 0;
static int           g_cutStat   = 0;

static volatile LONG g_shipSettle  = 0;
static float         g_arrowLenMul = 5.0f;

static volatile LONG  g_ctForce    = 0;
static DWORD          g_ctNextPoll = 0;
static int            s_ctProbed   = 0;
static MonoClass*     c_GameMode   = NULL;
static MonoMethod*    m_GM_IsC3    = NULL;
static MonoMethod*    m_GM_ShowC4  = NULL;
static MonoClassField* f_MoveCmd   = NULL;
static MonoMethod*    m_CtDown     = NULL;
static int            s_ctDiag     = -1;
static MonoMethod*    m_TrkGetAlly = NULL;
static void DoCtTick(void);
static void ActForceTransferNow(void);

static MonoClassField* f_TrkPlayer    = NULL;
static MonoClassField* f_TrkPlayerFPM = NULL;
static MonoMethod*     m_UpdArmorData = NULL;
static MonoClassField* f_ArmorPieces  = NULL;

static volatile LONG   g_tickStage    = -1;

static DWORD         g_sceneCoolEnd = 0;
static int           g_playerGoneN  = 0;
static volatile LONG g_renderPaused = 0;
static volatile LONG g_wasInLevel   = 0;

static MonoObject* PeekPlayer(void)
{
    MonoObject* pl = TryGetPlayer();
    if (!pl) return NULL;
    if (IsUnityDead(pl)) return NULL;
    return pl;
}

static void OnLeaveLevel(void)
{

    ActTimeScaleEx(1.0f, FALSE);

    InterlockedExchange(&g_flyOn, 0);
    InterlockedExchange(&g_wallOn, 0);
    InterlockedExchange(&g_aiOffOn, 0);
    InterlockedExchange(&g_arrowBig, 0);
    InterlockedExchange(&g_reachOn, 0);
    InterlockedExchange(&g_wlenOn, 0);
    InterlockedExchange(&g_laserMove, 0);
    InterlockedExchange(&g_epLowGrav, 0);
    InterlockedExchange(&g_epSuperKick, 0);
    InterlockedExchange(&g_epNoFall, 0);
    InterlockedExchange(&g_aimOn, 0);
    InterlockedExchange(&g_autoAtkOn, 0);
    InterlockedExchange(&g_cutFreeOn, 0);
    InterlockedExchange(&g_freeCamOn, 0);
    InterlockedExchange(&g_tsOn, 0);
    InterlockedExchange(&g_slowOn, 0);
    InterlockedExchange(&g_infEnergy, 0);
    InterlockedExchange(&g_infTransfer, 0);
    InterlockedExchange(&g_freeAll, 0);
    InterlockedExchange(&g_cloneAllyOn, 0);
    InterlockedExchange(&g_fenceOffOn, 0);
    InterlockedExchange(&g_noCountOn, 0);
    g_cnEditor = 0;

    g_aiOffCacheN = 0;

    AiTgtCacheReset();

    InterlockedExchange(&g_renderPaused, 1);
    g_sceneCoolEnd = GetTickCount() + 3000;
    Log("★ 离开关卡: 已停用全部持续功能 + 清空缓存, 3 秒内不碰游戏内存 (菜单暂停)");
}

static void DoSceneGuard(void)
{
    MonoObject* pl = PeekPlayer();
    if (pl) {
        g_playerGoneN = 0;
        InterlockedExchange(&g_wasInLevel, 1);
        if (InterlockedCompareExchange(&g_renderPaused, 0, 0)) {
            InterlockedExchange(&g_renderPaused, 0);
            Log("已回到关卡: 菜单渲染恢复");
        }
        return;
    }

    if (!InterlockedCompareExchange(&g_wasInLevel, 0, 0)) return;
    if (++g_playerGoneN < 5) return;
    g_playerGoneN = 0;
    InterlockedExchange(&g_wasInLevel, 0);
    OnLeaveLevel();
}

static const char* TickStageName(LONG s)
{
    switch (s) {
    case 0:  return "入口";
    case 1:  return "场景守卫";
    case 2:  return "时标";
    case 3:  return "飞天";
    case 4:  return "穿墙";
    case 5:  return "点名台快照";
    case 6:  return "斗蛐蛐目标覆盖";
    case 7:  return "AI停电补漏";
    case 8:  return "克隆体收编";
    case 9:  return "废除拦截墙";
    case 10: return "夺舍看门狗";
    case 11: return "编辑器中文化";
    case 12: return "武器判定范围";
    case 13: return "编辑器对象库";
    case 14: return "恶作剧持续项";
    case 15: return "自动瞄准";
    case 16: return "自动攻击";
    case 17: return "过场自由行动";
    case 18: return "无差别攻击";
    case 19: return "激光敌人可移动";
    case 20: return "武器整体加长";
    case 21: return "弓箭放大";
    case 22: return "斗兽场意识转移";
    case 23: return "骨骼动作";
    case 24: return "AI变聪明";
    case 25: return "精准崩溃清除";
    case 26: return "玩家功能门禁";
    case 27: return "无限能量";
    case 28: return "无限意识转移";
    default: return "未知";
    }
}

static void DoTick(void)
{
    if (!g_inited) return;

    g_tickStage = 0;

    {
        DWORD nowMs = GetTickCount();
        if (nowMs < g_sceneCoolEnd) return;

        if (InterlockedCompareExchange(&g_renderPaused, 0, 0)) {
            InterlockedExchange(&g_renderPaused, 0);
            Log("静默期结束: 菜单渲染已恢复");
        }
        g_tickStage = 1; DoSceneGuard();
    }

    g_tickStage = 2;
    if (g_slowOn)      ActTimeScaleEx(g_slowValue, FALSE);
    else if (g_tsOn)   ActTimeScaleEx(g_tsValue, FALSE);

    g_tickStage = 3; if (g_flyOn)  DoFlyTick();
    g_tickStage = 4; if (g_wallOn) DoWallTick();

    g_tickStage = 5; DoRosterTick();
    g_tickStage = 6; DoAiTargetTick();

    g_tickStage = 7; DoAIOffTick();

    g_tickStage = 8; DoCloneAllyTick();
    g_tickStage = 9; DoFenceOffTick();

    g_tickStage = 10; DoPossessWatchdog();

    g_tickStage = 11; if (g_cnEditor) DoCnEditorTick();

    g_tickStage = 12; if (g_reachOn) DoWeaponReachTick();

    g_tickStage = 13; DoEditorListTick();

    g_tickStage = 14; if (g_epLowGrav || g_epSuperKick || g_epNoFall) DoEpTick();

    g_tickStage = 15; if (g_aimOn)     DoAimTick();
    g_tickStage = 16; if (g_autoAtkOn) DoAutoAtkTick();

    g_tickStage = 17; if (g_cutFreeOn || g_freeCamOn) DoCutsceneTick();

    g_tickStage = 18; DoFreeAllTick();

    g_tickStage = 19; if (g_laserMove) DoLaserMoveTick();
    g_tickStage = 20; if (g_wlenOn)    DoWeaponLenTick();

    g_tickStage = 21; if (g_arrowBig)  DoArrowBigTick();

    g_tickStage = 22; if (g_ctForce)   DoCtTick();

    g_tickStage = 23; DoPoseTick();

    g_tickStage = 24; DoAiSmartTick();

    g_tickStage = 25; DoCrashCleanTick();

    g_tickStage = 26;
    if (!g_infEnergy && !g_infTransfer) return;

    MonoObject* pl = TryGetPlayer();
    if (!pl) return;

    g_tickStage = 27; if (g_infEnergy) ActInfEnergyEx(pl, 1, FALSE);
    g_tickStage = 28; if (g_infTransfer) ActInfTransferEx(1, FALSE);
    g_tickStage = 29;
}

static void ActPlayerNeutral(int on)
{
    MonoObject* p = GetPlayerCharacter();
    if (!p) { Log("!! AI不理: 找不到玩家角色"); return; }
    if (!f_IsNeutral) f_IsNeutral = FindField(c_Character, "IsNeutral");
    if (!f_IsNeutral) { Log("!! IsNeutral 字段找不到"); return; }
    int v = on ? 1 : 0;
    p_field_set_value(p, f_IsNeutral, &v);
    Log("AI不理 (玩家 IsNeutral) = %d", on);
}

static BOOL AIOffCached(void* ai)
{
    for (int i = 0; i < g_aiOffCacheN; i++)
        if (g_aiOffCache[i] == ai) return TRUE;
    return FALSE;
}

static void AIOffRemember(void* ai)
{
    if (g_aiOffCacheN >= AIOFF_CACHE_MAX) {

        g_aiOffCacheN = 0;
    }
    g_aiOffCache[g_aiOffCacheN++] = ai;
}

static void AIOffCacheReset(void)
{
    g_aiOffCacheN = 0;
}

static int ActAIOffScan(int off)
{
    if (!c_AI) c_AI = Cls("", "AIController");
    if (!c_AI) { Log("!! AIController 类找不到"); return 0; }

    MonoObject* me = GetPlayerCharacter();
    MonoObject* list = GetLivingList();
    int n = ListCount(list), hit = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c) continue;
        if (me && c == me) continue;
        MonoObject* ai = GetComponentT(c, c_AI, "GetComponent");
        if (!ai) ai = GetComponentT(c, c_AI, "GetComponentInChildren");
        if (!ai) continue;
        if (off && AIOffCached(ai)) continue;
        SetBehaviourEnabled(ai, off ? 0 : 1);
        if (off) AIOffRemember(ai);
        hit++;
    }
    return hit;
}

static void ActAIOff(int off)
{
    if (off) {
        AIOffCacheReset();
        int hit = ActAIOffScan(1);
        g_aiOffOn = 1;
        Log("AI停电 = 开  (立即冻结 %d 个 AI 控制器; 之后每 250ms 补漏新召唤的)", hit);
    } else {
        g_aiOffOn = 0;
        int hit = ActAIOffScan(0);
        AIOffCacheReset();
        Log("AI停电 = 关  (解冻 %d 个 AI 控制器)", hit);
    }
}

static void DoAIOffTick(void)
{
    if (!g_aiOffOn) return;
    DWORD now = GetTickCount();
    if (now - g_aiOffTick < AIOFF_RESCAN_MS) return;
    g_aiOffTick = now;

    int hit = ActAIOffScan(1);
    if (hit > 0)
        Log("AI停电 补漏: 新冻住 %d 个 (新召唤的敌人/友军已接管)", hit);
}

static int   g_aiSmartOn   = 0;
static DWORD g_aiSmartTick = 0;

static MonoClass*      c_AISw      = NULL;
static MonoClass*      c_MmRange   = NULL;
static MonoClass*      c_FPM2      = NULL;
static MonoClassField* f_Mm_Min    = NULL;
static MonoClassField* f_Mm_Max    = NULL;
static MonoClassField* f_AimErr    = NULL;
static MonoClassField* f_Equipped  = NULL;
static MonoClassField* f_CharModel = NULL;
static MonoClassField* f_WpnModels = NULL;
static MonoClassField* f_WpnType   = NULL;
static MonoClassField* f_L_Items   = NULL;
static MonoClassField* f_L_Size    = NULL;
static int             s_swInit    = 0;

typedef struct { const char* n; float v;        MonoClassField* f; } SwFloatKnob;
typedef struct { const char* n; int   v;        MonoClassField* f; } SwBoolKnob;
typedef struct { const char* n; float a, b;     MonoClassField* f; } SwRangeKnob;

static SwFloatKnob g_swF[] = {
    { "MaxRangeToAttack",              6.0f, NULL },
    { "RunForwardUntilRange",          3.2f, NULL },
    { "TimeToCommitToAttacks",         0.6f, NULL },
    { "MaxRangeToAttackBow",           9.0f, NULL },
    { "MaxRangeToKick",                3.2f, NULL },
    { "MinDistanceToDashAttack",       2.0f, NULL },
    { "DistanceForPerlinMovement",     8.0f, NULL },
    { "PerlinSpeedMultiplier",         1.3f, NULL },
    { "PerlinBufferAroundKicks",       2.0f, NULL },
    { "MaxAngleToFire",               -1.0f, NULL },
    { "RunForwardUntilSwordDistance",  3.0f, NULL },
    { "RunForwardUntilHammerDistance", 2.4f, NULL },
    { "RangeToSwitchToBow",           12.0f, NULL },
    { "RangeToSwitchToSwords",         6.5f, NULL },
};

static SwBoolKnob g_swB[] = {
    { "AllowStrafing",                  1, NULL },
    { "AllowMovingTowardsPlayer",       1, NULL },
    { "AllowSwitchingWeapons",          1, NULL },
    { "DashForEachAttack",              1, NULL },
    { "AttackOnJetpack",                1, NULL },
    { "KickOnJetpack",                  1, NULL },
    { "UsePredictiveAiming",            1, NULL },
    { "UseEnergyLaser",                 1, NULL },
    { "UseSummonAlly",                  1, NULL },
    { "EnableTargetHighJump",           1, NULL },
    { "ShouldHighJumpAfterDroppingBombs", 1, NULL },
    { "EnableMountedDriveBys",          1, NULL },
    { "DriveByUsesJetpack",             1, NULL },
    { "DriveByUsesKick",                1, NULL },
    { "TryToKickIntoHazards",           1, NULL },
    { "TiltTowardsTargetWhenNotUsingBow", 1, NULL },
    { "UsePathfinding",                 1, NULL },
    { "AllowMovingWhileFlameBreathing", 1, NULL },
};

static SwRangeKnob g_swMM[] = {
    { "WaitBetweenAttacks",        0.15f,  0.45f, NULL },
    { "WaitBetweenJumps",          0.6f,   1.6f,  NULL },
    { "WaitbetweenStrafes",        0.4f,   1.0f,  NULL },
    { "StrafeDuration",            0.5f,   1.2f,  NULL },
    { "WaitbetweenJetpacks",       1.0f,   2.5f,  NULL },
    { "JetpackDuration",           0.8f,   1.5f,  NULL },
    { "WaitBetweenKicks",          0.8f,   2.0f,  NULL },
    { "WaitBetweenFlameBreaths",   3.0f,   7.0f,  NULL },
    { "MaxFlameBreathTime",        1.5f,   2.5f,  NULL },
    { "InitialWaitForFlameBreath", 2.0f,   4.0f,  NULL },
    { "WaitBetweenDriveBys",       1.5f,   4.0f,  NULL },
    { "WaitBetweenSpecialAttacks", 1.5f,   4.0f,  NULL },
    { "WaitBetweenHighJumps",      6.0f,  12.0f,  NULL },
    { "WaitBetweenEnergyLasers",   3.0f,   6.0f,  NULL },
    { "WaitBetweenSummonAllies",   8.0f,  14.0f,  NULL },
    { "JumpKickFrequency",         1.0f,   3.0f,  NULL },
    { "AttackDuration",            0.3f,   0.8f,  NULL },
    { "TargetDistanceToPlayer",    4.0f,   7.0f,  NULL },
    { "TimeToStayAtTargetDistance", 1.5f,  3.0f,  NULL },
};

#define SW_NF (int)(sizeof(g_swF) /sizeof(g_swF[0]))
#define SW_NB (int)(sizeof(g_swB) /sizeof(g_swB[0]))
#define SW_NM (int)(sizeof(g_swMM)/sizeof(g_swMM[0]))

static MonoObject* SwGetFpm(MonoObject* ai)
{
    if (!c_FPM2) c_FPM2 = Cls("", "FirstPersonMover");
    if (!c_FPM2) return NULL;
    MonoObject* f = GetComponentT(ai, c_FPM2, "GetComponent");
    if (!f) f = GetComponentT(ai, c_FPM2, "GetComponentInChildren");
    return f;
}

static void SwApplyWeapons(MonoObject* fpm)
{
    if (!fpm || IsUnityDead(fpm)) return;
    MonoClass* kc = p_object_get_class(fpm);

    if (!f_Equipped)  f_Equipped  = FindField(kc, "_equippedWeapons");
    if (!f_CharModel) f_CharModel = FindField(kc, "_characterModel");
    if (!f_Equipped || !f_CharModel) return;

    MonoObject* list = NULL;
    MonoObject* cm   = NULL;
    p_field_get_value(fpm, f_Equipped,  &list);
    p_field_get_value(fpm, f_CharModel, &cm);
    if (!list || !cm || IsUnityDead(cm)) return;

    if (!f_WpnModels) f_WpnModels = FindField(p_object_get_class(cm), "WeaponModels");
    if (!f_WpnModels) return;
    MonoObject* arr = NULL;
    p_field_get_value(cm, f_WpnModels, &arr);
    if (!arr) return;

    MonoClass* lc = p_object_get_class(list);
    if (!f_L_Items) f_L_Items = FindField(lc, "_items");
    if (!f_L_Size)  f_L_Size  = FindField(lc, "_size");
    if (!f_L_Items || !f_L_Size) return;

    MonoObject* items = NULL;
    int size = 0;
    p_field_get_value(list, f_L_Items, &items);
    p_field_get_value(list, f_L_Size,  &size);
    if (!items || size < 0) return;
    uintptr_t cap = p_array_length(items);
    if (!cap) return;

    int nmodels = (int)p_array_length(arr);
    int added = 0;
    for (int i = 0; i < nmodels; i++) {
        MonoObject* wm = *(MonoObject**)p_array_addr_with_size(arr, sizeof(void*), i);
        if (!wm || IsUnityDead(wm)) continue;
        if (!f_WpnType) f_WpnType = FindField(p_object_get_class(wm), "WeaponType");
        if (!f_WpnType) continue;
        int wt = 0;
        p_field_get_value(wm, f_WpnType, &wt);
        if (wt <= 0) continue;

        int have = 0;
        for (int j = 0; j < size; j++) {
            if (*(int*)p_array_addr_with_size(items, 4, j) == wt) { have = 1; break; }
        }
        if (have) continue;
        if ((uintptr_t)size >= cap) continue;

        *(int*)p_array_addr_with_size(items, 4, size) = wt;
        size++;
        added++;
    }
    if (added) p_field_set_value(list, f_L_Size, &size);
}

static void SwApplyOne(MonoObject* ai)
{
    if (!ai || IsUnityDead(ai)) return;
    int i;

    for (i = 0; i < SW_NF; i++)
        if (g_swF[i].f) { float v = g_swF[i].v; p_field_set_value(ai, g_swF[i].f, &v); }

    for (i = 0; i < SW_NB; i++)
        if (g_swB[i].f) { int v = g_swB[i].v; p_field_set_value(ai, g_swB[i].f, &v); }

    for (i = 0; i < SW_NM; i++) {
        if (!g_swMM[i].f) continue;
        MonoObject* mm = NULL;
        p_field_get_value(ai, g_swMM[i].f, &mm);
        if (!mm) continue;
        float a = g_swMM[i].a, b = g_swMM[i].b;
        if (f_Mm_Min) p_field_set_value(mm, f_Mm_Min, &a);
        if (f_Mm_Max) p_field_set_value(mm, f_Mm_Max, &b);
    }

    if (f_AimErr) { float z3[3]; z3[0] = z3[1] = z3[2] = 0.0f; p_field_set_value(ai, f_AimErr, z3); }

    { MonoObject* fpm = SwGetFpm(ai); if (fpm) SwApplyWeapons(fpm); }
}

static void SwInit(void)
{
    if (s_swInit) return;
    s_swInit = 1;

    c_AISw = Cls("", "AISwordsmanController");
    if (!c_AISw) { Log("!! [聪明AI] AISwordsmanController 找不到"); return; }

    int i, miss = 0;
    for (i = 0; i < SW_NF; i++) { g_swF[i].f  = FindField(c_AISw, g_swF[i].n);  if (!g_swF[i].f)  miss++; }
    for (i = 0; i < SW_NB; i++) { g_swB[i].f  = FindField(c_AISw, g_swB[i].n);  if (!g_swB[i].f)  miss++; }
    for (i = 0; i < SW_NM; i++) { g_swMM[i].f = FindField(c_AISw, g_swMM[i].n); if (!g_swMM[i].f) miss++; }

    c_MmRange = Cls("", "MinMaxRange");
    if (c_MmRange) { f_Mm_Min = FindField(c_MmRange, "Min"); f_Mm_Max = FindField(c_MmRange, "Max"); }
    f_AimErr = FindField(c_AISw, "RandomAimError");

    Log("[聪明AI] 旋钮就绪: float %d / bool %d / range %d (缺 %d), MinMax=%s",
        SW_NF, SW_NB, SW_NM, miss, c_MmRange ? "ok" : "!!无");
}

static int DoAiSmartScan(void)
{
    if (!c_AISw) return 0;
    MonoObject* me   = GetPlayerCharacter();
    MonoObject* list = GetLivingList();
    if (!list) return 0;

    int n = ListCount(list), hit = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (me && c == me) continue;
        MonoObject* ai = GetComponentT(c, c_AISw, "GetComponent");
        if (!ai) ai = GetComponentT(c, c_AISw, "GetComponentInChildren");
        if (!ai) continue;
        __try { SwApplyOne(ai); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        hit++;
    }
    return hit;
}

static void ActAiSmart(int on)
{
    g_aiSmartOn = on ? 1 : 0;
    if (on) {
        s_swInit = 0;
        SwInit();
        int hit = DoAiSmartScan();
        g_aiSmartTick = GetTickCount();
        Log("AI变聪明 = 开  (已武装 %d 个 AI; 之后每秒补漏新召唤的)", hit);
    } else {
        Log("AI变聪明 = 关  (停止补刷; 已改数值本场保留, 重进关卡恢复默认)");
    }
}

static void DoAiSmartTick(void)
{
    if (!g_aiSmartOn) return;
    DWORD now = GetTickCount();
    if (now - g_aiSmartTick < 1000) return;
    g_aiSmartTick = now;
    DoAiSmartScan();
}

static MonoClassField* f_CM_clones     = NULL;
static MonoClass*      c_GarbFF        = NULL;
static MonoClassField* f_GFF_disabled  = NULL;
static MonoClassField* f_GFF_renderers = NULL;
static MonoClassField* f_ChAI          = NULL;
static MonoMethod*     m_CloneIsClone  = NULL;
static MonoMethod*     m_GO_SetActive  = NULL;
static int             s_fenceTried    = 0;
static int             s_recruitDiag   = 0;

static int CloneRecruitOne(MonoObject* ch)
{
    if (!ch || IsUnityDead(ch)) return 0;
    MonoClass* kc = p_object_get_class(ch);
    if (!f_IsPlayerTeam) f_IsPlayerTeam = FindField(kc, "IsPlayerTeam");
    if (!f_IsNeutral)    f_IsNeutral    = FindField(kc, "IsNeutral");
    if (!f_IsPlayerTeam || !f_IsNeutral) return 0;

    int team = 0, neu = 0;
    p_field_get_value(ch, f_IsPlayerTeam, &team);
    p_field_get_value(ch, f_IsNeutral,    &neu);
    if (team == 1 && neu == 0) return 0;

    int one = 1, zero = 0;
    p_field_set_value(ch, f_IsPlayerTeam, &one);
    p_field_set_value(ch, f_IsNeutral,    &zero);

    if (!f_ChAI) f_ChAI = FindField(kc, "_aiController");
    if (f_ChAI) {
        MonoObject* ai = NULL;
        p_field_get_value(ch, f_ChAI, &ai);
        if (ai && !IsUnityDead(ai))
            SafeCall(ai, "refreshTargetCharacterOnOpposingTeam", 0, NULL, "克隆体: 刷新敌对目标");
    }
    return 1;
}

static int CloneRecruitScan(void)
{
    if (!c_CloneMgr) c_CloneMgr = Cls("", "CloneManager");
    int hit = 0;

    if (c_CloneMgr) {
        if (!f_CM_clones) f_CM_clones = FindField(c_CloneMgr, "_clones");
        if (f_CM_clones) {
            const char* cerr = "?";
            MonoObject* cm = GetSingleton(c_CloneMgr, &cerr);
            if (cm && !IsUnityDead(cm)) {
                MonoObject* lst = NULL;
                p_field_get_value(cm, f_CM_clones, &lst);
                if (lst) {
                    int ln = ListCount(lst);
                    for (int k = 0; k < ln; k++)
                        if (CloneRecruitOne(ListItem(lst, k))) hit++;
                }
            } else if (s_recruitDiag++ < 3) {
                Log("克隆体收编: CloneManager 单例拿不到 (%s), 改用 IsClone 兜底", cerr);
            }
        } else if (s_recruitDiag++ < 3) {
            Log("克隆体收编: _clones 字段找不到, 改用 IsClone 兜底");
        }
    }

    if (hit == 0) {
        MonoObject* list = GetLivingList();
        int n = ListCount(list);
        for (int i = 0; i < n; i++) {
            MonoObject* c = ListItem(list, i);
            if (!c || IsUnityDead(c)) continue;
            if (!m_CloneIsClone) m_CloneIsClone = FindMethod(p_object_get_class(c), "IsClone", 0);
            if (!m_CloneIsClone) break;
            MonoObject* b = Invoke(m_CloneIsClone, c, NULL, "Character.IsClone");
            if (!UnboxBool(b, 0)) continue;
            if (CloneRecruitOne(c)) hit++;
        }
    }
    return hit;
}

static void ActCloneAlly(int on)
{
    if (on) {
        InterlockedExchange(&g_cloneAllyOn, 1);
        int hit = CloneRecruitScan();
        InterlockedExchange(&g_cloneAllyHit, hit);
        Log("克隆体收编 -> 开 (本轮收编 %d 个; 之后每 250ms 补漏新克隆)", hit);
    } else {
        InterlockedExchange(&g_cloneAllyOn, 0);
        Log("克隆体收编 -> 关 (不再收编新克隆; 已收编的保持玩家队, 换关自然重置)");
    }
}

static void DoCloneAllyTick(void)
{
    if (!g_cloneAllyOn) return;
    DWORD now = GetTickCount();
    if (now - g_cloneAllyTick < CLONEALLY_RESCAN_MS) return;
    g_cloneAllyTick = now;
    int hit = CloneRecruitScan();
    if (hit > 0) {
        InterlockedExchange(&g_cloneAllyHit, g_cloneAllyHit + hit);
        Log("克隆体收编 补漏: 新收编 %d 个", hit);
    }
}

static int FenceOffScan(int off)
{
    if (!c_GarbFF && !s_fenceTried) {
        c_GarbFF = ClsHard("GarbageForceField");
        s_fenceTried = 1;
        Log("废除拦截墙: GarbageForceField %s", c_GarbFF ? "ok" : "!! 没找到");
    }
    if (!c_GarbFF) return 0;
    if (!f_GFF_disabled)  f_GFF_disabled  = FindField(c_GarbFF, "_isRepulsionDisabled");
    if (!f_GFF_renderers) f_GFF_renderers = FindField(c_GarbFF, "ForcefieldRenderers");
    if (!f_GFF_disabled) { Log("!! 废除拦截墙: _isRepulsionDisabled 字段找不到"); return 0; }

    uintptr_t n = 0;
    void* arr = FindAllOfType(c_GarbFF, &n);
    if (!arr) return 0;

    char* base = p_array_addr_with_size(arr, (int)sizeof(void*), 0);
    int v = off ? 1 : 0, hit = 0;
    for (uintptr_t i = 0; base && i < n && i < 128; i++) {
        MonoObject* ff = *(MonoObject**)(base + i * sizeof(void*));
        if (!ff || IsUnityDead(ff)) continue;
        int cur = 0;
        p_field_get_value(ff, f_GFF_disabled, &cur);
        if (cur != v) p_field_set_value(ff, f_GFF_disabled, &v);

        if (f_GFF_renderers) {
            MonoObject* ra = NULL;
            p_field_get_value(ff, f_GFF_renderers, &ra);
            if (ra && !IsUnityDead(ra) && p_array_length && p_array_addr_with_size) {
                uintptr_t rn = p_array_length(ra);
                char* rb = p_array_addr_with_size(ra, (int)sizeof(void*), 0);
                for (uintptr_t j = 0; rb && j < rn && j < 64; j++) {
                    MonoObject* go = *(MonoObject**)(rb + j * sizeof(void*));
                    if (!go || IsUnityDead(go)) continue;
                    if (!m_GO_SetActive) m_GO_SetActive = FindMethod(p_object_get_class(go), "SetActive", 1);
                    if (!m_GO_SetActive) break;
                    int act = off ? 0 : 1;
                    void* ga[1]; ga[0] = &act;
                    Invoke(m_GO_SetActive, go, ga, "GameObject.SetActive");
                }
            }
        }
        hit++;
    }
    return hit;
}

static void ActFenceOff(int on)
{
    InterlockedExchange(&g_fenceOffOn, on ? 1 : 0);
    g_fenceOffTick = 0;
    int hit = FenceOffScan(on ? 1 : 0);
    InterlockedExchange(&g_fenceOffHit, hit);
    Log("废除电梯拦截墙 -> %s (场上力场 %d 个)", on ? "开" : "关", hit);
}

static void DoFenceOffTick(void)
{
    if (!g_fenceOffOn) return;
    DWORD now = GetTickCount();
    if (now - g_fenceOffTick < FENCEOFF_RESCAN_MS) return;
    g_fenceOffTick = now;
    FenceOffScan(1);
}

static void ActKillAllEnemies(void)
{

    MonoObject* tr = GetTracker();
    if (!tr) return;
    MonoMethod* m = FindMethod(p_object_get_class(tr), "GetEnemyCharacters", 0);
    MonoObject* list = Invoke(m, tr, NULL, "GetEnemyCharacters");
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c) continue;
        if (CharKill(c)) cnt++;
    }

    if (cnt == 0) {
        MonoObject* lv = GetLivingList();
        int ln = ListCount(lv);
        for (int i = ln - 1; i >= 0; i--) {
            MonoObject* c = ListItem(lv, i);
            if (!c || SkipAlly(c)) continue;
            if (CharKill(c)) cnt++;
        }
    }
    Log("消灭所有敌人 -> 击杀 %d 个 (走官方 Character.Kill, 游戏认账)", cnt);
}

static int g_shipSweep = 1;
static int g_shipSunk  = 0;

static MonoObject* GetSingletonByName(const char* cls)
{
    MonoClass* k = Cls("", cls);
    if (!k) { Log("  !! 单例类 %s 找不到", cls); return NULL; }
    const char* err = NULL;
    MonoObject* o = GetSingleton(k, &err);
    if (!o) Log("  !! %s.Instance 取不到 (%s)", cls, err ? err : "?");
    return o;
}

static void CountArenaFoes(int* norm, int* all)
{
    int n1 = 0, n2 = 0;
    MonoObject* list = GetLivingList();
    int n = ListCount(list);
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || CharIsPlayerTeam(c)) continue;

        if (f_IsNeutral) {
            unsigned char neu = 0;
            p_field_get_value(c, f_IsNeutral, &neu);
            if (neu) continue;
        }
        n2++;

        MonoClassField* fi = FindField(p_object_get_class(c), "_ignoreForVictoryCount");
        if (fi) {
            unsigned char ig = 0;
            p_field_get_value(c, fi, &ig);
            if (ig) continue;
        }
        n1++;
    }
    if (norm) *norm = n1;
    if (all)  *all  = n2;
}

static MonoClassField* f_WonRound = NULL;
static void ForceFinishLevel(const char* why)
{
    int norm = 0, all = 0;
    CountArenaFoes(&norm, &all);
    if (norm > 0) {
        Log("  (剧情没推进: 场上还剩 %d 个该杀的敌人, 光清战舰不够 —— 勾上'顺带清场')", norm);
        return;
    }

    MonoObject* gfm = GetSingletonByName("GameFlowManager");
    if (!gfm) { Log("  (补结算跳过: 拿不到 GameFlowManager, 可能不在关卡里)"); return; }

    if (!f_WonRound) f_WonRound = FindField(p_object_get_class(gfm), "_hasWonRound");
    if (f_WonRound) {
        unsigned char won = 0;
        p_field_get_value(gfm, f_WonRound, &won);
        if (won) { Log("  (本关已经判过胜利了, 不重复触发)"); return; }
    }

    int arg = 0;
    void* a[1]; a[0] = &arg;
    SafeCall(gfm, "CelebrateVictoryAndStartReturnToElevator", 1, a,
             "CelebrateVictoryAndStartReturnToElevator");
    Log("  ★ 已补官方结算 (%s) -> 关卡判定 / 剧情推进 / 存档一起走完", why ? why : "?");
}

static void ActKillEnemyCruisers(void)
{
    MonoObject* tr   = GetTracker();
    MonoObject* list = GetLivingList();
    int n = ListCount(list), sunk = 0, untag = 0, retrack = 0;

    for (int i = n - 1; i >= 0; i--) {
        MonoObject* c = ListItem(list, i);
        if (!c || SkipAlly(c)) continue;
        int t = CharTypeId(c);
        int hit = 0;
        for (unsigned k = 0; k < JQR_WARSHIP_COUNT; k++)
            if (t == g_warshipIds[k]) { hit = 1; break; }
        if (!hit) continue;

        MonoClassField* fi = FindField(p_object_get_class(c), "_ignoreForVictoryCount");
        if (fi) {
            unsigned char ig = 0;
            p_field_get_value(c, fi, &ig);
            if (ig) {
                int off = 0;
                void* a1[1]; a1[0] = &off;
                SafeCall(c, "SetIgnoreForVictoryCount", 1, a1, "SetIgnoreForVictoryCount");
                untag++;
            }
        }

        if (tr && !InLiving(c)) {
            void* a2[1]; a2[0] = &c;
            SafeCall(tr, "TrackLivingCharacter", 1, a2, "TrackLivingCharacter");
            retrack++;
        }

        if (CharKill(c)) sunk++;
    }
    g_shipSunk = sunk;
    Log("击败敌军战舰 -> 击沉 %d 艘 (清胜利标记 %d, 补死亡登记 %d)", sunk, untag, retrack);

    if (g_shipSweep) {
        int norm = 0, all = 0;
        CountArenaFoes(&norm, &all);
        if (norm > 0) {
            MonoObject* lv = GetLivingList();
            int ln = ListCount(lv), extra = 0;
            for (int i = ln - 1; i >= 0; i--) {
                MonoObject* c = ListItem(lv, i);
                if (!c || SkipAlly(c)) continue;
                if (CharKill(c)) extra++;
            }
            Log("   顺带清场 -> 又把 %d 个护卫 / 杂兵一起清了", extra);
        }
    }

    if (g_shipSettle) {
        ForceFinishLevel("击败敌军战舰");
    } else {
        Log("   (跳过官方结算 —— 只炸船, 不播胜利音、不推进剧情; 要推剧情请开「击杀后补结算」)");
    }
}

static void ActForceWin(void)
{
    MonoObject* gfm = GetSingletonByName("GameFlowManager");
    if (!gfm) { Log("!! 强制结算: 拿不到 GameFlowManager (多半不在关卡里)"); return; }

    MonoObject* lm = GetSingletonByName("LevelManager");
    if (lm) SafeCall(lm, "MarkCurrentLevelAsWon", 0, NULL, "LevelManager.MarkCurrentLevelAsWon");

    int arg = 0;
    void* a[1]; a[0] = &arg;
    SafeCall(gfm, "CelebrateVictoryAndStartReturnToElevator", 1, a,
             "CelebrateVictoryAndStartReturnToElevator");

    int norm = 0, all = 0;
    CountArenaFoes(&norm, &all);
    Log("强制结算本关 -> 已按官方流程判为通关 (场上还剩 %d 个敌人也一并略过), 剧情/存档一起往下走", norm);
}

static MonoClass*  c_PlayerAnimator = NULL;
static MonoClass*  c_CineCam        = NULL;
static MonoClass*  c_UnityObject    = NULL;
static MonoMethod* m_FindObjsOfType = NULL;
static MonoMethod* m_PA_Release     = NULL;
static MonoMethod* m_CC_TurnOff     = NULL;
static MonoMethod* m_CC_HasTaken    = NULL;
static int         s_cutDiag        = 0;

static void* FindAllOfType(MonoClass* kind, uintptr_t* outLen)
{
    if (outLen) *outLen = 0;
    if (!kind || !g_domain) {
        if (s_cutDiag++ < 4) Log("  [场景扫描] 放弃: kind=%p domain=%p", (void*)kind, (void*)g_domain);
        return NULL;
    }

    if (!c_UnityObject) c_UnityObject = FindUnityObjectClass();
    if (!c_UnityObject) {
        if (s_cutDiag++ < 4) Log("  [场景扫描] 放弃: UnityEngine.Object 没找到");
        return NULL;
    }
    if (!m_FindObjsOfType) {
        m_FindObjsOfType = FindMethod(c_UnityObject, "FindObjectsOfType", 1);
        if (!m_FindObjsOfType) {
            MonoClass* cRes = ClsAny("UnityEngine", "Resources");
            if (!cRes) cRes = ClsScan("UnityEngine", "Resources");
            if (cRes) m_FindObjsOfType = FindMethod(cRes, "FindObjectsOfTypeAll", 1);
        }
        Log("  [场景扫描] FindObjectsOfType %s", m_FindObjsOfType ? "ok" : "!! 没找到");
    }
    if (!m_FindObjsOfType) return NULL;

    MonoObject* tobj = p_type_get_object(g_domain, p_class_get_type(kind));
    if (!tobj) {
        if (s_cutDiag++ < 4) Log("  [场景扫描] Type 对象拿不到 (类=%s)",
                                 p_class_get_name ? p_class_get_name(kind) : "?");
        return NULL;
    }
    void* a[1]; a[0] = tobj;
    MonoObject* arr = Invoke(m_FindObjsOfType, NULL, a, "FindObjectsOfType");
    if (!arr) {
        if (s_cutDiag++ < 4) Log("  [场景扫描] 数组返回 NULL (类=%s)",
                                 p_class_get_name ? p_class_get_name(kind) : "?");
        return NULL;
    }
    if (outLen && p_array_length) *outLen = p_array_length(arr);
    return arr;
}

static int UnboxBool(MonoObject* o, int dflt)
{
    if (!o || !p_object_unbox) return dflt;
    return *(unsigned char*)p_object_unbox(o) ? 1 : 0;
}

static int ReleaseAllPlayerAnimators(void)
{

    static int tried = 0;
    if (!c_PlayerAnimator && !tried) { c_PlayerAnimator = ClsHard("PlayerAnimator"); tried = 1; }
    if (!c_PlayerAnimator) return 0;
    if (!m_PA_Release) m_PA_Release = FindMethod(c_PlayerAnimator, "ReleasePlayer", 0);
    if (!m_PA_Release || !p_array_addr_with_size) {
        if (s_cutDiag++ < 4) Log("  [过场] ReleasePlayer=%s arrayAPI=%s",
                                 m_PA_Release ? "ok" : "缺失", p_array_addr_with_size ? "ok" : "缺失");
        return 0;
    }

    uintptr_t n = 0;
    void* arr = FindAllOfType(c_PlayerAnimator, &n);
    if (!arr) return 0;

    char* base = p_array_addr_with_size(arr, (int)sizeof(void*), 0);
    int hit = 0;
    for (uintptr_t i = 0; base && i < n && i < 64; i++) {
        MonoObject* pa = *(MonoObject**)(base + i * sizeof(void*));
        if (!pa) continue;
        Invoke(m_PA_Release, pa, NULL, "PlayerAnimator.ReleasePlayer");
        hit++;
    }
    if (s_cutDiag++ < 8) Log("  [过场] PlayerAnimator 命中 %u 个, 放人 %d 次", (unsigned)n, hit);
    return hit;
}

static MonoObject* FindAnimatorViaPlayer(MonoObject* pl)
{
    if (!pl || !c_PlayerAnimator) return NULL;
    MonoObject* pa = GetComponentT(pl, c_PlayerAnimator, "GetComponentInParent");
    if (pa) Log("  [过场] PlayerAnimator 没扫到, 从玩家祖先反查命中 -> 放人");
    return pa;
}

static int TurnOffActiveCineCams(void)
{

    static int tried = 0;
    if (!c_CineCam && !tried) { c_CineCam = ClsHard("LevelEditorCinematicCamera"); tried = 1; }
    if (!c_CineCam) return 0;
    if (!m_CC_TurnOff)  m_CC_TurnOff  = FindMethod(c_CineCam, "TurnOff", 0);
    if (!m_CC_HasTaken) m_CC_HasTaken = FindMethod(c_CineCam, "HasTakenOverPlayerCamera", 0);
    if (!m_CC_TurnOff || !p_array_addr_with_size) {
        if (s_cutDiag++ < 4) Log("  [过场] 剧情相机 TurnOff %s", m_CC_TurnOff ? "ok" : "缺失");
        return 0;
    }

    uintptr_t n = 0;
    void* arr = FindAllOfType(c_CineCam, &n);
    if (!arr) return 0;

    char* base = p_array_addr_with_size(arr, (int)sizeof(void*), 0);
    int hit = 0;
    for (uintptr_t i = 0; base && i < n && i < 64; i++) {
        MonoObject* cc = *(MonoObject**)(base + i * sizeof(void*));
        if (!cc) continue;
        int taken = 1;
        if (m_CC_HasTaken)
            taken = UnboxBool(Invoke(m_CC_HasTaken, cc, NULL, "HasTakenOverPlayerCamera"), 1);
        if (!taken) continue;
        Invoke(m_CC_TurnOff, cc, NULL, "LevelEditorCinematicCamera.TurnOff");
        hit++;
    }
    if (s_cutDiag++ < 8) Log("  [过场] 剧情相机 命中 %u 个, 关掉 %d 个", (unsigned)n, hit);
    return hit;
}

static int ExitCutsceneState(void)
{
    MonoObject* csm = GetSingletonByName("CutSceneManager");
    if (!csm) return 0;

    MonoMethod* mSet = FindMethod(p_object_get_class(csm), "SetIsInCutscene", 1);
    if (!mSet) return 0;
    MonoMethod* mGet = FindMethod(p_object_get_class(csm), "IsInCutscene", 0);

    int inCs = mGet ? UnboxBool(Invoke(mGet, csm, NULL, "IsInCutscene"), 0) : 1;
    if (!inCs) return 0;

    int z = 0; void* a[1]; a[0] = &z;
    Invoke(mSet, csm, a, "CutSceneManager.SetIsInCutscene(false)");
    return 1;
}

static void UnbindPlayerForCutscene(MonoObject* pl)
{
    if (!pl) return;
    int z = 0, o = 1;
    void* a[1];
    a[0] = &z;
    SafeCall(pl, "SetMovementSimulationDisabled", 1, a, "SetMovementSimulationDisabled(false)");
    a[0] = &o;
    SafeCall(pl, "EnableMainPlayerInput", 1, a, "EnableMainPlayerInput(true)");
    SafeCall(pl, "ResumeRotationSmoothing", 0, NULL, "ResumeRotationSmoothing");
}

static void DoCutsceneOnce(void)
{
    int freeOn = (InterlockedCompareExchange(&g_cutFreeOn, 0, 0) != 0);
    int camOn  = (InterlockedCompareExchange(&g_freeCamOn, 0, 0) != 0);
    if (!freeOn && !camOn) return;

    int nAct = 0;
    MonoObject* pl = TryGetPlayer();

    if (freeOn) {
        int h = ReleaseAllPlayerAnimators();
        nAct += h;

        if (!h && pl) {
            MonoObject* pa = FindAnimatorViaPlayer(pl);
            if (pa && m_PA_Release) {
                Invoke(m_PA_Release, pa, NULL, "PlayerAnimator.ReleasePlayer(反查)");
                nAct++;
            }
        }
        UnbindPlayerForCutscene(pl);
        nAct++;
        nAct += ExitCutsceneState();
    }
    if (camOn) {
        int cams = TurnOffActiveCineCams();
        nAct += cams;

        if (pl) {
            int o = 1; void* a[1]; a[0] = &o;
            SafeCall(pl, "SetCameraAnimatorEnabled", 1, a, "SetCameraAnimatorEnabled(true)");
        }
    }
    g_cutStat = nAct;
}

static void DoCutsceneTick(void)
{
    if (!InterlockedCompareExchange(&g_cutFreeOn, 0, 0) &&
        !InterlockedCompareExchange(&g_freeCamOn, 0, 0)) return;
    DWORD now = GetTickCount();
    if (now - g_cutTick < 100) return;
    g_cutTick = now;
    DoCutsceneOnce();
}

static void ActCutsceneFree(int on)
{
    InterlockedExchange(&g_cutFreeOn, on ? 1 : 0);
    if (on) {
        Log("过场自由行动 -> 开 (过场里也能动/能砍, 每 250ms 从 PlayerAnimator 手里抢一次)");
        DoCutsceneOnce();
    } else {
        Log("过场自由行动 -> 关 (交还控制权, 过场会重新接管玩家)");
    }
}

static void ActFreeCam(int on)
{
    InterlockedExchange(&g_freeCamOn, on ? 1 : 0);
    if (on) {
        Log("脱离剧情相机 -> 开 (相机从剧情机位切回你的肩后)");
        DoCutsceneOnce();
    } else {
        Log("脱离剧情相机 -> 关 (剧情相机会重新接管)");
    }
}

static volatile LONG g_crashKill   = 0;
static volatile LONG g_crashKilled = 0;
static int           s_werDone     = 0;

static const char* kCrashKeys[] = {
    "crash", "werfault", "unhandled", "fatal error", "crashpad",
    "已停止工作", "无响应", "未响应", "程序错误", "崩溃", "错误报告"
};

static int HasCrashKey(const char* s)
{
    if (!s || !*s) return 0;
    char low[320]; int i = 0;
    for (; s[i] && i < 319; i++) low[i] = (char)tolower((unsigned char)s[i]);
    low[i] = 0;
    for (unsigned k = 0; k < sizeof(kCrashKeys) / sizeof(kCrashKeys[0]); k++)
        if (strstr(low, kCrashKeys[k])) return 1;
    return 0;
}

static BOOL CALLBACK CrashKillEnum(HWND h, LPARAM lp)
{
    (void)lp;
    if (!IsWindow(h)) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    int self = (pid == GetCurrentProcessId());

    char cls[128] = {0}, ttl[300] = {0};
    GetClassNameA(h, cls, sizeof(cls) - 1);
    GetWindowTextA(h, ttl, sizeof(ttl) - 1);

    int isDlg = (_stricmp(cls, "#32770") == 0);
    int kw    = HasCrashKey(ttl) || HasCrashKey(cls);

    if (kw || (self && isDlg && IsWindowVisible(h))) {
        Log("  [崩溃杀手] 关掉窗口 \"%s\"  (pid=%lu cls=%s)", ttl, pid, cls);
        PostMessageA(h, WM_CLOSE, 0, 0);
        InterlockedIncrement(&g_crashKilled);
    }
    return TRUE;
}

static void KillCrashAgentProcs(void)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    DWORD self = GetCurrentProcessId();

    if (Process32First(snap, &pe)) {
        do {
            if (pe.th32ProcessID == self) continue;
            char low[300] = {0};
#ifdef UNICODE
            WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, low, sizeof(low) - 1, NULL, NULL);
#else
            strncpy_s(low, sizeof(low), pe.szExeFile, _TRUNCATE);
#endif
            for (int i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
            if (strstr(low, "unitycrashhandler") || strstr(low, "werfault") ||
                strstr(low, "crashpad") || strstr(low, "crashreport")) {
                HANDLE ph = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (ph) {
                    if (TerminateProcess(ph, 0)) {
                        Log("  [崩溃杀手] 终止崩溃报告器 %s (pid=%lu)", low, pe.th32ProcessID);
                        InterlockedIncrement(&g_crashKilled);
                    }
                    CloseHandle(ph);
                }
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
}

static void SuppressWerUI(void)
{
    if (s_werDone) return;
    s_werDone = 1;
    HKEY k = NULL;
    if (RegCreateKeyExA(HKEY_CURRENT_USER,
                        "Software\\Microsoft\\Windows\\Windows Error Reporting",
                        0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        DWORD one = 1;
        RegSetValueExA(k, "DontShowUI", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
        RegSetValueExA(k, "Disabled",   0, REG_DWORD, (const BYTE*)&one, sizeof(one));
        RegCloseKey(k);
        Log("  [崩溃杀手] 已压制 Windows 错误报告 UI (HKCU)");
    }
}

static DWORD WINAPI CrashKillerBody(LPVOID p)
{
    (void)p;

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    Log("[崩溃杀手] 线程就绪 (开关打开后才开始扫)");
    while (1) {
        if (InterlockedCompareExchange(&g_crashKill, 0, 0) != 0) {
            EnumWindows(CrashKillEnum, 0);
            KillCrashAgentProcs();
        }
        Sleep(500);
    }
    return 0;
}

static void ActCrashKiller(int on)
{
    InterlockedExchange(&g_crashKill, on ? 1 : 0);
    if (on) {
        Log("崩溃弹窗杀手 -> 开 (自动按掉 UnityCrashHandler / WerFault / 本进程异常框)");
        SuppressWerUI();
        EnumWindows(CrashKillEnum, 0);
        KillCrashAgentProcs();
    } else {
        Log("崩溃弹窗杀手 -> 关 (崩溃框恢复正常显示)");
    }
}

static volatile LONG   g_crashClean     = 0;
static volatile LONG   g_ccCleared      = 0;
static DWORD           g_ccTick         = 0;
static MonoClass*      c_ErrMgr         = NULL;
static MonoClass*      c_ErrWin         = NULL;
static int             s_ErrMgrMiss     = 0;
static int             s_ErrWinMiss     = 0;
static MonoClassField* f_ErrMgr_Crashed = NULL;
static MonoMethod*     m_ErrWin_Hide    = NULL;

static int ClearGameCrashOnce(void)
{
    if (!p_array_addr_with_size) return 0;
    int did = 0;

    if (!c_ErrMgr && !s_ErrMgrMiss) {
        c_ErrMgr = ClsHard("ErrorManager");
        if (!c_ErrMgr) { s_ErrMgrMiss = 1; Log("!! [崩溃清除] 找不到 ErrorManager 类"); }
    }
    if (c_ErrMgr) {
        if (!f_ErrMgr_Crashed && p_class_get_field_from_name)
            f_ErrMgr_Crashed = p_class_get_field_from_name(c_ErrMgr, "_hasCrashed");
        uintptr_t n = 0;
        void* arr = FindAllOfType(c_ErrMgr, &n);
        char* base = arr ? p_array_addr_with_size(arr, (int)sizeof(void*), 0) : NULL;
        for (uintptr_t i = 0; base && i < n && i < 16; i++) {
            MonoObject* o = *(MonoObject**)(base + i * sizeof(void*));
            if (!o || !f_ErrMgr_Crashed || !p_field_get_value || !p_field_set_value) continue;
            int v = 0;
            p_field_get_value(o, f_ErrMgr_Crashed, &v);
            if (v) {
                int z = 0;
                p_field_set_value(o, f_ErrMgr_Crashed, &z);
                did++;
                Log("  [崩溃清除] _hasCrashed: true -> false (游戏 UI 恢复更新)");
            }
        }
    }

    if (did > 0) {
        if (!c_ErrWin && !s_ErrWinMiss) {
            c_ErrWin = ClsHard("ErrorWindow");
            if (!c_ErrWin) { s_ErrWinMiss = 1; Log("!! [崩溃清除] 找不到 ErrorWindow 类"); }
        }
        if (c_ErrWin) {
            if (!m_ErrWin_Hide) m_ErrWin_Hide = FindMethod(c_ErrWin, "Hide", 0);
            uintptr_t n = 0;
            void* arr = FindAllOfType(c_ErrWin, &n);
            char* base = arr ? p_array_addr_with_size(arr, (int)sizeof(void*), 0) : NULL;
            for (uintptr_t i = 0; base && i < n && i < 16; i++) {
                MonoObject* o = *(MonoObject**)(base + i * sizeof(void*));
                if (!o || !m_ErrWin_Hide) continue;
                Invoke(m_ErrWin_Hide, o, NULL, "ErrorWindow.Hide");
                did++;
                Log("  [崩溃清除] ErrorWindow.Hide() 藏掉一个堆栈面板");
            }
        }
    }

    if (did > 0) InterlockedIncrement(&g_ccCleared);
    return did;
}

static void DoCrashCleanTick(void)
{
    if (InterlockedCompareExchange(&g_crashClean, 0, 0) == 0) return;
    DWORD now = GetTickCount();
    if (now - g_ccTick < 250) return;
    g_ccTick = now;
    ClearGameCrashOnce();
}

static void ActCrashClean(int on)
{
    InterlockedExchange(&g_crashClean, on ? 1 : 0);
    if (on) {
        s_ErrMgrMiss = s_ErrWinMiss = 0;
        Log("崩溃无视 (精准) -> 开 (清 ErrorManager._hasCrashed + 藏 ErrorWindow)");
        Log("  立即清了一遍: %d 处", ClearGameCrashOnce());
    } else {
        Log("崩溃无视 (精准) -> 关 (游戏自己的崩溃框恢复显示)");
    }
}

static MonoClassField* f_GravityMult  = NULL;
static MonoClassField* f_CharCtrl     = NULL;
static MonoClassField* f_DetectCol    = NULL;
static MonoClassField* f_MoveSpeedMul = NULL;
static MonoClassField* f_JumpVel      = NULL;
static MonoClassField* f_EquippedWpns = NULL;
static MonoClassField* f_CurWeapon    = NULL;

static MonoMethod* m_TfGetPos     = NULL;
static MonoMethod* m_TfSetPos     = NULL;
static MonoMethod* m_TfGetFwd     = NULL;
static MonoMethod* m_TfGetRight   = NULL;
static MonoMethod* m_SetDetectCol = NULL;
static MonoMethod* m_SetMoveSpeed = NULL;
static MonoMethod* m_SetWeapon    = NULL;
static MonoMethod* m_ListAdd      = NULL;
static MonoMethod* m_TimeDelta    = NULL;

static float g_flySpeed     = 15.0f;
static float g_flyGravOrig  = 1.0f;
static int   g_flyGravSaved = 0;
static float g_wallY        = 0.0f;
static int   g_wallSaved    = 0;
static float g_speedMul     = 1.0f;
static float g_jumpMul      = 1.0f;
static Vec3  g_jumpBase;
static int   g_jumpSaved    = 0;

static BOOL KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) ? TRUE : FALSE; }

static float FrameDelta(void)
{
    if (!m_TimeDelta) {
        MonoClass* k = GetUnityTimeClass();
        if (k) m_TimeDelta = FindMethod(k, "get_deltaTime", 0);
    }
    if (!m_TimeDelta) return 0.0166f;
    MonoObject* b = Invoke(m_TimeDelta, NULL, NULL, "Time.deltaTime");
    if (!b) return 0.0166f;
    float* p = (float*)p_object_unbox(b);
    float d = p ? *p : 0.0166f;
    if (d <= 0.0001f || d > 0.25f) d = 0.0166f;
    return d;
}

static MonoObject* PlayerFPM(void)
{
    MonoObject* pl = TryGetPlayer();
    if (!pl) return NULL;
    MonoClass* k = p_object_get_class(pl);

    if (!m_GetTransform) m_GetTransform = FindMethod(k, "get_transform", 0);
    if (!f_GravityMult)  f_GravityMult  = FindField(k, "GravityMultiplier");
    if (!f_CharCtrl)     f_CharCtrl     = FindField(k, "_characterController");
    if (!f_MoveSpeedMul) f_MoveSpeedMul = FindField(k, "_moveSpeedMultiplier");
    if (!f_JumpVel)      f_JumpVel      = FindField(k, "JumpVelocity");
    if (!f_EquippedWpns) f_EquippedWpns = FindField(k, "_equippedWeapons");
    if (!f_CurWeapon)    f_CurWeapon    = FindField(k, "_currentWeapon");
    if (!m_SetMoveSpeed) m_SetMoveSpeed = FindMethod(k, "setMoveSpeedMultiplier", 1);
    if (!m_SetWeapon)    m_SetWeapon    = FindMethod(k, "SetEquippedWeaponType", 2);
    return pl;
}

static MonoObject* PlayerTf(MonoObject* pl)
{
    if (!pl || !m_GetTransform) return NULL;
    MonoObject* tf = Invoke(m_GetTransform, pl, NULL, "get_transform");
    if (!tf) return NULL;
    MonoClass* kt = p_object_get_class(tf);
    if (!m_TfGetPos)   m_TfGetPos   = FindMethod(kt, "get_position", 0);
    if (!m_TfSetPos)   m_TfSetPos   = FindMethod(kt, "set_position", 1);
    if (!m_TfGetFwd)   m_TfGetFwd   = FindMethod(kt, "get_forward", 0);
    if (!m_TfGetRight) m_TfGetRight = FindMethod(kt, "get_right", 0);
    return tf;
}

static BOOL TfGetPos(MonoObject* tf, Vec3* out)
{
    if (!tf || !m_TfGetPos) return FALSE;
    MonoObject* b = Invoke(m_TfGetPos, tf, NULL, "Transform.get_position");
    if (!b) return FALSE;
    Vec3* p = (Vec3*)p_object_unbox(b);
    if (!p) return FALSE;
    *out = *p;
    return TRUE;
}

static void TfSetPos(MonoObject* tf, const Vec3* v)
{
    if (!tf || !m_TfSetPos) return;
    void* a[1]; a[0] = (void*)v;
    Invoke(m_TfSetPos, tf, a, "Transform.set_position");
}

static void TfDir(MonoObject* tf, MonoMethod* m, Vec3* out)
{
    out->x = out->y = out->z = 0.0f;
    if (!tf || !m) return;
    MonoObject* b = Invoke(m, tf, NULL, "Transform.dir");
    if (!b) return;
    Vec3* p = (Vec3*)p_object_unbox(b);
    if (!p) return;
    float len = sqrtf(p->x * p->x + p->z * p->z);
    if (len < 0.0001f) return;
    out->x = p->x / len; out->z = p->z / len;
}

static MonoMethod* m_FPM_SetMoveSim = NULL;
static MonoMethod* m_FPM_SetPosUs   = NULL;
static MonoMethod* m_FPM_GetPosUs   = NULL;
static MonoMethod* m_Chr_SetVel     = NULL;
static int         g_moveSimHeld    = 0;

static BOOL FlyKey(int a, int b)
{
    return (KeyDown(a) || (b && KeyDown(b))) ? TRUE : FALSE;
}

static void DoFlyTickBody(void)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) return;
    MonoClass* kc = p_object_get_class(pl);

    if (!m_FPM_SetPosUs) m_FPM_SetPosUs = FindMethod(kc, "SetPositionOfUsOrMount", 1);
    if (!m_FPM_GetPosUs) m_FPM_GetPosUs = FindMethod(kc, "GetPositionOfUsOrMount", 0);
    if (!m_Chr_SetVel)   m_Chr_SetVel   = FindMethod(kc, "SetVelocity", 1);

    if (f_GravityMult) { float z = 0.0f; p_field_set_value(pl, f_GravityMult, &z); }

    MonoObject* tf = PlayerTf(pl);
    if (!tf || IsUnityDead(tf)) return;

    Vec3 cur; int have = 0;
    if (m_FPM_GetPosUs) {
        MonoObject* b = Invoke(m_FPM_GetPosUs, pl, NULL, "GetPositionOfUsOrMount");
        Vec3* q = b ? (Vec3*)p_object_unbox(b) : NULL;
        if (q) { cur = *q; have = 1; }
    }
    if (!have) { if (!TfGetPos(tf, &cur)) return; }

    Vec3 fwd, rt;
    TfDir(tf, m_TfGetFwd, &fwd);
    TfDir(tf, m_TfGetRight, &rt);

    float dx = 0.0f, dy = 0.0f, dz = 0.0f;
    if (FlyKey(VK_UP,    VK_NUMPAD8)) { dx += fwd.x; dz += fwd.z; }
    if (FlyKey(VK_DOWN,  VK_NUMPAD2)) { dx -= fwd.x; dz -= fwd.z; }
    if (FlyKey(VK_RIGHT, VK_NUMPAD6)) { dx += rt.x;  dz += rt.z;  }
    if (FlyKey(VK_LEFT,  VK_NUMPAD4)) { dx -= rt.x;  dz -= rt.z;  }
    if (FlyKey(VK_PRIOR, VK_NUMPAD9)) dy += 1.0f;
    if (FlyKey(VK_NEXT,  VK_NUMPAD3)) dy -= 1.0f;

    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 0.0001f) return;

    float step = g_flySpeed * FrameDelta() / len;
    dx *= step; dy *= step; dz *= step;

    Vec3 np;
    np.x = cur.x + dx; np.y = cur.y + dy; np.z = cur.z + dz;

    if (m_FPM_SetPosUs) {
        void* a[1]; a[0] = &np;
        Invoke(m_FPM_SetPosUs, pl, a, "SetPositionOfUsOrMount");
    }
    TfSetPos(tf, &np);
}

static void DoFlyTick(void)
{
    __try { DoFlyTickBody(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_flyOn, 0);
        Log("!! 飞天: 访问违例 code=%08X, 已自动关闭 (防止每帧崩)", GetExceptionCode());
    }
}

static BOOL GrabGravity(MonoObject* pl)
{
    if (!pl || !f_GravityMult) return FALSE;
    if (!g_flyGravSaved) {
        float g = 1.0f;
        p_field_get_value(pl, f_GravityMult, &g);
        if (g <= 0.001f) g = 1.0f;
        g_flyGravOrig = g;
        g_flyGravSaved = 1;
        Log("  记录原重力 = %.2f", g);
    }
    float zero = 0.0f;
    p_field_set_value(pl, f_GravityMult, &zero);
    return TRUE;
}

static void ReleaseGravityIfIdle(void)
{
    if (g_flyOn || g_wallOn) return;
    MonoObject* pl = TryGetPlayer();
    if (!pl || !f_GravityMult || !g_flyGravSaved) return;
    p_field_set_value(pl, f_GravityMult, &g_flyGravOrig);
    g_flyGravSaved = 0;
    Log("重力已还原为 %.2f", g_flyGravOrig);
}

static void ActFly(int on)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 飞天: 找不到玩家 (要在关卡里)"); return; }

    MonoClass* kc = p_object_get_class(pl);
    if (!m_FPM_SetMoveSim) m_FPM_SetMoveSim = FindMethod(kc, "SetMovementSimulationDisabled", 1);
    if (!m_Chr_SetVel)     m_Chr_SetVel     = FindMethod(kc, "SetVelocity", 1);

    if (on) {
        if (!GrabGravity(pl)) { Log("!! GravityMultiplier 字段找不到"); return; }

        if (m_FPM_SetMoveSim) {
            int t = 1;
            void* a[1]; a[0] = &t;
            Invoke(m_FPM_SetMoveSim, pl, a, "SetMovementSimulationDisabled(true)");
            g_moveSimHeld = 1;
        } else {
            Log("   ⚠ 没找到 SetMovementSimulationDisabled —— 位置可能被游戏覆盖");
        }

        if (m_Chr_SetVel) {
            Vec3 zero; zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
            void* a[1]; a[0] = &zero;
            Invoke(m_Chr_SetVel, pl, a, "SetVelocity(0)");
        }
        Log("飞天 = 1   [方向键 或 小键盘 8/2/4/6 = 前后左右;  PageUp/PageDown 或 小键盘 9/3 = 升降]");
    } else {
        if (m_FPM_SetMoveSim && g_moveSimHeld) {
            int f = 0;
            void* a[1]; a[0] = &f;
            Invoke(m_FPM_SetMoveSim, pl, a, "SetMovementSimulationDisabled(false)");
            g_moveSimHeld = 0;
        }
        Log("飞天 = 0");
        ReleaseGravityIfIdle();
    }
}

static void ActWallHack(int on)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 穿墙: 找不到玩家 (要在关卡里)"); return; }

    if (!f_CharCtrl)    { Log("!! _characterController 字段找不到"); return; }
    MonoObject* cc = NULL;
    p_field_get_value(pl, f_CharCtrl, &cc);
    if (!cc) { Log("!! CharacterController 是 null"); return; }

    MonoClass* kcc = p_object_get_class(cc);
    if (!m_SetDetectCol) m_SetDetectCol = FindMethod(kcc, "set_detectCollisions", 1);
    if (!f_DetectCol)    f_DetectCol    = FindField(kcc, "m_DetectCollisions");

    int v = on ? 0 : 1;

    if (m_SetDetectCol) {
        void* a[1]; a[0] = &v;
        Invoke(m_SetDetectCol, cc, a, "set_detectCollisions");
    } else if (f_DetectCol) {
        p_field_set_value(cc, f_DetectCol, &v);
    } else {
        Log("!! 穿墙失败: set_detectCollisions 和 m_DetectCollisions 都找不到");
        return;
    }

    if (on) {
        Vec3 cur;
        MonoObject* tf = PlayerTf(pl);
        if (TfGetPos(tf, &cur)) { g_wallY = cur.y; g_wallSaved = 1; }
        GrabGravity(pl);
        Log("穿墙 = 1   高度锁定在 y=%.2f, 只在下落时拉回", g_wallY);
    } else {
        g_wallSaved = 0;
        Log("穿墙 = 0   碰撞已恢复");
        ReleaseGravityIfIdle();
    }
}

static void DoWallTick(void)
{
    if (!g_wallSaved) return;

    MonoObject* pl = PlayerFPM();
    if (!pl) return;

    if (f_GravityMult) { float z = 0.0f; p_field_set_value(pl, f_GravityMult, &z); }

    if (g_flyOn) return;

    MonoObject* tf = PlayerTf(pl);
    if (!tf) return;

    Vec3 cur;
    if (!TfGetPos(tf, &cur)) return;
    if (cur.y >= g_wallY - 0.05f) return;

    Vec3 np; np.x = cur.x; np.y = g_wallY; np.z = cur.z;
    TfSetPos(tf, &np);
}

static void ActSpeedMul(int pct)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 速度: 找不到玩家"); return; }
    if (!m_SetMoveSpeed) { Log("!! setMoveSpeedMultiplier 找不到"); return; }

    float v = pct / 100.0f;
    if (v < 0.1f) v = 0.1f;
    if (v > 20.0f) v = 20.0f;
    g_speedMul = v;
    void* a[1]; a[0] = &v;
    Invoke(m_SetMoveSpeed, pl, a, "setMoveSpeedMultiplier");
    Log("移动速度倍率 = %.2fx", v);
}

static void ActJumpMul(int pct)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 跳跃: 找不到玩家"); return; }
    if (!f_JumpVel) { Log("!! JumpVelocity 字段找不到"); return; }

    Vec3 base;
    p_field_get_value(pl, f_JumpVel, &base);
    if (!g_jumpSaved) { g_jumpBase = base; g_jumpSaved = 1; }

    float m = pct / 100.0f;
    if (m < 0.1f) m = 0.1f;
    if (m > 20.0f) m = 20.0f;
    g_jumpMul = m;

    Vec3 v;
    v.x = g_jumpBase.x * m; v.y = g_jumpBase.y * m; v.z = g_jumpBase.z * m;
    p_field_set_value(pl, f_JumpVel, &v);
    Log("跳跃力倍率 = %.2fx   (%.2f -> %.2f)", m, g_jumpBase.y, v.y);
}

static MonoClass*      c_WeaponModel  = NULL;
static MonoClass*      c_EmoteManager = NULL;
static MonoMethod*     m_TransToTgt   = NULL;
static MonoMethod*     m_ClosestEnemy = NULL;
static MonoMethod*     m_DropWeapon   = NULL;
static MonoMethod*     m_StopEmote    = NULL;
static MonoMethod*     m_GetAllEmotes = NULL;
static MonoMethod*     m_GetEmoteIdx  = NULL;
static MonoMethod*     m_SetActEmote  = NULL;
static MonoMethod*     m_StartRiding  = NULL;
static MonoMethod*     m_StopRiding   = NULL;
static MonoClassField* f_CharModelRef = NULL;
static MonoClassField* f_WeaponModels = NULL;
static MonoClassField* f_CurWeaponMdl = NULL;
static MonoClassField* f_SecWeaponMdl = NULL;
static MonoClassField* f_IsPlayingWin = NULL;
static MonoClassField* f_EmoteID      = NULL;
static MonoClassField* f_CurRiding    = NULL;
static MonoClassField* f_AttackSpeed  = NULL;
static MonoClassField* f_MinHeadVox   = NULL;
static MonoClassField* f_KillOnHead   = NULL;
static MonoClassField* f_ImmuneToDmg  = NULL;

static const char* MonoStrToUtf8(void* s, char* buf, int cap)
{
    buf[0] = 0;
    if (!s || !p_string_to_utf8 || cap < 2) return buf;
    char* u = p_string_to_utf8(s);
    if (!u) return buf;
    _snprintf_s(buf, cap, _TRUNCATE, "%s", u);
    if (p_free) p_free(u);
    return buf;
}

static int DropWeaponsOf(MonoObject* fpm)
{
    if (!fpm) return 0;
    MonoClass* k = p_object_get_class(fpm);
    if (!f_CharModelRef) f_CharModelRef = FindField(k, "_characterModel");
    if (!f_CurWeaponMdl) f_CurWeaponMdl = FindField(k, "_currentWeaponModel");
    if (!f_SecWeaponMdl) f_SecWeaponMdl = FindField(k, "_secondaryWeaponModel");

    if (!c_WeaponModel) c_WeaponModel = Cls("", "WeaponModel");
    if (c_WeaponModel && !m_DropWeapon) m_DropWeapon = FindMethod(c_WeaponModel, "DropWeapon", 0);
    if (!m_DropWeapon) return 0;

    int n = 0;
    MonoObject* cm = NULL;
    if (f_CharModelRef) p_field_get_value(fpm, f_CharModelRef, &cm);
    if (cm) {
        MonoClass* kc = p_object_get_class(cm);
        if (!f_WeaponModels) f_WeaponModels = FindField(kc, "WeaponModels");
        void* arr = NULL;
        if (f_WeaponModels) p_field_get_value(cm, f_WeaponModels, &arr);
        if (arr && p_array_length && p_array_addr_with_size) {
            uintptr_t len = p_array_length(arr);
            char* base = p_array_addr_with_size(arr, (int)sizeof(void*), 0);
            for (uintptr_t i = 0; base && i < len && i < 64; i++) {
                MonoObject* wm = *(MonoObject**)(base + i * sizeof(void*));
                if (!wm) continue;
                Invoke(m_DropWeapon, wm, NULL, "WeaponModel.DropWeapon");
                n++;
            }
        }
    }

    if (n == 0) {
        MonoObject* fs[2] = { NULL, NULL };
        if (f_CurWeaponMdl) p_field_get_value(fpm, f_CurWeaponMdl, &fs[0]);
        if (f_SecWeaponMdl) p_field_get_value(fpm, f_SecWeaponMdl, &fs[1]);
        for (int i = 0; i < 2; i++) {
            if (!fs[i]) continue;
            Invoke(m_DropWeapon, fs[i], NULL, "WeaponModel.DropWeapon");
            n++;
        }
    }
    return n;
}

static void ActDisarmEnemies(void)
{
    if (!c_Character) { Log("!! 缴械: Character 类没准备好"); return; }
    MonoObject* list = GetLivingList();
    int ln = ListCount(list), hit = 0, wm = 0;
    for (int i = 0; i < ln; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || SkipAlly(c)) continue;
        wm += DropWeaponsOf(c);
        hit++;
    }
    if (hit == 0) { Log("!! 缴械: 场上没有敌人"); return; }
    Log("敌人缴械 -> %d 个敌人, 丢掉 %d 件武器", hit, wm);
}

static MonoObject* CharTf(MonoObject* c)
{
    if (!c) return NULL;
    MonoMethod* m = FindMethod(p_object_get_class(c), "get_transform", 0);
    if (!m) return NULL;
    MonoObject* tf = Invoke(m, c, NULL, "get_transform");

    if (tf && !m_TfGetPos) {
        MonoClass* kt = p_object_get_class(tf);
        m_TfGetPos = FindMethod(kt, "get_position", 0);
    }
    return tf;
}

static MonoObject* CharFPM(MonoObject* c);

static int IsUnityDead(MonoObject* o);

static MonoObject* PickNearestChar(MonoObject* player, int wantEnemy,
                                   float* outDist, int* outScan, int needFpm)
{
    if (outDist) *outDist = -1.0f;
    if (outScan) *outScan = 0;

    MonoObject* list = GetLivingList();
    int n = ListCount(list);
    if (n <= 0) return NULL;

    MonoObject* tf = PlayerTf(player);
    Vec3 pp; pp.x = pp.y = pp.z = 0.0f;
    int havePos = (tf && TfGetPos(tf, &pp));

    MonoObject* best = NULL;
    float bestD = 1e30f;
    int scan = 0;

    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || c == player) continue;
        if (IsUnityDead(c)) continue;
        if (wantEnemy && SkipAlly(c)) continue;

        MonoClassField* fm = FindField(p_object_get_class(c), "_characterModel");
        MonoObject* cm = NULL;
        if (fm) p_field_get_value(c, fm, &cm);
        if (!cm || IsUnityDead(cm)) continue;

        if (needFpm && !CharFPM(c)) continue;

        scan++;
        float d = 0.0f;
        if (havePos) {
            MonoObject* otf = CharTf(c);
            Vec3 op;
            if (otf && TfGetPos(otf, &op)) {
                float dx = op.x - pp.x, dy = op.y - pp.y, dz = op.z - pp.z;
                d = sqrtf(dx * dx + dy * dy + dz * dz);
            }
        }
        if (d < bestD) { bestD = d; best = c; }
    }

    if (outScan) *outScan = scan;
    if (!best) return NULL;
    if (outDist) *outDist = havePos ? bestD : 0.0f;
    return best;
}

static void TopUpTransfers(void)
{
    if (!c_GameData) c_GameData = Cls("", "GameDataManager");
    if (c_GameData && !m_GDM_SetTransfers)
        m_GDM_SetTransfers = FindMethod(c_GameData, "SetNumConsciousnessTransfersLeft", 1);
    if (!m_GDM_SetTransfers) { Log("  (意识次数接口没找到, 跳过)"); return; }
    int n = 999;
    void* a[1]; a[0] = &n;
    Invoke(m_GDM_SetTransfers, NULL, a, "SetNumConsciousnessTransfersLeft");
    Log("  意识转移次数 -> %d", n);
}

static DWORD       g_possessTick    = 0;
static int         g_possessWatch   = 0;
static MonoObject* g_possessOldBody = NULL;

static MonoClass*  c_TimeMgr   = NULL;
static MonoMethod* m_GetBaseTS = NULL;
static MonoMethod* m_SetBaseTS = NULL;

static void ActPossessEnemy(void)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 夺舍: 找不到玩家 (要先在关卡里)"); return; }

    TopUpTransfers();

    float d = -1.0f; int scan = 0;
    MonoObject* tgtF = NULL;
    int fromOfficial = 0;

    MonoObject* c = PickNearestChar(pl, 1, &d, &scan, 1);
    Log("  候选敌人(有 FPM 的) %d 个", scan);
    if (c) tgtF = CharFPM(c);

    if (!tgtF) {

        MonoObject* tr = GetTracker();
        if (tr) {
            if (!m_ClosestEnemy)
                m_ClosestEnemy = FindMethod(p_object_get_class(tr),
                                            "GetClosestLivingEnemyMindTransferTarget", 1);
            MonoObject* tf = PlayerTf(pl);
            Vec3 pos;
            if (m_ClosestEnemy && tf && TfGetPos(tf, &pos)) {
                void* a1[1]; a1[0] = &pos;
                tgtF = Invoke(m_ClosestEnemy, tr, a1, "GetClosestLivingEnemyMindTransferTarget");
                if (tgtF) { fromOfficial = 1; Log("  (官方接口给了一个目标)"); }
            }
        }
    }
    if (!tgtF) { Log("!! 夺舍: 场上没有可夺舍的敌人"); return; }
    if (d >= 0.0f && !fromOfficial) Log("  目标距离 %.1f 米", d);

    Log("夺舍 -> 目标 [%s]  执行原子步骤 (跳过演出/剧情冻结)", PickNameOf(c));

    SafeCall(tgtF, "MatchSpeedAndUpgradesToPlayerAfterTransfer", 0, NULL, "同步升级/速度");

    {
        int one = 1;
        void* a1[1]; a1[0] = &one;
        SafeCall(pl, "SetToNotBeMainPlayer", 1, a1, "旧身体卸任主玩家");
    }

    SafeCall(tgtF, "SetToBeMainPlayer", 0, NULL, "新身体就任主玩家");

    {
        MonoObject* tf = CharTf(tgtF);
        if (tf) {
            MonoObject* nul = NULL;
            int w = 1;
            void* a2[2]; a2[0] = &nul; a2[1] = &w;
            SafeCall(tf, "SetParent", 2, a2, "新身体 SetParent(null)");
        }
    }

    SafeCall(tgtF, "OnMindTransferFinished", 0, NULL, "夺舍收尾");

    SafeCall(pl, "EnableAI", 0, NULL, "旧身体交给 AI");

    {
        MonoObject* tr2 = GetTracker();
        if (tr2) {
            if (!m_TrackerSetPl) m_TrackerSetPl = FindMethod(p_object_get_class(tr2), "SetPlayer", 1);
            if (m_TrackerSetPl) {
                void* a3[1]; a3[0] = &tgtF;
                SafeCall(tr2, "SetPlayer", 1, a3, "修补 CharacterTracker.SetPlayer(新身体)");
            }
        }
        {
            int one = 1;
            void* a4[1]; a4[0] = &one;
            SafeCall(tgtF, "EnableMainPlayerInput", 1, a4, "修补 打开玩家输入");
        }

        {
            if (!c_TimeMgr) c_TimeMgr = Cls("", "TimeManager");
            if (c_TimeMgr) {
                const char* e2 = "?";
                MonoObject* tm2 = GetSingleton(c_TimeMgr, &e2);
                if (tm2) {
                    float one = 1.0f;
                    void* a5[1]; a5[0] = &one;
                    SafeCall(tm2, "SetBaseTimeScale", 1, a5, "修补 主时标=1");
                }
            }
        }
    }

    {
        MonoObject* now = GetPlayerCharacter();
        if (now) Log("夺舍 -> 完成。当前玩家 = [%s], 旧身体已交给 AI。",
                     TypeNameCN(CharTypeId(now)));
        else     Log("!! 夺舍: 修补后 CharacterTracker 里仍没有玩家 —— 大概率该关禁用了意识转移, 别在这关用。");
    }

    g_possessOldBody = pl;
    g_possessTick    = GetTickCount();
    g_possessWatch   = 1;
}

static void ActForceTransferNow(void)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 意识转移: 找不到玩家 (要先在关卡里)"); return; }

    TopUpTransfers();

    MonoObject* tgt = NULL;
    MonoObject* tr  = GetTracker();
    if (tr) {
        if (!m_TrkGetAlly)
            m_TrkGetAlly = FindMethod(p_object_get_class(tr), "GetPlayerAlly", 0);
        if (m_TrkGetAlly) {
            MonoObject* al = Invoke(m_TrkGetAlly, tr, NULL, "GetPlayerAlly");
            if (al && !IsUnityDead(al)) {
                int alive = UnboxBool(SafeCall(al, "IsAlive", 0, NULL, "目标 IsAlive"), 0);
                if (alive) tgt = CharFPM(al);
            }
        }
    }

    if (!tgt) {
        float d = -1.0f; int scan = 0;
        MonoObject* c = PickNearestChar(pl, 1, &d, &scan, 1);
        if (c) {
            tgt = CharFPM(c);
            if (tgt) Log("  (场上没有盟友, 取最近的敌人: %.1f 米)", d);
        }
    }
    if (!tgt) { Log("!! 意识转移: 场上没有可转移的目标"); return; }

    Log("意识转移 -> 目标 [%s]", PickNameOf(tgt));

    Log("  心智转移(原子步骤) 开始");

    SafeCall(tgt, "MatchSpeedAndUpgradesToPlayerAfterTransfer", 0, NULL, "①同步速度与升级");
    Log("    [CT] ① ok");

    { int one = 1; void* a1[1]; a1[0] = &one;
      SafeCall(pl, "SetToNotBeMainPlayer", 1, a1, "②旧身体卸任"); }
    Log("    [CT] ② ok  (此刻 CharacterTracker._player = null)");

    SafeCall(tgt, "SetToBeMainPlayer", 0, NULL, "③新身体就任");
    Log("    [CT] ③ ok");

    {
        MonoObject* tr3 = GetTracker();
        if (tr3) {
            MonoClass* kt = p_object_get_class(tr3);
            if (!f_TrkPlayer)    f_TrkPlayer    = FindField(kt, "_player");
            if (!f_TrkPlayerFPM) f_TrkPlayerFPM = FindField(kt, "_playerFirstPersonMover");
            if (f_TrkPlayer)    p_field_set_value(tr3, f_TrkPlayer,    &tgt);
            if (f_TrkPlayerFPM) p_field_set_value(tr3, f_TrkPlayerFPM, &tgt);
            Log("    [CT] ③b 字段直写: _player=%s  _playerFirstPersonMover=%s",
                f_TrkPlayer ? "ok" : "!!没找到", f_TrkPlayerFPM ? "ok" : "!!没找到");
        } else Log("    [CT] ③b !! CharacterTracker 拿不到");
    }

    {
        if (!f_IsNeutral)    f_IsNeutral    = FindField(c_Character, "IsNeutral");
        if (!f_IsPlayerTeam) f_IsPlayerTeam = FindField(c_Character, "IsPlayerTeam");
        if (f_IsNeutral)    { int z = 0; p_field_set_value(tgt, f_IsNeutral,    &z); }
        if (f_IsPlayerTeam) { int o = 1; p_field_set_value(tgt, f_IsPlayerTeam, &o); }
        Log("    [CT] ③c 阵营: IsPlayerTeam=1 IsNeutral=0 (%s)",
            (f_IsNeutral && f_IsPlayerTeam) ? "ok" : "!!字段没找全");
    }

    { MonoObject* tf = CharTf(tgt);
      if (tf) { MonoObject* nul = NULL; int w = 1; void* a2[2]; a2[0] = &nul; a2[1] = &w;
                SafeCall(tf, "SetParent", 2, a2, "④新身体脱离父级"); } }
    Log("    [CT] ④ ok");

    SafeCall(tgt, "OnMindTransferFinished", 0, NULL, "⑤收尾(接管输入/恢复旋转/免疫一秒)");
    Log("    [CT] ⑤ ok");

    SafeCall(pl, "EnableAI", 0, NULL, "⑥旧身体交给 AI");
    Log("    [CT] ⑥ ok");

    { MonoObject* tr2 = GetTracker();
      if (tr2) {
          if (!m_TrackerSetPl) m_TrackerSetPl = FindMethod(p_object_get_class(tr2), "SetPlayer", 1);
          if (m_TrackerSetPl) { void* a3[1]; a3[0] = &tgt;
              SafeCall(tr2, "SetPlayer", 1, a3, "⑦登记新玩家(官方接口)"); }
      }
    }
    Log("    [CT] ⑦ ok");

    {
        MonoObject* gd = NULL;
        if (!c_GameData) c_GameData = ClsHard("GameDataManager");
        if (c_GameData) { const char* e = "?"; gd = GetSingleton(c_GameData, &e); }
        if (gd) {
            if (!m_UpdArmorData) m_UpdArmorData = FindMethod(p_object_get_class(gd), "UpdatePlayerArmorData", 1);
            if (!f_ArmorPieces)  f_ArmorPieces  = FindField(p_object_get_class(tgt), "_armorPieces");
            if (m_UpdArmorData && f_ArmorPieces) {
                MonoObject* ap = NULL;
                p_field_get_value(tgt, f_ArmorPieces, &ap);
                void* a4[1]; a4[0] = &ap;
                SafeCall(gd, "UpdatePlayerArmorData", 1, a4, "⑧同步护甲数据");
            }
        }
    }
    Log("  心智转移(原子步骤) 结束");

    g_possessOldBody = pl;
    g_possessTick    = GetTickCount();
    g_possessWatch   = 1;

    {
        MonoObject* now = GetPlayerCharacter();
        if (now) {
            Log("意识转移 -> 完成。当前玩家 = [%s]", TypeNameCN(CharTypeId(now)));
        } else {

            Log("!! 意识转移: 新身体登记失败 -> 立即回滚旧身体");
            {
                MonoObject* trr = GetTracker();
                if (trr) {
                    MonoClass* kt = p_object_get_class(trr);
                    if (!f_TrkPlayer)    f_TrkPlayer    = FindField(kt, "_player");
                    if (!f_TrkPlayerFPM) f_TrkPlayerFPM = FindField(kt, "_playerFirstPersonMover");
                    if (f_TrkPlayer)    p_field_set_value(trr, f_TrkPlayer,    &pl);
                    if (f_TrkPlayerFPM) p_field_set_value(trr, f_TrkPlayerFPM, &pl);
                }
            }
            SafeCall(pl, "SetToBeMainPlayer", 0, NULL, "回滚: 旧身体复位");
            {
                int one = 1; void* a5[1]; a5[0] = &one;
                SafeCall(pl, "EnableMainPlayerInput", 1, a5, "回滚: 恢复输入");
            }
            g_possessWatch = 0;
            {
                MonoObject* bk = GetPlayerCharacter();
                if (bk) Log("意识转移: 已回滚, 玩家 = [%s]  (这次没转移成功, 但没卡死)",
                            TypeNameCN(CharTypeId(bk)));
                else    Log("!! 意识转移: 回滚后仍然没有玩家 —— 按 M 回主菜单重进一次关卡");
            }
        }
    }
}

static void ActCtForce(int on)
{
    InterlockedExchange(&g_ctForce, on ? 1 : 0);
    if (on) { g_ctNextPoll = 0; s_ctProbed = 0; s_ctDiag = -1;
              Log("斗兽场强制意识转移 = 开  (第1/2章: 按 H 键钻身体)"); }
    else    { Log("斗兽场强制意识转移 = 关"); }
}

static void CtDiag(int code, const char* msg)
{
    if (code == s_ctDiag) return;
    s_ctDiag = code;
    if (msg) Log("斗兽场意识转移: %s", msg);
}

static void DoCtTick(void)
{
    DWORD now = GetTickCount();
    if (now < g_ctNextPoll) return;
    g_ctNextPoll = now + 80;

    if (!s_ctProbed) {
        s_ctProbed = 1;
        c_GameMode = ClsHard("GameModeManager");
        if (c_GameMode) {
            m_GM_IsC3   = FindMethod(c_GameMode, "IsStoryChapter3", 0);
            m_GM_ShowC4 = FindMethod(c_GameMode, "ShouldShowChapter4TransferMessage", 0);
            Log("斗兽场意识转移: GameModeManager ok (IsStoryChapter3 %s)",
                m_GM_IsC3 ? "ok" : "!! 没找到");
        } else {
            Log("!! 斗兽场意识转移: 找不到 GameModeManager");
        }
    }

    {
        static int s_hPrev = 0;
        int hn = (GetAsyncKeyState('H') & 0x8000) ? 1 : 0;
        if (hn && !s_hPrev) {
            s_hPrev = hn;
            CtDiag(9, "检测到 H 键 -> 执行意识转移");
            ActForceTransferNow();
            return;
        }
        s_hPrev = hn;
    }

    if (m_GM_IsC3) {
        MonoObject* b = Invoke(m_GM_IsC3, NULL, NULL, "IsStoryChapter3");
        if (b && UnboxBool(b, 0)) { CtDiag(5, "当前是第3章 (原生本来就能转移, 本功能自动让位)"); return; }
    }

    MonoObject* pl = PlayerFPM();
    if (!pl) { CtDiag(1, "不在关卡里, 等玩家出现"); return; }

    if (!f_MoveCmd) f_MoveCmd = FindField(p_object_get_class(pl), "_moveCommandInput");
    if (!f_MoveCmd) { CtDiag(2, "!! 玩家身上找不到字段 _moveCommandInput"); return; }

    MonoObject* mi = NULL;
    p_field_get_value(pl, f_MoveCmd, &mi);
    if (!mi || IsUnityDead(mi)) { CtDiag(3, "输入对象为空 (换关卡/死亡瞬间), 稍后自动重试"); return; }

    if (!m_CtDown) {
        MonoClass* km = p_object_get_class(mi);
        if (!m_CtDown) m_CtDown = FindMethod(km, "get_TransferConsciousnessDown", 0);
        if (!m_CtDown) m_CtDown = FindMethodSub(km, "TransferConsciousnessDown", 0);
        if (m_CtDown) {
            const char* mn = p_method_get_name(m_CtDown);
            __try { Log("斗兽场意识转移: 读键 ok (%s :: %s)", p_class_get_name(km), mn ? mn : "?"); }
            __except (EXCEPTION_EXECUTE_HANDLER) { Log("斗兽场意识转移: 读键 ok (类名解析失败)"); }
        }
    }
    if (!m_CtDown) { CtDiag(4, "!! 输入对象上没有 TransferConsciousnessDown 这个 getter"); return; }

    MonoObject* dn = Invoke(m_CtDown, mi, NULL, "get_TransferConsciousnessDown");

    {
        static int s_dnPrev = 0;
        int dnNow = (dn && UnboxBool(dn, 0)) ? 1 : 0;
        if (dnNow && !s_dnPrev) {
            s_dnPrev = dnNow;
            CtDiag(9, "检测到游戏原生转移键 -> 执行意识转移");
            ActForceTransferNow();
            return;
        }
        s_dnPrev = dnNow;
        if (!dnNow) CtDiag(0, "一切就绪: 按 H 键即可转移 (第1/2章斗兽场)");
    }
}

static void DoPossessWatchdog(void)
{
    if (!g_possessWatch) return;
    if (GetTickCount() - g_possessTick < 3000) return;
    g_possessWatch = 0;

    MonoObject* now = GetPlayerCharacter();
    if (!now) {
        MonoObject* back = g_possessOldBody;
        Log("[看门狗] 夺舍 3 秒后 CharacterTracker 里没有玩家 -> %s",
            back ? "把旧身体还给你" : "没有可复位的旧身体");
        if (back) {
            SafeCall(back, "SetToBeMainPlayer", 0, NULL, "看门狗 旧身体复位");
            MonoObject* tr = GetTracker();
            if (tr) {
                if (!m_TrackerSetPl) m_TrackerSetPl = FindMethod(p_object_get_class(tr), "SetPlayer", 1);
                if (m_TrackerSetPl) {
                    void* a3[1]; a3[0] = &back;
                    SafeCall(tr, "SetPlayer", 1, a3, "看门狗 SetPlayer(旧身体)");
                }
            }
            {
                int one = 1; void* a4[1]; a4[0] = &one;
                SafeCall(back, "EnableMainPlayerInput", 1, a4, "看门狗 打开输入");
            }
            Log("[看门狗] 已复位 -> 你还是原来的身体 (这次夺舍没成功, 但游戏没卡)");
        }
    } else {
        Log("[看门狗] 夺舍完成, 玩家在线 = [%s]", TypeNameCN(CharTypeId(now)));
    }
    g_possessOldBody = NULL;

    if (!c_TimeMgr) c_TimeMgr = Cls("", "TimeManager");
    if (!c_TimeMgr) return;
    const char* err = "?";
    MonoObject* tm = GetSingleton(c_TimeMgr, &err);
    if (!tm) return;

    if (!m_GetBaseTS) m_GetBaseTS = FindMethod(p_object_get_class(tm), "GetBaseTimeScale", 0);
    if (!m_SetBaseTS) m_SetBaseTS = FindMethod(p_object_get_class(tm), "SetBaseTimeScale", 1);
    if (!m_GetBaseTS || !m_SetBaseTS) return;

    MonoObject* r = Invoke(m_GetBaseTS, tm, NULL, "GetBaseTimeScale");
    float bts = 1.0f;
    if (r) { float* pf = (float*)p_object_unbox(r); if (pf) bts = *pf; }

    if (bts < 0.01f) {
        float one = 1.0f;
        void* a[1]; a[0] = &one;
        Invoke(m_SetBaseTS, tm, a, "SetBaseTimeScale");
        Log("[看门狗] 主时标卡在 0, 已强制恢复为 1");
    }
}

#define JQR_MAX_EMOTES 64
static char g_emoteIDs[JQR_MAX_EMOTES][64];
static int  g_emoteIndex[JQR_MAX_EMOTES];
static int  g_emoteCount  = 0;
static int  g_emoteCursor = 0;
static int  g_emoteTried  = 0;

static void LoadEmotes(void)
{
    if (g_emoteCount > 0) return;
    g_emoteTried++;
    if (g_emoteTried > 20) return;

    if (!c_EmoteManager) c_EmoteManager = Cls("", "EmoteManager");
    if (!c_EmoteManager) { Log("  (EmoteManager 类找不到)"); return; }

    const char* err = "?";
    MonoObject* em = GetSingleton(c_EmoteManager, &err);
    if (!em) { Log("  (EmoteManager 实例拿不到: %s)", err); return; }

    if (!m_GetAllEmotes) m_GetAllEmotes = FindMethod(p_object_get_class(em), "GetAllEmotes", 0);
    MonoObject* list = m_GetAllEmotes ? Invoke(m_GetAllEmotes, em, NULL, "GetAllEmotes") : NULL;
    if (!list) { Log("  (GetAllEmotes 拿不到列表)"); return; }

    int n = ListCount(list);
    Log("  EmoteManager.GetAllEmotes() -> %d 条", n);

    if (!m_GetEmoteIdx) m_GetEmoteIdx = FindMethod(p_object_get_class(em), "GetEmoteIndex", 1);

    for (int i = 0; i < n && g_emoteCount < JQR_MAX_EMOTES; i++) {
        MonoObject* def = ListItem(list, i);
        if (!def) continue;
        if (!f_EmoteID) f_EmoteID = FindField(p_object_get_class(def), "EmoteID");
        if (!f_EmoteID) { Log("  (EmoteID 字段找不到)"); break; }

        void* s = NULL;
        p_field_get_value(def, f_EmoteID, &s);
        if (!s) continue;
        char buf[64];
        MonoStrToUtf8(s, buf, sizeof(buf));
        if (!buf[0]) continue;

        int idx = -1;
        if (m_GetEmoteIdx) {
            void* a[1]; a[0] = &def;
            MonoObject* r = Invoke(m_GetEmoteIdx, em, a, "GetEmoteIndex");
            if (r) { int* p = (int*)p_object_unbox(r); if (p) idx = *p; }
        }
        if (idx < 0) continue;

        _snprintf_s(g_emoteIDs[g_emoteCount], 64, _TRUNCATE, "%s", buf);
        g_emoteIndex[g_emoteCount] = idx;
        g_emoteCount++;
    }
    if (g_emoteCount > 0) Log("胜利动作: 拿到 %d 个官方表情", g_emoteCount);
    else                  Log("胜利动作: 表情表是空的 (该关卡可能禁用了表情)");
}

static void ActVictoryPose(void)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 胜利动作: 找不到玩家 (要先在关卡里)"); return; }

    LoadEmotes();
    if (PoseCount() <= 0) PoseLoadNames();
    int n = PoseCount();
    if (n <= 0) { Log("!! 胜利动作: 拿不到表情表 (先进关卡再点)"); return; }

    int k = g_emoteCursor % n;
    g_emoteCursor++;
    Log("胜利动作 -> 第 %d/%d 个 [%s]", k + 1, n, PoseNameAt(k));

    ActPoseSet(k + 1);

    if (g_emoteCount > 0 && m_SetActEmote) {
        int idx = g_emoteIndex[k % g_emoteCount];
        void* a[1]; a[0] = &idx;
        Invoke(m_SetActEmote, pl, a, "SetActiveEmoteIndex");
    }
}

static void ActStopPose(void)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 停止动作: 找不到玩家"); return; }
    PoseStop();
    if (!m_StopEmote) m_StopEmote = FindMethod(p_object_get_class(pl), "stopCurrentEmote", 0);
    if (m_StopEmote) Invoke(m_StopEmote, pl, NULL, "stopCurrentEmote");
    if (!f_IsPlayingWin) f_IsPlayingWin = FindField(p_object_get_class(pl), "_isPlayingVictoryAnimation");
    if (f_IsPlayingWin) { int z = 0; p_field_set_value(pl, f_IsPlayingWin, &z); }
    Log("动作已停止");
}

static void ActDismount(void)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 下车: 找不到玩家"); return; }
    if (!m_StopRiding) m_StopRiding = FindMethod(p_object_get_class(pl), "StopRiding", 0);
    if (!m_StopRiding) { Log("!! 下车: StopRiding 没找到"); return; }
    Invoke(m_StopRiding, pl, NULL, "StopRiding");
    Log("下车 -> 已下来 (会继承坐骑一半速度)");
}

static void ActRideNearest(void)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 骑乘: 找不到玩家 (要先在关卡里)"); return; }

    if (!f_CurRiding) f_CurRiding = FindField(p_object_get_class(pl), "_characterWeAreRiding");
    if (f_CurRiding) {
        MonoObject* cur = NULL;
        p_field_get_value(pl, f_CurRiding, &cur);
        if (cur) { Log("  检测到已经骑在某角色身上, 先下车"); ActDismount(); }
    }

    float d = -1.0f; int scan = 0;
    MonoObject* c = PickNearestChar(pl, 0, &d, &scan, 1);
    if (!c) { Log("!! 骑乘: 场上没有可骑的角色 (候选 %d 个, 只挑有 FirstPersonMover 的)", scan); return; }

    MonoObject* tgt = CharFPM(c);
    if (!tgt) { Log("!! 骑乘: 选中的角色拿不到 FirstPersonMover 组件"); return; }

    {
        MonoObject* mdl = SafeCall(tgt, "GetCharacterModel", 0, NULL, "坐骑 GetCharacterModel");
        if (!mdl) {
            Log("!! 骑乘: 这个角色没有 CharacterModel (刚生成/已被拆), 换一个目标再试");
            return;
        }
    }
    Log("  锁定坐骑: 距离 %.1f 米 (候选 %d 个)", d, scan);

    if (!m_StartRiding) m_StartRiding = FindMethod(p_object_get_class(pl), "StartRiding", 1);
    if (!m_StartRiding) { Log("!! 骑乘: StartRiding 没找到"); return; }

    void* a[1]; a[0] = &tgt;
    Invoke(m_StartRiding, pl, a, "StartRiding");
    Log("骑乘 -> 已骑上 (目标当场变友军, 骑乘期间双方互免武器伤害)");
    Log("  想下来就点「下车」");
}

static void ActOneShotEnemies(int on)
{
    MonoObject* list = GetLivingList();
    int n = ListCount(list), hit = 0, skip = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || SkipAlly(c)) continue;

        MonoObject* fpm = CharFPM(c);
        if (!fpm) { skip++; continue; }

        MonoClass* k = p_object_get_class(fpm);
        if (!f_MinHeadVox) f_MinHeadVox = FindField(k, "MinHeadVoxelsDamagedForDeath");
        if (!f_MinHeadVox) { skip++; continue; }
        if (!f_KillOnHead) f_KillOnHead = FindField(k, "KillOnHeadDamage");

        int v = on ? 1 : 5;
        p_field_set_value(fpm, f_MinHeadVox, &v);
        if (f_KillOnHead) { int x = on ? 1 : 0; p_field_set_value(fpm, f_KillOnHead, &x); }
        hit++;
    }
    if (hit == 0) {
        Log("!! 一击必杀: 场上没有可改的敌人%s", skip ? " (其余都是非人形单位)" : "");
        return;
    }
    Log("敌人一击必杀 = %d   (改了 %d 个敌人%s)", on, hit, skip ? ", 另有非人形单位已跳过" : "");
    if (!on) Log("  注: 关闭只是把阈值放宽到 5, 不保证与原值一致");
}

static void ActEnemyAtkSpeed(int on)
{
    MonoObject* list = GetLivingList();
    int n = ListCount(list), hit = 0, skip = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || SkipAlly(c)) continue;

        MonoObject* fpm = CharFPM(c);
        if (!fpm) { skip++; continue; }

        if (!f_AttackSpeed) f_AttackSpeed = FindField(p_object_get_class(fpm), "AttackSpeed");
        if (!f_AttackSpeed) { skip++; continue; }

        float v = on ? 0.0f : 1.0f;
        p_field_set_value(fpm, f_AttackSpeed, &v);
        hit++;
    }
    if (hit == 0) {
        Log("!! 敌人攻速: 场上没有可改的敌人%s", skip ? " (其余都是非人形单位)" : "");
        return;
    }
    Log("敌人攻速 = %s   (%d 个敌人%s)", on ? "0 (木头人)" : "1 (恢复)", hit,
        skip ? ", 另有非人形单位已跳过" : "");
}

static void ActMagBoots(int on)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 磁力靴: 找不到玩家"); return; }
    MonoMethod* m = FindMethod(p_object_get_class(pl), "SetIsUsingMagBoots", 1);
    if (!m) { Log("!! 磁力靴: SetIsUsingMagBoots 没找到"); return; }
    int v = on ? 1 : 0;
    void* a[1]; a[0] = &v;
    Invoke(m, pl, a, "SetIsUsingMagBoots");
    Log("磁力靴 = %d   (面向墙壁/天花板时才会吸住)", v);
}

static void ActJetpack(int on)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 喷气背包: 找不到玩家"); return; }
    MonoMethod* m = FindMethod(p_object_get_class(pl), "SetJetpackEngaged", 2);
    if (!m) { Log("!! 喷气背包: SetJetpackEngaged 没找到"); return; }
    int v = on ? 1 : 0;
    void* a[2]; a[0] = &v; a[1] = NULL;
    Invoke(m, pl, a, "SetJetpackEngaged");
    Log("喷气背包 = %d   (配合 无限能量 一起用效果最好)", v);
}

static const int   kWeaponIds[]   = { 1, 2, 3, 4, 5, 6, 7, 8 };
static const char* kWeaponNames[] = { "剑 Sword", "弓 Bow", "锤 Hammer", "手雷 Grenade",
                                      "矛 Spear", "盾 Shield", "双剑 DualSword", "双锤 DualHammer" };
#define JQR_WEAPON_COUNT (sizeof(kWeaponIds)/sizeof(kWeaponIds[0]))

static void ActSetWeapon(int weaponType)
{
    MonoObject* pl = PlayerFPM();
    if (!pl) { Log("!! 武器: 找不到玩家"); return; }
    if (!m_SetWeapon) { Log("!! SetEquippedWeaponType 找不到"); return; }

    int added = 0;
    if (f_EquippedWpns) {
        MonoObject* list = NULL;
        p_field_get_value(pl, f_EquippedWpns, &list);
        if (list) {
            if (!m_ListAdd)
                m_ListAdd = FindMethod(p_object_get_class(list), "Add", 1);
            if (m_ListAdd) {
                int contained = 0;
                MonoMethod* mc = FindMethod(p_object_get_class(list), "Contains", 1);
                if (mc) {
                    void* ca[1]; ca[0] = &weaponType;
                    MonoObject* b = Invoke(mc, list, ca, "List.Contains");
                    if (b) { int* pb = (int*)p_object_unbox(b); contained = pb ? (*pb & 0xFF) : 0; }
                }
                if (!contained) {
                    void* aa[1]; aa[0] = &weaponType;
                    Invoke(m_ListAdd, list, aa, "List.Add");
                    added = 1;
                }
            }
        }
    }

    int force = 1;
    void* a[2]; a[0] = &weaponType; a[1] = &force;
    Invoke(m_SetWeapon, pl, a, "SetEquippedWeaponType");

    const char* nm = "?";
    for (unsigned i = 0; i < JQR_WEAPON_COUNT; i++)
        if (kWeaponIds[i] == weaponType) { nm = kWeaponNames[i]; break; }
    Log("换武器 -> %s (%d)%s", nm, weaponType, added ? "  [已先加入持有列表]" : "");
}

#define ACT_CH_KILL       90
#define ACT_CH_RECRUIT    91
#define ACT_CH_DISARM     92
#define ACT_CH_BETRAY     93
#define ACT_CH_CALM       94
#define ACT_CH_ARMOR      95
#define ACT_CH_UNARMOR    96
#define ACT_CH_IMM_ON     97
#define ACT_CH_IMM_OFF    98
#define ACT_CH_WEAPON     99
#define ACT_CH_MORPH     100
#define ACT_CH_SCALE     101
#define ACT_CH_SPEED     102
#define ACT_CH_ATKSPD    103
#define ACT_CH_REFRESH   104
#define ACT_RUMBLE_ON    105
#define ACT_RUMBLE_OFF   106

#define ACT_RB_TEAMS         180
#define ACT_RB_TEAMOFF       181
#define ACT_RB_DUEL_NEAR     182
#define ACT_RB_DUEL_ME       183
#define ACT_RB_DUEL_STOP     184
#define ACT_RB_CLEARLOG      185

#define ACT_TILT_ON          192
#define ACT_TILT_OFF         193
#define ACT_FOE_BLIND        194
#define ACT_FOE_FRENZY       195
#define ACT_BURN_ALL         196
#define ACT_STUN_ALL         197
#define ACT_GOD_ALL_ON       198
#define ACT_GOD_ALL_OFF      199

#define ACT_RB_FORMATION     210
#define ACT_RB_PREP          211
#define ACT_RB_BET_RED       212
#define ACT_RB_BET_BLUE      213
#define ACT_RB_BET_OFF       214
#define ACT_RB_RANDWEAPON    215

static MonoClass*      c_FPM           = NULL;
static MonoClass*      c_ArmorMgr      = NULL;
static MonoMethod*     m_ApplyArmor    = NULL;
static MonoMethod*     m_RemArmor      = NULL;
static MonoMethod*     m_OneArmor      = NULL;
static MonoMethod*     m_GetCharModel  = NULL;
static MonoClassField* f_CharName      = NULL;
static MonoMethod*     m_SetTgtOpp     = NULL;
static MonoClassField* f_AIIgnoreTeam  = NULL;
static MonoClassField* f_AIMaxDist     = NULL;
static MonoMethod*     m_SetLocalScale = NULL;

#define JQR_SNAP_MAX 64
typedef struct {
    MonoObject* obj;
    int typeId;
    int isTeam;
    int isPlayer;
    int isBetray;
} JqrSnapEntry;

static JqrSnapEntry  g_snap[JQR_SNAP_MAX];
static volatile LONG g_snapCount = 0;
static volatile LONG g_snapSeq   = 0;
static DWORD         g_snapTick  = 0;

#define JQR_BETRAY_MAX 32
static MonoObject* g_betray[JQR_BETRAY_MAX];
static int         g_betrayCount = 0;
static int         g_rumbleOn    = 0;
static DWORD       g_aiTgtTick   = 0;

static void ActDuelStop(void);
#define JQR_RP_MAX 12
static void*         g_hfSelChar = NULL;
static volatile LONG g_rbTeamOn = 0;
static volatile LONG g_duelOn   = 0;
static MonoObject*   g_duelA    = NULL;
static MonoObject*   g_duelB    = NULL;
static char          g_rpText[JQR_RP_MAX][96];
static volatile LONG g_rpCount  = 0;

static MonoObject*   g_rpPrev[JQR_SNAP_MAX];
static char          g_rpPrevName[JQR_SNAP_MAX][48];
static int           g_rpPrevCount = 0;

#define JQR_TEAM_MAX 64
static MonoObject*   g_rbRed[JQR_TEAM_MAX];
static MonoObject*   g_rbBlue[JQR_TEAM_MAX];
static int           g_rbRedN  = 0;
static int           g_rbBlueN = 0;
static volatile LONG g_betTeam = 0;
static volatile LONG g_matchOver = 0;
static volatile LONG g_rbRedAlive  = 0;
static volatile LONG g_rbBlueAlive = 0;

static int g_pendWeapon = 1;
static int g_pendMorph  = 1;
static int g_pendPose   = 0;
static int g_pendScale  = 100;
static int g_pendSpeed  = 100;
static int g_pendAtk    = 100;

static const char* TypeNameCN(int id)
{
    for (int i = 0; i < JQR_ENEMYTYPE_COUNT; i++)
        if (g_typeIds[i] == id) return g_typeNamesCN[i];
    return "未知兵种";
}

static const char* PickNameOf(MonoObject* c)
{
    static char buf[80];
    if (!c) return "?";
    int id = CharTypeId(c);
    const char* n = TypeNameCN(id);
    if (n && n[0] && strcmp(n, "未知兵种") != 0 && strcmp(n, "无") != 0) return n;

    if (!f_CharName) f_CharName = FindField(c_Character, "CharacterName");
    if (f_CharName) {
        MonoObject* s = NULL;
        p_field_get_value(c, f_CharName, &s);
        if (s) {
            buf[0] = 0;
            MonoStrToUtf8(s, buf, sizeof(buf));
            if (buf[0]) return buf;
        }
    }
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "兵种#%d", id);
    return buf;
}

static int InLiving(MonoObject* c)
{
    if (!c) return 0;
    MonoObject* list = GetLivingList();
    int n = ListCount(list);
    for (int i = 0; i < n; i++)
        if (ListItem(list, i) == c) return 1;
    return 0;
}

static MonoMethod* m_ObjOpEq = NULL;

static int IsUnityDead(MonoObject* o)
{
    if (!o) return 1;
    if (!m_ObjOpEq) {
        m_ObjOpEq = FindMethod(p_object_get_class(o), "op_Equality", 2);
        if (!m_ObjOpEq) return 0;
    }
    MonoObject* nul = NULL;
    void* a[2];
    a[0] = &o;
    a[1] = &nul;
    MonoObject* r = p_runtime_invoke(m_ObjOpEq, NULL, a, NULL);
    if (!r) return 0;
    unsigned char* pb = (unsigned char*)p_object_unbox(r);
    return (pb && *pb) ? 1 : 0;
}

static int IsBetraying(MonoObject* c)
{
    for (int i = 0; i < g_betrayCount; i++)
        if (g_betray[i] == c) return 1;
    return 0;
}

static MonoObject* CharFPM(MonoObject* c)
{
    if (!c) return NULL;
    if (!c_FPM) c_FPM = Cls("", "FirstPersonMover");
    if (!c_FPM) return NULL;
    MonoObject* f = GetComponentT(c, c_FPM, "GetComponent");
    if (!f) f = GetComponentT(c, c_FPM, "GetComponentInChildren");
    return f;
}

static MonoObject* CharAI(MonoObject* c)
{
    if (!c) return NULL;
    if (!c_AI) c_AI = Cls("", "AIController");
    if (!c_AI) return NULL;
    MonoObject* a = GetComponentT(c, c_AI, "GetComponent");
    if (!a) a = GetComponentT(c, c_AI, "GetComponentInChildren");
    return a;
}

static void BuildRoster(void)
{
    if (!c_Character) return;
    MonoObject* list = GetLivingList();
    MonoObject* me   = GetPlayerCharacter();
    int n = ListCount(list);
    if (n > JQR_SNAP_MAX) n = JQR_SNAP_MAX;

    InterlockedIncrement(&g_snapSeq);
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c) continue;
        if (IsUnityDead(c)) continue;
        g_snap[cnt].obj      = c;
        g_snap[cnt].typeId   = CharTypeId(c);
        g_snap[cnt].isTeam   = CharIsPlayerTeam(c);
        g_snap[cnt].isPlayer = (me && c == me) ? 1 : 0;
        g_snap[cnt].isBetray = IsBetraying(c);
        cnt++;
    }
    g_snapCount = (LONG)cnt;
    InterlockedIncrement(&g_snapSeq);
}

static void DoRosterTick(void)
{
    DWORD now = GetTickCount();
    if (now - g_snapTick < 150) return;
    g_snapTick = now;

    if (g_betrayCount > 0) {
        int w = 0;
        for (int i = 0; i < g_betrayCount; i++)
            if (InLiving(g_betray[i])) g_betray[w++] = g_betray[i];
        g_betrayCount = w;
    }
    BuildRoster();
}

static MonoObject* NearestTarget(MonoObject* self, MonoObject* me, int sameTeamOnly)
{
    MonoObject* tf = CharTf(self);
    Vec3 p;
    if (!tf || !TfGetPos(tf, &p)) return NULL;
    int myTeam = CharIsPlayerTeam(self);

    MonoObject* best = NULL;
    float bestD = 1e30f;
    MonoObject* list = GetLivingList();
    int n = ListCount(list);
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || c == self || c == me) continue;
        if (IsUnityDead(c)) continue;
        if (sameTeamOnly && CharIsPlayerTeam(c) != myTeam) continue;
        MonoObject* t2 = CharTf(c);
        Vec3 q;
        if (!t2 || !TfGetPos(t2, &q)) continue;
        float dx = q.x - p.x, dy = q.y - p.y, dz = q.z - p.z;
        float d = dx * dx + dy * dy + dz * dz;
        if (d < bestD) { bestD = d; best = c; }
    }
    return best;
}

static void RbAddReport(const char* who)
{
    LONG idx  = InterlockedIncrement(&g_rpCount) - 1;
    int  slot = (int)(idx % JQR_RP_MAX);
    _snprintf_s(g_rpText[slot], sizeof(g_rpText[slot]), _TRUNCATE, "%s 倒下了", who ? who : "?");
    Log("[战报] %s 倒下了", who ? who : "?");
}

static void RbAddRaw2(const char* a, const char* b)
{
    LONG idx  = InterlockedIncrement(&g_rpCount) - 1;
    int  slot = (int)(idx % JQR_RP_MAX);
    _snprintf_s(g_rpText[slot], sizeof(g_rpText[slot]), _TRUNCATE, "%s%s",
                a ? a : "", b ? b : "");
    Log("[战报] %s%s", a ? a : "", b ? b : "");
}

static void RbSnapshot(MonoObject* me)
{
    MonoObject* list = GetLivingList();
    int n = ListCount(list);
    int k = 0;
    for (int i = 0; i < n && k < JQR_SNAP_MAX; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || c == me) continue;
        if (IsUnityDead(c)) continue;
        g_rpPrev[k] = c;
        _snprintf_s(g_rpPrevName[k], sizeof(g_rpPrevName[k]), _TRUNCATE, "%s", PickNameOf(c));
        k++;
    }
    g_rpPrevCount = k;
}

static void RbCheckDeaths(MonoObject* me)
{
    for (int i = 0; i < g_rpPrevCount; i++) {
        MonoObject* c = g_rpPrev[i];
        if (!c) continue;
        if (InLiving(c) && !IsUnityDead(c)) continue;
        RbAddReport(g_rpPrevName[i]);
        g_rpPrev[i] = NULL;
    }
    RbSnapshot(me);
}

#define JQR_AITGT_CACHE 96
static void* g_aiTgtWho[JQR_AITGT_CACHE];
static void* g_aiTgtObj[JQR_AITGT_CACHE];
static int   g_aiTgtN      = 0;
static DWORD g_rbCheckTick = 0;

static int AiTgtCached(void* who, void* tgt)
{
    for (int i = 0; i < g_aiTgtN; i++)
        if (g_aiTgtWho[i] == who) return (g_aiTgtObj[i] == tgt);
    return 0;
}
static void AiTgtRemember(void* who, void* tgt)
{
    for (int i = 0; i < g_aiTgtN; i++)
        if (g_aiTgtWho[i] == who) { g_aiTgtObj[i] = tgt; return; }
    if (g_aiTgtN >= JQR_AITGT_CACHE) g_aiTgtN = 0;
    g_aiTgtWho[g_aiTgtN] = who;
    g_aiTgtObj[g_aiTgtN] = tgt;
    g_aiTgtN++;
}
static void AiTgtCacheReset(void) { g_aiTgtN = 0; }

static void DoAiTargetTick(void)
{
    int duel = (InterlockedCompareExchange(&g_duelOn, 0, 0) != 0);
    if (!g_rumbleOn && !g_rbTeamOn && !duel && g_betrayCount <= 0) return;
    DWORD now = GetTickCount();
    if (now - g_aiTgtTick < 400) return;
    g_aiTgtTick = now;

    if (!c_AI) c_AI = Cls("", "AIController");
    if (!c_AI) return;
    if (!m_SetTgtOpp)    m_SetTgtOpp    = FindMethod(c_AI, "SetTargetOpponentCharacter", 1);
    if (!f_AIIgnoreTeam) f_AIIgnoreTeam = FindField(c_AI, "IgnoreOpposingTeam");
    if (!f_AIMaxDist)    f_AIMaxDist    = FindField(c_AI, "MaxEnemyTargetDistance");
    if (!m_SetTgtOpp) return;

    MonoObject* me   = GetPlayerCharacter();
    MonoObject* list = GetLivingList();
    int n = ListCount(list);
    int bad = 0;
    int reporting = (duel || g_rumbleOn) ? 1 : 0;

    if (duel) {
        MonoObject* A = g_duelA;
        MonoObject* B = g_duelB;
        if (!A || IsUnityDead(A) || !InLiving(A)) {
            Log("斗蛐蛐 -> 单挑的甲方不在了, 自动收工");
            ActDuelStop();
            duel = 0;
        } else if (B && (IsUnityDead(B) || !InLiving(B))) {
            Log("斗蛐蛐 -> 单挑分出胜负, 收工!");
            ActDuelStop();
            duel = 0;
        } else {
            for (int i = 0; i < n; i++) {
                MonoObject* c = ListItem(list, i);
                if (!c || IsUnityDead(c)) continue;
                MonoObject* ai = CharAI(c);
                if (!ai) continue;
                DWORD code = 0;
                __try {
                    if (c == A) {
                        MonoObject* t = B ? B : me;
                        if (t) { void* a1[1]; a1[0] = &t; p_runtime_invoke(m_SetTgtOpp, ai, a1, NULL); }
                    } else if (B && c == B) {
                        void* a1[1]; a1[0] = &A; p_runtime_invoke(m_SetTgtOpp, ai, a1, NULL);
                    } else {

                        if (f_AIMaxDist) { float z = 0.0f; p_field_set_value(ai, f_AIMaxDist, &z); }
                        MonoObject* nul = NULL;
                        void* a1[1]; a1[0] = &nul;
                        p_runtime_invoke(m_SetTgtOpp, ai, a1, NULL);
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) { code = GetExceptionCode(); }
                if (code) { bad++; if (bad == 1) Log("!! 单挑锁目标: 访问违例 (%08X), 已跳过", code); }
            }
        }
    }

    if (!duel) {
        for (int i = 0; i < n; i++) {
            MonoObject* c = ListItem(list, i);
            if (!c || c == me) continue;
            if (IsUnityDead(c)) continue;
            int betray = IsBetraying(c);
            if (!betray && !g_rumbleOn) continue;

            DWORD code = 0;
            __try {
                MonoObject* ai = CharAI(c);
                if (ai) {

                    if (g_rumbleOn && !betray) {
                        if (f_AIIgnoreTeam) { int yes = 1;       p_field_set_value(ai, f_AIIgnoreTeam, &yes); }
                        if (f_AIMaxDist)    { float inf = -1.0f; p_field_set_value(ai, f_AIMaxDist, &inf); }
                    }
                    MonoObject* tgt = NearestTarget(c, me, betray ? 1 : 0);
                    if (tgt && !AiTgtCached(c, tgt)) {
                        void* a[1]; a[0] = &tgt;
                        p_runtime_invoke(m_SetTgtOpp, ai, a, NULL);
                        AiTgtRemember(c, tgt);
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { code = GetExceptionCode(); }

            if (code) {
                bad++;
                if (bad == 1) Log("!! 内讧锁目标: 某个角色访问违例 (%08X), 已单独跳过它", code);
                for (int k = 0; k < g_betrayCount; k++) {
                    if (g_betray[k] == c) {
                        g_betray[k] = g_betray[g_betrayCount - 1];
                        g_betrayCount--;
                        break;
                    }
                }
            }
        }
    }

    DoRbMatchTick();

    if (reporting && (now - g_rbCheckTick >= 1000)) { g_rbCheckTick = now; RbCheckDeaths(me); }

    if (bad) Log("   (本次共跳过 %d 个有问题的角色)", bad);
}

static MonoObject* MustAlive(MonoObject* c, const char* who)
{
    if (!c) { Log("!! 点名[%s]: 没选中角色", who); return NULL; }

    if (IsUnityDead(c)) { Log("!! 点名[%s]: 这个角色的物体已经销毁了 (幽灵), 跳过", who); return NULL; }
    if (!InLiving(c)) { Log("!! 点名[%s]: 目标已经不在场上了", who); return NULL; }
    return c;
}

static void ActPickKill(MonoObject* c)
{
    if (!MustAlive(c, "击杀")) return;
    const char* nm = PickNameOf(c);
    if (!CharKill(c)) { Log("!! 点名[击杀]: Character.Kill 没生效 (可能本来就死了)"); return; }
    Log("点名 -> 处决 [%s]  (走官方死亡链, 会算进关卡进度)", nm);
}

static void ActPickRecruit(MonoObject* c)
{
    if (!MustAlive(c, "收编")) return;
    if (CharIsPlayerTeam(c)) { Log("点名 -> 它已经是我方了 (不重复收编)"); return; }
    if (!f_IsPlayerTeam) f_IsPlayerTeam = FindField(c_Character, "IsPlayerTeam");
    if (!f_IsPlayerTeam) { Log("!! IsPlayerTeam 字段找不到"); return; }
    int one = 1;
    p_field_set_value(c, f_IsPlayerTeam, &one);
    Log("点名 -> 收编 [%s] 为友军", PickNameOf(c));
}

static void ActPickDisarm(MonoObject* c)
{
    if (!MustAlive(c, "缴械")) return;
    int n = DropWeaponsOf(c);
    Log("点名 -> [%s] 丢掉 %d 件武器", PickNameOf(c), n);
}

static void ActPickBetray(MonoObject* c)
{
    if (!MustAlive(c, "内讧")) return;
    if (IsBetraying(c)) { Log("点名 -> 它已经在内讧了"); return; }
    if (g_betrayCount >= JQR_BETRAY_MAX) { Log("!! 内讧名单满了 (最多 %d 个)", JQR_BETRAY_MAX); return; }
    if (!CharAI(c)) { Log("!! 点名: 这个角色没有 AIController, 内讧不了"); return; }
    g_betray[g_betrayCount++] = c;
    Log("点名 -> [%s] 开始痛击自己队友 (每 0.15 秒强制锁最近的同阵营)", PickNameOf(c));
}

static void ActPickCalm(MonoObject* c)
{
    for (int i = 0; i < g_betrayCount; i++) {
        if (g_betray[i] == c) {
            g_betray[i] = g_betray[g_betrayCount - 1];
            g_betrayCount--;
            Log("点名 -> [%s] 停止内讧", PickNameOf(c));
            return;
        }
    }
    Log("点名 -> 它本来就没在内讧");
}

static void ActPickArmor(MonoObject* c, int on)
{
    if (!MustAlive(c, "护甲")) return;
    if (!c_ArmorMgr) c_ArmorMgr = Cls("", "ArmorManager");
    if (!c_ArmorMgr) { Log("!! ArmorManager 类找不到"); return; }
    const char* err = "?";
    MonoObject* am = GetSingleton(c_ArmorMgr, &err);
    if (!am) { Log("!! ArmorManager 实例拿不到: %s", err); return; }

    if (on) {

        MonoObject* fpm = CharFPM(c);
        MonoObject* mdl = NULL;
        if (fpm) {
            if (!m_GetCharModel) m_GetCharModel = FindMethod(p_object_get_class(fpm), "GetCharacterModel", 0);
            if (m_GetCharModel) mdl = Invoke(m_GetCharModel, fpm, NULL, "GetCharacterModel");
        }
        if (!fpm) {
            Log("!! 护甲: 这个角色没有第一人称身体 (战舰一类), 穿不了甲");
            return;
        }
        if (!mdl || IsUnityDead(mdl)) {
            Log("!! 护甲: 角色模型还没建好 (刚出生/正在换装?), 过一会儿再点");
            return;
        }

        int placed = 0;
        if (!m_ApplyArmor) m_ApplyArmor = FindMethod(c_ArmorMgr, "ApplyArmorTo", 2);
        if (m_ApplyArmor) {
            int one = 1;
            void* a[2]; a[0] = &c; a[1] = &one;
            if (Invoke(m_ApplyArmor, am, a, "ApplyArmorTo")) placed = 1;
        }
        if (placed <= 0) {

            if (!m_OneArmor) m_OneArmor = FindMethod(c_ArmorMgr, "AddOneArmorPieceTo", 1);
            if (m_OneArmor) {
                for (int i = 0; i < 8; i++) {
                    void* a[1]; a[0] = &c;
                    MonoObject* r = Invoke(m_OneArmor, am, a, "AddOneArmorPieceTo");
                    if (!r) break;
                    int n = ListCount(r);
                    if (n <= 0) break;
                    placed += n;
                }
            }
            if (placed > 0) Log("点名 -> 给 [%s] 逐件穿上护甲 (%d 件)", PickNameOf(c), placed);
            else           Log("!! 点名[护甲]: 这个角色穿不上甲 (部位/模型不完整)");
            return;
        }
        Log("点名 -> 给 [%s] 穿上护甲", PickNameOf(c));
    } else {
        if (!m_RemArmor) m_RemArmor = FindMethod(c_ArmorMgr, "RemoveArmorFrom", 1);
        if (!m_RemArmor) { Log("!! RemoveArmorFrom 找不到"); return; }
        void* a[1]; a[0] = &c;
        Invoke(m_RemArmor, am, a, "RemoveArmorFrom");
        Log("点名 -> 卸掉 [%s] 的护甲", PickNameOf(c));
    }
}

static void ActPickImmune(MonoObject* c, int on)
{
    if (!MustAlive(c, "无敌")) return;
    CharSetImmune(c, on);
    Log("点名 -> [%s] 无敌 = %d", PickNameOf(c), on);
}

static const char* WeaponNameOf(int id)
{
    for (unsigned i = 0; i < JQR_WEAPON_COUNT; i++)
        if (kWeaponIds[i] == id) return kWeaponNames[i];
    return "?";
}

static void ActPickWeapon(MonoObject* c, int weaponType)
{
    if (!MustAlive(c, "换武器")) return;
    MonoObject* fpm = CharFPM(c);
    if (!fpm) { Log("!! 点名: 这个角色没有 FirstPersonMover, 换不了武器"); return; }
    MonoClass* k = p_object_get_class(fpm);

    if (!f_EquippedWpns) f_EquippedWpns = FindField(k, "_equippedWeapons");
    if (f_EquippedWpns) {
        MonoObject* ls = NULL;
        p_field_get_value(fpm, f_EquippedWpns, &ls);
        if (ls) {
            if (!m_ListAdd) m_ListAdd = FindMethod(p_object_get_class(ls), "Add", 1);
            MonoMethod* mc = FindMethod(p_object_get_class(ls), "Contains", 1);
            int contained = 0;
            if (mc) {
                void* ca[1]; ca[0] = &weaponType;
                MonoObject* b = Invoke(mc, ls, ca, "List.Contains");
                if (b) { int* pb = (int*)p_object_unbox(b); contained = pb ? (*pb & 0xFF) : 0; }
            }
            if (!contained && m_ListAdd) {
                void* aa[1]; aa[0] = &weaponType;
                Invoke(m_ListAdd, ls, aa, "List.Add");
            }
        }
    }

    MonoMethod* ms = FindMethod(k, "SetEquippedWeaponType", 2);
    if (!ms) { Log("!! SetEquippedWeaponType 找不到"); return; }
    int force = 1;
    void* a[2]; a[0] = &weaponType; a[1] = &force;
    Invoke(ms, fpm, a, "SetEquippedWeaponType");
    Log("点名 -> [%s] 换上 %s (%d)  注: AI 可能自己换回去",
        PickNameOf(c), WeaponNameOf(weaponType), weaponType);
}

static void ActPickMorph(MonoObject* c, int newType)
{
    if (!MustAlive(c, "变装")) return;

    MonoObject* fpm = CharFPM(c);
    if (!fpm) { Log("!! 变装: 目标没有 FirstPersonMover (战舰/建筑变不了装)"); return; }
    if (!InitRefs()) { Log("!! InitRefs 失败"); return; }

    const char* err = "?";
    MonoObject* factory = GetSingleton(c_Factory, &err);
    if (!factory) { Log("!! EnemyFactory: %s", err); return; }

    if (!m_GetEnemyCfg) m_GetEnemyCfg = FindMethod(c_Factory, "GetEnemyConfiguration", 1);
    if (!m_GetEnemyCfg) { Log("!! EnemyFactory.GetEnemyConfiguration 没找到"); return; }

    int ty = newType;
    void* ca[1]; ca[0] = &ty;
    MonoObject* cfg = SafeCall(factory, "GetEnemyConfiguration", 1, ca, "GetEnemyConfiguration");
    if (!cfg) {
        Log("!! 变装: 本关卡的敌兵表里没有 [%s], 换不了", TypeNameCN(ty));
        return;
    }

    if (!f_EnemyPrefab) f_EnemyPrefab = FindField(p_object_get_class(cfg), "EnemyPrefab");
    if (!f_EnemyPrefab) { Log("!! EnemyConfiguration.EnemyPrefab 字段找不到"); return; }
    MonoObject* prefab = NULL;
    p_field_get_value(cfg, f_EnemyPrefab, &prefab);
    if (!prefab) { Log("!! 该兵种的 EnemyPrefab 是空的"); return; }

    int oldType = CharTypeId(c);

    if (!m_GetGO) m_GetGO = FindMethod(p_object_get_class(prefab), "get_gameObject", 0);
    MonoObject* prefabGO = m_GetGO ? SafeCall(prefab, "get_gameObject", 0, NULL, "预制体 get_gameObject") : NULL;
    if (!prefabGO) { Log("!! 变装: 拿不到兵种预制体的 GameObject"); return; }

    MonoObject* cmPrefab = NULL;
    MonoObject* refFpm = GetComponentT(prefabGO, c_FPM, "GetComponent");
    if (!refFpm) refFpm = GetComponentT(prefabGO, c_FPM, "GetComponentInChildren");
    if (refFpm) {
        if (!f_CMPrefab) f_CMPrefab = FindField(p_object_get_class(refFpm), "CharacterModelPrefab");
        if (f_CMPrefab) p_field_get_value(refFpm, f_CMPrefab, &cmPrefab);
    }
    if (!cmPrefab) {

        if (!c_CharModel) c_CharModel = Cls("", "CharacterModel");
        if (c_CharModel) cmPrefab = GetComponentT(prefabGO, c_CharModel, "GetComponentInChildren");
    }
    if (!cmPrefab) {
        Log("!! 变装: [%s] 的预制体上挖不到 CharacterModel, 身体保持不变", TypeNameCN(ty));
        return;
    }

    if (!f_CharModelRef) f_CharModelRef = FindField(p_object_get_class(fpm), "_characterModel");
    MonoObject* oldModel = NULL;
    if (f_CharModelRef) p_field_get_value(fpm, f_CharModelRef, &oldModel);

    void* pa[1]; pa[0] = &cmPrefab;
    SafeCall(fpm, "CreateCharacterModel", 1, pa, "CreateCharacterModel(变装)");

    MonoObject* nowModel = NULL;
    if (f_CharModelRef) p_field_get_value(fpm, f_CharModelRef, &nowModel);

    int newBad = (nowModel && IsUnityDead(nowModel));
    if (!nowModel || nowModel == oldModel || newBad) {
        Log("!! 变装: 新模型没建起来 (now=%p dead=%d), 保留原来的身体 ([%s] 不变)",
            (void*)nowModel, newBad, TypeNameCN(oldType));
        if (f_CharModelRef && oldModel) p_field_set_value(fpm, f_CharModelRef, &oldModel);
        return;
    }
    if (oldModel && !IsUnityDead(oldModel)) {
        MonoObject* oldGO = SafeCall(oldModel, "get_gameObject", 0, NULL, "旧模型 get_gameObject");
        if (oldGO) {

            if (!m_ObjDestroy) m_ObjDestroy = FindMethod(p_object_get_class(oldGO), "Destroy", 1);
            if (m_ObjDestroy) {
                DWORD code = 0;
                void* da[1]; da[0] = &oldGO;
                __try { p_runtime_invoke(m_ObjDestroy, NULL, da, NULL); }
                __except (EXCEPTION_EXECUTE_HANDLER) { code = GetExceptionCode(); }
                if (code) Log("  (销毁旧模型时异常 %08X, 已跳过)", code);
            } else {
                Log("  (找不到 Object.Destroy, 旧模型会残留)");
            }
        }
    }

    SafeCall(fpm, "RefreshWeaponAnimatorProperties", 0, NULL, "刷新武器动画属性");
    SafeCall(fpm, "refreshWeaponVisibility", 0, NULL, "刷新武器可见性");
    SafeCall(fpm, "refreshVisibilityOfAdditionalWeaponModels", 0, NULL, "刷新副武器可见性");
    SafeCall(fpm, "refreshCameraWeaponBools", 0, NULL, "刷新相机武器状态");

    if (!f_CharType) f_CharType = FindField(c_Character, "CharacterType");
    if (f_CharType) p_field_set_value(c, f_CharType, &ty);

    Log("变装 -> [%s] => [%s]   (就地换模型; 角色对象没换, 所以不卡、名单不乱)",
        TypeNameCN(oldType), TypeNameCN(ty));
}

static int MorphForbidden(int id)
{
    if (id <= 0) return 1;
    if (id == 9 || id == 10 || id == 20) return 1;
    if (id == 46) return 1;
    if (id == 58) return 1;
    if (id >= 64 && id <= 67) return 1;
    if (id == 83 || id == 84) return 1;
    for (unsigned i = 0; i < JQR_WARSHIP_COUNT; i++)
        if (g_warshipIds[i] == id) return 1;
    return 0;
}

static void ActSelfMorph(int typeId)
{
    MonoObject* c = GetPlayerCharacter();
    if (!c) { Log("!! 变身: 先进入关卡 (现在拿不到玩家)"); return; }
    if (MorphForbidden(typeId)) {
        Log("!! 变身: [%s] 已列入禁变身型 (垃圾机器人/战舰/意识空间类), 变了会搞坏游戏 —— 已拦下",
            TypeNameCN(typeId));
        return;
    }
    if (!CharFPM(c)) { Log("!! 变身: 玩家身上没有 FirstPersonMover, 变不了"); return; }
    Log("变身(自己) -> 目标 [%s], 开始就地换模型……", TypeNameCN(typeId));
    ActPickMorph(c, typeId);
}

#define JQR_POSE_MAX 128
static char          g_poseNames[JQR_POSE_MAX][64];
static volatile LONG g_poseCount = 0;
static volatile LONG g_poseSel   = -1;
static DWORD         g_poseNext  = 0;
static int           g_poseLoadTried = 0;
static DWORD         g_poseCrashLog = 0;

static MonoMethod*   m_PlayUpper    = NULL;
static MonoMethod*   m_PlayLegs     = NULL;
static MonoMethod*   m_GetCM        = NULL;
static MonoMethod*   m_CrossIdle    = NULL;

static void PoseLoadNames(void)
{
    InterlockedExchange(&g_poseCount, 0);
    InterlockedExchange(&g_poseSel, -1);
    MonoObject* em = GetSingletonByName("EmoteManager");
    if (!em) { Log("!! 动作: EmoteManager 单例拿不到"); return; }
    if (!m_GetAllEmotes) m_GetAllEmotes = FindMethod(p_object_get_class(em), "GetAllEmotes", 0);
    if (!m_GetAllEmotes) { Log("!! 动作: EmoteManager.GetAllEmotes 找不到"); return; }
    MonoObject* list = Invoke(m_GetAllEmotes, em, NULL, "EmoteManager.GetAllEmotes");
    if (!list) { Log("!! 动作: emote 名单返回空"); return; }
    int n = ListCount(list), k = 0;
    for (int i = 0; i < n && k < JQR_POSE_MAX; i++) {
        MonoObject* def = ListItem(list, i);
        if (!def) continue;
        MonoClassField* fID = FindField(p_object_get_class(def), "EmoteID");
        if (!fID) continue;
        MonoObject* s = NULL;
        p_field_get_value(def, fID, &s);
        if (!s) continue;
        g_poseNames[k][0] = 0;
        MonoStrToUtf8(s, g_poseNames[k], sizeof(g_poseNames[k]));
        if (g_poseNames[k][0]) k++;
    }
    InterlockedExchange(&g_poseCount, k);
    Log("★ 动作名单: 读到 %d 个游戏 emote:", k);
    for (int i = 0; i < k; i++) Log("   [动作 %d] Emote_%s", i + 1, g_poseNames[i]);
}

static int         PoseCount(void)         { return (int)g_poseCount; }
static const char* PoseNameAt(int i)       { return (i >= 0 && i < (int)g_poseCount) ? g_poseNames[i] : ""; }

static void PoseApplyBody(const char* id, int verbose)
{
    if (!g_domain || !p_string_new) { if (verbose) Log("!! 动作: mono 字符串接口没就绪"); return; }
    if (!id || !id[0]) { if (verbose) Log("!! 动作: emote 名为空"); return; }
    MonoObject* c = GetPlayerCharacter();
    if (!c) { if (verbose) Log("!! 动作: 拿不到玩家角色 (是不是没进关卡?)"); return; }
    MonoObject* fpm = CharFPM(c);
    if (!fpm || IsUnityDead(fpm)) { if (verbose) Log("!! 动作: 玩家身上没有可用的 FirstPersonMover"); return; }
    if (!m_GetCM) m_GetCM = FindMethod(p_object_get_class(fpm), "GetCharacterModel", 0);
    if (!m_GetCM) { if (verbose) Log("!! 动作: GetCharacterModel 方法找不到"); return; }
    MonoObject* ex = NULL;
    MonoObject* cm = p_runtime_invoke(m_GetCM, fpm, NULL, &ex);
    if (ex || !cm || IsUnityDead(cm)) { if (verbose) Log("!! 动作: CharacterModel 无效"); return; }

    char nm[96]; int k = 0;
    const char* pfx = "Emote_";
    for (int i = 0; pfx[i] && k < 94; i++) nm[k++] = pfx[i];
    for (int i = 0; id[i] && k < 94; i++) nm[k++] = id[i];
    nm[k] = 0;

    if (!m_PlayUpper) m_PlayUpper = FindMethod(p_object_get_class(cm), "PlayUpper", 2);
    if (!m_PlayLegs)  m_PlayLegs  = FindMethod(p_object_get_class(cm), "PlayLegs", 2);
    if (!m_PlayUpper) { if (verbose) Log("!! 动作: CharacterModel.PlayUpper 找不到"); return; }

    if (verbose) {
        MonoClassField* fUA = FindField(p_object_get_class(cm), "UpperAnimator");
        MonoObject* ua = NULL;
        if (fUA) p_field_get_value(cm, fUA, &ua);
        Log("   动作自检: cm=%p UpperAnimator=%p(%s) 目标=%s", (void*)cm, (void*)ua,
            ua ? "有" : "【空! 播不出来】", nm);
    }

    float t = 0.0f;
    MonoObject* s = p_string_new(g_domain, nm);
    void* a[2]; a[0] = &s; a[1] = &t;
    MonoObject* xe = NULL;
    p_runtime_invoke(m_PlayUpper, cm, a, &xe);
    if (verbose) Log("   PlayUpper(%s) 已调用%s", nm, xe ? " (抛异常!)" : "");

    if (m_PlayLegs) {
        MonoObject* sl = p_string_new(g_domain, nm);
        void* al[2]; al[0] = &sl; al[1] = &t;
        MonoObject* xl = NULL;
        p_runtime_invoke(m_PlayLegs, cm, al, &xl);
    }
}

static void PoseApply(const char* id, int verbose)
{
    __try {
        PoseApplyBody(id, verbose);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_poseSel, -1);
        DWORD t = GetTickCount();
        if (verbose || t - g_poseCrashLog > 3000) {
            g_poseCrashLog = t;
            Log("!! 动作: 访问违例 code=%08X, 已停 (老版在这里会 250ms 崩一次, 现在崩一次就停)",
                GetExceptionCode());
        }
    }
}

static void PoseStopBody(void)
{
    MonoObject* c = GetPlayerCharacter();
    if (!c) return;
    MonoObject* fpm = CharFPM(c);
    if (!fpm || IsUnityDead(fpm)) return;
    if (!m_CrossIdle) m_CrossIdle = FindMethod(p_object_get_class(fpm), "CrossfadeUpperAnimationToIdle", 0);
    if (m_CrossIdle) Invoke(m_CrossIdle, fpm, NULL, "CrossfadeUpperAnimationToIdle");
    Log("动作 -> 已停止, 恢复正常");
}

static void PoseStop(void)
{
    InterlockedExchange(&g_poseSel, -1);
    __try { PoseStopBody(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { Log("动作 -> 停止时访问违例, 已忽略 (不影响游戏)"); }
}

static void ActPoseSet(int which)
{
    if (which <= 0) { PoseStop(); return; }
    MonoObject* c = GetPlayerCharacter();
    if (!c) { Log("!! 动作: 先进入关卡 (现在拿不到玩家)"); return; }
    if (!CharFPM(c)) { Log("!! 动作: 玩家身上没有 FirstPersonMover"); return; }
    if ((int)g_poseCount <= 0) PoseLoadNames();
    int idx = which - 1;
    if (idx < 0 || idx >= (int)g_poseCount) {
        Log("!! 动作: 第 %d 项不存在 (当前共 %d 项, 先点\"重新读取 emote\")", which, (int)g_poseCount);
        return;
    }
    InterlockedExchange(&g_poseSel, idx);
    g_poseNext = 0;
    Log("动作 -> 播放 [%d] Emote_%s   (每 250ms 补刷; 点\"停止动作\"还原)", which, g_poseNames[idx]);
    PoseApply(g_poseNames[idx], 1);
}

static void DumpTfTree(MonoObject* tf, int depth, int* budget)
{
    if (!tf || depth > 6 || *budget <= 0) return;
    MonoClass* kt = p_object_get_class(tf);
    MonoMethod* mName  = FindMethod(kt, "get_name", 0);
    MonoMethod* mCount = FindMethod(kt, "get_childCount", 0);
    MonoMethod* mChild = FindMethod(kt, "GetChild", 1);

    char nm[96]; nm[0] = 0;
    if (mName) {
        MonoObject* s = Invoke(mName, tf, NULL, "tf.get_name");
        if (s) MonoStrToUtf8(s, nm, sizeof(nm));
    }
    char ind[48]; int j = 0;
    for (int i = 0; i < depth && j < 44; i++) { ind[j++] = ' '; ind[j++] = ' '; }
    ind[j] = 0;
    Log("    [骨] %s%s", ind, nm[0] ? nm : "?");
    (*budget)--;

    if (mCount && mChild) {
        MonoObject* cno = Invoke(mCount, tf, NULL, "tf.childCount");
        int cn = cno ? *(int*)p_object_unbox(cno) : 0;
        if (cn > 24) cn = 24;
        for (int i = 0; i < cn; i++) {
            int idx = i;
            void* a[1]; a[0] = &idx;
            MonoObject* ch = Invoke(mChild, tf, a, "tf.GetChild");
            if (ch && !IsUnityDead(ch)) DumpTfTree(ch, depth + 1, budget);
        }
    }
}

static void ActPoseDumpBones(void)
{
    MonoObject* c = GetPlayerCharacter();
    if (!c) { Log("!! 骨骼导出: 先进入关卡"); return; }
    MonoObject* fpm = CharFPM(c);
    if (!fpm) { Log("!! 骨骼导出: 玩家没有 FirstPersonMover"); return; }
    MonoObject* cm = SafeCall(fpm, "GetCharacterModel", 0, NULL, "取 CharacterModel");
    if (!cm) { Log("!! 骨骼导出: 拿不到 CharacterModel"); return; }
    MonoMethod* mTf = FindMethod(p_object_get_class(cm), "get_transform", 0);
    MonoObject* tf = mTf ? Invoke(mTf, cm, NULL, "cm.get_transform") : NULL;
    if (!tf) { Log("!! 骨骼导出: 拿不到 transform"); return; }
    Log("===== 玩家骨骼树 (CharacterModel) 开始 (最多 260 个节点 / 6 层) =====");
    int budget = 260;
    DumpTfTree(tf, 0, &budget);
    Log("===== 玩家骨骼树结束, 共输出 %d 个节点 =====", 260 - budget);
    Log("提示: 把上面 [骨] 开头的行整段发给我, 就能据此做精确的抬手/竖中指姿势");
}

static void DoPoseTick(void)
{
    int sel = (int)InterlockedCompareExchange(&g_poseSel, 0, 0);
    if (sel < 0) {

        if ((int)g_poseCount <= 0 && !g_poseLoadTried && g_domain) {
            __try {
                if (GetPlayerCharacter()) { g_poseLoadTried = 1; PoseLoadNames(); }
            } __except (EXCEPTION_EXECUTE_HANDLER) { g_poseLoadTried = 1; }
        }
        return;
    }
    DWORD now = GetTickCount();
    if (now < g_poseNext) return;
    g_poseNext = now + 250;
    if (sel >= (int)g_poseCount) { InterlockedExchange(&g_poseSel, -1); return; }
    PoseApply(g_poseNames[sel], 0);
}

static void ActPickScale(MonoObject* c, int pct)
{
    if (!MustAlive(c, "体型")) return;
    MonoObject* tf = CharTf(c);
    if (!tf) { Log("!! 点名: 拿不到 Transform"); return; }
    float s = (float)pct / 100.0f;
    if (s < 0.2f) s = 0.2f;
    if (s > 5.0f) s = 5.0f;
    Vec3 v; v.x = v.y = v.z = s;
    if (!m_SetLocalScale) m_SetLocalScale = FindMethod(p_object_get_class(tf), "set_localScale", 1);
    if (!m_SetLocalScale) { Log("!! Transform.set_localScale 找不到"); return; }
    void* a[1]; a[0] = &v;
    Invoke(m_SetLocalScale, tf, a, "Transform.set_localScale");
    Log("点名 -> [%s] 体型 = %.2fx", PickNameOf(c), s);
}

static void ActPickSpeed(MonoObject* c, int pct)
{
    if (!MustAlive(c, "速度")) return;
    MonoObject* fpm = CharFPM(c);
    if (!fpm) { Log("!! 点名: 这个角色没有 FirstPersonMover"); return; }
    MonoClass* k = p_object_get_class(fpm);
    if (!f_MoveSpeedMul) f_MoveSpeedMul = FindField(k, "_moveSpeedMultiplier");
    if (!f_MoveSpeedMul) { Log("!! _moveSpeedMultiplier 找不到"); return; }
    float v = (float)pct / 100.0f;
    p_field_set_value(fpm, f_MoveSpeedMul, &v);
    Log("点名 -> [%s] 移动速度 = %.2fx", PickNameOf(c), v);
}

static void ActPickAtkSpeed(MonoObject* c, int pct)
{
    if (!MustAlive(c, "攻速")) return;
    MonoObject* fpm = CharFPM(c);
    if (!fpm) { Log("!! 点名: 这个角色没有 FirstPersonMover"); return; }
    MonoClass* k = p_object_get_class(fpm);
    if (!f_AttackSpeed) f_AttackSpeed = FindField(k, "AttackSpeed");
    if (!f_AttackSpeed) { Log("!! AttackSpeed 找不到"); return; }
    float v = (float)pct / 100.0f;
    p_field_set_value(fpm, f_AttackSpeed, &v);
    Log("点名 -> [%s] 攻击速度 = %.2fx", PickNameOf(c), v);
}

static void ActRumble(int on)
{
    g_rumbleOn = on;

    AiTgtCacheReset();

    if (!c_AI) c_AI = Cls("", "AIController");
    if (c_AI) {
        if (!f_AIIgnoreTeam) f_AIIgnoreTeam = FindField(c_AI, "IgnoreOpposingTeam");
        if (!f_AIMaxDist)    f_AIMaxDist    = FindField(c_AI, "MaxEnemyTargetDistance");
        MonoObject* me   = GetPlayerCharacter();
        MonoObject* list = GetLivingList();
        int n = ListCount(list);
        for (int i = 0; i < n; i++) {
            MonoObject* c = ListItem(list, i);
            if (!c || c == me || IsUnityDead(c)) continue;
            MonoObject* ai = CharAI(c);
            if (!ai) continue;
            __try {
                if (f_AIIgnoreTeam) { int      v = on ? 1 : 0;  p_field_set_value(ai, f_AIIgnoreTeam, &v); }
                if (f_AIMaxDist)    { float    v = -1.0f;       p_field_set_value(ai, f_AIMaxDist,    &v); }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        if (on) RbSnapshot(me);
    }
    Log("全场乱斗 = %d   (%s)", on, on ? "所有 AI 互相锁最近的其它角色, 站远点看戏" : "已停止");
}

static void ActRumbleTeams(void)
{
    if (!f_IsPlayerTeam) f_IsPlayerTeam = FindField(c_Character, "IsPlayerTeam");
    if (!f_IsPlayerTeam) { Log("!! 分队: IsPlayerTeam 字段找不到"); return; }
    MonoObject* me   = GetPlayerCharacter();
    MonoObject* list = GetLivingList();
    int n = ListCount(list), k = 0, red = 0, blue = 0;
    g_rbRedN = 0; g_rbBlueN = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || c == me || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        int one = (k & 1) ? 1 : 0;
        p_field_set_value(c, f_IsPlayerTeam, &one);
        if (one) {
            if (red < JQR_TEAM_MAX) g_rbRed[red] = c;
            red++;
        } else {
            if (blue < JQR_TEAM_MAX) g_rbBlue[blue] = c;
            blue++;
        }
        k++;
    }
    if (red  > JQR_TEAM_MAX) red  = JQR_TEAM_MAX;
    if (blue > JQR_TEAM_MAX) blue = JQR_TEAM_MAX;
    g_rbRedN  = red;
    g_rbBlueN = blue;
    InterlockedExchange(&g_matchOver, 0);
    InterlockedExchange(&g_rbTeamOn, 1);
    RbSnapshot(me);
    Log("斗蛐蛐 -> 红蓝分队: 红队 %d 人 / 蓝队 %d 人 (共 %d)", red, blue, k);
    Log("   红队带友军标记 -> 不会被算进关卡敌人, 想收工点「全员恢复为敌」");
}

static void ActRumbleTeamOff(void)
{
    if (!f_IsPlayerTeam) f_IsPlayerTeam = FindField(c_Character, "IsPlayerTeam");
    if (!f_IsPlayerTeam) { Log("!! 恢复为敌: IsPlayerTeam 字段找不到"); return; }
    MonoObject* me   = GetPlayerCharacter();
    MonoObject* list = GetLivingList();
    int n = ListCount(list), k = 0;
    for (int i = 0; i < n; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || c == me || IsUnityDead(c)) continue;
        int zero = 0;
        p_field_set_value(c, f_IsPlayerTeam, &zero);
        k++;
    }
    InterlockedExchange(&g_rbTeamOn, 0);
    g_rbRedN = 0; g_rbBlueN = 0;
    InterlockedExchange(&g_betTeam, 0);
    InterlockedExchange(&g_matchOver, 0);
    InterlockedExchange(&g_rbRedAlive, 0);
    InterlockedExchange(&g_rbBlueAlive, 0);
    Log("斗蛐蛐 -> 全员恢复为敌 (%d 个), 阵营回到关卡原始状态", k);
}

static void ActDuelStart(MonoObject* c, int vsMe)
{
    if (!MustAlive(c, "单挑")) return;
    MonoObject* foe = NULL;
    if (vsMe) {
        foe = GetPlayerCharacter();
        if (!foe) { Log("!! 单挑: 找不到你本人"); return; }
    } else {
        foe = NearestTarget(c, GetPlayerCharacter(), 0);
        if (!foe) { Log("!! 单挑: 它附近没有可打的对手"); return; }
    }
    g_duelA = c;
    g_duelB = vsMe ? NULL : foe;
    InterlockedExchange(&g_duelOn, 1);
    if (vsMe) Log("斗蛐蛐 -> 单挑开始: 你 VS [%s]   (其他人围观)", PickNameOf(c));
    else      Log("斗蛐蛐 -> 单挑开始: [%s] VS [%s]   (其他人围观)", PickNameOf(c), PickNameOf(foe));
}

static void ActDuelStop(void)
{
    int was = (int)InterlockedExchange(&g_duelOn, 0);
    g_duelA = NULL;
    g_duelB = NULL;
    if (!c_AI) c_AI = Cls("", "AIController");
    if (c_AI) {
        if (!f_AIMaxDist) f_AIMaxDist = FindField(c_AI, "MaxEnemyTargetDistance");
        MonoObject* me   = GetPlayerCharacter();
        MonoObject* list = GetLivingList();
        int n = ListCount(list);
        for (int i = 0; i < n && f_AIMaxDist; i++) {
            MonoObject* c = ListItem(list, i);
            if (!c || c == me || IsUnityDead(c)) continue;
            MonoObject* ai = CharAI(c);
            if (!ai) continue;
            __try { float inf = -1.0f; p_field_set_value(ai, f_AIMaxDist, &inf); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (was) Log("斗蛐蛐 -> 单挑结束, 围观群众解除限制");
}

static void ActRumbleClearLog(void)
{
    InterlockedExchange(&g_rpCount, 0);
    for (int i = 0; i < JQR_RP_MAX; i++) g_rpText[i][0] = 0;
    for (int i = 0; i < JQR_SNAP_MAX; i++) g_rpPrev[i] = NULL;
    g_rpPrevCount = 0;
    Log("斗蛐蛐 -> 战报已清空");
}

static void DoActionPtr(int id, void* ptr)
{
    MonoObject* c = (MonoObject*)ptr;
    switch (id) {
    case ACT_CH_KILL:    ActPickKill(c);               break;

    case ACT_RB_DUEL_NEAR: ActDuelStart(c, 0);         break;
    case ACT_RB_DUEL_ME:   ActDuelStart(c, 1);         break;
    case ACT_CH_RECRUIT: ActPickRecruit(c);            break;
    case ACT_CH_DISARM:  ActPickDisarm(c);             break;
    case ACT_CH_BETRAY:  ActPickBetray(c);             break;
    case ACT_CH_CALM:    ActPickCalm(c);               break;
    case ACT_CH_ARMOR:   ActPickArmor(c, 1);           break;
    case ACT_CH_UNARMOR: ActPickArmor(c, 0);           break;
    case ACT_CH_IMM_ON:  ActPickImmune(c, 1);          break;
    case ACT_CH_IMM_OFF: ActPickImmune(c, 0);          break;
    case ACT_CH_WEAPON:  ActPickWeapon(c, g_pendWeapon); break;
    case ACT_CH_MORPH:   ActPickMorph(c, g_pendMorph);   break;
    case ACT_CH_SCALE:   ActPickScale(c, g_pendScale);   break;
    case ACT_CH_SPEED:   ActPickSpeed(c, g_pendSpeed);   break;
    case ACT_CH_ATKSPD:  ActPickAtkSpeed(c, g_pendAtk);  break;
    case ACT_CH_REFRESH: g_snapTick = 0; Log("点名台 -> 手动刷新名单"); break;

#define ACT_EP_EXPLODE_SEL   120
#define ACT_EP_FLING_SEL     121
#define ACT_EP_STUN_SEL      122

    case ACT_EP_EXPLODE_SEL: ActEpExplodeSel(c); break;
    case ACT_EP_FLING_SEL:   ActEpFlingSel(c);   break;
    case ACT_EP_STUN_SEL:    ActEpStunSel(c);    break;
    default: Log("?? 未知指针动作 %d", id); break;
    }
}

static volatile LONG g_showMenu = 1;

#define ACT_SUMMON_ALLY   1
#define ACT_SUMMON_ENEMY  2
#define ACT_IMMUNE_ON     10
#define ACT_IMMUNE_OFF    11
#define ACT_NEUTRAL_ON    12
#define ACT_NEUTRAL_OFF   13
#define ACT_CLONE_PLUS    14
#define ACT_SLOWMO_ON     15
#define ACT_SLOWMO_OFF    16
#define ACT_AIOFF_ON      17
#define ACT_AIOFF_OFF     18
#define ACT_KILL_ALL      19
#define ACT_KILL_SHIPS    20
#define ACT_SHIPIMM_ON    21
#define ACT_SHIPIMM_OFF   22
#define ACT_ALLYIMM_ON    23
#define ACT_ALLYIMM_OFF   24
#define ACT_ENERGY_ON     25
#define ACT_ENERGY_OFF    26
#define ACT_TRANSFER_ON   27
#define ACT_TRANSFER_OFF  28

#define ACT_DEV_BRANCH_ON  30
#define ACT_DEV_BRANCH_OFF 31
#define ACT_DEV_MENU       32
#define ACT_DEV_KILLALL    33
#define ACT_DEV_SKILL      34

#define ACT_FLY_ON         40
#define ACT_FLY_OFF        41
#define ACT_WALL_ON        50
#define ACT_WALL_OFF       51
#define ACT_WEAPON         60
#define ACT_SPEED          61
#define ACT_JUMP           62

#define ACT_POSSESS        70
#define ACT_DISARM         71
#define ACT_VICTORY        72
#define ACT_STOP_POSE      73

#define ACT_RIDE           74
#define ACT_DISMOUNT       75
#define ACT_ONESHOT_ON     76
#define ACT_ONESHOT_OFF    77
#define ACT_ATKSPD0_ON     78
#define ACT_ATKSPD0_OFF    79
#define ACT_MAGBOOT_ON     80
#define ACT_MAGBOOT_OFF    81
#define ACT_JETPACK_ON     82
#define ACT_JETPACK_OFF    83

#define ACT_REACH_ON       110
#define ACT_REACH_OFF      111
#define ACT_ED_ENEMY_FILTER 112
#define ACT_ED_FILTER_RESET 113
#define ACT_ED_PLACE_ENEMY  114

#define ACT_EP_EXPLODE_ALL   123
#define ACT_EP_FLING_ALL     124
#define ACT_EP_STUN_ALL      125
#define ACT_EP_FALL_ALL      126
#define ACT_EP_SUPERKICK_ON  127
#define ACT_EP_SUPERKICK_OFF 128
#define ACT_EP_NOFALL_ON     129
#define ACT_EP_NOFALL_OFF    130
#define ACT_EP_RUNANIM       131
#define ACT_EP_BOW           132
#define ACT_EP_CINE          133
#define ACT_EP_CINE_OFF      134
#define ACT_EP_LOWGRAV_ON    135
#define ACT_EP_LOWGRAV_OFF   136

#define ACT_TS               140
#define ACT_AIM_ON           141
#define ACT_AIM_OFF          142
#define ACT_AUTOATK_ON       143
#define ACT_AUTOATK_OFF      144
#define ACT_BLINK            145
#define ACT_EMP              146
#define ACT_SUPERJUMP        147
#define ACT_NOCOUNT_ON       148
#define ACT_NOCOUNT_OFF      149
#define ACT_TELEPORT_ALL     150

#define ACT_RECRUIT_ALL      160
#define ACT_SCALE_ALL        161
#define ACT_FORCE_WEAPON     162
#define ACT_KILL_NEAREST     163
#define ACT_SUICIDE          164
#define ACT_MAXSKILL         165
#define ACT_TAKE_ALL_WEAPON  166
#define ACT_DBG_HIDEUI_ON    167
#define ACT_DBG_HIDEUI_OFF   168
#define ACT_DBG_SEAMS_ON     169
#define ACT_DBG_SEAMS_OFF    170
#define ACT_DBG_NOSMOOTH_ON  171
#define ACT_DBG_NOSMOOTH_OFF 172

#define ACT_FORCE_WIN        220

#define ACT_CUT_FREE_ON      225
#define ACT_CUT_FREE_OFF     226
#define ACT_FREE_CAM_ON      227
#define ACT_FREE_CAM_OFF     228

#define ACT_FREEALL_ON       229
#define ACT_FREEALL_OFF      230

#define ACT_LASERMOVE_ON     231
#define ACT_LASERMOVE_OFF    232
#define ACT_WLEN_ON          233
#define ACT_WLEN_OFF         234
#define ACT_CRASHKILL_ON     235
#define ACT_CRASHKILL_OFF    236
#define ACT_CRASHCLEAN_ON    237
#define ACT_CRASHCLEAN_OFF   238

#define ACT_CLONEALLY_ON     240
#define ACT_CLONEALLY_OFF    241
#define ACT_FENCEOFF_ON      242
#define ACT_FENCEOFF_OFF     243

#define ACT_ARROWBIG_ON      244
#define ACT_ARROWBIG_OFF     245

#define ACT_SHIPSETTLE_ON    250
#define ACT_SHIPSETTLE_OFF   251

#define ACT_CTFORCE_ON       246
#define ACT_CTFORCE_OFF      247
#define ACT_CT_NOW           248

#define ACT_SELF_MORPH       256
#define ACT_POSE_WAVE        257
#define ACT_POSE_SIT         258
#define ACT_POSE_PRONE       259
#define ACT_POSE_SKY         260
#define ACT_POSE_OFF         261
#define ACT_POSE_DUMP        262
#define ACT_POSE_SET         266
#define ACT_POSE_RELOAD      267

#define ACT_AISMART_ON       249
#define ACT_AISMART_OFF      252

#define ACT_MUSIC_PLAY       263
#define ACT_MUSIC_STOP       264
#define ACT_MUSIC_RELOAD     265

#define JQR_MUSIC_MAX  160
static char          g_musicNames[JQR_MUSIC_MAX][64];
static volatile LONG g_musicCount  = 0;
static volatile LONG g_musicSel    = -1;
static volatile LONG g_musicLoaded = 0;

static MonoObject* MusicClipDef(MonoObject* lib, const char* fname)
{
    MonoClassField* f = FindField(p_object_get_class(lib), fname);
    if (!f) return NULL;
    MonoObject* v = NULL;
    p_field_get_value(lib, f, &v);
    if (!v) return NULL;
    MonoClass* vk = p_object_get_class(v);
    const char* vn = vk ? p_class_get_name(vk) : NULL;
    if (vn && strstr(vn, "[]")) {
        if (p_array_length(v) == 0) return NULL;
        char* a = p_array_addr_with_size(v, sizeof(void*), 0);
        if (!a) return NULL;
        v = *(MonoObject**)a;
    }
    return v;
}

static void MusicReload(void)
{
    InterlockedExchange(&g_musicCount, 0);
    InterlockedExchange(&g_musicSel, -1);
    MonoObject* lib = GetSingletonByName("AudioLibrary");
    if (!lib) { Log("!! 音乐: AudioLibrary 单例拿不到 (游戏还没加载音频库?)"); return; }
    MonoMethod* mGet = FindMethod(p_object_get_class(lib), "GetMusicClipNames", 0);
    if (!mGet) { Log("!! 音乐: GetMusicClipNames 找不到"); return; }
    MonoObject* list = Invoke(mGet, lib, NULL, "AudioLibrary.GetMusicClipNames");
    if (!list) { Log("!! 音乐: 名单返回空"); return; }
    int n = ListCount(list), k = 0;
    if (n > JQR_MUSIC_MAX) n = JQR_MUSIC_MAX;
    for (int i = 0; i < n; i++) {
        MonoObject* s = ListItem(list, i);
        if (!s) continue;
        g_musicNames[k][0] = 0;
        MonoStrToUtf8(s, g_musicNames[k], sizeof(g_musicNames[k]));
        if (g_musicNames[k][0]) k++;
    }
    InterlockedExchange(&g_musicCount, k);
    InterlockedExchange(&g_musicLoaded, 1);

    int okf = (k > 0 && MusicClipDef(lib, g_musicNames[0]) != NULL) ? 1 : 0;
    Log("★ 音乐名单: 读到 %d 首 (字段路径可读=%d, 首项=%s)", k, okf, k > 0 ? g_musicNames[0] : "-");
}

static void MusicPlay(int idx)
{
    int cnt = (int)InterlockedCompareExchange(&g_musicCount, 0, 0);
    if (idx < 0 || idx >= cnt) return;
    MonoObject* lib = GetSingletonByName("AudioLibrary");
    if (!lib) return;
    MonoObject* clipdef = MusicClipDef(lib, g_musicNames[idx]);
    if (!clipdef) { Log("!! 音乐: 字段 %s 拿不到 AudioClipDefinition", g_musicNames[idx]); return; }

    MonoObject* am = GetSingletonByName("AudioManager");
    if (!am) { Log("!! 音乐: AudioManager 单例拿不到"); return; }
    MonoClass* amk = p_object_get_class(am);

    MonoClassField* fMas = FindField(amk, "MusicAudioSource");
    MonoObject* mas = NULL;
    if (fMas) p_field_get_value(am, fMas, &mas);

    MonoMethod* mPlay = FindMethod(amk, "PlayMusicClip", 3);
    if (mPlay) {
        int t1 = 1, t2 = 1;
        void* ap[3]; ap[0] = &clipdef; ap[1] = &t1; ap[2] = &t2;
        Invoke(mPlay, am, ap, "AudioManager.PlayMusicClip");
    } else {
        Log("!! 音乐: PlayMusicClip(AudioClipDefinition,bool,bool) 找不到");
    }

    int ok = 0;
    if (mas && !IsUnityDead(mas)) {
        MonoMethod* mGetClip = FindMethod(p_object_get_class(mas), "get_clip", 0);
        MonoObject* cur = mGetClip ? Invoke(mGetClip, mas, NULL, "AudioSource.get_clip") : NULL;
        if (cur && !IsUnityDead(cur)) ok = 1;
    }
    if (!ok && mas && !IsUnityDead(mas)) {

        MonoClassField* fClip = FindField(p_object_get_class(clipdef), "Clip");
        MonoObject* clip = NULL;
        if (fClip) p_field_get_value(clipdef, fClip, &clip);
        if (clip && !IsUnityDead(clip)) {
            MonoClass* mk = p_object_get_class(mas);
            int one = 1, zero = 0;
            void* pa[1];
            pa[0] = &clip; Invoke(FindMethod(mk, "set_clip", 1), mas, pa, "AudioSource.set_clip");
            pa[0] = &one;  Invoke(FindMethod(mk, "set_loop", 1), mas, pa, "AudioSource.set_loop");
            pa[0] = &zero; Invoke(FindMethod(mk, "set_mute", 1), mas, pa, "AudioSource.set_mute");
            Invoke(FindMethod(mk, "Play", 0), mas, NULL, "AudioSource.Play");
            ok = 1;
            Log("  (官方 PlayMusicClip 没生效, 已用 AudioSource 兜底)");
        } else {
            Log("!! 音乐: AudioClipDefinition.Clip 为空, 兜底也放不了");
        }
    }
    InterlockedExchange(&g_musicSel, idx);
    Log("♪ 播放音乐: %s  (已设上=%d)", g_musicNames[idx], ok);
}

static void MusicStop(void)
{
    MonoObject* am = GetSingletonByName("AudioManager");
    if (!am) return;
    MonoMethod* mS = FindMethod(p_object_get_class(am), "StopMusic", 0);
    if (!mS) { Log("!! 音乐: StopMusic 找不到"); return; }
    Invoke(mS, am, NULL, "AudioManager.StopMusic");
    InterlockedExchange(&g_musicSel, -1);
    Log("♪ 音乐已停止");
}

static MonoClass* c_ExpBranch  = NULL;
static MonoClass* c_DebugMgr   = NULL;
static MonoClass* c_UpgradeMgr = NULL;

static void ActDevBranch(int on)
{
    if (!c_ExpBranch) c_ExpBranch = Cls("", "ExperimentalBranchManager");
    if (!c_ExpBranch) { Log("!! 找不到 ExperimentalBranchManager"); return; }

    const char* err = "";
    MonoObject* inst = GetSingleton(c_ExpBranch, &err);
    if (!inst) { Log("!! ExperimentalBranchManager 实例拿不到: %s", err); return; }

    MonoClassField* f = FindField(c_ExpBranch, "IsExperimentalBranch");
    if (!f) { Log("!! 找不到 IsExperimentalBranch 字段"); return; }
    int v = on ? 1 : 0;
    p_field_set_value(inst, f, &v);
    Log("实验分支模式 = %d", v);

    if (on) {

        MonoClassField* f5 = FindField(c_ExpBranch, "ShowChapter5");
        if (f5) { int one = 1; p_field_set_value(inst, f5, &one); Log("  + 第5章(太空篇) 已放行"); }
        MonoClassField* ff = FindField(c_ExpBranch, "ShowFPSCounter");
        if (ff) { int one = 1; p_field_set_value(inst, ff, &one); Log("  + FPS 计数已开"); }
    }
}

static void ActDevMenu(void)
{
    ActDevBranch(1);

    InterlockedExchange(&g_showMenu, 0);
    if (g_hwnd) SetForegroundWindow(g_hwnd);
    Sleep(60);
    keybd_event(VK_RCONTROL, 0, 0, 0);
    Sleep(40);
    keybd_event(VK_RCONTROL, 0, KEYEVENTF_KEYUP, 0);
    Log("已模拟按下 右Ctrl -> 官方调试菜单应该弹出来了 (再按一次右Ctrl 关闭)");
}

static void ActDevKillAll(void)
{
    if (!c_DebugMgr) c_DebugMgr = Cls("", "DebugManager");
    if (!c_DebugMgr) { Log("!! 找不到 DebugManager"); return; }

    const char* err = "";
    MonoObject* inst = GetSingleton(c_DebugMgr, &err);
    if (!inst) { Log("!! DebugManager 实例拿不到: %s", err); return; }

    MonoMethod* m = FindMethod(c_DebugMgr, "KillAllEnemies", 0);
    if (!m) { Log("!! 找不到 KillAllEnemies"); return; }
    Invoke(m, inst, NULL, "KillAllEnemies");
    Log("官方 KillAllEnemies() 已调用");
}

static void ActDevSkill(void)
{
    if (!c_UpgradeMgr) c_UpgradeMgr = Cls("", "UpgradeManager");
    if (!c_UpgradeMgr) { Log("!! 找不到 UpgradeManager"); return; }

    const char* err = "";
    MonoObject* inst = GetSingleton(c_UpgradeMgr, &err);
    if (!inst) { Log("!! UpgradeManager 实例拿不到: %s", err); return; }

    MonoMethod* m = FindMethod(c_UpgradeMgr, "IncreaseSkillPoints", 0);
    if (!m) { Log("!! 找不到 IncreaseSkillPoints"); return; }
    Invoke(m, inst, NULL, "IncreaseSkillPoints");
    Log("官方 IncreaseSkillPoints() 已调用");
}

static void DoAction(int id, int arg)
{
    switch (id) {
    case ACT_SUMMON_ALLY:  ActSummon(arg, 1); break;
    case ACT_SUMMON_ENEMY: ActSummon(arg, 0); break;
    case ACT_IMMUNE_ON:    ActPlayerImmune(1); break;
    case ACT_IMMUNE_OFF:   ActPlayerImmune(0); break;
    case ACT_NEUTRAL_ON:   ActPlayerNeutral(1); break;
    case ACT_NEUTRAL_OFF:  ActPlayerNeutral(0); break;
    case ACT_CLONE_PLUS:   ActClonePlus(); break;
    case ACT_SLOWMO_ON:
        if (arg > 0) g_slowValue = arg / 100.0f;

        InterlockedExchange(&g_tsOn, 0);
        InterlockedExchange(&g_slowOn, 1);
        ActTimeScale(g_slowValue);
        break;
    case ACT_SLOWMO_OFF:
        InterlockedExchange(&g_slowOn, 0);
        InterlockedExchange(&g_tsOn, 0);
        ActTimeScale(1.0f);
        break;
    case ACT_AIOFF_ON:     ActAIOff(1); break;
    case ACT_AIOFF_OFF:    ActAIOff(0); break;
    case ACT_KILL_ALL:     ActKillAllEnemies(); break;
    case ACT_KILL_SHIPS:   ActKillEnemyCruisers(); break;
    case ACT_FORCE_WIN:    ActForceWin();     break;

    case ACT_CUT_FREE_ON:  ActCutsceneFree(1); break;
    case ACT_CUT_FREE_OFF: ActCutsceneFree(0); break;
    case ACT_FREE_CAM_ON:  ActFreeCam(1);      break;
    case ACT_FREE_CAM_OFF: ActFreeCam(0);      break;

    case ACT_FREEALL_ON:   ActFreeAll(1);      break;
    case ACT_FREEALL_OFF:  ActFreeAll(0);      break;

    case ACT_LASERMOVE_ON:
        InterlockedExchange(&g_laserMove, 1);
        LaserMoveSweep();
        Log("激光敌人可移动 -> 开 (发射中照样边走边射)");
        break;
    case ACT_LASERMOVE_OFF:
        InterlockedExchange(&g_laserMove, 0);
        Log("激光敌人可移动 -> 关 (恢复原版站桩)");
        break;
    case ACT_WLEN_ON:
        InterlockedExchange(&g_wlenOn, 1);
        if (arg > 0) g_wlenMul = (float)arg / 100.0f;
        Log("武器加长 -> 开 (倍率 %.1fx, 刀身和判定一起变长)", g_wlenMul);
        break;
    case ACT_WLEN_OFF:
        InterlockedExchange(&g_wlenOn, 0);
        WLenRestore();
        Log("武器加长 -> 关 (已还原成原版长度)");
        break;

    case ACT_CRASHKILL_ON:  ActCrashKiller(1); break;
    case ACT_CRASHKILL_OFF: ActCrashKiller(0); break;

    case ACT_CRASHCLEAN_ON:  ActCrashClean(1); break;
    case ACT_CRASHCLEAN_OFF: ActCrashClean(0); break;

    case ACT_CLONEALLY_ON:  ActCloneAlly(1); break;
    case ACT_CLONEALLY_OFF: ActCloneAlly(0); break;
    case ACT_FENCEOFF_ON:   ActFenceOff(1);  break;
    case ACT_FENCEOFF_OFF:  ActFenceOff(0);  break;

    case ACT_ARROWBIG_ON:
        InterlockedExchange(&g_arrowBig, 1);
        if (arg > 0) g_arrowMul = (float)arg / 100.0f;
        if (g_arrowMul < 1.0f)  g_arrowMul = 1.0f;
        if (g_arrowMul > 20.0f) g_arrowMul = 20.0f;
        Log("弓箭放大 -> 开 (%.1f 倍, 一箭串一串)", g_arrowMul);
        break;
    case ACT_ARROWBIG_OFF:
        InterlockedExchange(&g_arrowBig, 0);
        s_arrowNext = 0;
        DoArrowBigTick();
        Log("弓箭放大 -> 关 (箭矢已还原)");
        break;

    case ACT_SHIPSETTLE_ON:
        InterlockedExchange(&g_shipSettle, 1);
        Log("击杀战舰后补官方结算 -> 开 (会播胜利音 + 推进剧情)");
        break;
    case ACT_SHIPSETTLE_OFF:
        InterlockedExchange(&g_shipSettle, 0);
        Log("击杀战舰后补官方结算 -> 关 (只炸船, 不播胜利音)");
        break;

    case ACT_CTFORCE_ON:  ActCtForce(1);        break;
    case ACT_CTFORCE_OFF: ActCtForce(0);        break;
    case ACT_CT_NOW:      ActForceTransferNow(); break;

    case ACT_SELF_MORPH:  ActSelfMorph(g_pendMorph); break;
    case ACT_POSE_SET:    ActPoseSet(g_pendPose); break;
    case ACT_POSE_OFF:    ActPoseSet(0); break;
    case ACT_POSE_RELOAD: PoseLoadNames(); break;
    case ACT_POSE_DUMP:   ActPoseDumpBones(); break;

    case ACT_POSE_WAVE:   ActPoseSet(1); break;
    case ACT_POSE_SIT:    ActPoseSet(2); break;
    case ACT_POSE_PRONE:  ActPoseSet(3); break;
    case ACT_POSE_SKY:    ActPoseSet(4); break;

    case ACT_AISMART_ON:  ActAiSmart(1);        break;
    case ACT_AISMART_OFF: ActAiSmart(0);        break;

    case ACT_SHIPIMM_ON:   ActCruiserImmune(1); break;
    case ACT_SHIPIMM_OFF:  ActCruiserImmune(0); break;
    case ACT_ALLYIMM_ON:   ActAlliesImmune(1); break;
    case ACT_ALLYIMM_OFF:  ActAlliesImmune(0); break;
    case ACT_ENERGY_ON:
        InterlockedExchange(&g_infEnergy, 1);
        ActInfEnergyEx(TryGetPlayer(), 1, TRUE);
        break;
    case ACT_ENERGY_OFF:
        InterlockedExchange(&g_infEnergy, 0);
        ActInfEnergyEx(TryGetPlayer(), 0, TRUE);
        break;
    case ACT_TRANSFER_ON:
        InterlockedExchange(&g_infTransfer, 1);
        ActInfTransferEx(1, TRUE);
        break;
    case ACT_TRANSFER_OFF:
        InterlockedExchange(&g_infTransfer, 0);
        ActInfTransferEx(0, TRUE);
        break;

    case ACT_DEV_BRANCH_ON:  ActDevBranch(1); break;
    case ACT_DEV_BRANCH_OFF: ActDevBranch(0); break;
    case ACT_DEV_MENU:       ActDevMenu();    break;
    case ACT_DEV_KILLALL:    ActDevKillAll(); break;
    case ACT_DEV_SKILL:      ActDevSkill();   break;

    case ACT_FLY_ON:
        InterlockedExchange(&g_flyOn, 1);  ActFly(1);  break;
    case ACT_FLY_OFF:
        InterlockedExchange(&g_flyOn, 0);  ActFly(0);  break;
    case ACT_WALL_ON:
        InterlockedExchange(&g_wallOn, 1); ActWallHack(1); break;
    case ACT_WALL_OFF:
        InterlockedExchange(&g_wallOn, 0); ActWallHack(0); break;
    case ACT_WEAPON:         ActSetWeapon(arg);   break;
    case ACT_SPEED:          ActSpeedMul(arg);    break;
    case ACT_JUMP:           ActJumpMul(arg);     break;

    case ACT_POSSESS:        ActPossessEnemy();   break;
    case ACT_DISARM:         ActDisarmEnemies();  break;
    case ACT_VICTORY:        ActVictoryPose();    break;
    case ACT_STOP_POSE:      ActStopPose();       break;

    case ACT_RIDE:           ActRideNearest();    break;
    case ACT_DISMOUNT:       ActDismount();       break;
    case ACT_ONESHOT_ON:     ActOneShotEnemies(1); break;
    case ACT_ONESHOT_OFF:    ActOneShotEnemies(0); break;
    case ACT_ATKSPD0_ON:     ActEnemyAtkSpeed(1);  break;
    case ACT_ATKSPD0_OFF:    ActEnemyAtkSpeed(0);  break;
    case ACT_MAGBOOT_ON:     ActMagBoots(1);       break;
    case ACT_MAGBOOT_OFF:    ActMagBoots(0);       break;
    case ACT_JETPACK_ON:     ActJetpack(1);        break;
    case ACT_JETPACK_OFF:    ActJetpack(0);        break;

    case ACT_REACH_ON:
        InterlockedExchange(&g_reachOn, 1);
        if (arg > 0) g_reachMul = (float)arg / 100.0f;
        break;
    case ACT_REACH_OFF:
        InterlockedExchange(&g_reachOn, 0);
        ReachRestore();
        break;
    case ACT_ED_ENEMY_FILTER: ActEditorEnemyFilter(0); break;
    case ACT_ED_FILTER_RESET: ActEditorEnemyFilter(1); break;
    case ACT_ED_PLACE_ENEMY:  ActEditorPlaceEnemy(arg); break;

    case ACT_EP_EXPLODE_ALL:  ActEpExplodeAll();      break;
    case ACT_EP_FLING_ALL:    ActEpFlingAll();        break;
    case ACT_EP_STUN_ALL:     ActEpStunAll(3.0f);     break;
    case ACT_EP_FALL_ALL:     ActEpFallAll();         break;
    case ACT_EP_SUPERKICK_ON:  ActEpSuperKick(1, g_epKickMul); break;
    case ACT_EP_SUPERKICK_OFF: ActEpSuperKick(0, 1.0f);        break;
    case ACT_EP_NOFALL_ON:    ActEpNoFall(1);         break;
    case ACT_EP_NOFALL_OFF:   ActEpNoFall(0);         break;
    case ACT_EP_RUNANIM:      ActEpRunAnim(g_epRunMul); break;
    case ACT_EP_BOW:          ActEpBow();             break;
    case ACT_EP_CINE:         ActEpCine((float)arg);  break;
    case ACT_EP_CINE_OFF:     ActEpCineOff();         break;
    case ACT_EP_LOWGRAV_ON:
        InterlockedExchange(&g_epLowGrav, 1);         break;
    case ACT_EP_LOWGRAV_OFF:
        InterlockedExchange(&g_epLowGrav, 0);         break;

    case ACT_TS:            ActGameSpeed(arg);        break;
    case ACT_AIM_ON:
        InterlockedExchange(&g_aimOn, 1);
        Log("自动瞄准 -> 开 (每帧转向最近敌人)");
        break;
    case ACT_AIM_OFF:
        InterlockedExchange(&g_aimOn, 0);
        Log("自动瞄准 -> 关");
        break;
    case ACT_AUTOATK_ON:
        InterlockedExchange(&g_autoAtkOn, 1);
        Log("自动攻击 -> 开 (自动挥砍, 手不用碰鼠标)");
        break;
    case ACT_AUTOATK_OFF:
        InterlockedExchange(&g_autoAtkOn, 0);
        Log("自动攻击 -> 关");
        break;
    case ACT_BLINK:         ActBlink();               break;
    case ACT_EMP:           ActEmpAll();              break;
    case ACT_SUPERJUMP:     ActSuperJump();           break;
    case ACT_NOCOUNT_ON:    ActNoCount(1);            break;
    case ACT_NOCOUNT_OFF:   ActNoCount(0);            break;
    case ACT_TELEPORT_ALL:  ActTeleportAll();         break;

    case ACT_RECRUIT_ALL:    ActRecruitAll();          break;
    case ACT_SCALE_ALL:      ActScaleAll(arg);         break;
    case ACT_FORCE_WEAPON:   ActForceWeaponAll(arg);   break;
    case ACT_KILL_NEAREST:   ActKillNearest();         break;
    case ACT_SUICIDE:        ActSuicide();             break;
    case ACT_MAXSKILL:       ActMaxSkill();            break;
    case ACT_TAKE_ALL_WEAPON: ActTakeAllWeapon();      break;
    case ACT_DBG_HIDEUI_ON:  ActDbgFlag(&f_DbgHideUI, "HideUI", 1);                         break;
    case ACT_DBG_HIDEUI_OFF: ActDbgFlag(&f_DbgHideUI, "HideUI", 0);                         break;
    case ACT_DBG_SEAMS_ON:   ActDbgFlag(&f_DbgSeams, "ShowBodyPartSeams", 1);               break;
    case ACT_DBG_SEAMS_OFF:  ActDbgFlag(&f_DbgSeams, "ShowBodyPartSeams", 0);               break;
    case ACT_DBG_NOSMOOTH_ON:  ActDbgFlag(&f_DbgNoSmooth, "DisableRotationSmoothing", 1);   break;
    case ACT_DBG_NOSMOOTH_OFF: ActDbgFlag(&f_DbgNoSmooth, "DisableRotationSmoothing", 0);   break;

    case ACT_CH_REFRESH:     g_snapTick = 0;       break;
    case ACT_RUMBLE_ON:      ActRumble(1);         break;
    case ACT_RUMBLE_OFF:     ActRumble(0);         break;

    case ACT_RB_TEAMS:       ActRumbleTeams();     break;
    case ACT_RB_TEAMOFF:     ActRumbleTeamOff();   break;
    case ACT_RB_DUEL_STOP:   ActDuelStop();        break;
    case ACT_RB_CLEARLOG:    ActRumbleClearLog();  break;

    case ACT_RB_DUEL_NEAR:   ActDuelStart((MonoObject*)g_hfSelChar, 0); break;
    case ACT_RB_DUEL_ME:     ActDuelStart((MonoObject*)g_hfSelChar, 1); break;

    case ACT_TILT_ON:        ActCamTilt(1);              break;
    case ACT_TILT_OFF:       ActCamTilt(0);              break;
    case ACT_FOE_BLIND:      ActFoeSight(0);             break;
    case ACT_FOE_FRENZY:     ActFoeSight(1);             break;
    case ACT_BURN_ALL:       ActBurnAll();               break;
    case ACT_STUN_ALL:       ActStunAll((float)arg / 10.0f); break;
    case ACT_GOD_ALL_ON:     ActGodAll(1);               break;
    case ACT_GOD_ALL_OFF:    ActGodAll(0);               break;

    case ACT_RB_FORMATION:   ActRbFormation((float)arg / 10.0f); break;
    case ACT_RB_PREP:        ActRbPrep(arg);             break;
    case ACT_RB_BET_RED:     ActRbBet(1);                break;
    case ACT_RB_BET_BLUE:    ActRbBet(2);                break;
    case ACT_RB_BET_OFF:     ActRbBet(0);                break;
    case ACT_RB_RANDWEAPON:  ActRbRandomWeapon();        break;

    case ACT_MUSIC_PLAY:     MusicPlay(arg);              break;
    case ACT_MUSIC_STOP:     MusicStop();                 break;
    case ACT_MUSIC_RELOAD:   MusicReload();               break;

    default: Log("?? 未知动作 %d", id); break;
    }
}

static volatile LONG g_ready = 0;
static int  g_selType = 1;

static int  g_summonCustomId  = 8;
static char g_summonSearch[48] = "";
static int  g_summonSearchOn   = 0;
static char g_hkSearch[48]     = "";
static int  g_hkSearchOn       = 0;
static int  g_hkView           = 0;
static void PostAction(int id, int arg)
{
    if (g_hwnd) PostMessageW(g_hwnd, WM_APP + 210, (WPARAM)id, (LPARAM)arg);
}

#define WM_JQR_ACTION_PTR (WM_APP + 212)
static void PostActionPtr(int id, void* obj)
{
    if (g_hwnd) PostMessageW(g_hwnd, WM_JQR_ACTION_PTR, (WPARAM)id, (LPARAM)obj);
}

static int uiImmune = 0, uiNeutral = 0, uiAIOff = 0, uiShipImm = 0, uiAllyImm = 0;
static int uiCloneAlly = 0, uiFenceOff = 0;
static int uiEnergy = 0, uiTransfer = 0;
static int uiDevBranch = 0;

static int   uiFly = 0, uiWall = 0;
static float uiFlySpeed = 15.0f;

static int   uiShipSettle = 0;
static int   uiCtForce = 0;
static int   uiAiSmart = 0;
static float uiSpeedMul = 1.0f, uiJumpMul = 1.0f;
static int   g_selWeapon = 0;
static float uiSlow = 1.0f;

static int uiOneShot = 0, uiAtkSpd0 = 0, uiMagBoot = 0, uiJetpack = 0;

static int   uiAim = 0, uiAutoAtk = 0, uiNoCount = 0;
static float uiGameSpeed = 1.0f;

static char        g_typeLabelBuf[JQR_ENEMYTYPE_COUNT][72];
static const char* g_typeLabels[JQR_ENEMYTYPE_COUNT];
static BOOL        g_labelsReady = FALSE;

static void BuildTypeLabels(void)
{
    if (g_labelsReady) return;
    for (int i = 0; i < JQR_ENEMYTYPE_COUNT; i++) {
        _snprintf_s(g_typeLabelBuf[i], sizeof(g_typeLabelBuf[i]), _TRUNCATE,
                    "%s  [%d]", g_typeNamesCN[i], g_typeIds[i]);
        g_typeLabels[i] = g_typeLabelBuf[i];
    }
    g_labelsReady = TRUE;
}

#define COL_WARN ImVec4(1.00f, 0.45f, 0.35f, 1.00f)
#define COL_OK   ImVec4(0.45f, 0.90f, 0.55f, 1.00f)
#define COL_DIM  ImVec4(0.62f, 0.64f, 0.70f, 1.00f)

static void RowToggle(const char* label, const char* hint, int* state, int onId, int offId)
{
    ImGui::PushID(label);
    bool v = (*state != 0);
    if (ImGui::Checkbox("##v", &v)) { *state = v ? 1 : 0; PostAction(v ? onId : offId, 0); }
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    if (hint && *hint) {
        ImGui::SameLine();
        ImGui::TextColored(COL_DIM, "%s", hint);
    }
    ImGui::PopID();
}

struct CnPair { const char* en; const char* cn; };

static const CnPair kCnFull[] = {
    { "Cannot Fall Over",                     "永不摔倒" },
    { "Kill On Head Damage",                  "爆头即死" },
    { "Min Head Voxels Damaged For Death",    "头部致死体素下限" },
    { "Min Spine Voxels Damaged For Death",   "躯干致死体素下限" },
    { "Min Sawblade Voxels Damaged For Death","锯刃致死体素下限" },
    { "Attack Speed",                         "攻击速度" },
    { "Max Speed",                            "最大速度" },
    { "Jump Velocity",                        "跳跃初速" },
    { "Disable Riding Others",                "禁止被骑" },
    { "Disable Riding",                       "禁止骑乘" },
    { "Has Infinite Energy",                  "无限能量" },
    { "Fill Rate",                            "能量回复速率" },
    { "Custom Rotation",                      "允许旋转" },
    { "Allow Custom Rotation",                "允许旋转" },
    { "Max Distance To Kick",                 "最远踢击距离" },
    { "Kick Velocity Magnitude",              "踢击力度" },
    { "Kick Animation Duration",              "踢击动画时长" },
    { "Cursor Rotation Speed Horizontal",     "水平视角灵敏度" },
    { "Cursor Rotation Speed Vertical",       "垂直视角灵敏度" },
    { "Min Camera Tilt",                      "相机俯角下限" },
    { "Min Camera Tilt Hammer",               "相机俯角下限(锤)" },
    { "Max Camera Tilt",                      "相机仰角上限" },
    { "Acceleration From Move Input",         "移动加速度" },
    { "Max Speed To Accelerate To From Input","输入加速上限" },
    { "Not Grounded Time To Allow Jump",      "离地跳跃宽限" },
    { "Spawn Cost",                           "生成费用" },
    { "Elite Bits Cost",                      "精英点数费用" },
    { "Is Player Team",                       "属于玩家阵营" },
    { "Is Neutral",                           "中立" },
    { "Disable Mind Transfers",               "禁止意识转移" },
    { "Disable Mind Transfers If Inactive",   "未激活时禁止意识转移" },
    { "Can Be Escorted",                      "可被护送" },
    { "Rider Controlled Turn Speed",          "载具转向速度" },
    { "Velocity Knockback On Impact",         "撞击击退力度" },
    { "Distance To Knockback",                "击退触发距离" },
    { "Disable Armor Shatter Reaction",       "禁用护甲破碎反应" },
    { "Ignore Weapon Animator Override",      "忽略武器动画覆盖" },
    { "Scale Kick Probes With Character",     "踢击探测随体型缩放" },
    { "Is Mind Space Character",              "心智空间角色" },
    { "Additional Grounded Buffer",           "额外着地缓冲" },
    { "Dangling Acceleration",                "悬空加速度" },
    { "Center Of Character Offset",           "角色中心偏移" },
    { "Arrow Rotation Per Scroll Wheel Delta","滚轮旋转箭矢" },
    { "Riding Torso Attack Offset",           "骑乘攻击偏移" },
    { "Riding Offset Y",                      "骑乘 Y 偏移" },
    { "Max Legs Rotation",                    "腿部最大旋转" },
    { "Collision Radius",                     "碰撞半径" },
    { "Spawn Id",                             "生成 ID" },
    { "Character Type",                       "兵种" },
    { "Character Category",                   "兵种分类" },
    { "Character Name",                       "角色名称" },
    { "Should Be Ignored By Level Triggers",  "忽略关卡触发器" },
    { "Support Partial Move Speed",           "允许部分移速" },
    { "Should Override Default Camera Offset","覆盖默认相机偏移" },
    { "Camera Offset",                        "相机偏移" },
    { "Min Twitch Auto Spawn Tier",           "Twitch 自动生成最低档" },
    { "Max Twitch Auto Spawn Tier",           "Twitch 自动生成最高档" },
    { "Don't Move Until Player",              "玩家靠近才动" },
    { "Dont Move Until Player",               "玩家靠近才动" },
    { "Armor Available",                      "可加护甲" },
    { "Light Settings Override",              "灯光设置覆盖" },
    { "Arena Style",                          "竞技场风格" },
    { "Markers Enabled",                      "显示标记" },

    { "I D",                                  "ID" },
    { "U I Prefab",                           "UI 预制体" },
    { "V F X",                                "特效" },
    { "Challenge I D",                        "挑战 ID" },
    { "Workshop Upload I D",                  "创意工坊 ID" },
    { "Check Point I D",                      "检查点 ID" },
    { "Owner I D",                            "拥有者 ID" },
    { "Attack I D",                           "攻击 ID" },
    { "Damage Origin I D",                    "伤害来源 ID" },
    { "Client Event I D",                     "客户端事件 ID" },
    { "Speech Sequence I D",                  "语音序列 ID" },
    { "B G Image",                            "背景图" },
    { "Location U I Marker Prefab",           "位置 UI 标记预制体" },
    { "A I Behaviour Prefabs",                "AI 行为预制体" },
    { "Create Environment Impact V F X",      "生成环境冲击特效" },
    { "Jetpack V F X",                        "喷气背包特效" },
    { "Use Predictive Aiming",                "使用预判瞄准" },
    { "Closest Ally Refresh Interval",        "最近盟友刷新间隔" },
    { "Constant Turn Speed",                  "恒定转向速度" },
    { "Near Turn Speed",                      "近距转向速度" },
    { "Gameplay Modifiers",                   "玩法修正项" },
    { "Double Click Threshold",               "双击判定阈值" },
    { "Moving Platform",                      "移动平台" },
};

static const CnPair kCnWord[] = {

    { "Min","最小" }, { "Max","最大" }, { "Num","数量" }, { "Count","数量" },
    { "Number","编号" }, { "Amount","数量" }, { "Total","总数" }, { "All","全部" },
    { "Any","任意" }, { "Only","仅" }, { "Upper","上" }, { "Lower","下" },
    { "High","高" }, { "Low","低" }, { "First","第一" }, { "Last","最后" },
    { "Next","下一个" }, { "New","新" }, { "Default","默认" }, { "Normal","普通" },
    { "Index","索引" }, { "Version","版本" }, { "Tier","档位" }, { "Level","关卡" },

    { "Time","时间" }, { "Delay","延迟" }, { "Duration","持续时间" },
    { "Seconds","秒" }, { "Second","秒" }, { "Countdown","倒计时" },
    { "Start","开始" }, { "End","结束" }, { "Before","之前" }, { "After","之后" },
    { "During","期间" }, { "When","当" }, { "If","如果" },

    { "Speed","速度" }, { "Velocity","速度" }, { "Move","移动" }, { "Movement","移动" },
    { "Acceleration","加速度" }, { "Jump","跳跃" }, { "Dash","冲刺" },
    { "Rotation","旋转" }, { "Angle","角度" }, { "Tilt","俯仰" },
    { "Position","位置" }, { "Positions","位置" }, { "Offset","偏移" },
    { "Distance","距离" }, { "Radius","半径" }, { "Size","尺寸" }, { "Scale","缩放" },
    { "Height","高度" }, { "Width","宽度" }, { "Range","范围" }, { "Area","区域" },
    { "Vertical","垂直" }, { "Horizontal","水平" }, { "Forward","前" },
    { "Back","后" }, { "Up","上" }, { "Down","下" }, { "Left","左" }, { "Right","右" },
    { "Top","顶部" }, { "Bottom","底部" }, { "Center","中心" }, { "Ground","地面" },
    { "Grounded","着地" }, { "Knockback","击退" }, { "Impact","冲击" },

    { "Damage","伤害" }, { "Damaged","受伤" }, { "Attack","攻击" }, { "Hit","命中" },
    { "Kill","击杀" }, { "Death","死亡" }, { "Dead","死亡" }, { "Health","生命" },
    { "Armor","护甲" }, { "Shield","护盾" }, { "Weapon","武器" }, { "Sword","剑" },
    { "Bow","弓" }, { "Hammer","锤" }, { "Spear","矛" }, { "Laser","激光" },
    { "Flame","火焰" }, { "Fire","火" }, { "Explosion","爆炸" }, { "Kick","踢" },
    { "Voxels","体素" }, { "Voxel","体素" }, { "Head","头部" }, { "Spine","躯干" },
    { "Sawblade","锯刃" }, { "Torso","躯干" }, { "Legs","双腿" }, { "Leg","腿" },
    { "Arm","手臂" }, { "Eye","眼" }, { "Face","脸" }, { "Body","身体" },
    { "Parts","部位" }, { "Part","部位" }, { "For","用于" }, { "Team","阵营" },
    { "Enemies","敌人" }, { "Enemy","敌人" }, { "Ally","盟友" }, { "Player","玩家" },
    { "Bot","机器人" }, { "Robot","机器人" }, { "Human","人类" }, { "Character","角色" },
    { "Cost","费用" }, { "Coins","金币" }, { "Bits","点数" }, { "Points","点数" },

    { "Colors","颜色" }, { "Color","颜色" }, { "Material","材质" }, { "Texture","贴图" },
    { "Sprite","精灵图" }, { "Image","图像" }, { "Icon","图标" }, { "Model","模型" },
    { "Prefab","预制体" }, { "Particles","粒子" }, { "Particle","粒子" },
    { "Glow","发光" }, { "Light","灯光" }, { "Sound","音效" }, { "Audio","音频" },
    { "Music","音乐" }, { "Animation","动画" }, { "Animator","动画器" },
    { "Emote","表情" }, { "Speech","语音" }, { "Volume","音量" },

    { "Is","是" }, { "Has","有" }, { "Can","可以" }, { "Be","被" }, { "Not","不" },
    { "Dont","不要" }, { "Disable","禁用" }, { "Disabled","已禁用" },
    { "Enable","启用" }, { "Enabled","已启用" }, { "Ignore","忽略" },
    { "Override","覆盖" }, { "Show","显示" }, { "Hide","隐藏" }, { "Hidden","隐藏" },
    { "Toggle","开关" }, { "Select","选择" }, { "Selected","已选中" },
    { "Selection","选择" }, { "Activate","激活" }, { "Activation","激活" },
    { "Active","激活" }, { "Allow","允许" }, { "Auto","自动" }, { "Random","随机" },
    { "Force","强制" }, { "Destroy","销毁" }, { "Spawn","生成" }, { "Spawns","生成" },
    { "Drop","掉落" }, { "Fill","填充" }, { "Stop","停止" }, { "Win","胜利" },
    { "Wins","胜利次数" }, { "Live","存活" }, { "Seen","可见" },
    { "Waiting","等待" }, { "Wait","等待" }, { "Use","使用" }, { "By","按" },
    { "Of","的" }, { "At","在" }, { "In","在" }, { "To","到" }, { "From","从" },
    { "Over","超过" }, { "Under","低于" }, { "Between","之间" }, { "Per","每" },
    { "And","和" }, { "Or","或" }, { "On","开" }, { "Off","关" }, { "Mode","模式" },

    { "Levels","关卡" }, { "Game","游戏" }, { "Camera","相机" }, { "Screen","屏幕" },
    { "Cursor","准星" }, { "Mouse","鼠标" }, { "Sensitivity","灵敏度" },
    { "Input","输入" }, { "Controller","控制器" }, { "Upgrades","升级" },
    { "Upgrade","升级" }, { "Skill","技能" }, { "Difficulty","难度" },
    { "Challenge","挑战" }, { "Story","剧情" }, { "Tutorial","教程" },
    { "Arena","竞技场" }, { "Battle","战斗" }, { "Royale","大逃杀" },
    { "Coop","合作" }, { "Multiplayer","多人" }, { "Workshop","创意工坊" },
    { "Achievement","成就" }, { "Progress","进度" }, { "Emperor","帝王" },
    { "Mind","心智" }, { "Space","太空" }, { "Room","房间" }, { "Tag","标签" },
    { "Folder","文件夹" }, { "Path","路径" }, { "Loading","加载" }, { "Error","错误" },
    { "Message","消息" }, { "Title","标题" }, { "Description","描述" }, { "Info","信息" },
    { "Settings","设置" }, { "Menu","菜单" }, { "Panel","面板" }, { "Group","分组" },
    { "List","列表" }, { "Data","数据" }, { "Values","值" }, { "Value","值" },
    { "Type","类型" }, { "Name","名称" }, { "Username","用户名" }, { "Field","字段" },
    { "Object","对象" }, { "Items","物品" }, { "Item","物品" },
    { "Container","容器" }, { "Holder","挂点" }, { "Parent","父级" }, { "Child","子级" },
    { "Root","根" }, { "Transform","变换" }, { "Preview","预览" }, { "Display","显示" },
    { "Slider","滑块" }, { "Multiplier","倍率" }, { "Energy","能量" },
    { "Infinite","无限" }, { "Rate","速率" }, { "Source","来源" }, { "Owner","拥有者" },
    { "Target","目标" }, { "Triggers","触发器" }, { "Trigger","触发器" },
    { "Collider","碰撞体" }, { "Block","方块" }, { "Box","盒子" },
    { "Construction","结构" }, { "Cruiser","巡洋舰" }, { "Bet","下注" },
    { "Cheer","打赏" }, { "Audience","观众" }, { "Supporter","支持者" }, { "Pack","包" },
    { "Buy","购买" }, { "Scene","场景" }, { "Background","背景" }, { "Overlay","覆盖层" },
    { "Confirm","确认" }, { "Cancel","取消" }, { "Open","打开" }, { "Repair","修复" },
    { "Match","对局" }, { "Round","回合" }, { "Idle","待机" }, { "Travel","移动" },
    { "Aim","瞄准" }, { "Scroll","滚动" }, { "Region","区域" }, { "Local","本地" },
    { "Misc","杂项" }, { "Environment","环境" }, { "Clone","分身" },
    { "Jetpack","喷气背包" }, { "Arrows","箭矢" }, { "Arrow","箭" },
    { "Fade","淡入淡出" }, { "Dust","灰尘" }, { "Sparks","火花" },

    { "Moving","移动" }, { "Platform","平台" }, { "Predictive","预判" },
    { "Closest","最近" }, { "Refresh","刷新" }, { "Interval","间隔" },
    { "Constant","恒定" }, { "Turn","转向" }, { "Near","近" },
    { "Double","双击" }, { "Click","点击" }, { "Threshold","阈值" },
    { "Gameplay","玩法" }, { "Modifiers","修正项" }, { "Volumes","体积" },
    { "Check","检查" }, { "Point","点" }, { "Author","作者" },
    { "Upload","上传" }, { "Main","主" }, { "Reached","达到" },
    { "Play","播放" }, { "State","状态" }, { "Event","事件" },
    { "Behaviour","行为" }, { "Origin","原点" }, { "Sequence","序列" },
    { "Client","客户端" }, { "Reward","奖励" }, { "Graphic","图形" },
    { "Exit","退出" }, { "Button","按钮" }, { "Label","标签" },
    { "Text","文本" }, { "Page","页" }, { "Children","子级" },
    { "Tags","标签" }, { "Challenges","挑战" }, { "Weapons","武器" },
    { "Current","当前" }, { "Old","旧" }, { "Renderers","渲染器" },
    { "Edge","边缘" }, { "Kills","击杀数" }, { "Mouth","嘴" }, { "Chest","胸" },
};

static BOOL CnLooksLikeFieldLabel(const char* s)
{
    if (!s) return FALSE;
    int len = (int)strlen(s);
    if (len < 5 || len > 96) return FALSE;
    int words = 0, inWord = 0;
    for (int i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == ' ' || c == '_') { inWord = 0; continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9'))) return FALSE;
        if (!inWord) { words++; inWord = 1; }
    }
    return words >= 2;
}

static BOOL CnTranslateLabel(const char* en, char* out, int cap)
{
    if (!en || !out || cap < 8) return FALSE;
    out[0] = 0;

    for (int i = 0; i < (int)(sizeof(kCnFull) / sizeof(kCnFull[0])); ++i) {
        if (strcmp(en, kCnFull[i].en) == 0) {
            _snprintf_s(out, cap, _TRUNCATE, "%s", kCnFull[i].cn);
            return TRUE;
        }
    }

    char w[64];
    int  wi = 0, inWord = 0, tokens = 0, hit = 0;
    for (const char* p = en; ; ++p) {
        char c = *p;
        if (c != 0 && c != ' ' && c != '_') {
            if (wi < (int)sizeof(w) - 1) w[wi++] = c;
            inWord = 1;
            continue;
        }
        if (inWord) {
            w[wi] = 0; wi = 0; inWord = 0;
            tokens++;
            const char* cn = NULL;
            for (int i = 0; i < (int)(sizeof(kCnWord) / sizeof(kCnWord[0])); ++i) {
                if (strcmp(w, kCnWord[i].en) == 0) { cn = kCnWord[i].cn; break; }
            }
            const char* piece = cn ? cn : w;
            if (cn) hit++;
            if (strlen(out) + strlen(piece) + 2 < (size_t)cap) strcat_s(out, cap, piece);
        }
        if (c == 0) break;
    }

    if (hit == 0 || hit * 2 < tokens) { out[0] = 0; return FALSE; }
    return TRUE;
}

static MonoClass*      c_UiText   = NULL;
static MonoClass*      c_GUIRoot  = NULL;
static MonoMethod*     m_TxtGet   = NULL;
static MonoMethod*     m_TxtSet   = NULL;
static MonoMethod*     m_TfCount  = NULL;
static MonoMethod*     m_TfChild  = NULL;
static MonoClassField* f_LeUiField = NULL;
static MonoClassField* f_InspTf    = NULL;
static int             g_cnScanNodes = 0;
static int             g_cnLastCount = -1;

static void CnWalkTransform(MonoObject* tf, int depth)
{
    if (!tf || depth > 7 || g_cnScanNodes > 400) return;
    g_cnScanNodes++;

    MonoObject* txt = GetComponentT(tf, c_UiText, "GetComponent");
    if (txt && m_TxtGet && m_TxtSet && p_string_new) {
        MonoObject* s = Invoke(m_TxtGet, txt, NULL, "Text.get_text");
        char buf[192];
        MonoStrToUtf8(s, buf, sizeof(buf));
        if (CnLooksLikeFieldLabel(buf)) {
            char cn[512];
            if (CnTranslateLabel(buf, cn, sizeof(cn))) {
                MonoObject* ns = p_string_new(g_domain, cn);
                if (ns) {
                    void* a[1]; a[0] = &ns;
                    Invoke(m_TxtSet, txt, a, "Text.set_text");
                    g_cnLastCount++;
                }
            }
        }
    }

    if (!m_TfCount || !m_TfChild) return;
    MonoObject* cb = Invoke(m_TfCount, tf, NULL, "Transform.get_childCount");
    int n = 0;
    if (cb) { int* p = (int*)p_object_unbox(cb); if (p) n = *p; }
    if (n > 80) n = 80;
    for (int i = 0; i < n; ++i) {
        int idx = i;
        void* a[1]; a[0] = &idx;
        MonoObject* ch = Invoke(m_TfChild, tf, a, "Transform.GetChild");
        if (ch) CnWalkTransform(ch, depth + 1);
    }
}

static MonoObject* CnGetPanelRoot(void)
{
    if (!c_GUIRoot) c_GUIRoot = Cls("", "GameUIRoot");
    if (!c_GUIRoot) return NULL;

    const char* err = "?";
    MonoObject* root = GetSingleton(c_GUIRoot, &err);
    if (!root || IsUnityDead(root)) return NULL;

    if (!f_LeUiField) f_LeUiField = FindField(c_GUIRoot, "LevelEditorUI");
    if (!f_LeUiField) { Log("!! 编辑器汉化: 找不到 GameUIRoot.LevelEditorUI"); g_cnEditor = 0; return NULL; }

    MonoObject* leui = NULL;
    p_field_get_value(root, f_LeUiField, &leui);
    if (!leui || IsUnityDead(leui)) return NULL;

    if (!f_InspTf) f_InspTf = FindField(p_object_get_class(leui), "InspectorTransform");
    if (!f_InspTf) { Log("!! 编辑器汉化: 找不到 LevelEditorUI.InspectorTransform"); g_cnEditor = 0; return NULL; }

    MonoObject* tf = NULL;
    p_field_get_value(leui, f_InspTf, &tf);
    if (tf && IsUnityDead(tf)) return NULL;
    return tf;
}

static void DoCnEditorTick(void)
{
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 700;

    if (!c_UiText) {
        c_UiText = Cls("UnityEngine.UI", "Text");
        if (!c_UiText) { Log("!! 编辑器汉化: 找不到 UnityEngine.UI.Text"); g_cnEditor = 0; return; }
        m_TxtGet = FindMethod(c_UiText, "get_text", 0);
        m_TxtSet = FindMethod(c_UiText, "set_text", 1);
        if (!m_TxtGet || !m_TxtSet) {
            Log("!! 编辑器汉化: Text.get_text/set_text 没找到");
            g_cnEditor = 0; return;
        }
    }

    MonoObject* panel = CnGetPanelRoot();
    if (!panel) return;

    if (!m_TfCount || !m_TfChild) {
        MonoClass* kt = p_object_get_class(panel);
        m_TfCount = FindMethod(kt, "get_childCount", 0);
        m_TfChild = FindMethod(kt, "GetChild", 1);
        if (!m_TfCount || !m_TfChild) {
            Log("!! 编辑器汉化: Transform.get_childCount/GetChild 没找到");
            g_cnEditor = 0; return;
        }
    }

    g_cnScanNodes = 0;
    g_cnLastCount = 0;
    CnWalkTransform(panel, 0);
}

static MonoImage* g_imgList[64];
static int        g_imgCount   = 0;
static BOOL       g_imgScanned = FALSE;

static void CollectAsmCB(MonoAssembly* a, void* ud)
{
    (void)ud;
    if (g_imgCount >= 64 || !a || !p_assembly_get_image) return;
    MonoImage* im = p_assembly_get_image(a);
    if (im) g_imgList[g_imgCount++] = im;
}

static MonoClass* ClsAny(const char* ns, const char* name)
{
    if (!p_class_from_name) return NULL;
    if (g_image) {
        MonoClass* k = p_class_from_name(g_image, ns, name);
        if (k) return k;
    }
    if (!g_imgScanned && p_assembly_foreach) {
        g_imgScanned = TRUE;
        p_assembly_foreach(CollectAsmCB, NULL);
    }
    for (int i = 0; i < g_imgCount; i++) {
        MonoClass* k = p_class_from_name(g_imgList[i], ns, name);
        if (k) return k;
    }
    return NULL;
}

static MonoClass* ClsScan(const char* ns, const char* name)
{
    if (!name || !p_class_get || !p_image_get_table_info) return NULL;
    if (!g_imgScanned && p_assembly_foreach) {
        g_imgScanned = TRUE;
        p_assembly_foreach(CollectAsmCB, NULL);
    }
    for (int i = 0; i < g_imgCount; i++) {
        MonoImage* im = g_imgList[i];
        if (!im || !p_table_info_get_rows) continue;
        const void* ti = p_image_get_table_info(im, 2  );
        if (!ti) continue;
        int rows = p_table_info_get_rows(ti);
        for (int r = 1; r <= rows; r++) {
            MonoClass* k = p_class_get(im, (2u << 24) | (unsigned int)r);
            if (!k || !p_class_get_name) continue;
            const char* n = p_class_get_name(k);
            if (!n || strcmp(n, name) != 0) continue;
            if (ns) {
                const char* kns = p_class_get_namespace ? p_class_get_namespace(k) : NULL;
                if (!kns || strcmp(kns, ns) != 0) continue;
            }
            return k;
        }
    }
    return NULL;
}

static MonoClass* ClsHard(const char* name)
{
    MonoClass* k = Cls("", name);
    if (k) return k;
    k = ClsScan(NULL, name);
    Log("  [类查找] %s: %s", name, k ? "常规没中, 翻元数据表命中" : "!! 常规+翻表全没中");
    return k;
}

static MonoClass* FindUnityObjectClass(void)
{
    static int tried = 0;
    if (c_UnityObject) return c_UnityObject;
    if (tried) return NULL;
    tried = 1;
    c_UnityObject = ClsAny("UnityEngine", "Object");
    if (!c_UnityObject) c_UnityObject = ClsScan("UnityEngine", "Object");
    if (!c_UnityObject) {
        MonoClass* c = ClsHard("FirstPersonMover");
        int depth = 0;
        while (c && depth++ < 12) {
            const char* nm = p_class_get_name ? p_class_get_name(c) : NULL;
            if (nm && strcmp(nm, "Object") == 0) { c_UnityObject = c; break; }
            c = p_class_get_parent ? p_class_get_parent(c) : NULL;
        }
    }
    Log("  [类查找] UnityEngine.Object: %s", c_UnityObject ? "ok" : "!! 全挂");
    return c_UnityObject;
}

static MonoClass*      c_MiClass     = NULL;
static MonoClassField* f_WmImpact    = NULL;
static MonoClassField* f_WmType      = NULL;
static MonoClassField* f_EdgePt1     = NULL;
static MonoClassField* f_EdgePt2     = NULL;
static MonoMethod*     m_GetLocalScl = NULL;
static MonoMethod*     m_GetLocalPos = NULL;
static MonoMethod*     m_SetLocalPos = NULL;
static MonoMethod*     m_IsChildOf   = NULL;

static MonoObject* s_rMi      = NULL;
static MonoObject* s_rTf      = NULL;
static MonoObject* s_rE2      = NULL;
static Vec3        s_rScale   = { 1.0f, 1.0f, 1.0f };
static Vec3        s_rE2Loc   = { 0.0f, 0.0f, 0.0f };
static int         s_rIsChild = 0;
static BOOL        s_rHasScale = FALSE;
static int         s_rWarmup  = 0;
static int         s_rApplied = 0;

static BOOL TfGetLocalPos(MonoObject* tf, Vec3* out)
{
    if (!tf) return FALSE;
    if (!m_GetLocalPos) m_GetLocalPos = FindMethod(p_object_get_class(tf), "get_localPosition", 0);
    if (!m_GetLocalPos) return FALSE;
    MonoObject* b = Invoke(m_GetLocalPos, tf, NULL, "get_localPosition");
    if (!b) return FALSE;
    Vec3* p = (Vec3*)p_object_unbox(b);
    if (!p) return FALSE;
    *out = *p;
    return TRUE;
}

static void TfSetLocalPos(MonoObject* tf, const Vec3* v)
{
    if (!tf) return;
    if (!m_SetLocalPos) m_SetLocalPos = FindMethod(p_object_get_class(tf), "set_localPosition", 1);
    if (!m_SetLocalPos) return;
    void* a[1]; a[0] = (void*)v;
    Invoke(m_SetLocalPos, tf, a, "set_localPosition");
}

static void ReachRestore(void)
{
    if (s_rTf && !IsUnityDead(s_rTf) && m_SetLocalScale && s_rHasScale) {
        void* a[1]; a[0] = &s_rScale;
        Invoke(m_SetLocalScale, s_rTf, a, "判定范围: 还原 localScale");
    }
    if (s_rE2 && !IsUnityDead(s_rE2) && m_SetLocalPos) {
        void* a[1]; a[0] = &s_rE2Loc;
        Invoke(m_SetLocalPos, s_rE2, a, "判定范围: 还原刃点");
    }
    s_rMi = s_rTf = s_rE2 = NULL;
    s_rIsChild = 0; s_rHasScale = FALSE; s_rApplied = 0; s_rWarmup = 0;
}

static void DoWeaponReachTick(void)
{
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 33;

    MonoObject* pl = TryGetPlayer();
    if (!pl) return;

    if (!c_WeaponModel) c_WeaponModel = Cls("", "WeaponModel");
    if (!c_MiClass)     c_MiClass     = Cls("", "MeleeImpactArea");
    if (!c_WeaponModel || !c_MiClass) return;

    if (!f_CurWeaponMdl) f_CurWeaponMdl = FindField(p_object_get_class(pl), "_currentWeaponModel");
    if (!f_WmImpact)     f_WmImpact     = FindField(c_WeaponModel, "MeleeImpactArea");
    if (!f_WmType)       f_WmType       = FindField(c_WeaponModel, "WeaponType");
    if (!f_CurWeaponMdl || !f_WmImpact || !f_WmType) {
        Log("!! 判定范围: WeaponModel 字段找不到 -> 关闭该功能");
        InterlockedExchange(&g_reachOn, 0);
        return;
    }

    MonoObject* wm = NULL;
    p_field_get_value(pl, f_CurWeaponMdl, &wm);
    if (!wm || IsUnityDead(wm)) return;

    int wt = 0;

    p_field_get_value(wm, f_WmType, &wt);
    if (wt != 1 && wt != 5) return;

    MonoObject* mi = NULL;
    p_field_get_value(wm, f_WmImpact, &mi);
    if (!mi || IsUnityDead(mi)) return;

    float mul = g_reachMul;
    if (mul < 1.0f) mul = 1.0f;
    if (mul > 8.0f) mul = 8.0f;
    if (mul <= 1.001f) {
        if (s_rApplied) ReachRestore();
        return;
    }

    if (mi != s_rMi) {
        ReachRestore();
        s_rMi = mi;
        s_rWarmup = 10;
    }

    if (!s_rTf) {
        if (s_rWarmup > 0) { s_rWarmup--; return; }

        s_rTf = SafeCall(mi, "get_transform", 0, NULL, "MI.get_transform");
        if (!s_rTf) return;
        MonoClass* kt = p_object_get_class(s_rTf);
        if (!m_GetLocalScl)   m_GetLocalScl   = FindMethod(kt, "get_localScale", 0);
        if (!m_SetLocalScale) m_SetLocalScale = FindMethod(kt, "set_localScale", 1);
        if (!m_IsChildOf)     m_IsChildOf     = FindMethod(kt, "IsChildOf", 1);
        if (!m_TfGetPos)      m_TfGetPos      = FindMethod(kt, "get_position", 0);
        if (!m_TfSetPos)      m_TfSetPos      = FindMethod(kt, "set_position", 1);

        if (m_GetLocalScl) {
            MonoObject* b = Invoke(m_GetLocalScl, s_rTf, NULL, "get_localScale");
            if (b) { Vec3* p = (Vec3*)p_object_unbox(b); if (p) { s_rScale = *p; s_rHasScale = TRUE; } }
        }

        if (!f_EdgePt1) f_EdgePt1 = FindField(p_object_get_class(mi), "EdgePoint1");
        if (!f_EdgePt2) f_EdgePt2 = FindField(p_object_get_class(mi), "EdgePoint2");
        if (f_EdgePt1 && f_EdgePt2) {
            MonoObject* e1 = NULL; MonoObject* e2 = NULL;
            p_field_get_value(mi, f_EdgePt1, &e1);
            p_field_get_value(mi, f_EdgePt2, &e2);
            if (e1 && e2 && !IsUnityDead(e1) && !IsUnityDead(e2)) {
                if (TfGetLocalPos(e2, &s_rE2Loc)) s_rE2 = e2;
                if (s_rE2 && m_IsChildOf) {
                    void* a[1]; a[0] = &s_rTf;
                    MonoObject* b = Invoke(m_IsChildOf, s_rE2, a, "IsChildOf");
                    if (b) { int* p = (int*)p_object_unbox(b); if (p) s_rIsChild = (*p & 0xFF); }
                }
            }
        }
        Log("判定范围: 判定物就绪 (武器=%s, 刃线点=%s, 刃在判定物下=%d, 基准缩放=%.2f/%.2f/%.2f)",
            wt == 1 ? "剑" : "矛", s_rE2 ? "有" : "无", s_rIsChild,
            s_rScale.x, s_rScale.y, s_rScale.z);
    }

    if (IsUnityDead(s_rTf) || (s_rE2 && IsUnityDead(s_rE2))) {
        s_rTf = NULL; s_rE2 = NULL; s_rHasScale = FALSE; s_rWarmup = 0;
        return;
    }

    if (!s_rHasScale || !m_SetLocalScale) return;

    Vec3 sc;
    sc.x = s_rScale.x * mul;
    sc.y = s_rScale.y * mul;
    sc.z = s_rScale.z * mul;
    {
        void* a[1]; a[0] = &sc;
        Invoke(m_SetLocalScale, s_rTf, a, "判定范围: set_localScale");
    }

    if (s_rE2 && !s_rIsChild) {
        TfSetLocalPos(s_rE2, &s_rE2Loc);
        MonoObject* e1 = NULL;
        p_field_get_value(s_rMi, f_EdgePt1, &e1);
        if (e1 && !IsUnityDead(e1)) {
            Vec3 p1, p2;
            if (TfGetPos(e1, &p1) && TfGetPos(s_rE2, &p2)) {
                float dx = p2.x - p1.x, dy = p2.y - p1.y, dz = p2.z - p1.z;
                float len = sqrtf(dx * dx + dy * dy + dz * dz);
                if (len > 0.01f) {
                    float extra = len * (mul - 1.0f);
                    Vec3 want;
                    want.x = p2.x + dx / len * extra;
                    want.y = p2.y + dy / len * extra;
                    want.z = p2.z + dz / len * extra;
                    TfSetPos(s_rE2, &want);
                }
            }
        }
    }
    s_rApplied = 1;
}

static MonoClass*      c_LibMgr       = NULL;
static MonoClass*      c_PlaceMgr     = NULL;
static MonoClass*      c_EntryCls     = NULL;
static MonoClass*      c_Camera       = NULL;
static MonoClassField* f_LibUi        = NULL;
static MonoClassField* f_SearchStr    = NULL;
static MonoClassField* f_SearchInp    = NULL;
static MonoClassField* f_EntryPath    = NULL;
static MonoClassField* f_EntryName    = NULL;
static MonoMethod*     m_LibPopulate  = NULL;
static MonoMethod*     m_GetEnemies   = NULL;
static MonoMethod*     m_CreateByPath = NULL;
static MonoMethod*     m_GetCamMain   = NULL;

static MonoClass*      c_Res          = NULL;
static MonoClass*      c_TextAsset    = NULL;
static MonoMethod*     m_ResLoad      = NULL;
static MonoMethod*     m_TAGetText    = NULL;
static volatile LONG   g_edListWant   = 0;

#define JQR_ED_MAX 48
static char          g_edNames[JQR_ED_MAX][72];
static char          g_edPaths[JQR_ED_MAX][160];
static volatile LONG g_edCount = 0;
static int           uiEdSel   = 0;

static MonoObject* GetLevelEditorUI(void)
{
    if (!c_GUIRoot) c_GUIRoot = Cls("", "GameUIRoot");
    if (!c_GUIRoot) return NULL;
    const char* err = "?";
    MonoObject* root = GetSingleton(c_GUIRoot, &err);
    if (!root || IsUnityDead(root)) return NULL;
    if (!f_LeUiField) f_LeUiField = FindField(c_GUIRoot, "LevelEditorUI");
    if (!f_LeUiField) return NULL;
    MonoObject* leui = NULL;
    p_field_get_value(root, f_LeUiField, &leui);
    if (!leui || IsUnityDead(leui)) return NULL;
    return leui;
}

static MonoObject* LevelEditorLib(void)
{
    MonoObject* leui = GetLevelEditorUI();
    if (!leui) return NULL;
    if (!f_LibUi) f_LibUi = FindField(p_object_get_class(leui), "LibraryUI");
    if (!f_LibUi) return NULL;
    MonoObject* lib = NULL;
    p_field_get_value(leui, f_LibUi, &lib);
    if (!lib || IsUnityDead(lib)) return NULL;
    return lib;
}

static const char* JsonFindStr(const char* from, const char* end,
                               const char* key, int* outLen)
{
    size_t klen = strlen(key);
    for (const char* p = from; p + klen + 4 < end; p++) {
        if (*p != '"') continue;
        if (memcmp(p + 1, key, klen) != 0) continue;
        if (p[1 + klen] != '"') continue;
        const char* q = p + klen + 2;
        while (q < end && (*q == ' ' || *q == ':')) q++;
        if (q >= end || *q != '"') continue;
        const char* v = q + 1;
        const char* e = v;
        while (e < end && *e != '"') {
            if (*e == '\\' && e + 1 < end) e++;
            e++;
        }
        if (outLen) *outLen = (int)(e - v);
        return v;
    }
    return NULL;
}

static int ReadEnemyManifest(void)
{
    static int s_err = 0;
    if (!c_Res)       c_Res       = Cls("UnityEngine", "Resources");
    if (!c_TextAsset) c_TextAsset = Cls("UnityEngine", "TextAsset");
    if (!c_Res || !c_TextAsset) {
        if (!s_err) { Log("!! 清单: UnityEngine.Resources / TextAsset 找不到"); s_err = 1; }
        return -1;
    }
    if (!m_ResLoad) m_ResLoad = FindMethod(c_Res, "Load", 1);
    if (!m_ResLoad) {
        if (!s_err) { Log("!! 清单: Resources.Load(string) 找不到"); s_err = 1; }
        return -1;
    }

    char* json = (char*)malloc(768 * 1024);
    if (!json) return -1;
    json[0] = 0;
    int rc = 0;

    DWORD code = 0;
    __try {
        MonoObject* ps = p_string_new(g_domain, "Data/LevelObjects/LevelObjectManifest");
        if (!ps) { rc = -2; __leave; }
        void* a[1]; a[0] = &ps;
        MonoObject* ta = Invoke(m_ResLoad, NULL, a, "Resources.Load(manifest)");
        if (!ta) { rc = -3; __leave; }
        if (!m_TAGetText) m_TAGetText = FindMethod(c_TextAsset, "get_text", 0);
        if (!m_TAGetText) { rc = -4; __leave; }
        MonoObject* ms = Invoke(m_TAGetText, ta, NULL, "TextAsset.get_text");
        if (!ms) { rc = -5; __leave; }
        MonoStrToUtf8(ms, json, 768 * 1024);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        code = GetExceptionCode();
    }
    if (code) { free(json); if (!s_err) { Log("!! 清单: 读 text 抛硬件异常 code=%08X", code); s_err = 1; } return -1; }
    if (rc < 0) {
        free(json);
        if (!s_err) { Log("!! 清单: 读取失败 (rc=%d, 资源路径 Data/LevelObjects/LevelObjectManifest)", rc); s_err = 1; }
        return rc;
    }

    size_t len = strlen(json);
    const char* end = json + len;
    const char* p = json;
    int n = 0;
    while (n < JQR_ED_MAX && p < end) {
        const char* blk = (const char*)memchr(p, '}', (size_t)(end - p));
        if (!blk) break;
        int plen = 0;
        const char* vp = JsonFindStr(p, blk, "PathUnderResources", &plen);
        if (vp && plen > 0) {
            char tmp[200];
            int cl = plen < 190 ? plen : 190;
            memcpy(tmp, vp, cl); tmp[cl] = 0;
            if (strstr(tmp, "EnemySpawns") && !strstr(tmp, "AITriggerArea")) {
                int c2 = plen < (int)sizeof(g_edPaths[0]) - 1 ? plen : (int)sizeof(g_edPaths[0]) - 1;
                memcpy(g_edPaths[n], vp, c2); g_edPaths[n][c2] = 0;
                g_edNames[n][0] = 0;
                int dlen = 0;
                const char* vn = JsonFindStr(p, blk, "DisplayName", &dlen);
                if (vn && dlen > 0) {
                    int c3 = dlen < (int)sizeof(g_edNames[0]) - 1 ? dlen : (int)sizeof(g_edNames[0]) - 1;
                    memcpy(g_edNames[n], vn, c3); g_edNames[n][c3] = 0;
                }
                if (!g_edNames[n][0]) {
                    const char* sl = strrchr(g_edPaths[n], '/');
                    _snprintf_s(g_edNames[n], sizeof(g_edNames[n]), _TRUNCATE, "%s",
                                sl ? (sl + 1) : g_edPaths[n]);
                }
                n++;
            }
        }
        p = blk + 1;
    }
    free(json);
    return n;
}

static void DoEditorListTick(void)
{
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 4000;

    if (!g_edListWant || !GetLevelEditorUI()) {
        if (g_edCount) InterlockedExchange(&g_edCount, 0);
        return;
    }

    int n = ReadEnemyManifest();
    if (n < 0) { if (g_edCount) InterlockedExchange(&g_edCount, 0); return; }
    if (n > JQR_ED_MAX) n = JQR_ED_MAX;
    InterlockedExchange(&g_edCount, n);
}

static void ActEditorEnemyFilter(int reset)
{
    MonoObject* lib = LevelEditorLib();
    if (!lib) { Log("!! 对象库: 没进关卡编辑器 (或 LibraryUI 还没建好)"); return; }
    MonoClass* k = p_object_get_class(lib);
    if (!f_SearchStr)   f_SearchStr   = FindField(k, "SearchString");
    if (!f_SearchInp)   f_SearchInp   = FindField(k, "SearchInput");
    if (!m_LibPopulate) m_LibPopulate = FindMethod(k, "Populate", 0);
    if (!f_SearchStr || !m_LibPopulate) {
        Log("!! 对象库: SearchString / Populate 找不到");
        return;
    }

    MonoObject* s = p_string_new(g_domain, reset ? "" : "EnemySpawns");
    if (!s) return;
    p_field_set_value(lib, f_SearchStr, &s);

    if (f_SearchInp) {
        MonoObject* inp = NULL;
        p_field_get_value(lib, f_SearchInp, &inp);
        if (inp && !IsUnityDead(inp)) {
            MonoMethod* mc = FindMethod(p_object_get_class(inp), "set_text", 1);
            if (mc) { void* a[1]; a[0] = &s; Invoke(mc, inp, a, "InputField.set_text"); }
        }
    }
    Invoke(m_LibPopulate, lib, NULL, "LibraryUI.Populate");
    Log("对象库 -> %s", reset ? "显示全部" : "只显示敌人 (搜 EnemySpawns)");
}

static void ActEditorPlaceEnemy(int idx)
{
    if (!GetLevelEditorUI()) { Log("!! 放置敌人: 没进关卡编辑器"); return; }
    if (!c_PlaceMgr) c_PlaceMgr = Cls("", "LevelEditorObjectPlacementManager");
    if (!c_PlaceMgr) { Log("!! 放置敌人: LevelEditorObjectPlacementManager 找不到"); return; }

    int n = (int)g_edCount;
    if (n <= 0) {
        n = ReadEnemyManifest();
        if (n > 0) InterlockedExchange(&g_edCount, n);
    }
    if (n <= 0) { Log("!! 放置敌人: 敌人清单是空的 (读不到 LevelObjectManifest?)"); return; }
    if (idx < 0 || idx >= n) idx = 0;

    char path[192];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s", g_edPaths[idx]);
    if (!path[0]) { Log("!! 放置敌人: 条目路径为空"); return; }

    const char* err = "?";

    MonoObject* pm = GetSingleton(c_PlaceMgr, &err);
    if (!pm) { Log("!! 放置敌人: PlacementManager %s (关卡真的打开了吗?)", err); return; }
    if (!m_CreateByPath) m_CreateByPath = FindMethod(p_object_get_class(pm), "CreateObjectFromPrefabPath", 1);
    if (!m_CreateByPath) { Log("!! 放置敌人: CreateObjectFromPrefabPath 找不到"); return; }

    MonoObject* ms = p_string_new(g_domain, path);
    if (!ms) return;
    void* a[1]; a[0] = &ms;
    MonoObject* placed = Invoke(m_CreateByPath, pm, a, "CreateObjectFromPrefabPath");
    if (!placed) { Log("!! 放置敌人: 创建失败 (%s)", path); return; }

    BOOL ok = FALSE;
    Vec3 dst;
    if (!c_Camera) c_Camera = ClsAny("UnityEngine", "Camera");
    if (c_Camera) {
        if (!m_GetCamMain) m_GetCamMain = FindMethod(c_Camera, "get_main", 0);
        if (m_GetCamMain) {
            MonoObject* cam = Invoke(m_GetCamMain, NULL, NULL, "Camera.main");
            if (cam && !IsUnityDead(cam)) {
                MonoObject* ctf = SafeCall(cam, "get_transform", 0, NULL, "cam.get_transform");
                if (ctf) {
                    MonoClass* kt = p_object_get_class(ctf);
                    if (!m_TfGetPos) m_TfGetPos = FindMethod(kt, "get_position", 0);
                    if (!m_TfGetFwd) m_TfGetFwd = FindMethod(kt, "get_forward", 0);
                    Vec3 p;
                    if (TfGetPos(ctf, &p) && m_TfGetFwd) {
                        MonoObject* b = Invoke(m_TfGetFwd, ctf, NULL, "get_forward");
                        Vec3* pf = b ? (Vec3*)p_object_unbox(b) : NULL;
                        if (pf) {
                            dst.x = p.x + pf->x * 6.0f;
                            dst.y = p.y + pf->y * 6.0f;
                            dst.z = p.z + pf->z * 6.0f;
                            ok = TRUE;
                        }
                    }
                }
            }
        }
    }
    if (ok) {
        MonoObject* ptf = SafeCall(placed, "get_transform", 0, NULL, "placed.get_transform");
        if (ptf) {
            if (!m_TfSetPos) m_TfSetPos = FindMethod(p_object_get_class(ptf), "set_position", 1);
            TfSetPos(ptf, &dst);
        }
    }
    Log("放置敌人 -> %s  (列表第 %d 项)%s", path, idx,
        ok ? "" : "  [没拿到相机, 位置用了默认值]");
}

static MonoClassField* f_EpCannotFall = NULL;
static MonoClassField* f_EpKickMag    = NULL;
static float           g_epKickBase   = -1.0f;

static float           g_epStunSec    = 3.0f;

static int CallBool0(MonoObject* o, const char* name)
{
    if (!o) return 0;
    MonoObject* b = SafeCall(o, name, 0, NULL, name);
    if (!b) return 0;
    unsigned char* p = (unsigned char*)p_object_unbox(b);
    return (p && *p) ? 1 : 0;
}

typedef void (*EpFoeFn)(MonoObject* c);

static int ForEachFoe(EpFoeFn fn)
{
    MonoObject* list = GetLivingList();
    if (!list) return 0;
    int n = ListCount(list), done = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        if (SkipAlly(c)) continue;
        fn(c);
        done++;
    }
    return done;
}

static void EpFoeExplode(MonoObject* c) { SafeCall(c, "ExplodeCharacter", 0, NULL, "ExplodeCharacter"); }
static void EpFoeFling(MonoObject* c)
{
    Vec3 up = { 0.0f, 24.0f, 0.0f };
    void* a[1]; a[0] = &up;
    SafeCall(c, "AddVelocity", 1, a, "AddVelocity(上天)");
}
static void EpFoeStun(MonoObject* c)
{
    float t = g_epStunSec;
    void* a[1]; a[0] = &t;
    SafeCall(c, "SetStunned", 1, a, "SetStunned");
}
static void EpFoeFall(MonoObject* c)
{
    Vec3 d = { 0.0f, 0.0f, -1.0f };
    float pw = 2.2f, sd = 2.0f;
    void* a[3]; a[0] = &d; a[1] = &pw; a[2] = &sd;
    SafeCall(c, "SendFalling", 3, a, "SendFalling");
}

static void ActEpExplodeAll(void) { int n = ForEachFoe(EpFoeExplode); Log("恶作剧 -> 全场 %d 个敌人炸成零件", n); }
static void ActEpFlingAll(void)   { int n = ForEachFoe(EpFoeFling);   Log("恶作剧 -> 全场 %d 个敌人被扔上天", n); }
static void ActEpStunAll(float s) { g_epStunSec = s; int n = ForEachFoe(EpFoeStun); Log("恶作剧 -> 全场 %d 个敌人眩晕 %.1f 秒", n, s); }
static void ActEpFallAll(void)    { int n = ForEachFoe(EpFoeFall);    Log("恶作剧 -> 全场 %d 个敌人集体扑街", n); }

static void FreeAllSweep(void)
{
    MonoObject* list = GetLivingList();
    if (!list) return;
    int n = ListCount(list), clr = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        if (!f_ImmuneToDmg) f_ImmuneToDmg = FindField(p_object_get_class(c), "_isImmuneToDamage");
        if (!f_ImmuneToDmg) continue;
        unsigned char im = 0;
        p_field_get_value(c, f_ImmuneToDmg, &im);
        if (!im) continue;
        SafeCall(c, "MakeNotImmuneToDamage", 0, NULL, "解除无敌");
        clr++;
    }
    if (clr) Log("无差别攻击 -> 拆掉 %d 个角色的无敌保护", clr);
}

static void DoFreeAllTick(void)
{
    if (!g_freeAll) return;
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 250;
    FreeAllSweep();
}

static void ActFreeAll(int on)
{
    InterlockedExchange(&g_freeAll, on ? 1 : 0);
    if (on) {
        FreeAllSweep();
        Log("无差别攻击 -> 开 (敌我通吃: 友军 / 召唤物 / 剧情角色照打, 一切无敌强制解除)");
    } else {
        Log("无差别攻击 -> 关 (恢复只打敌人, 游戏自己的保护不再干预)");
    }
}

static void LaserMoveSweep(void)
{
    MonoObject* list = GetLivingList();
    if (!list) return;
    int n = ListCount(list), cnt = 0;

    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;

        MonoObject* fpm = GetComponentT(c, c_FPM, "GetComponent");
        if (!fpm) fpm = GetComponentT(c, c_FPM, "GetComponentInChildren");
        if (!fpm) continue;

        int ab = 24;
        void* aa[1]; aa[0] = &ab;
        MonoObject* b = SafeCall(fpm, "IsUsingAbility", 1, aa, "查激光");
        if (!b) continue;
        int* pv = (int*)p_object_unbox(b);
        if (!pv || !(*pv & 0xFF)) continue;

        unsigned char on = 1, off = 0;
        void* ka[1]; ka[0] = &on;
        void* kb[1]; kb[0] = &off;
        SafeCall(fpm, "SetUpKeyDown",   1, ka, "激光期: 前进键");
        SafeCall(fpm, "SetDownKeyDown", 1, kb, "激光期: 松后退键");
        cnt++;
    }
    if (cnt) Log("激光可移动 -> %d 个正在激光的敌人边走边射", cnt);
}

static void DoLaserMoveTick(void)
{
    if (!g_laserMove) return;
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 50;
    LaserMoveSweep();
}

static MonoObject* s_wlTf      = NULL;
static Vec3        s_wlScale   = { 1.0f, 1.0f, 1.0f };
static int         s_wlAxis    = 1;
static BOOL        s_wlHas     = FALSE;
static int         s_wlApplied = 0;
static MonoMethod* m_InvTfDir  = NULL;

static void WLenRestore(void)
{
    if (s_wlTf && !IsUnityDead(s_wlTf) && m_SetLocalScale && s_wlHas) {
        void* a[1]; a[0] = &s_wlScale;
        Invoke(m_SetLocalScale, s_wlTf, a, "武器加长: 还原 localScale");
    }
    s_wlTf = NULL; s_wlHas = FALSE; s_wlApplied = 0;
}

static int WLenPickAxis(MonoObject* tf, MonoObject* wm)
{

    int manual = (int)InterlockedCompareExchange(&g_wlenAxisSel, -1, -1);
    if (manual >= 0 && manual <= 2) {
        if (s_wlDiag++ < 6) Log("武器加长: 长度轴 = %c (手动指定)", "xyz"[manual]);
        return manual;
    }

    Vec3 dir;
    dir.x = 0.0f; dir.y = 0.0f; dir.z = 0.0f;
    int got = 0;

    if (!f_WmImpact) f_WmImpact = FindField(p_object_get_class(wm), "MeleeImpactArea");
    MonoObject* mi = NULL;
    if (f_WmImpact) p_field_get_value(wm, f_WmImpact, &mi);
    if (mi && !IsUnityDead(mi)) {
        if (!f_MiaCollider) f_MiaCollider = FindField(p_object_get_class(mi), "_collider");
        MonoObject* col = NULL;
        if (f_MiaCollider) p_field_get_value(mi, f_MiaCollider, &col);
        if (col && !IsUnityDead(col)) {
            if (!m_ColGetBounds) m_ColGetBounds = FindMethod(p_object_get_class(col), "get_bounds", 0);
            if (m_ColGetBounds) {
                MonoObject* b = Invoke(m_ColGetBounds, col, NULL, "Collider.get_bounds");
                float* f = b ? (float*)p_object_unbox(b) : NULL;
                if (f) {

                    float ex = f[3], ey = f[4], ez = f[5];
                    if (ex > ey && ex > ez)      { dir.x = 1.0f; got = 1; }
                    else if (ey > ez)            { dir.y = 1.0f; got = 1; }
                    else                         { dir.z = 1.0f; got = 1; }
                    if (got && s_wlDiag++ < 6)
                        Log("武器加长: collider AABB extents = %.3f/%.3f/%.3f -> 世界轴 %c",
                            ex, ey, ez, got ? "xyz"[(int)(dir.x > 0 ? 0 : (dir.y > 0 ? 1 : 2))] : '?');
                }
            }
        }
    }

    if (!got) {
        if (s_wlDiag++ < 6) Log("武器加长: !! 拿不到判定体包围盒 -> 长度轴退回 y (可能是错的, 请在菜单里手动指定)");
        return 1;
    }

    Vec3 ld = dir;
    if (!m_InvTfDir) m_InvTfDir = FindMethod(p_object_get_class(tf), "InverseTransformDirection", 1);
    if (m_InvTfDir) {
        void* a[1]; a[0] = &dir;
        MonoObject* b = Invoke(m_InvTfDir, tf, a, "InverseTransformDirection");
        Vec3* p = b ? (Vec3*)p_object_unbox(b) : NULL;
        if (p) ld = *p;
    }
    float ax = fabsf(ld.x), ay = fabsf(ld.y), az = fabsf(ld.z);
    int best;
    if      (ax >= ay && ax >= az) best = 0;
    else if (ay >= az)             best = 1;
    else                           best = 2;
    if (s_wlDiag++ < 6) Log("武器加长: 长度轴 = %c (本地分量 %.3f/%.3f/%.3f)", "xyz"[best], ax, ay, az);
    return best;
}

static void DoWeaponLenTick(void)
{
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 33;

    MonoObject* pl = TryGetPlayer();
    if (!pl) return;

    if (!c_WeaponModel) c_WeaponModel = Cls("", "WeaponModel");
    if (!c_WeaponModel) return;
    if (!f_CurWeaponMdl) f_CurWeaponMdl = FindField(p_object_get_class(pl), "_currentWeaponModel");
    if (!f_WmType)       f_WmType       = FindField(c_WeaponModel, "WeaponType");
    if (!f_CurWeaponMdl || !f_WmType) {
        Log("!! 武器加长: WeaponModel 字段找不到 -> 关闭该功能");
        InterlockedExchange(&g_wlenOn, 0);
        return;
    }

    MonoObject* wm = NULL;
    p_field_get_value(pl, f_CurWeaponMdl, &wm);
    if (!wm || IsUnityDead(wm)) { if (s_wlApplied) WLenRestore(); return; }

    int wt = 0;
    p_field_get_value(wm, f_WmType, &wt);
    if (wt != 1 && wt != 5) {
        if (s_wlApplied) WLenRestore();
        return;
    }

    float mul = g_wlenMul;
    if (mul < 1.0f) mul = 1.0f;
    if (mul > 8.0f) mul = 8.0f;

    MonoObject* tf = SafeCall(wm, "get_transform", 0, NULL, "武器根 transform");
    if (!tf || IsUnityDead(tf)) { if (s_wlApplied) WLenRestore(); return; }

    if (tf != s_wlTf) {
        WLenRestore();
        MonoClass* kt = p_object_get_class(tf);
        if (!m_GetLocalScl)   m_GetLocalScl   = FindMethod(kt, "get_localScale", 0);
        if (!m_SetLocalScale) m_SetLocalScale = FindMethod(kt, "set_localScale", 1);
        s_wlTf  = tf;
        s_wlHas = FALSE;
        if (m_GetLocalScl) {
            MonoObject* b = Invoke(m_GetLocalScl, tf, NULL, "get_localScale");
            Vec3* p = b ? (Vec3*)p_object_unbox(b) : NULL;
            if (p) { s_wlScale = *p; s_wlHas = TRUE; }
        }
        s_wlAxis = WLenPickAxis(tf, wm);
        Log("武器加长: 就绪 (武器=%s, 原缩放=%.2f/%.2f/%.2f, 拉长轴=%c)",
            wt == 1 ? "剑" : "矛", s_wlScale.x, s_wlScale.y, s_wlScale.z, "xyz"[s_wlAxis]);
    }

    if (!s_wlHas || !m_SetLocalScale) return;

    Vec3 sc = s_wlScale;
    if      (s_wlAxis == 0) sc.x *= mul;
    else if (s_wlAxis == 1) sc.y *= mul;
    else                    sc.z *= mul;
    {
        void* a[1]; a[0] = &sc;
        Invoke(m_SetLocalScale, s_wlTf, a, "武器加长: set_localScale");
    }
    s_wlApplied = 1;
}

static MonoMethod*     m_SetBladeWidth = NULL;
static MonoClassField* f_BladeScaleTf  = NULL;
static MonoMethod*     m_ArrowGetOwner = NULL;

static void DoArrowBigTick(void)
{
    {
        DWORD now = GetTickCount();
        if (now < s_arrowNext) return;
        s_arrowNext = now + 250;
    }
    if (!c_ArrowProj && !s_arrowTried) {
        c_ArrowProj = Cls("", "ArrowProjectile");
        if (!c_ArrowProj) c_ArrowProj = ClsScan("", "ArrowProjectile");
        s_arrowTried = 1;
        Log("弓箭放大: ArrowProjectile %s", c_ArrowProj ? "ok" : "!! 没找到");
    }
    if (!c_ArrowProj || !p_array_addr_with_size) return;

    float mul = g_arrowMul;
    if (mul < 1.0f)  mul = 1.0f;
    if (mul > 20.0f) mul = 20.0f;
    float lenMul = g_arrowLenMul;
    if (lenMul < 1.0f)  lenMul = 1.0f;
    if (lenMul > 20.0f) lenMul = 20.0f;

    int on = (int)InterlockedCompareExchange(&g_arrowBig, 0, 0);

    if (!m_SetBladeWidth) {
        m_SetBladeWidth = FindMethod(c_ArrowProj, "SetBladeWidth", 1);
        Log("弓箭放大: SetBladeWidth = %s", m_SetBladeWidth ? "ok (官方宽度接口)" : "!! 没找到");
    }
    if (!f_BladeScaleTf) f_BladeScaleTf = FindField(c_ArrowProj, "BladeScaleTransforms");

    uintptr_t n = 0;
    void* arr = FindAllOfType(c_ArrowProj, &n);
    if (!arr) return;
    char* base = p_array_addr_with_size(arr, (int)sizeof(void*), 0);
    if (!base) return;

    MonoObject* me = on ? GetPlayerCharacter() : NULL;

    int hit = 0, skipped = 0;
    for (uintptr_t i = 0; i < n && i < 128; i++) {
        MonoObject* ap = *(MonoObject**)(base + i * sizeof(void*));
        if (!ap || IsUnityDead(ap)) continue;

        if (on) {
            if (!m_ArrowGetOwner) m_ArrowGetOwner = FindMethod(p_object_get_class(ap), "GetOwner", 0);
            MonoObject* ow = NULL;
            if (m_ArrowGetOwner) {
                MonoObject* ex = NULL;
                ow = p_runtime_invoke(m_ArrowGetOwner, ap, NULL, &ex);
                if (ex) ow = NULL;
            }
            if (!me || ow != me) { skipped++; continue; }
        }

        if (m_SetBladeWidth) {
            float w = on ? mul : 1.0f;
            void* a[1]; a[0] = &w;
            Invoke(m_SetBladeWidth, ap, a, "弓箭放大: SetBladeWidth");
        }

        if (f_BladeScaleTf && p_array_length && p_array_addr_with_size) {
            MonoObject* ta = NULL;
            p_field_get_value(ap, f_BladeScaleTf, &ta);
            if (ta && !IsUnityDead(ta)) {
                uintptr_t tn = p_array_length(ta);
                char* tb = p_array_addr_with_size(ta, (int)sizeof(void*), 0);
                for (uintptr_t j = 0; tb && j < tn && j < 16; j++) {
                    MonoObject* t = *(MonoObject**)(tb + j * sizeof(void*));
                    if (!t || IsUnityDead(t)) continue;
                    if (!m_SetLocalScale) m_SetLocalScale = FindMethod(p_object_get_class(t), "set_localScale", 1);
                    if (!m_SetLocalScale) break;
                    Vec3 sc;
                    if (on) {
                        float zz = 0.5f * mul; if (zz < 1.0f) zz = 1.0f;
                        sc.x = mul;  sc.y = lenMul; sc.z = zz;
                    } else {
                        sc.x = 1.0f; sc.y = 1.0f;   sc.z = 1.0f;
                    }
                    void* a[1]; a[0] = &sc;
                    Invoke(m_SetLocalScale, t, a, "弓箭放大: 刃部 set_localScale");
                }
            }
        }
        hit++;
    }
    if ((hit || skipped) && s_wlDiag++ < 40)
        Log("弓箭放大: %s, 玩家箭 %d 支 (宽 %.1f 倍 / 长 %.1f 倍), 跳过的敌方箭 %d 支",
            on ? "开" : "关", hit, mul, lenMul, skipped);
}

static void ActEpExplodeSel(MonoObject* c)
{
    if (!MustAlive(c, "炸碎")) return;
    SafeCall(c, "ExplodeCharacter", 0, NULL, "ExplodeCharacter");
    Log("恶作剧 -> 把 [%s] 炸成零件", PickNameOf(c));
}
static void ActEpFlingSel(MonoObject* c)
{
    if (!MustAlive(c, "扔上天")) return;
    Vec3 up = { 0.0f, 24.0f, 0.0f };
    void* a[1]; a[0] = &up;
    SafeCall(c, "AddVelocity", 1, a, "AddVelocity(上天)");
    Log("恶作剧 -> 把 [%s] 扔上天", PickNameOf(c));
}
static void ActEpStunSel(MonoObject* c)
{
    if (!MustAlive(c, "眩晕")) return;
    float t = g_epStunSec;
    void* a[1]; a[0] = &t;
    SafeCall(c, "SetStunned", 1, a, "SetStunned");
    Log("恶作剧 -> 眩晕 [%s] %.1f 秒", PickNameOf(c), t);
}

static void ActEpSuperKick(int on, float mul)
{
    if (on) g_epKickMul = mul;
    InterlockedExchange(&g_epSuperKick, on ? 1 : 0);
    if (!on) {
        MonoObject* fpm = PlayerFPM();
        if (fpm && g_epKickBase > 0.0f) {
            if (!f_EpKickMag) f_EpKickMag = FindField(p_object_get_class(fpm), "KickVelocityMagnitude");
            if (f_EpKickMag) { float v = g_epKickBase; p_field_set_value(fpm, f_EpKickMag, &v); }
        }
        Log("恶作剧 -> 超级踢 关 (踢击力度已还原)");
    } else {
        Log("恶作剧 -> 超级踢 开 (%.1fx, 一脚把敌人踹出老远)", g_epKickMul);
    }
}

static void ActEpNoFall(int on)
{
    InterlockedExchange(&g_epNoFall, on ? 1 : 0);
    if (!on) {
        MonoObject* fpm = PlayerFPM();
        if (fpm) {
            if (!f_EpCannotFall) f_EpCannotFall = FindField(p_object_get_class(fpm), "CannotFallOver");
            if (f_EpCannotFall) { int v = 0; p_field_set_value(fpm, f_EpCannotFall, &v); }
        }
    }
    Log("恶作剧 -> 永不摔倒 %s", on ? "开 (被打也不会躺下)" : "关");
}

static void ActEpRunAnim(float mul)
{
    g_epRunMul = mul;
    MonoObject* fpm = PlayerFPM();
    if (!fpm) { Log("!! 跑步鬼畜: 找不到玩家 (要先在关卡里)"); return; }
    float v = mul;
    void* a[1]; a[0] = &v;
    SafeCall(fpm, "SetRunAnimationSpeedMultiplier", 1, a, "SetRunAnimationSpeedMultiplier");
    Log("恶作剧 -> 奔跑动画 %.1fx (腿要甩飞了)", mul);
}

static void ActEpBow(void)
{
    MonoObject* fpm = PlayerFPM();
    if (!fpm) { Log("!! 凭空拉弓: 找不到玩家"); return; }
    int id = -1;
    void* a[1]; a[0] = &id;
    SafeCall(fpm, "CreateArrowAndDrawBow", 1, a, "CreateArrowAndDrawBow");
    Log("恶作剧 -> 凭空拉弓 (手上没弓也能射一箭)");
}

static void ActEpCine(float deg)
{
    MonoObject* fpm = PlayerFPM();
    if (!fpm) { Log("!! 电影镜头: 找不到玩家"); return; }
    float v = deg;
    void* a[1]; a[0] = &v;
    SafeCall(fpm, "SetCinematicCameraRotation", 1, a, "SetCinematicCameraRotation");
    Log("恶作剧 -> 电影镜头转到 %.0f 度", deg);
}

static void ActEpCineOff(void)
{
    MonoObject* fpm = PlayerFPM();
    if (!fpm) return;
    SafeCall(fpm, "DisableCinematicCameraRotation", 0, NULL, "DisableCinematicCameraRotation");
    Log("恶作剧 -> 电影镜头 关");
}

static Vec3          g_tpPos;
static int           g_tpValid = 0;

static void ActGameSpeed(int percent)
{

    if (percent <= 0 || percent == 100) {
        InterlockedExchange(&g_tsOn, 0);
        g_tsValue = 1.0f;
        if (!g_slowOn) ActTimeScaleEx(1.0f, FALSE);
        Log("游戏速度 -> 恢复原速 1.00x (时标控制权已交还)");
        return;
    }
    if (percent < 10)   percent = 10;
    if (percent > 500)  percent = 500;
    g_tsValue = (float)percent / 100.0f;
    InterlockedExchange(&g_slowOn, 0);
    InterlockedExchange(&g_tsOn, 1);
    ActTimeScaleEx(g_tsValue, FALSE);
    Log("游戏速度 -> %.2fx  (%s)", g_tsValue,
        g_tsValue > 1.0f ? "快进" : (g_tsValue < 1.0f ? "慢动作" : "原速"));
}

static int GetCharPos(MonoObject* c, Vec3* out)
{
    if (!c || !out) return 0;
    MonoObject* v = SafeCall(c, "GetPositionOfUsOrMount", 0, NULL, "GetPositionOfUsOrMount");
    if (!v) return 0;
    Vec3* p = (Vec3*)p_object_unbox(v);
    if (!p) return 0;
    *out = *p;
    return 1;
}

static void DoAimTick(void)
{
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 50;
    MonoObject* me = TryGetPlayer();
    if (!me) return;
    MonoObject* foe = PickNearestChar(me, 1, NULL, NULL, 1);
    if (!foe || IsUnityDead(foe)) return;
    Vec3 p;
    if (!GetCharPos(foe, &p)) return;
    MonoObject* fpm = CharFPM(me);
    if (!fpm) return;
    void* a[1]; a[0] = &p;
    SafeCall(fpm, "InstantlyOrientTowards", 1, a, "自动瞄准");
}

static void DoAutoAtkTick(void)
{
    static DWORD s_next = 0;
    static int   s_up   = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    MonoObject* me = TryGetPlayer();
    if (!me) return;
    int one = 1;
    void* a[1]; a[0] = &one;
    if (s_up) {
        SafeCall(me, "SetAttackKeyUp", 1, a, "自动攻击: 松手");
        s_up = 0; s_next = now + 400;
    } else {
        SafeCall(me, "SetAttackKeyDown", 1, a, "自动攻击: 出手");
        s_up = 1; s_next = now + 120;
    }
}

static void ActBlink(void)
{
    MonoObject* me = TryGetPlayer();
    if (!me) { Log("!! 闪现: 找不到玩家"); return; }
    MonoObject* foe = PickNearestChar(me, 1, NULL, NULL, 1);
    if (!foe || IsUnityDead(foe)) { Log("!! 闪现: 附近没有敌人"); return; }
    Vec3 p;
    if (!GetCharPos(foe, &p)) { Log("!! 闪现: 拿不到目标位置"); return; }
    p.z -= 1.5f;
    void* a[1]; a[0] = &p;
    SafeCall(me, "SetPositionOfUsOrMount", 1, a, "闪现");
    Log("闪现 -> 已贴到最近敌人背后");
}

static void EpFoeEmp(MonoObject* c)
{
    Vec3 d = { 0.0f, 1.0f, 0.0f };
    void* a[1]; a[0] = &d;
    SafeCall(c, "DestroyAllArmor", 1, a, "DestroyAllArmor");
}
static void ActEmpAll(void)
{
    int n = ForEachFoe(EpFoeEmp);
    Log("全场碎甲 -> %d 个敌人的护甲被 EMP 剥光", n);
}

static void ActSuperJump(void)
{
    MonoObject* fpm = PlayerFPM();
    if (!fpm) { Log("!! 超级跳: 找不到玩家"); return; }
    Vec3 v = { 0.0f, 20.0f, 0.0f };
    void* a[1]; a[0] = &v;
    SafeCall(fpm, "TriggerJump", 1, a, "TriggerJump");
    SafeCall(fpm, "AddVelocity", 1, a, "AddVelocity(起跳)");
    Log("超级跳 -> 一脚蹬上天 (20 m/s)");
}

static void EpFoeNoCount(MonoObject* c)
{
    int v = (int)g_noCountOn;
    void* a[1]; a[0] = &v;
    SafeCall(c, "SetIgnoreForVictoryCount", 1, a, "SetIgnoreForVictoryCount");
}
static void ActNoCount(int on)
{
    InterlockedExchange(&g_noCountOn, on ? 1 : 0);
    int n = ForEachFoe(EpFoeNoCount);
    Log("速通 -> %d 个敌人 %s计入胜利条件", n, on ? "不再" : "重新");
}

static void EpFoeTeleport(MonoObject* c)
{
    if (!g_tpValid) return;
    void* a[1]; a[0] = &g_tpPos;
    SafeCall(c, "SetPositionOfUsOrMount", 1, a, "拉怪");
}
static void ActTeleportAll(void)
{
    MonoObject* me = TryGetPlayer();
    if (!me) { Log("!! 拉怪: 找不到玩家"); return; }
    Vec3 p;
    if (!GetCharPos(me, &p)) { Log("!! 拉怪: 拿不到玩家位置"); return; }
    p.z += 3.0f;
    g_tpPos = p; g_tpValid = 1;
    int n = ForEachFoe(EpFoeTeleport);
    g_tpValid = 0;
    Log("拉怪 -> %d 个敌人被拽到面前 (省得一个个追)", n);
}

static const char* WeaponNameCN2(int wt)
{
    switch (wt) {
    case 0:  return "空手";
    case 1:  return "剑";
    case 2:  return "弓";
    case 3:  return "锤";
    case 4:  return "手雷";
    case 5:  return "矛";
    case 6:  return "盾";
    case 7:  return "双剑";
    case 8:  return "双锤";
    case 9:  return "左爪";
    case 10: return "右爪";
    case 11: return "迅猛龙撕咬";
    case 12: return "锯片";
    default: return "?";
    }
}

static void ActDbgFlag(MonoClassField** cache, const char* field, int on)
{
    if (!c_DebugMgr) c_DebugMgr = Cls("", "DebugManager");
    if (!c_DebugMgr) { Log("!! %s: 找不到 DebugManager", field); return; }
    const char* err = "?";
    MonoObject* dm = GetSingleton(c_DebugMgr, &err);
    if (!dm) { Log("!! %s: DebugManager 实例拿不到 (%s)", field, err); return; }
    if (!*cache) *cache = FindField(c_DebugMgr, field);
    if (!*cache) { Log("!! %s 字段找不到", field); return; }
    bool v = (on != 0);
    p_field_set_value(dm, *cache, &v);
    Log("开发者开关 -> %s = %d", field, on);
}

static void ActRecruitAll(void)
{
    if (!c_Character) c_Character = Cls("", "Character");
    if (!c_Character) return;
    if (!f_IsPlayerTeam) f_IsPlayerTeam = FindField(c_Character, "IsPlayerTeam");
    if (!f_IsPlayerTeam) { Log("!! 全场招安: IsPlayerTeam 字段找不到"); return; }

    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        if (CharIsPlayerTeam(c)) continue;
        int one = 1;
        p_field_set_value(c, f_IsPlayerTeam, &one);
        cnt++;
    }
    Log("全场招安 -> %d 个敌人变成你的兵 (他们转头打自己人了)", cnt);
}

static void ActScaleAll(int percent)
{
    if (percent < 20)  percent = 20;
    if (percent > 300) percent = 300;
    float s = (float)percent / 100.0f;
    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        if (SkipAlly(c)) continue;
        MonoObject* tf = SafeCall(c, "get_transform", 0, NULL, "get_transform");
        if (!tf) continue;
        Vec3 v; v.x = v.y = v.z = s;
        void* a[1]; a[0] = &v;
        SafeCall(tf, "set_localScale", 1, a, "set_localScale");
        cnt++;
    }
    Log("全场体型 -> %d 个敌人缩放到 %d%%", cnt, percent);
}

static void ActForceWeaponAll(int wt)
{
    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        if (SkipAlly(c)) continue;
        MonoObject* fpm = CharFPM(c);
        if (!fpm) continue;
        int w = wt, cd = 0;
        void* a[2]; a[0] = &w; a[1] = &cd;
        SafeCall(fpm, "SetEquippedWeaponType", 2, a, "SetEquippedWeaponType");
        cnt++;
    }
    Log("全场换武器 -> %d 个敌人被强制换成 [%s]", cnt, WeaponNameCN2(wt));
}

static void ActKillNearest(void)
{
    MonoObject* me = TryGetPlayer();
    if (!me) { Log("!! 秒最近: 玩家不在关卡里"); return; }
    MonoObject* foe = PickNearestChar(me, 1, NULL, NULL, 1);
    if (!foe || IsUnityDead(foe)) { Log("!! 秒最近: 附近没有敌人"); return; }
    const char* nm = PickNameOf(foe);
    if (CharKill(foe)) Log("秒杀 -> 最近的 [%s] 已处决", nm);
    else               Log("!! 秒杀: [%s] 没杀掉 (可能本来就死了)", nm);
}

static void ActSuicide(void)
{
    MonoObject* me = TryGetPlayer();
    if (!me) { Log("!! 自杀: 玩家不在关卡里"); return; }
    if (CharKill(me)) Log("自杀 -> 玩家已倒下, 等克隆体接管 (等于快速重开这一波)");
    else              Log("!! 自杀: 没杀掉 (可能已经死了)");
}

static void ActMaxSkill(void)
{
    if (!c_UpgradeMgr) c_UpgradeMgr = Cls("", "UpgradeManager");
    if (!c_UpgradeMgr) { Log("!! 找不到 UpgradeManager"); return; }
    const char* err = "?";
    MonoObject* um = GetSingleton(c_UpgradeMgr, &err);
    if (!um) { Log("!! UpgradeManager 实例拿不到 (%s)", err); return; }
    MonoMethod* m = FindMethod(c_UpgradeMgr, "SetAvailableSkillPoints", 1);
    if (!m) { Log("!! SetAvailableSkillPoints 找不到"); return; }
    int v = 99;
    void* a[1]; a[0] = &v;
    Invoke(m, um, a, "SetAvailableSkillPoints");
    Log("技能点 -> SetAvailableSkillPoints(99) 已调用 (去升级站随便点)");
}

static void ActTakeAllWeapon(void)
{
    ActDisarmEnemies();
    ActSetWeapon(1);
    Log("抢武器 -> 敌人武器全丢, 你手里已换成剑");
}

static MonoClassField* f_MinTilt   = NULL;
static MonoClassField* f_MaxTilt   = NULL;
static float s_tiltMin = 0.0f, s_tiltMax = 0.0f;
static int   s_tiltHas = 0;

static void ActCamTilt(int on)
{
    MonoObject* fpm = PlayerFPM();
    if (!fpm) { Log("!! 镜头俯仰: 找不到玩家"); return; }
    MonoClass* k = p_object_get_class(fpm);
    if (!f_MinTilt) f_MinTilt = FindField(k, "MinCameraTilt");
    if (!f_MaxTilt) f_MaxTilt = FindField(k, "MaxCameraTilt");
    if (!f_MinTilt || !f_MaxTilt) { Log("!! 镜头俯仰: 字段找不到 (换武器时可能改过类)"); return; }

    if (!s_tiltHas) {
        p_field_get_value(fpm, f_MinTilt, &s_tiltMin);
        p_field_get_value(fpm, f_MaxTilt, &s_tiltMax);
        s_tiltHas = 1;
        Log("镜头俯仰 -> 基准 [%.1f, %.1f] 度", s_tiltMin, s_tiltMax);
    }
    float a = on ? -89.0f : s_tiltMin;
    float b = on ?  89.0f : s_tiltMax;
    p_field_set_value(fpm, f_MinTilt, &a);
    p_field_set_value(fpm, f_MaxTilt, &b);
    Log("镜头俯仰 -> %s [%.0f, %.0f] 度", on ? "已解锁" : "已还原", a, b);
}

static void ActFoeSight(int mode)
{
    if (!c_AI) c_AI = Cls("", "AIController");
    if (!c_AI) { Log("!! 敌人视野: AIController 类找不到"); return; }
    if (!f_AIMaxDist) f_AIMaxDist = FindField(c_AI, "MaxEnemyTargetDistance");
    if (!f_AIMaxDist) { Log("!! 敌人视野: MaxEnemyTargetDistance 字段找不到"); return; }

    float v = (mode == 0) ? 2.0f : -1.0f;
    MonoObject* me   = GetPlayerCharacter();
    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || c == me || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        MonoObject* ai = CharAI(c);
        if (!ai) continue;
        __try { p_field_set_value(ai, f_AIMaxDist, &v); cnt++; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    Log("敌人视野 -> %s (%d 个 AI)", mode == 0 ? "变瞎: 只咬 2 米内的东西" : "疯狗: 无限追击", cnt);
}

static MonoClass* c_FireMgr = NULL;

static void ActBurnAll(void)
{
    if (!c_FireMgr) c_FireMgr = Cls("", "FireManager");
    if (!c_FireMgr) { Log("!! 点燃: FireManager 类找不到"); return; }
    const char* err = "?";
    MonoObject* fm = GetSingleton(c_FireMgr, &err);
    if (!fm) { Log("!! 点燃: FireManager 实例拿不到 (%s)", err); return; }

    MonoMethod* gm = FindMethod(c_FireMgr, "GetFireSpreadDefinition", 1);
    if (!gm) { Log("!! 点燃: GetFireSpreadDefinition 找不到"); return; }
    int ft = 2;
    void* ga[1]; ga[0] = &ft;
    MonoObject* def = Invoke(gm, fm, ga, "GetFireSpreadDefinition");
    if (!def) { Log("!! 点燃: 火种定义为空"); return; }

    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        if (SkipAlly(c)) continue;
        Vec3 pos;
        if (!GetCharPos(c, &pos)) continue;
        MonoObject* parts = SafeCall(c, "GetAllBaseBodyParts", 0, NULL, "GetAllBaseBodyParts");
        if (!parts) continue;
        MonoObject* bp = ListItem(parts, 0);
        if (!bp) continue;
        int dst = 0;
        void* a[3]; a[0] = &pos; a[1] = &def; a[2] = &dst;
        DWORD code = 0;
        __try { SafeCall(bp, "TrySetOnFire", 3, a, "TrySetOnFire"); }
        __except (EXCEPTION_EXECUTE_HANDLER) { code = GetExceptionCode(); }
        if (!code) cnt++;
    }
    Log("全场点燃 -> %d 个敌人着了 (火会自己往身上蔓延)", cnt);
}

static void ActStunAll(float sec)
{
    if (sec < 0.2f) sec = 0.2f;
    if (sec > 30.0f) sec = 30.0f;
    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        void* a[1]; a[0] = &sec;
        SafeCall(c, "SetStunned", 1, a, "SetStunned");
        cnt++;
    }
    Log("全场眩晕 -> %d 个角色被定住 %.1f 秒 (这段时间随便砍)", cnt, sec);
}

static void ActGodAll(int on)
{
    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        SafeCall(c, on ? "MakeImmuneToDamage" : "MakeNotImmuneToDamage", 0, NULL, "全场无敌");
        cnt++;
    }
    Log("全场无敌 -> %d 个敌人%s", cnt,
        on ? "现在打不死 (配合乱斗看它们打一辈子)" : "恢复可被伤害");
}

static void ActRbFormation(float radius)
{
    if (radius < 4.0f)  radius = 4.0f;
    if (radius > 60.0f) radius = 60.0f;

    MonoObject* me = GetPlayerCharacter();
    Vec3 cen; cen.x = 0.0f; cen.y = 2.0f; cen.z = 0.0f;
    if (me) GetCharPos(me, &cen);

    MonoObject* list = GetLivingList();
    int n = ListCount(list);
    MonoObject* arr[JQR_TEAM_MAX];
    int k = 0;
    for (int i = 0; i < n && k < JQR_TEAM_MAX; i++) {
        MonoObject* x = ListItem(list, i);
        if (!x || x == me || IsUnityDead(x)) continue;
        if (CallBool0(x, "IsPlayer")) continue;
        arr[k++] = x;
    }
    if (k < 2) { Log("!! 布阵: 场上能摆的角色不足 2 个"); return; }

    for (int i = 0; i < k; i++) {
        float ang = 6.2831853f * (float)i / (float)k;
        Vec3 p;
        p.x = cen.x + cosf(ang) * radius;
        p.y = cen.y + 0.5f;
        p.z = cen.z + sinf(ang) * radius;
        void* a[1]; a[0] = &p;
        SafeCall(arr[i], "SetPositionOfUsOrMount", 1, a, "布阵");
    }
    RbSnapshot(me);
    Log("斗蛐蛐 -> 竞技场布阵: %d 个角色在半径 %.0f 米的圈上站好了", k, radius);
}

static void ActRbBet(int team)
{
    if (team < 0 || team > 2) team = 0;
    InterlockedExchange(&g_betTeam, team);
    if (team == 1)      Log("斗蛐蛐 -> 押注: 你押红队, 打完看答案");
    else if (team == 2) Log("斗蛐蛐 -> 押注: 你押蓝队, 打完看答案");
    else                Log("斗蛐蛐 -> 押注: 已取消");
}

static void ActRbPrep(int weapon)
{
    if (weapon < 0 || weapon > 5) weapon = 1;
    ActForceWeaponAll(weapon);
    ActScaleAll(100);
    ActRbFormation(12.0f);
    ActRumbleTeams();
    InterlockedExchange(&g_matchOver, 0);
    Log("斗蛐蛐 -> 比赛准备完成: 统一武器 + 还原体型 + 环形布阵 + 分队, 你退场看戏");
}

static void ActRbRandomWeapon(void)
{
    static const int pool[] = { 1, 2, 3, 5, 0 };
    static unsigned int s_seed = 0x1234ABCDu;
    MonoObject* list = GetLivingList();
    int n = ListCount(list), cnt = 0;
    for (int i = 0; i < n && i < 256; i++) {
        MonoObject* c = ListItem(list, i);
        if (!c || IsUnityDead(c)) continue;
        if (CallBool0(c, "IsPlayer")) continue;
        if (CharIsPlayerTeam(c)) continue;
        MonoObject* fpm = CharFPM(c);
        if (!fpm) continue;
        s_seed = s_seed * 1103515245u + 12345u + GetTickCount();
        int w = pool[(s_seed >> 16) % 5], cd = 0;
        void* a[2]; a[0] = &w; a[1] = &cd;
        SafeCall(fpm, "SetEquippedWeaponType", 2, a, "SetEquippedWeaponType");
        cnt++;
    }
    Log("斗蛐蛐 -> 随机武器: %d 个敌人各抓了一件随机武器 (剑/弓/锤/矛/空手)", cnt);
}

static void DoRbMatchTick(void)
{
    if (!g_rbTeamOn) return;
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 500;

    int red = 0, blue = 0;
    for (int i = 0; i < g_rbRedN; i++) {
        MonoObject* c = g_rbRed[i];
        if (c && !IsUnityDead(c) && InLiving(c)) red++;
    }
    for (int i = 0; i < g_rbBlueN; i++) {
        MonoObject* c = g_rbBlue[i];
        if (c && !IsUnityDead(c) && InLiving(c)) blue++;
    }
    InterlockedExchange(&g_rbRedAlive,  red);
    InterlockedExchange(&g_rbBlueAlive, blue);

    if (InterlockedCompareExchange(&g_matchOver, 0, 0)) return;
    if (red == 0 && blue == 0) return;
    if (red > 0 && blue > 0)   return;

    int winner = (red > 0) ? 1 : 2;
    const char* wn = (winner == 1) ? "红队" : "蓝队";
    RbAddRaw2(wn, "获胜");
    InterlockedExchange(&g_matchOver, 1);

    LONG bet = InterlockedCompareExchange(&g_betTeam, 0, 0);
    Log("斗蛐蛐 -> 比赛结束: %s 获胜 (红 %d : 蓝 %d)", wn, red, blue);
    if (bet) {
        if (bet == winner) Log("斗蛐蛐 -> 押注结算: 你押的是 %s —— 押中了!",
                               bet == 1 ? "红队" : "蓝队");
        else               Log("斗蛐蛐 -> 押注结算: 你押的是 %s, 输了 :)",
                               bet == 1 ? "红队" : "蓝队");
    }
}

static void DoEpTick(void)
{
    static DWORD s_next = 0;
    DWORD now = GetTickCount();
    if (now < s_next) return;
    s_next = now + 150;

    if (g_epLowGrav) {
        MonoObject* list = GetLivingList();
        int n = ListCount(list);
        for (int i = 0; i < n && i < 256; i++) {
            MonoObject* c = ListItem(list, i);
            if (!c || IsUnityDead(c)) continue;
            if (CallBool0(c, "IsPlayer")) continue;
            if (SkipAlly(c)) continue;
            Vec3 up = { 0.0f, 5.0f, 0.0f };
            void* a[1]; a[0] = &up;
            SafeCall(c, "AddVelocity", 1, a, "AddVelocity(飘浮)");
        }
    }

    if (g_epSuperKick || g_epNoFall) {
        MonoObject* fpm = PlayerFPM();
        if (fpm && !IsUnityDead(fpm)) {
            MonoClass* k = p_object_get_class(fpm);
            if (g_epSuperKick) {
                if (!f_EpKickMag) f_EpKickMag = FindField(k, "KickVelocityMagnitude");
                if (f_EpKickMag) {
                    if (g_epKickBase < 0.0f) {
                        float cur = 0.0f;
                        p_field_get_value(fpm, f_EpKickMag, &cur);
                        if (cur > 0.01f) g_epKickBase = cur;
                    }
                    if (g_epKickBase > 0.0f) {
                        float v = g_epKickBase * g_epKickMul;
                        p_field_set_value(fpm, f_EpKickMag, &v);
                    }
                }
            }
            if (g_epNoFall) {
                if (!f_EpCannotFall) f_EpCannotFall = FindField(k, "CannotFallOver");
                if (f_EpCannotFall) { int v = 1; p_field_set_value(fpm, f_EpCannotFall, &v); }
            }
        }
    }
}

static int   uiRumble    = 0;
static MonoObject* uiSelChar = NULL;
static int   uiSelIdx    = -1;
static int   uiWeaponSel = 4;
static int   uiPoseSel   = 0;
static int   uiMorphSel  = 1;
static float uiScale     = 1.0f;
static float uiSpd       = 1.0f;
static float uiAtk       = 1.0f;

static void DrawRosterTab(void)
{

    static MonoObject* lobj[JQR_SNAP_MAX];
    static int ltype[JQR_SNAP_MAX], lteam[JQR_SNAP_MAX], lplay[JQR_SNAP_MAX], lbetr[JQR_SNAP_MAX];
    static int lcount = 0;

    LONG s1 = g_snapSeq;
    if ((s1 & 1) == 0) {
        int cnt = (int)g_snapCount;
        if (cnt > JQR_SNAP_MAX) cnt = JQR_SNAP_MAX;
        for (int i = 0; i < cnt; i++) {
            lobj[i]  = g_snap[i].obj;
            ltype[i] = g_snap[i].typeId;
            lteam[i] = g_snap[i].isTeam;
            lplay[i] = g_snap[i].isPlayer;
            lbetr[i] = g_snap[i].isBetray;
        }
        if (g_snapSeq == s1) lcount = cnt;
    }

    int ally = 0, foe = 0;
    for (int i = 0; i < lcount; i++) { if (lteam[i]) ally++; else foe++; }

    ImGui::TextColored(COL_OK, "场上 %d 个", lcount);
    ImGui::SameLine(); ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "我方 %d", ally);
    ImGui::SameLine(); ImGui::TextColored(COL_WARN, "敌方 %d", foe);
    ImGui::SameLine(ImGui::GetWindowWidth() - 70);
    if (ImGui::SmallButton("刷新##ro")) PostAction(ACT_CH_REFRESH, 0);

    if (lcount <= 0) {
        ImGui::TextColored(COL_DIM, "读不到角色 —— 请先进关卡 (玩家活着)");
        ImGui::Spacing();
        RowToggle("全场乱斗", "  所有 AI 互相打, 你躲旁边看", &uiRumble, ACT_RUMBLE_ON, ACT_RUMBLE_OFF);
        return;
    }

    if (ImGui::BeginTable("##roster", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY, ImVec2(0, 196))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#",    ImGuiTableColumnFlags_WidthFixed, 26);
        ImGui::TableSetupColumn("兵种", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("阵营", ImGuiTableColumnFlags_WidthFixed, 58);
        ImGui::TableHeadersRow();

        for (int i = 0; i < lcount; i++) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i + 1);

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(i);
            char lbl[128];
            _snprintf_s(lbl, sizeof(lbl), _TRUNCATE, "%s%s%s",
                        TypeNameCN(ltype[i]),
                        lplay[i] ? "   (你)" : "",
                        lbetr[i] ? "   ★内讧中" : "");
            bool sel = (uiSelChar && uiSelChar == lobj[i]);
            if (ImGui::Selectable(lbl, sel, ImGuiSelectableFlags_SpanAllColumns))
                uiSelChar = lobj[i];
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            if (lplay[i])      ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "你");
            else if (lteam[i]) ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "我方");
            else               ImGui::TextColored(COL_WARN, "敌方");
        }
        ImGui::EndTable();
    }

    uiSelIdx = -1;
    if (uiSelChar) {
        for (int i = 0; i < lcount; i++)
            if (lobj[i] == uiSelChar) { uiSelIdx = i; break; }
        if (uiSelIdx < 0) uiSelChar = NULL;
    }

    ImGui::Separator();
    if (!uiSelChar) {
        ImGui::TextColored(COL_DIM, "点上面一行选中它, 下面是能对它做的事");
        ImGui::Spacing();
        RowToggle("全场乱斗", "  所有 AI 互相打, 你躲旁边看", &uiRumble, ACT_RUMBLE_ON, ACT_RUMBLE_OFF);
        ImGui::TextColored(COL_DIM, "想安全看戏就先去「玩家」页开无敌");
        return;
    }

    ImGui::TextColored(COL_OK, "选中 #%d  %s", uiSelIdx + 1, TypeNameCN(ltype[uiSelIdx]));
    ImGui::SameLine();
    if (lplay[uiSelIdx])       ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "[你]");
    else if (lteam[uiSelIdx])  ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "[我方]");
    else                       ImGui::TextColored(COL_WARN, "[敌方]");
    if (lbetr[uiSelIdx]) { ImGui::SameLine(); ImGui::TextColored(COL_WARN, "★内讧中"); }

    if (ImGui::Button("击杀", ImVec2(66, 26)))          PostActionPtr(ACT_CH_KILL, uiSelChar);
    ImGui::SameLine();
    if (ImGui::Button("收编为友军", ImVec2(104, 26)))    PostActionPtr(ACT_CH_RECRUIT, uiSelChar);
    ImGui::SameLine();
    if (ImGui::Button("缴械", ImVec2(66, 26)))          PostActionPtr(ACT_CH_DISARM, uiSelChar);
    ImGui::SameLine();
    if (ImGui::Button(lbetr[uiSelIdx] ? "停止内讧" : "内讧", ImVec2(84, 26)))
        PostActionPtr(lbetr[uiSelIdx] ? ACT_CH_CALM : ACT_CH_BETRAY, uiSelChar);

    if (ImGui::Button("穿护甲", ImVec2(66, 26)))        PostActionPtr(ACT_CH_ARMOR, uiSelChar);
    ImGui::SameLine();
    if (ImGui::Button("脱护甲", ImVec2(66, 26)))        PostActionPtr(ACT_CH_UNARMOR, uiSelChar);
    ImGui::SameLine();
    if (ImGui::Button("选中无敌", ImVec2(86, 26)))      PostActionPtr(ACT_CH_IMM_ON, uiSelChar);
    ImGui::SameLine();
    if (ImGui::Button("取消", ImVec2(60, 26)))          PostActionPtr(ACT_CH_IMM_OFF, uiSelChar);

    ImGui::Separator();
    ImGui::TextColored(COL_OK, "斗蛐蛐 · 改造选中角色");

    ImGui::SetNextItemWidth(-116);
    ImGui::Combo("##wp", &uiWeaponSel, kWeaponNames, (int)JQR_WEAPON_COUNT);
    ImGui::SameLine();
    if (ImGui::Button("换武器##wp", ImVec2(104, 24))) {
        g_pendWeapon = kWeaponIds[uiWeaponSel];
        PostActionPtr(ACT_CH_WEAPON, uiSelChar);
    }

    ImGui::SetNextItemWidth(-116);
    ImGui::Combo("##mp", &uiMorphSel, g_typeLabels, JQR_ENEMYTYPE_COUNT);
    ImGui::SameLine();
    if (ImGui::Button("变装##mp", ImVec2(104, 24))) {
        g_pendMorph = g_typeIds[uiMorphSel];
        PostActionPtr(ACT_CH_MORPH, uiSelChar);
    }
    ImGui::TextColored(COL_DIM, "换武器: 手上那件换掉 (AI 可能自己换回来)");
    ImGui::TextColored(COL_DIM, "变装: 就地换模型 (原地生效, 不重建角色 -> 不卡)");

    if (ImGui::Button("★ 我自己变成这个兵种##selfmp", ImVec2(-1, 26))) {
        g_pendMorph = g_typeIds[uiMorphSel];
        PostAction(ACT_SELF_MORPH, 0);
    }
    ImGui::TextColored(COL_DIM, "把【你本人】就地换成上面选的兵种 (想变皇帝就选\"皇帝 - 战斗\")");
    ImGui::TextColored(COL_DIM, "已自动排除: 垃圾机器人 / 战舰战机 / 意识空间类 (变了会搞坏游戏)");

    ImGui::Separator();
    ImGui::TextColored(COL_OK, "身体动作 · 游戏自带 emote (挥手/跳舞/胜利姿势…)");
    {
        static const char* emoItems[JQR_POSE_MAX];
        int n = (int)g_poseCount;
        for (int i = 0; i < n; i++) emoItems[i] = g_poseNames[i];
        if (n <= 0) { emoItems[0] = "(还没读到名单, 点下面「重新读取 emote」)"; n = 1; }
        if (uiPoseSel >= n) uiPoseSel = 0;
        ImGui::ListBox("##emolist", &uiPoseSel, emoItems, n, 8);
    }
    if (ImGui::Button("播放选中##emop", ImVec2(120, 26))) {
        if ((int)g_poseCount > 0) { g_pendPose = uiPoseSel + 1; PostAction(ACT_POSE_SET, 0); }
    }
    ImGui::SameLine();
    if (ImGui::Button("停止动作##emo0", ImVec2(110, 26)))  PostAction(ACT_POSE_OFF, 0);
    ImGui::SameLine();
    if (ImGui::Button("重新读取##emor", ImVec2(110, 26)))  PostAction(ACT_POSE_RELOAD, 0);
    ImGui::SameLine();
    if (ImGui::Button("导出骨架##dmp", ImVec2(110, 26)))   PostAction(ACT_POSE_DUMP, 0);
    ImGui::TextColored(COL_DIM, "列表 = 游戏原生 emote (Emote_<ID>); 选中后点「播放选中」");
    ImGui::TextColored(COL_DIM, "「停止动作」= 立刻恢复游戏原本动画; 每 250ms 自动补刷保持");

    ImGui::Spacing();
    ImGui::TextColored(COL_DIM, "属性 (拖完点应用)");
    ImGui::SetNextItemWidth(-116);
    ImGui::SliderFloat("体型", &uiScale, 0.3f, 3.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("应用##sc", ImVec2(104, 24))) {
        g_pendScale = (int)(uiScale * 100.0f);
        PostActionPtr(ACT_CH_SCALE, uiSelChar);
    }
    ImGui::SetNextItemWidth(-116);
    ImGui::SliderFloat("移速", &uiSpd, 0.1f, 5.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("应用##sp", ImVec2(104, 24))) {
        g_pendSpeed = (int)(uiSpd * 100.0f);
        PostActionPtr(ACT_CH_SPEED, uiSelChar);
    }
    ImGui::SetNextItemWidth(-116);
    ImGui::SliderFloat("攻速", &uiAtk, 0.1f, 5.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("应用##at", ImVec2(104, 24))) {
        g_pendAtk = (int)(uiAtk * 100.0f);
        PostActionPtr(ACT_CH_ATKSPD, uiSelChar);
    }

    ImGui::Separator();
    g_hfSelChar = (void*)uiSelChar;
    ImGui::TextColored(COL_OK, "电子斗蛐蛐 2.0");
    RowToggle("全场乱斗", "  所有 AI 互相打, 你躲旁边看", &uiRumble, ACT_RUMBLE_ON, ACT_RUMBLE_OFF);
    ImGui::TextColored(COL_DIM, "  已接上官方 AIController.IgnoreOpposingTeam, AI 会自己扑上去咬");

    if (ImGui::Button("红蓝分队对抗", ImVec2(136, 26))) PostAction(ACT_RB_TEAMS, 0);
    ImGui::SameLine();
    if (ImGui::Button("全员恢复为敌", ImVec2(136, 26))) PostAction(ACT_RB_TEAMOFF, 0);
    ImGui::TextColored(COL_DIM, "  按顺序交替分两队, 让游戏自己的阵营系统驱动互殴");

    ImGui::Spacing();
    ImGui::TextColored(COL_DIM, "单挑 (对当前选中角色, 其他人停手围观)");
    if (ImGui::Button("单挑: VS 最近的敌人", ImVec2(196, 26)))
        PostActionPtr(ACT_RB_DUEL_NEAR, uiSelChar);
    ImGui::SameLine();
    if (ImGui::Button("单挑: VS 我", ImVec2(112, 26)))
        PostActionPtr(ACT_RB_DUEL_ME, uiSelChar);
    ImGui::SameLine();
    if (ImGui::Button("结束", ImVec2(60, 26)))
        PostAction(ACT_RB_DUEL_STOP, 0);

    ImGui::Spacing();
    ImGui::TextColored(COL_OK, "3.0 赛制");
    {
        static float uiRad = 12.0f;
        static int   uiPrepW = 1;
        static const char* pwns[] = { "空手", "剑", "弓", "锤", "矛" };
        static const int   pwids[] = { 0, 1, 2, 3, 5 };
        ImGui::SetNextItemWidth(-150);
        ImGui::SliderFloat("场地半径", &uiRad, 4.0f, 40.0f, "%.0f 米");
        ImGui::SameLine();
        if (ImGui::Button("环形布阵", ImVec2(142, 24)))
            PostAction(ACT_RB_FORMATION, (int)(uiRad * 10.0f));
        ImGui::SetNextItemWidth(-150);
        ImGui::Combo("统一武器##prep", &uiPrepW, pwns, 5);
        ImGui::SameLine();
        if (ImGui::Button("比赛准备", ImVec2(142, 24)))
            PostAction(ACT_RB_PREP, pwids[uiPrepW]);
        ImGui::TextColored(COL_DIM, "  比赛准备 = 统一武器 + 还原体型 + 环形布阵 + 红蓝分队");
        if (ImGui::Button("随机武器大乱斗", ImVec2(-1, 26)))
            PostAction(ACT_RB_RANDWEAPON, 0);

        {
            LONG ra = InterlockedCompareExchange(&g_rbRedAlive,  0, 0);
            LONG ba = InterlockedCompareExchange(&g_rbBlueAlive, 0, 0);
            LONG bet = InterlockedCompareExchange(&g_betTeam, 0, 0);
            LONG on  = InterlockedCompareExchange(&g_rbTeamOn, 0, 0);
            if (on || ra || ba) {
                ImGui::TextColored(COL_WARN, "红队 %d", (int)ra);
                ImGui::SameLine();
                ImGui::TextColored(COL_DIM, ":");
                ImGui::SameLine();
                ImGui::TextColored(COL_OK, "%d 蓝队", (int)ba);
                if (bet == 1)      { ImGui::SameLine(); ImGui::TextColored(COL_WARN, "  (你押红)"); }
                else if (bet == 2) { ImGui::SameLine(); ImGui::TextColored(COL_WARN, "  (你押蓝)"); }
            }
        }
    }
    ImGui::Spacing();
    {
        LONG bet = InterlockedCompareExchange(&g_betTeam, 0, 0);
        ImGui::TextColored(COL_DIM, "押注 (打完自动结算)");
        ImGui::SameLine();
        if (ImGui::SmallButton("押红队##bet1"))  PostAction(ACT_RB_BET_RED, 0);
        ImGui::SameLine();
        if (ImGui::SmallButton("押蓝队##bet2"))  PostAction(ACT_RB_BET_BLUE, 0);
        ImGui::SameLine();
        if (ImGui::SmallButton("取消##bet0"))    PostAction(ACT_RB_BET_OFF, 0);
        ImGui::SameLine();
        if (bet == 1)      ImGui::TextColored(COL_WARN, "已押红队");
        else if (bet == 2) ImGui::TextColored(COL_WARN, "已押蓝队");
        else               ImGui::TextColored(COL_DIM, "没押");
    }

    ImGui::Spacing();
    ImGui::TextColored(COL_WARN, "战报");
    ImGui::SameLine(ImGui::GetWindowWidth() - 90);
    if (ImGui::SmallButton("清空##rp")) PostAction(ACT_RB_CLEARLOG, 0);
    {
        LONG cnt  = InterlockedCompareExchange(&g_rpCount, 0, 0);
        int  show = (int)(cnt < JQR_RP_MAX ? cnt : JQR_RP_MAX);
        ImGui::BeginChild("##rplog", ImVec2(0, 92), true);
        if (show <= 0) ImGui::TextColored(COL_DIM, "(还没有人倒下)");
        for (int i = 0; i < show; i++) {
            int idx = (int)((cnt - 1 - i) % JQR_RP_MAX);
            if (idx < 0) idx += JQR_RP_MAX;
            ImGui::TextColored(i == 0 ? COL_OK : COL_DIM, "%s", g_rpText[idx]);
        }
        ImGui::EndChild();
    }
}

static void DrawMenu(void)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = (g_showMenu != 0);
    BuildTypeLabels();
    if (!g_showMenu) return;

    ImGui::SetNextWindowSize(ImVec2(760, 640), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(48, 48), ImGuiCond_FirstUseEver);
    bool open = true;
    if (ImGui::Begin("机器人角斗场 修改器   v0.41.2", &open, ImGuiWindowFlags_NoCollapse)) {
        if (!open) g_showMenu = 0;

        if (g_ready) ImGui::TextColored(COL_OK, "● 已连接游戏");
        else         ImGui::TextColored(COL_WARN, "● 正在连接游戏...");

        ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        ImGui::TextColored(COL_DIM, "Insert 隐藏/显示");

        ImGui::Separator();
        if (ImGui::BeginTabBar("jqrtabs", ImGuiTabBarFlags_FittingPolicyScroll)) {

            if (ImGui::BeginTabItem("召唤")) {

                ImGui::TextColored(COL_DIM, "找类型: 输入中文/英文关键字筛选 (留空 = 全部)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputTextWithHint("##srch", "例如: 蜘蛛 / 剑士 / Zombie", g_summonSearch, sizeof(g_summonSearch)))
                    g_summonSearchOn = (g_summonSearch[0] != 0);
                if (g_summonSearch[0] == 0) g_summonSearchOn = 0;
                ImGui::Spacing();

                static int  filtIdx[JQR_ENEMYTYPE_COUNT];
                static char szFilter[JQR_ENEMYTYPE_COUNT][72];
                static char* szPtrs[JQR_ENEMYTYPE_COUNT];
                int nf = 0;
                for (int i = 0; i < JQR_ENEMYTYPE_COUNT; i++) {
                    if (g_summonSearchOn) {
                        char en[72], q[48];
                        _snprintf_s(en, sizeof(en), _TRUNCATE, "%s", g_typeNames[i]);
                        _snprintf_s(q, sizeof(q), _TRUNCATE, "%s", g_summonSearch);
                        for (char* p = en; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
                        for (char* p = q; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
                        if (!strstr(g_typeNamesCN[i], q) && !strstr(en, q)) continue;
                    }
                    _snprintf_s(szFilter[nf], sizeof(szFilter[nf]), _TRUNCATE, "%s", g_typeLabels[i]);
                    szPtrs[nf] = szFilter[nf];
                    filtIdx[nf] = i;
                    nf++;
                }
                if (nf == 0) {
                    ImGui::TextColored(ImVec4(0.9f,0.5f,0.5f,1), "没匹配到类型 —— 清空搜索框试试");
                } else {
                    if (g_selType < 0 || g_selType >= nf) g_selType = 0;
                    ImGui::SetNextItemWidth(-1);
                    ImGui::ListBox("##types", &g_selType, szPtrs, nf, 12);
                }
                ImGui::Spacing();

                int curType = (nf > 0) ? g_typeIds[filtIdx[g_selType]] : 0;
                if (nf > 0) {
                    ImGui::Text("当前选中: %s", g_typeNamesCN[filtIdx[g_selType]]);
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  [%d] %s", curType, g_typeNames[filtIdx[g_selType]]);
                }
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.40f, 0.24f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.58f, 0.32f, 1));
                if (ImGui::Button("召唤盟友 (帮你打)", ImVec2(-1, 34)) && nf > 0)
                    PostAction(ACT_SUMMON_ALLY, curType);
                ImGui::PopStyleColor(2);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.44f, 0.20f, 0.20f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.62f, 0.26f, 0.26f, 1));
                if (ImGui::Button("召唤敌人 (来打我)", ImVec2(-1, 34)) && nf > 0)
                    PostAction(ACT_SUMMON_ENEMY, curType);
                ImGui::PopStyleColor(2);

                ImGui::Spacing();
                ImGui::TextColored(COL_DIM, "生成位置: 你前方偏右约 2 米");

                ImGui::Separator();
                ImGui::TextColored(COL_DIM, "列表里没有? 直接填 ID (0-255) 召唤:");
                ImGui::SetNextItemWidth(90);
                ImGui::InputInt("##cid", &g_summonCustomId, 1, 10);
                if (g_summonCustomId < 0)   g_summonCustomId = 0;
                if (g_summonCustomId > 255) g_summonCustomId = 255;
                ImGui::SameLine();
                if (ImGui::Button("召盟友", ImVec2(70, 24)))
                    PostAction(ACT_SUMMON_ALLY, g_summonCustomId);
                ImGui::SameLine();
                if (ImGui::Button("召敌人", ImVec2(70, 24)))
                    PostAction(ACT_SUMMON_ENEMY, g_summonCustomId);

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "我的快捷召唤");
                ImGui::SameLine();
                ImGui::TextColored(COL_DIM, "  左键=召一次 · 改键 · 清空键 · 删除");
                {
                    int capS = HfCapturing();
                    int nMyS = 0;
                    int hcS  = HfCount();
                    for (int i = 0; i < hcS; i++) if (HfIsCustom(i)) nMyS++;
                    if (nMyS == 0) {
                        ImGui::TextColored(COL_DIM, "  (还没有 —— 到「快捷键」页点兵种方块即可添加)");
                    } else {
                        ImGui::BeginChild("##mysm", ImVec2(0, 130), true);
                        for (int i = 0; i < hcS; i++) {
                            if (!HfIsCustom(i)) continue;
                            char kb[64];
                            ImGui::PushID(i);
                            ImGui::TextColored(HfCustomAlly(i) ? COL_OK : COL_WARN, "%s",
                                               HfCustomAlly(i) ? "[友]" : "[敌]");
                            ImGui::SameLine(40.0f);
                            ImGui::Text("%s", HfCustomTypeName(i));
                            ImGui::SameLine(150.0f);
                            if (capS == i) {
                                ImGui::TextColored(COL_WARN, "按键盘…");
                            } else if (HfBindOf(i)) {
                                ImGui::TextColored(COL_OK, "[ %s ]", HfKeyName(i, kb, sizeof(kb)));
                            } else {
                                ImGui::TextColored(COL_DIM, "[ 未绑 ]");
                            }
                            ImGui::SameLine(240.0f);
                            if (ImGui::SmallButton("绑键")) HfBeginCapture(i);
                            ImGui::SameLine();
                            if (ImGui::SmallButton("清空键")) HfSetBind(i, 0);
                            ImGui::SameLine();
                            if (ImGui::SmallButton("删除")) HfCustomDel(i);
                            ImGui::SameLine();
                            if (ImGui::SmallButton("召一次"))
                                PostAction(HfCustomAlly(i) ? ACT_SUMMON_ALLY : ACT_SUMMON_ENEMY,
                                           HfCustomTypeId(i));
                            ImGui::PopID();
                        }
                        ImGui::EndChild();
                    }
                    if (capS >= 0 && HfIsCustom(capS))
                        ImGui::TextColored(COL_WARN, "  现在按键盘任意键即可绑定 (Esc 取消)");
                }

                ImGui::Separator();
                if (ImGui::Button("克隆机器人  +1   (真造实体 + 数字)", ImVec2(-1, 32)))
                    PostAction(ACT_CLONE_PLUS, 0);
                ImGui::TextColored(COL_DIM, "会真的造一个克隆实体到场上, 同时把克隆数 +1");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("点名台")) {
                DrawRosterTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("玩家")) {
                RowToggle("无敌", "  打不死", &uiImmune, ACT_IMMUNE_ON, ACT_IMMUNE_OFF);
                ImGui::Spacing();
                RowToggle("友军无敌", "  召唤出来的也打不死", &uiAllyImm, ACT_ALLYIMM_ON, ACT_ALLYIMM_OFF);
                ImGui::Spacing();
                RowToggle("战舰无敌", "  后期开战舰时用", &uiShipImm, ACT_SHIPIMM_ON, ACT_SHIPIMM_OFF);

                ImGui::Separator();
                RowToggle("AI不理", "  敌人不打你, 但会打你的队友", &uiNeutral, ACT_NEUTRAL_ON, ACT_NEUTRAL_OFF);

                ImGui::Separator();
                RowToggle("无限能量", "  挥砍 / 技能不再消耗", &uiEnergy, ACT_ENERGY_ON, ACT_ENERGY_OFF);
                ImGui::Spacing();
                RowToggle("无限意识转移", "  转移到敌人身上的次数补满", &uiTransfer, ACT_TRANSFER_ON, ACT_TRANSFER_OFF);

                ImGui::Separator();
                ImGui::Text("子弹时间");
                bool slow = (uiSlow < 0.999f);
                if (ImGui::Checkbox("##slow", &slow)) {
                    if (slow) { uiSlow = g_slowValue; PostAction(ACT_SLOWMO_ON, (int)(g_slowValue * 100)); }
                    else      { uiSlow = 1.0f;        PostAction(ACT_SLOWMO_OFF, 0); }
                }
                ImGui::SameLine();
                ImGui::TextColored(COL_DIM, slow ? "已开启 (持续锁定)" : "关");
                if (slow) {
                    ImGui::SetNextItemWidth(-1);
                    float v = uiSlow * 100.0f;
                    if (ImGui::SliderFloat("##slowv", &v, 5.0f, 100.0f, "速度 %.0f%%")) {
                        uiSlow = v / 100.0f; g_slowValue = uiSlow;
                        PostAction(ACT_SLOWMO_ON, (int)v);
                    }
                    ImGui::TextColored(COL_DIM, "杀敌 / 过关都不会被顶回原速");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("机动")) {
                ImGui::TextColored(COL_OK, "飞天");
                ImGui::Separator();
                RowToggle("飞天", "  开启后用方向键控制", &uiFly, ACT_FLY_ON, ACT_FLY_OFF);
                ImGui::TextColored(COL_DIM, "方向键   ↑=前   ↓=后   ←=左   →=右");
                ImGui::TextColored(COL_DIM, "PageUp = 上升     PageDown = 下降");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##flyspd", &uiFlySpeed, 3.0f, 40.0f, "飞行速度 %.0f 米/秒")) {
                    g_flySpeed = uiFlySpeed;
                }
                ImGui::TextColored(COL_DIM, "取消勾选就恢复重力, 角色自然落地");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "穿墙");
                ImGui::Separator();
                RowToggle("平面穿墙", "  撞不到墙, 但不会掉下去", &uiWall, ACT_WALL_ON, ACT_WALL_OFF);
                ImGui::TextColored(COL_DIM, "高度锁在开启那一刻, 只想水平穿过去就用它");
                ImGui::TextColored(COL_DIM, "要上下动就跟上面的飞天一起开");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "角色状态");
                ImGui::Separator();
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##spd", &uiSpeedMul, 0.1f, 5.0f, "移动速度 %.2fx")) {
                    PostAction(ACT_SPEED, (int)(uiSpeedMul * 100.0f));
                }
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##jmp", &uiJumpMul, 0.1f, 8.0f, "跳跃力 %.2fx")) {
                    PostAction(ACT_JUMP, (int)(uiJumpMul * 100.0f));
                }

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "武器");
                ImGui::Separator();
                ImGui::SetNextItemWidth(-1);
                ImGui::Combo("##wp", &g_selWeapon, kWeaponNames, (int)JQR_WEAPON_COUNT);
                if (ImGui::Button("装备选中的武器", ImVec2(-1, 30)))
                    PostAction(ACT_WEAPON, kWeaponIds[g_selWeapon]);
                ImGui::TextColored(COL_DIM, "会先把它加进你的持有列表再换装");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "武器判定范围  (远程击杀)");
                ImGui::Separator();
                {
                    bool v = (g_reachOn != 0);
                    if (ImGui::Checkbox("判定范围放大##reach", &v)) {
                        g_reachOn = v ? 1 : 0;
                        PostAction(v ? ACT_REACH_ON : ACT_REACH_OFF,
                                   v ? (int)(g_reachMul * 100.0f) : 0);
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, v ? "  已开启" : "  只对 剑 / 矛 生效");
                }
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##reachmul", &g_reachMul, 1.0f, 8.0f, "范围 %.1fx   (1.0 = 原版)")) {
                    if (g_reachOn) PostAction(ACT_REACH_ON, (int)(g_reachMul * 100.0f));
                }
                ImGui::TextColored(COL_DIM, "刃线 + 判定盒一起放大, 隔着老远也能砍中");
                ImGui::TextColored(COL_DIM, "用的是官方自己缩放判定物的手法, 不动任何游戏数据");
                ImGui::TextColored(COL_DIM, "取消勾选立刻还原成原版范围");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("战舰")) {
                ImGui::TextColored(COL_WARN, "击杀敌军战舰");
                ImGui::Separator();

                RowToggle("击杀后补官方结算", "  默认关 = 只炸船; 开了才会播胜利音 + 推进剧情",
                          &uiShipSettle, ACT_SHIPSETTLE_ON, ACT_SHIPSETTLE_OFF);
                ImGui::TextColored(COL_DIM, "之前那一版默认会补结算 —— 所以你点一键击杀, 远处主持人就播胜利画面。");
                ImGui::TextColored(COL_DIM, "现在默认不补: 船炸了就完事, 不再触发决斗场那套结算流程。");

                ImGui::Separator();
                ImGui::TextColored(COL_DIM, "注: 原「联机自毁保护」已移除 —— 它会导致游戏启动闪退。");
                ImGui::TextColored(COL_DIM, "     正版号请自己注意, 别把修改器带进联机房间。");

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("音乐")) {
                ImGui::TextColored(COL_OK, "音乐播放器  (手动点播)");
                ImGui::Separator();
                int mc = (int)InterlockedCompareExchange(&g_musicCount, 0, 0);
                int ms = (int)InterlockedCompareExchange(&g_musicSel, 0, 0);
                static int musicAutoAsked = 0;
                if (!musicAutoAsked) { musicAutoAsked = 1; if (mc <= 0) PostAction(ACT_MUSIC_RELOAD, 0); }
                if (mc <= 0) {
                    ImGui::TextColored(COL_DIM, "还没读到音乐名单。");
                    ImGui::TextColored(COL_DIM, "先进一次关卡 (让游戏把音频库加载起来), 再点下面的按钮。");
                    ImGui::Spacing();
                    if (ImGui::Button("读取音乐名单", ImVec2(-1, 36)))
                        PostAction(ACT_MUSIC_RELOAD, 0);
                } else {
                    ImGui::Text("共 %d 首%s", mc, (ms >= 0 && ms < mc) ? "    正在播放…" : "");
                    if (ImGui::Button("停止音乐", ImVec2(120, 32))) PostAction(ACT_MUSIC_STOP, 0);
                    ImGui::SameLine();
                    if (ImGui::Button("重新读取名单", ImVec2(150, 32))) PostAction(ACT_MUSIC_RELOAD, 0);
                    ImGui::Spacing();
                    ImGui::TextColored(COL_DIM, "点一首就开始放 (循环); 想换风格直接点另一首");
                    ImGui::BeginChild("musiclist", ImVec2(0, 400), true);
                    for (int i = 0; i < mc; i++) {
                        ImGui::PushID(i);
                        if (ImGui::Selectable(g_musicNames[i], ms == i))
                            PostAction(ACT_MUSIC_PLAY, i);
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("花活")) {
                ImGui::TextColored(COL_OK, "夺舍");
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.22f, 0.52f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.30f, 0.72f, 1));
                if (ImGui::Button("夺舍最近的敌人", ImVec2(-1, 38)))
                    PostAction(ACT_POSSESS, 0);
                ImGui::PopStyleColor(2);
                ImGui::TextColored(COL_DIM, "意识钻进最近的敌人体内, 原身体交给 AI");
                ImGui::TextColored(COL_DIM, "会先把意识转移次数补满, 再自己挑最近的敌人");
                ImGui::TextColored(COL_DIM, "跟游戏里按夺舍键是同一套流程 (带相机过渡)");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "斗兽场意识转移 (v0.37.1)  ★ 第1/2章也能用");
                ImGui::Separator();
                RowToggle("斗兽场强制意识转移", "  默认关: 开了之后按 H 键就能钻身体 (第1/2章)",
                          &uiCtForce, ACT_CTFORCE_ON, ACT_CTFORCE_OFF);
                if (ImGui::Button("立即转移一次  (最近的盟友 / 敌人)", ImVec2(-1, 34)))
                    PostAction(ACT_CT_NOW, 0);
                ImGui::TextColored(COL_DIM, "开了开关后, 按 H 键就能转移; 也可以直接点上面的按钮");
                ImGui::TextColored(COL_DIM, "读的是玩家输入对象里的转移键 —— 就是你在游戏设置里绑的那个键, 不硬编码");
                ImGui::TextColored(COL_DIM, "优先转移到盟友 (跟原生一致); 场上没盟友就转移到最近的敌人");
                ImGui::TextColored(COL_DIM, "v0.37.1: 改走原子步骤 (身体切换+接管输入+免疫一秒), 不再调会崩的原生那句");
                ImGui::TextColored(COL_DIM, "第3章本来就解锁了 —— 那里本功能自动让位给游戏原生逻辑, 不插手");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "骑乘 (v0.7)");
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.38f, 0.55f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.52f, 0.74f, 1));
                if (ImGui::Button("骑最近的任意角色  (含敌人)", ImVec2(-1, 38)))
                    PostAction(ACT_RIDE, 0);
                ImGui::PopStyleColor(2);
                if (ImGui::Button("下车", ImVec2(-1, 28)))
                    PostAction(ACT_DISMOUNT, 0);
                ImGui::TextColored(COL_DIM, "目标会当场变友军, 骑乘期间双方互免武器伤害");
                ImGui::TextColored(COL_DIM, "v0.9 起只挑能骑的 (有第一人称身体的角色)");
                ImGui::TextColored(COL_DIM, "战舰/陨石那种本来就不是可骑角色, 会跳过");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "缴械");
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.48f, 0.34f, 0.14f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.66f, 0.46f, 0.20f, 1));
                if (ImGui::Button("敌人全部缴械 (丢武器)", ImVec2(-1, 38)))
                    PostAction(ACT_DISARM, 0);
                ImGui::PopStyleColor(2);
                ImGui::TextColored(COL_DIM, "场上每个敌人的武器都从手里掉出去");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "胜利动作");
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.42f, 0.30f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.60f, 0.42f, 1));
                if (ImGui::Button("播放胜利动作  (点一次换一个)", ImVec2(-1, 38)))
                    PostAction(ACT_VICTORY, 0);
                ImGui::PopStyleColor(2);
                if (ImGui::Button("停止当前动作", ImVec2(-1, 28)))
                    PostAction(ACT_STOP_POSE, 0);
                ImGui::TextColored(COL_DIM, "走官方表情索引 (GetAllEmotes -> SetActiveEmoteIndex)");
                ImGui::TextColored(COL_DIM, "每点一次换下一个; 取不到表情表就不播 (不会再崩)");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "战场实验 (v0.7)");
                ImGui::Separator();
                RowToggle("敌人一击必杀", "  打中头就倒", &uiOneShot,
                          ACT_ONESHOT_ON, ACT_ONESHOT_OFF);
                RowToggle("敌人攻速归零", "  全部变木头人", &uiAtkSpd0,
                          ACT_ATKSPD0_ON, ACT_ATKSPD0_OFF);
                RowToggle("磁力靴", "  墙面/天花板吸附行走", &uiMagBoot,
                          ACT_MAGBOOT_ON, ACT_MAGBOOT_OFF);
                RowToggle("喷气背包", "  常开, 配合无限能量最好用", &uiJetpack,
                          ACT_JETPACK_ON, ACT_JETPACK_OFF);
                ImGui::TextColored(COL_DIM, "前两项对所有敌人生效 (新刷出来的要重开一次)");

                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "高级玩法 (v0.15)");
                ImGui::Separator();

                ImGui::TextColored(COL_DIM, "游戏速度 (走官方 Time.timeScale 通道)");
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("##gspeed", &uiGameSpeed, 0.1f, 5.0f, "%.2fx");
                if (ImGui::IsItemDeactivated())
                    PostAction(ACT_TS, (int)(uiGameSpeed * 100.0f + 0.5f));
                ImGui::SameLine();
                if (ImGui::Button("恢复原速", ImVec2(90, 0))) {
                    uiGameSpeed = 1.0f;
                    PostAction(ACT_TS, 100);
                }
                if (ImGui::Button("0.3x · 子弹时间", ImVec2(-1, 26)))
                    PostAction(ACT_TS, 30);
                if (ImGui::Button("2.0x · 快进", ImVec2(-1, 26)))
                    PostAction(ACT_TS, 200);
                ImGui::TextColored(COL_WARN, "联机别用 — 游戏自带速度作弊检测, 会封号");

                ImGui::Spacing();
                RowToggle("自动瞄准", "  每帧转向最近的敌人", &uiAim,
                          ACT_AIM_ON, ACT_AIM_OFF);
                RowToggle("自动攻击", "  自动挥砍, 手不用碰鼠标", &uiAutoAtk,
                          ACT_AUTOATK_ON, ACT_AUTOATK_OFF);
                RowToggle("敌人不计入胜利条件", "  站着不动也能过关 (速通)", &uiNoCount,
                          ACT_NOCOUNT_ON, ACT_NOCOUNT_OFF);

                ImGui::Spacing();
                if (ImGui::Button("闪现 · 贴到最近敌人背后", ImVec2(-1, 32)))
                    PostAction(ACT_BLINK, 0);
                if (ImGui::Button("超级跳", ImVec2(-1, 26)))
                    PostAction(ACT_SUPERJUMP, 0);
                if (ImGui::Button("把敌人全拽到面前", ImVec2(-1, 26)))
                    PostAction(ACT_TELEPORT_ALL, 0);
                if (ImGui::Button("全场碎甲 (官方 EMP 路径)", ImVec2(-1, 26)))
                    PostAction(ACT_EMP, 0);
                ImGui::TextColored(COL_DIM, "以上几个全是游戏自己的公开方法, 没有自造逻辑");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("恶作剧")) {
                ImGui::TextColored(COL_WARN, "全走游戏自己的物理接口 (爆炸/击飞/眩晕/扑街)");
                ImGui::TextColored(COL_DIM, "不是自己造物理, 都是官方在用的那一套, 不会破坏状态机");
                ImGui::Separator();

                ImGui::TextColored(COL_OK, "全场敌人");
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.20f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.74f, 0.28f, 0.28f, 1));
                if (ImGui::Button("全部炸成零件", ImVec2(-1, 36))) PostAction(ACT_EP_EXPLODE_ALL, 0);
                ImGui::PopStyleColor(2);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.58f, 0.38f, 0.16f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.76f, 0.52f, 0.22f, 1));
                if (ImGui::Button("全部扔上天", ImVec2(-1, 30))) PostAction(ACT_EP_FLING_ALL, 0);
                ImGui::PopStyleColor(2);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.36f, 0.58f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.50f, 0.76f, 1));
                if (ImGui::Button("全部眩晕 3 秒", ImVec2(-1, 30))) PostAction(ACT_EP_STUN_ALL, 0);
                ImGui::PopStyleColor(2);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.24f, 0.52f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.54f, 0.32f, 0.70f, 1));
                if (ImGui::Button("全部扑街  (集体被击倒)", ImVec2(-1, 30))) PostAction(ACT_EP_FALL_ALL, 0);
                ImGui::PopStyleColor(2);
                ImGui::TextColored(COL_DIM, "炸/飞 会立刻见效; 扑街是让它们躺下再自己爬起来");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "选中角色");
                if (!uiSelChar) {
                    ImGui::TextColored(COL_DIM, "还没选中 —— 先去「点名台」页点一行");
                } else {
                    if (ImGui::Button("炸碎它", ImVec2(-1, 30)))       PostActionPtr(ACT_EP_EXPLODE_SEL, uiSelChar);
                    if (ImGui::Button("扔它上天", ImVec2(-1, 26)))     PostActionPtr(ACT_EP_FLING_SEL, uiSelChar);
                    if (ImGui::Button("眩晕它 3 秒", ImVec2(-1, 26)))  PostActionPtr(ACT_EP_STUN_SEL, uiSelChar);
                }

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "自己 · 夸张动作");
                {
                    bool v = (g_epSuperKick != 0);
                    if (ImGui::Checkbox("超级踢##sk", &v)) {
                        InterlockedExchange(&g_epSuperKick, v ? 1 : 0);
                        PostAction(v ? ACT_EP_SUPERKICK_ON : ACT_EP_SUPERKICK_OFF, (int)(g_epKickMul * 100));
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  一脚把人踹出老远");
                    if (v) {
                        ImGui::SetNextItemWidth(-1);
                        ImGui::SliderFloat("##skm", &g_epKickMul, 1.0f, 20.0f, "踢击力度 %.1fx");
                        if (g_epSuperKick) PostAction(ACT_EP_SUPERKICK_ON, (int)(g_epKickMul * 100));
                    }
                }
                {
                    bool v = (g_epNoFall != 0);
                    if (ImGui::Checkbox("永不摔倒##nf", &v)) {
                        InterlockedExchange(&g_epNoFall, v ? 1 : 0);
                        PostAction(v ? ACT_EP_NOFALL_ON : ACT_EP_NOFALL_OFF, 0);
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  被打也不躺下");
                }
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##runm", &g_epRunMul, 1.0f, 10.0f, "奔跑动画 %.1fx  (鬼畜)"))
                    PostAction(ACT_EP_RUNANIM, (int)(g_epRunMul * 100));

                if (ImGui::Button("凭空拉弓  (手上没弓也能射一箭)", ImVec2(-1, 28)))
                    PostAction(ACT_EP_BOW, 0);

                if (ImGui::Button("电影镜头  (环绕看自己)", ImVec2(-1, 26)))
                    PostAction(ACT_EP_CINE, 90);
                ImGui::SameLine();
                if (ImGui::Button("还原镜头##cine", ImVec2(-1, 26)))
                    PostAction(ACT_EP_CINE_OFF, 0);

                ImGui::Separator();
                ImGui::TextColored(COL_DIM, "反重力: 全场敌人缓缓飘起来, 打不着你");
                {
                    bool v = (g_epLowGrav != 0);
                    if (ImGui::Checkbox("敌人反重力飘浮##lg", &v)) {
                        InterlockedExchange(&g_epLowGrav, v ? 1 : 0);
                        PostAction(v ? ACT_EP_LOWGRAV_ON : ACT_EP_LOWGRAV_OFF, 0);
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  上天, 追不到你");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("战场")) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.22f, 0.22f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.28f, 0.28f, 1));
                if (ImGui::Button("消灭场上所有敌人", ImVec2(-1, 38)))
                    PostAction(ACT_KILL_ALL, 0);
                ImGui::PopStyleColor(2);
                ImGui::TextColored(COL_DIM, "一键清场 (不含你和盟友)");
                ImGui::TextColored(COL_DIM, "v0.9 起走官方死亡链: 会算进关卡进度");

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.36f, 0.16f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.50f, 0.22f, 1));
                if (ImGui::Button("击败敌军战舰 (重制版)", ImVec2(-1, 38)))
                    PostAction(ACT_KILL_SHIPS, 0);
                ImGui::PopStyleColor(2);
                ImGui::TextColored(COL_DIM, "只摧毁敌方战舰 / 战机");
                ImGui::TextColored(COL_DIM, "v0.19: 清掉胜利标记 + 补死亡登记 + 主动补官方结算");
                {
                    bool sweep = (g_shipSweep != 0);
                    if (ImGui::Checkbox("顺带清场", &sweep)) g_shipSweep = sweep ? 1 : 0;
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "清了战舰还不够就把护航/杂兵一起清");
                }
                if (g_shipSunk > 0)
                    ImGui::TextColored(COL_OK, "上次击沉 %d 艘", g_shipSunk);

                ImGui::Spacing();
                if (ImGui::Button("强制结算本关 (剧情卡住时用)", ImVec2(-1, 32)))
                    PostAction(ACT_FORCE_WIN, 0);
                ImGui::TextColored(COL_WARN, "无视敌人死活, 直接按官方流程判通关");
                ImGui::TextColored(COL_DIM, "有的剧情关胜利条件不在常规判定里, 用它硬推");

                ImGui::Separator();
                ImGui::TextColored(COL_WARN, "剧情过场 (本作的过场 = 时间轴动画 + 真实体接管)");
                ImGui::TextColored(COL_DIM, "过场里剧本会抢走操作权 (拥抱 / 被搬走 / 扔进死亡立方)");
                {
                    bool v = (g_cutFreeOn != 0);
                    if (ImGui::Checkbox("过场自由行动##cutfree", &v))
                        PostAction(v ? ACT_CUT_FREE_ON : ACT_CUT_FREE_OFF, 0);
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  过场里也能动, 想砍谁砍谁");
                }
                {
                    bool v = (g_freeCamOn != 0);
                    if (ImGui::Checkbox("脱离剧情相机##freecam", &v))
                        PostAction(v ? ACT_FREE_CAM_ON : ACT_FREE_CAM_OFF, 0);
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  相机切回你肩后, 变回决斗场视野");
                }
                ImGui::TextColored(COL_DIM, "勾上持续生效: 剧本抢人/抢相机, 这边每 250ms 抢回来");
                ImGui::TextColored(COL_DIM, "两个都开 = 过场里完全自己说了算 (剧情照样往下走)");

                ImGui::Separator();
                {
                    bool v = (g_freeAll != 0);
                    if (ImGui::Checkbox("无差别攻击##freeall", &v))
                        PostAction(v ? ACT_FREEALL_ON : ACT_FREEALL_OFF, 0);
                    ImGui::SameLine();
                    ImGui::TextColored(COL_WARN, "  敌我通吃 · 没有任何保护");
                }
                ImGui::TextColored(COL_DIM, "友军 / 召唤物 / 剧情角色全部照打, 每 250ms 拆掉场上一切无敌");
                ImGui::TextColored(COL_DIM, "剧情里挡路的 NPC 想砍就砍, 不再被跳过");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "敌人行为 (v0.22)");
                {
                    bool v = (g_laserMove != 0);
                    if (ImGui::Checkbox("激光敌人也能移动##lasermove", &v))
                        PostAction(v ? ACT_LASERMOVE_ON : ACT_LASERMOVE_OFF, 0);
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  发射激光时照样追着你走");
                }
                ImGui::TextColored(COL_DIM, "原版: 放激光的敌人 (皇帝 / 紫色舰长) 会钉在原地站桩");
                ImGui::TextColored(COL_DIM, "原理: 每 50ms 替它补一次官方迈腿指令, 激光一停就收手");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "武器加长 (v0.22)");
                {
                    bool v = (g_wlenOn != 0);
                    if (ImGui::Checkbox("刀身变长##wlen", &v)) {
                        g_wlenOn = v ? 1 : 0;
                        PostAction(v ? ACT_WLEN_ON : ACT_WLEN_OFF,
                                   v ? (int)(g_wlenMul * 100.0f) : 0);
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, v ? "  已开启" : "  只对 剑 / 矛 生效");
                }
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##wlenmul", &g_wlenMul, 1.0f, 8.0f, "长度 %.1fx   (1.0 = 原版)")) {
                    if (g_wlenOn) PostAction(ACT_WLEN_ON, (int)(g_wlenMul * 100.0f));
                }
                ImGui::TextColored(COL_DIM, "整把武器连模型带判定一起拉长 (这回是真看得见)");
                ImGui::TextColored(COL_DIM, "和上面判定范围能叠着用: 那个只加判定, 这个连刀身一起");
                ImGui::TextColored(COL_DIM, "取消勾选立刻还原成原版长度");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "长度轴 (v0.34 修正)  ★ 这个轴选错就会变成变宽变厚");
                {
                    int sel = (int)g_wlenAxisSel;
                    if (ImGui::RadioButton("自动 (推荐)", sel == -1)) { g_wlenAxisSel = -1; WLenRestore(); }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("X 轴##ax", sel == 0)) { g_wlenAxisSel = 0; WLenRestore(); }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Y 轴##ay", sel == 1)) { g_wlenAxisSel = 1; WLenRestore(); }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Z 轴##az", sel == 2)) { g_wlenAxisSel = 2; WLenRestore(); }
                }
                ImGui::TextColored(COL_DIM, "v0.22 的轴判定是坏的: 它在 MeleeImpactArea 上找 EdgePoint1/2,");
                ImGui::TextColored(COL_DIM, "可这俩字段属于子类 BladeCutArea —— 取不到就静默退回 Y 轴。");
                ImGui::TextColored(COL_DIM, "Y 在武器模型上是厚度方向, 所以「加长」实际是变宽变厚, 不是变长。");
                ImGui::TextColored(COL_WARN, "现在改成用判定体包围盒反推长度轴; 自动不对就手点 X / Y / Z, 点哪个刀变长就用哪个");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "弓箭放大 (v0.35 修好)  ★ 一箭串一串");
                {
                    bool v = (g_arrowBig != 0);
                    if (ImGui::Checkbox("弓箭放大##arrowbig", &v)) {
                        g_arrowBig = v ? 1 : 0;
                        PostAction(v ? ACT_ARROWBIG_ON : ACT_ARROWBIG_OFF,
                                   v ? (int)(g_arrowMul * 100.0f) : 0);
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, v ? "  已开启" : "  只放大箭矢本身");
                }
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##arrowmul", &g_arrowMul, 1.0f, 20.0f, "箭矢粗细 %.1f 倍   (5.0 = 五米级)")) {
                    if (g_arrowBig) PostAction(ACT_ARROWBIG_ON, (int)(g_arrowMul * 100.0f));
                }
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("##arrowlen", &g_arrowLenMul, 1.0f, 20.0f, "箭矢长度 %.1f 倍   (5.0 = 五米级)");
                ImGui::TextColored(COL_DIM, "粗细走官方 SetBladeWidth, 长度走 BladeScaleTransforms 的 y 轴 (游戏不碰)");
                ImGui::TextColored(COL_DIM, "关掉立刻还原成原版大小, 不留残留");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "崩溃无视 (v0.24)  ★ 游戏崩了也照玩");
                {
                    bool v = (g_crashClean != 0);
                    if (ImGui::Checkbox("弹窗直接消失，界面恢复##crashclean", &v))
                        PostAction(v ? ACT_CRASHCLEAN_ON : ACT_CRASHCLEAN_OFF, 0);
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  已清 %ld 次", (long)g_ccCleared);
                }
                ImGui::TextColored(COL_DIM, "本作弹的不是系统窗口, 是 Unity 自绘的 ErrorWindow —— 枚举窗口摸不到它");
                ImGui::TextColored(COL_DIM, "真凶: ErrorManager._hasCrashed 一置 true, GameUIRoot 每帧直接 return, UI 全停");
                ImGui::TextColored(COL_DIM, "但 AI / 物理 / 召唤跑在别的循环里照旧 —— 所以看着崩了其实没死");
                ImGui::TextColored(COL_DIM, "这项每 250ms 把标志清回 false + 藏掉堆栈面板, 界面立刻复活, 不用重启");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "崩溃弹窗杀手 (v0.23, 系统窗口)");
                {
                    bool v = (g_crashKill != 0);
                    if (ImGui::Checkbox("自动按掉崩溃弹窗##crashkill", &v))
                        PostAction(v ? ACT_CRASHKILL_ON : ACT_CRASHKILL_OFF, 0);
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  已清 %ld 个", (long)g_crashKilled);
                }
                ImGui::TextColored(COL_DIM, "砍死自己队友把游戏逼崩时, 那个弹窗会挡住你继续玩");
                ImGui::TextColored(COL_DIM, "开了它: 每 0.5s 扫一遍窗口, 原生异常框 / 未响应框直接按掉");
                ImGui::TextColored(COL_DIM, "UnityCrashHandler / WerFault 这类报告器连同进程一起做掉");
                ImGui::TextColored(COL_DIM, "游戏主循环其实还活着 —— 清掉框就能接着召唤、接着看它们打");
                ImGui::TextColored(COL_WARN, "注意: 标题带 crash/error 的正常窗口也会被一起按掉");

                ImGui::Separator();
                RowToggle("AI停电", "  除你以外全部静止", &uiAIOff, ACT_AIOFF_ON, ACT_AIOFF_OFF);
                ImGui::TextColored(COL_DIM, "敌人和队友都会停住, 你可以随便走");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "AI 变聪明 (v0.38)  ★ 敌人会走位 / 闪击 / 用所有武器");
                RowToggle("AI 变聪明", "  默认关: 开了之后敌方 AI 变高手 —— 蛇形走位、闪避、冲刺闪击、精瞄、技能全开",
                          &uiAiSmart, ACT_AISMART_ON, ACT_AISMART_OFF);
                ImGui::TextColored(COL_DIM, "走位: 侧移闪避 + 蛇形逼近 + 频繁跳跃/喷射机动");
                ImGui::TextColored(COL_DIM, "闪击: 每次攻击都带冲刺 + 擦身冲锋 + 踢击");
                ImGui::TextColored(COL_DIM, "武器: 按距离自动切弓/剑/锤, 并把模型上已有的武器全解锁给它用");
                ImGui::TextColored(COL_DIM, "准头: 随机散布清零 + 预判提前量, 弓手几乎箭箭咬人");
                ImGui::TextColored(COL_DIM, "手段: 改的是 AI 自己的行为参数 (不凭空造东西), 每秒补刷一次防游戏回写");
                ImGui::TextColored(COL_WARN, "注意: 关了开关, 本场仍保留已改的数值; 重进关卡才彻底恢复默认");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "克隆体收编 (v0.33)  ★ 让你场上的克隆变成自己人");
                RowToggle("克隆体收编", "  收编后它不再打你, 和你一起打敌人", &uiCloneAlly,
                          ACT_CLONEALLY_ON, ACT_CLONEALLY_OFF);
                ImGui::TextColored(COL_DIM, "原因: 克隆体的中立位和玩家不一致, 被游戏判成对立方才追着砍你");
                ImGui::TextColored(COL_DIM, "这里把中立位抹掉, 它就和玩家同队了; 名册里还是克隆, 身份不变");
                ImGui::TextColored(COL_DIM, "不是一次的 —— 每 250ms 补漏, 新生成的克隆也会被收编");
                ImGui::TextColored(COL_WARN, "注意: 收编后克隆算玩家队, 电梯口那堵力场会连它一起弹开");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "废除电梯拦截墙 (v0.33)");
                RowToggle("废除电梯拦截墙", "  被力场挡住、推不进电梯时按这个", &uiFenceOff,
                          ACT_FENCEOFF_ON, ACT_FENCEOFF_OFF);
                ImGui::TextColored(COL_DIM, "拦截墙 = GarbageForceField: 只弹玩家和玩家队角色");
                ImGui::TextColored(COL_DIM, "开了它, 场上力场的判定和渲染一起关掉, 直接走过去");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("官方调试")) {
                ImGui::TextColored(COL_WARN, "游戏内置的开发者功能");
                ImGui::TextColored(COL_DIM, "正式版默认锁着, 这里帮你打开");
                ImGui::Separator();

                RowToggle("实验分支模式", "  官方调试菜单的前置开关", &uiDevBranch,
                          ACT_DEV_BRANCH_ON, ACT_DEV_BRANCH_OFF);

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.34f, 0.52f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.48f, 0.72f, 1));
                if (ImGui::Button("打开官方调试菜单", ImVec2(-1, 40)))
                    PostAction(ACT_DEV_MENU, 0);
                ImGui::PopStyleColor(2);
                ImGui::TextColored(COL_DIM, "等效于自己按 右Ctrl (会先自动开实验分支)");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "官方功能 (直接调游戏自己的方法)");
                ImGui::Spacing();
                if (ImGui::Button("官方: 杀光所有敌人", ImVec2(-1, 32)))
                    PostAction(ACT_DEV_KILLALL, 0);
                ImGui::TextColored(COL_DIM, "DebugManager.KillAllEnemies()");
                ImGui::Spacing();
                if (ImGui::Button("官方: 加技能点", ImVec2(-1, 32)))
                    PostAction(ACT_DEV_SKILL, 0);
                ImGui::TextColored(COL_DIM, "UpgradeManager.IncreaseSkillPoints()");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("编辑器")) {
                ImGui::TextColored(COL_WARN, "关卡编辑器 · 属性面板中文化");
                ImGui::TextColored(COL_DIM, "原版面板上的字段名, 是代码里的英文标识符");
                ImGui::TextColored(COL_DIM, "例如  Min Head Voxels Damaged For Death");
                ImGui::TextColored(COL_DIM, "语言包管不到它们, 所以简中版里一直是英文");
                ImGui::Separator();

                {
                    bool v = (g_cnEditor != 0);
                    if (ImGui::Checkbox("属性面板中文标签##cn", &v)) g_cnEditor = v ? 1 : 0;
                    ImGui::SameLine();
                    ImGui::TextColored(COL_DIM, "  打开后在关卡编辑器里生效");
                }
                ImGui::TextColored(COL_DIM, "约 0.7 秒刷一次; 面板重建后会自动补上");
                ImGui::TextColored(COL_DIM, "只改界面文字, 不碰游戏逻辑和存档");
                ImGui::Separator();

                ImGui::TextColored(COL_OK, "覆盖范围");
                ImGui::BulletText("敌人 / 玩家 身上的组件属性");
                ImGui::BulletText("   生命 速度 跳跃 护甲 踢击 视角 …");
                ImGui::BulletText("触发器 灯光 场地 颜色 生成器");
                ImGui::BulletText("认不出来的词保留英文, 不会乱翻");
                ImGui::Spacing();
                ImGui::TextColored(COL_DIM, "还有哪些看不懂, 跟我说, 我加词表");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "放置敌人  (对象库只留敌人)");
                ImGui::TextColored(COL_DIM, "官方的敌人全在对象库的 EnemySpawns 文件夹里,");
                ImGui::TextColored(COL_DIM, "夹在 灯光 / 场地 / 特效 一堆英文文件夹中间, 不搜看不见。");
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.42f, 0.30f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.60f, 0.42f, 1));
                if (ImGui::Button("对象库 -> 只显示敌人", ImVec2(-1, 34)))
                    PostAction(ACT_ED_ENEMY_FILTER, 0);
                ImGui::PopStyleColor(2);
                if (ImGui::Button("对象库 -> 恢复显示全部", ImVec2(-1, 28)))
                    PostAction(ACT_ED_FILTER_RESET, 0);
                ImGui::TextColored(COL_DIM, "过滤完就照老办法从左边拖进场景");

                ImGui::Spacing();
                ImGui::TextColored(COL_DIM, "不想拖也行, 直接放一个到镜头前面:");
                {
                    static const char* edPtrs[JQR_ED_MAX];
                    int edN = (int)g_edCount;
                    if (edN > JQR_ED_MAX) edN = JQR_ED_MAX;
                    for (int i = 0; i < edN; i++) edPtrs[i] = g_edNames[i];
                    if (edN <= 0) {
                        ImGui::TextColored(COL_WARN, "读不到敌人列表 —— 先进入关卡编辑器");
                    } else {
                        if (uiEdSel >= edN) uiEdSel = 0;
                        ImGui::SetNextItemWidth(-1);
                        ImGui::Combo("##edenemy", &uiEdSel, edPtrs, edN);
                        if (ImGui::Button("在镜头前方放一个", ImVec2(-1, 32)))
                            PostAction(ACT_ED_PLACE_ENEMY, uiEdSel);
                        ImGui::TextColored(COL_DIM, "共 %d 种可放敌人; 会写进关卡, 记得保存", edN);
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("高级")) {
                static int uiFwSel = 1;
                static int uiDbgHideUI = 0, uiDbgSeams = 0, uiDbgNoSmooth = 0;
                static int uiTilt = 0, uiGodAll = 0;
                bool v;

                ImGui::TextColored(COL_OK, "全场招安");
                ImGui::TextColored(COL_DIM, "所有敌人当场倒戈变成你的兵, 他们转头打自己人");
                if (ImGui::Button("全场招安", ImVec2(-1, 28))) PostAction(ACT_RECRUIT_ALL, 0);

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "全场体型");
                ImGui::TextColored(COL_DIM, "只改敌人体型: 变小好砍, 变大当靶子");
                if (ImGui::Button("变小 50%", ImVec2(115, 26)))   PostAction(ACT_SCALE_ALL, 50);
                ImGui::SameLine();
                if (ImGui::Button("变大 150%", ImVec2(115, 26)))  PostAction(ACT_SCALE_ALL, 150);
                ImGui::SameLine();
                if (ImGui::Button("还原", ImVec2(80, 26)))        PostAction(ACT_SCALE_ALL, 100);

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "全场统一武器");
                ImGui::TextColored(COL_DIM, "把场上敌人的武器全换掉 (剑兵拿弓也照换)");
                {
                    static const char* wnames[] = { "空手", "剑", "弓", "锤", "矛" };
                    static const int   wids[]   = { 0, 1, 2, 3, 5 };
                    ImGui::SetNextItemWidth(150);
                    ImGui::Combo("##fwsel", &uiFwSel, wnames, 5);
                    ImGui::SameLine();
                    if (ImGui::Button("全场换", ImVec2(-1, 0)))
                        PostAction(ACT_FORCE_WEAPON, wids[uiFwSel]);
                }

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "一键 (适合绑快捷键)");
                if (ImGui::Button("秒杀最近敌人", ImVec2(-1, 26)))     PostAction(ACT_KILL_NEAREST, 0);
                if (ImGui::Button("抢光武器 (敌人缴械 + 我换剑)", ImVec2(-1, 26))) PostAction(ACT_TAKE_ALL_WEAPON, 0);
                if (ImGui::Button("技能点给满 (99)", ImVec2(-1, 26)))  PostAction(ACT_MAXSKILL, 0);
                ImGui::TextColored(COL_DIM, "自杀重生 = 倒下后由克隆体接管, 快速重开这一波");
                if (ImGui::Button("自杀重生", ImVec2(-1, 26)))         PostAction(ACT_SUICIDE, 0);

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "镜头 (v0.18)");
                v = (uiTilt != 0);
                if (ImGui::Checkbox("解锁镜头俯仰 (±89°, 抬头看天/低头看脚)", &v)) {
                    uiTilt = v ? 1 : 0;
                    PostAction(v ? ACT_TILT_ON : ACT_TILT_OFF, 0);
                }
                ImGui::TextColored(COL_DIM, "  移速 / 跳跃 / 踢力在「机动」页, 这里不重复放");

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "全场状态 (v0.18)");
                if (ImGui::Button("全场点燃", ImVec2(-1, 26)))          PostAction(ACT_BURN_ALL, 0);
                if (ImGui::Button("全场眩晕 3 秒", ImVec2(132, 26)))     PostAction(ACT_STUN_ALL, 30);
                ImGui::SameLine();
                if (ImGui::Button("眩晕 10 秒", ImVec2(-1, 26)))         PostAction(ACT_STUN_ALL, 100);
                RowToggle("全场无敌", "  敌人打不死: 配乱斗看它们打一辈子", &uiGodAll,
                          ACT_GOD_ALL_ON, ACT_GOD_ALL_OFF);
                ImGui::TextColored(COL_DIM, "敌人视野");
                if (ImGui::Button("变瞎 (只咬 2 米内)", ImVec2(160, 24))) PostAction(ACT_FOE_BLIND, 0);
                ImGui::SameLine();
                if (ImGui::Button("疯狗 (无限追击)", ImVec2(-1, 24)))     PostAction(ACT_FOE_FRENZY, 0);

                ImGui::Separator();
                ImGui::TextColored(COL_OK, "官方开发者开关 (DebugManager)");
                ImGui::TextColored(COL_DIM, "游戏自己留的调试字段, 正式版里没有界面");
                v = (uiDbgHideUI != 0);
                if (ImGui::Checkbox("拍照模式 (藏掉 HUD, 好截图)", &v)) {
                    uiDbgHideUI = v ? 1 : 0;
                    PostAction(v ? ACT_DBG_HIDEUI_ON : ACT_DBG_HIDEUI_OFF, 0);
                }
                v = (uiDbgSeams != 0);
                if (ImGui::Checkbox("显示身体接缝 (看机器人怎么拼起来的)", &v)) {
                    uiDbgSeams = v ? 1 : 0;
                    PostAction(v ? ACT_DBG_SEAMS_ON : ACT_DBG_SEAMS_OFF, 0);
                }
                v = (uiDbgNoSmooth != 0);
                if (ImGui::Checkbox("关掉镜头旋转平滑", &v)) {
                    uiDbgNoSmooth = v ? 1 : 0;
                    PostAction(v ? ACT_DBG_NOSMOOTH_ON : ACT_DBG_NOSMOOTH_OFF, 0);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("快捷键")) {
                int cap = HfCapturing();

                ImGui::TextColored(COL_DIM, "点方块 = 绑键 (再按键盘任意一键) · 右键方块 = 清空该键");
                ImGui::TextColored(COL_DIM, "内置功能: 子弹时间 / 无敌 / 飞行 等 · 召唤兵种: 84 个兵种方块, 左键绑盟友 右键绑敌人");
                ImGui::Spacing();

                if (ImGui::RadioButton("全部", g_hkView == 0)) g_hkView = 0;
                ImGui::SameLine();
                if (ImGui::RadioButton("内置功能", g_hkView == 1)) g_hkView = 1;
                ImGui::SameLine();
                if (ImGui::RadioButton("召唤兵种", g_hkView == 2)) g_hkView = 2;
                ImGui::SameLine();
                ImGui::TextColored(COL_DIM, "  (内置 %d 项 · 召唤 %d 项)",
                                   HfCount() - HfCustomCount(), HfCustomCount());
                ImGui::Spacing();

                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputTextWithHint("##hksrch", "搜索名称 (留空 = 全部)", g_hkSearch, sizeof(g_hkSearch)))
                    g_hkSearchOn = (g_hkSearch[0] != 0);
                if (g_hkSearch[0] == 0) g_hkSearchOn = 0;
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.46f, 0.20f, 0.20f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.64f, 0.28f, 0.28f, 1));
                if (ImGui::Button("一键还原出厂", ImVec2(150, 26)))
                    HfResetAll();
                ImGui::PopStyleColor(2);
                ImGui::SameLine();
                if (ImGui::Button("全部清空", ImVec2(150, 26)))
                    HfClearAll();
                ImGui::Separator();

                ImGui::BeginChild("##hkwall", ImVec2(0, 0), true);
                {
                    const float cw = 168.0f, chh = 46.0f;
                    float avail = ImGui::GetContentRegionAvail().x;
                    int perRow = (int)(avail / (cw + 6.0f));
                    if (perRow < 1) perRow = 1;
                    int col = 0;
                    int shownFunc = 0, shownSumm = 0;

                    if (g_hkView == 2) {

                        for (int i = 0; i < JQR_ENEMYTYPE_COUNT; i++) {
                            int tid = g_typeIds[i];
                            if (!tid) continue;
                            const char* cn = g_typeNamesCN[i];

                            if (g_hkSearchOn) {
                                char q[48], nm[96];
                                _snprintf_s(q, sizeof(q), _TRUNCATE, "%s", g_hkSearch);
                                for (char* p = q; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
                                _snprintf_s(nm, sizeof(nm), _TRUNCATE, "%s", cn);
                                for (char* p = nm; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
                                if (!strstr(nm, q)) continue;
                            }

                            int idxAlly = -1, idxEnemy = -1;
                            for (int k = 0; k < HfCount(); k++) {
                                if (!HfIsCustom(k)) continue;
                                if (HfCustomTypeId(k) != tid) continue;
                                if (HfCustomAlly(k)) idxAlly = k; else idxEnemy = k;
                            }

                            BOOL onA = (cap >= 0 && cap == idxAlly);
                            BOOL onE = (cap >= 0 && cap == idxEnemy);

                            ImGui::PushID(100000 + i);
                            if (col > 0) ImGui::SameLine(0, 6);
                            col = (col + 1) % perRow;

                            ImVec4 bg, hv;
                            if (onA || onE)  { bg = ImVec4(0.62f,0.40f,0.10f,1); hv = ImVec4(0.80f,0.54f,0.14f,1); }
                            else if (idxAlly >= 0 || idxEnemy >= 0)
                                             { bg = ImVec4(0.15f,0.30f,0.40f,1); hv = ImVec4(0.22f,0.44f,0.56f,1); }
                            else             { bg = ImVec4(0.20f,0.22f,0.27f,1); hv = ImVec4(0.30f,0.34f,0.42f,1); }

                            ImGui::PushStyleColor(ImGuiCol_Button, bg);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hv);
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, hv);
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

                            char ka[32], ke[32];
                            const char* sa = onA ? "按键盘…"
                                           : (idxAlly  >= 0 ? HfKeyName(idxAlly,  ka, sizeof(ka))  : "未绑");
                            const char* se = onE ? "按键盘…"
                                           : (idxEnemy >= 0 ? HfKeyName(idxEnemy, ke, sizeof(ke)) : "未绑");
                            char lbl[256];
                            _snprintf_s(lbl, sizeof(lbl), _TRUNCATE, "%s\n友 %s · 敌 %s", cn, sa, se);

                            if (ImGui::Button(lbl, ImVec2(cw, chh))) {
                                if (idxAlly >= 0) HfBeginCapture(idxAlly);
                                else              HfCustomAddEx(tid, 1, 1);
                            }
                            if (ImGui::IsItemClicked(1)) {
                                if (idxEnemy >= 0) HfBeginCapture(idxEnemy);
                                else               HfCustomAddEx(tid, 0, 1);
                            }
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("兵种编号 %d\n左键: 绑盟友键   右键: 绑敌人键\n已经绑过的会直接改绑",
                                                  tid);
                            ImGui::PopStyleVar();
                            ImGui::PopStyleColor(3);
                            ImGui::PopID();
                            shownSumm++;
                        }
                    } else {
                    for (int i = 0; i < HfCount(); i++) {
                        BOOL isCustom = HfIsCustom(i);

                        if (g_hkView == 1 && isCustom) continue;

                        if (g_hkSearchOn) {
                            char q[48];
                            _snprintf_s(q, sizeof(q), _TRUNCATE, "%s", g_hkSearch);
                            for (char* p = q; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
                            if (isCustom) {
                                if (!strstr(HfCustomTypeName(i), q)) continue;
                            } else {
                                char nm[96];
                                _snprintf_s(nm, sizeof(nm), _TRUNCATE, "%s", HfName(i));
                                for (char* p = nm; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
                                if (!strstr(nm, q)) continue;
                            }
                        }

                        if (isCustom) shownSumm++; else shownFunc++;

                        BOOL on    = (cap == i);
                        BOOL bound = (HfBindOf(i) != 0);

                        ImGui::PushID(i);
                        if (col > 0) ImGui::SameLine(0, 6);
                        col = (col + 1) % perRow;

                        ImVec4 bg, hv;
                        if (on)          { bg = ImVec4(0.62f,0.40f,0.10f,1); hv = ImVec4(0.80f,0.54f,0.14f,1); }
                        else if (bound)  { bg = ImVec4(0.14f,0.34f,0.22f,1); hv = ImVec4(0.20f,0.50f,0.30f,1); }
                        else             { bg = ImVec4(0.20f,0.22f,0.27f,1); hv = ImVec4(0.30f,0.34f,0.42f,1); }
                        if (isCustom && !on) { bg.x += 0.03f; bg.z += 0.07f; }

                        ImGui::PushStyleColor(ImGuiCol_Button, bg);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hv);
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hv);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

                        char kb[64];
                        const char* kn = on ? "按键盘…"
                                       : (bound ? HfKeyName(i, kb, sizeof(kb)) : "未绑");
                        char lbl[256];
                        if (isCustom) {
                            _snprintf_s(lbl, sizeof(lbl), _TRUNCATE, "%s %s\n[ %s ]",
                                        HfCustomAlly(i) ? "友" : "敌", HfCustomTypeName(i), kn);
                        } else {
                            _snprintf_s(lbl, sizeof(lbl), _TRUNCATE, "%s\n[ %s ]", HfName(i), kn);
                        }

                        if (ImGui::Button(lbl, ImVec2(cw, chh)))
                            HfBeginCapture(i);

                        if (ImGui::IsItemClicked(1))
                            HfSetBind(i, 0);
                        if (ImGui::IsItemHovered()) {
                            if (isCustom)
                                ImGui::SetTooltip("%s · %s\n左键: 绑键   右键: 清空\n(要删除请到「召唤」页的管理区)",
                                                  HfCustomAlly(i) ? "召唤盟友" : "召唤敌人",
                                                  HfCustomTypeName(i));
                            else
                                ImGui::SetTooltip("%s\n左键: 绑键   右键: 清空", HfName(i));
                        }
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor(3);
                        ImGui::PopID();
                    }
                    }

                    if (shownFunc == 0 && shownSumm == 0)
                        ImGui::TextColored(COL_DIM, "  (这里没有匹配的项)");
                }
                ImGui::EndChild();

                if (cap >= 0)
                    ImGui::TextColored(COL_WARN, "  绑定中: 现在按键盘任意键 (Esc 取消)");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("说明")) {
                ImGui::TextColored(COL_OK, "热键 (不用开菜单)");
                ImGui::Separator();
                ImGui::Text("F1  盟友 剑士 I       F6  盟友 装甲机器人");
                ImGui::Text("F2  敌人 剑士 I       F7  盟友 召唤行者");
                ImGui::Text("F3  盟友 锤兵 I       F8  盟友 锯片独轮车");
                ImGui::Text("F4  盟友 蜘蛛龙5000   F9  盟友 可骑射弹爬虫");
                ImGui::Text("F5  盟友 弓箭手 I     F10 盟友 剑士 IV");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "Insert");
                ImGui::SameLine();
                ImGui::Text("显示 / 隐藏本菜单");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(COL_OK, "v0.24 更新  ★ 崩溃无视 (游戏崩了也照玩)");
                ImGui::BulletText("挖出真相: 游戏弹的不是系统窗口, 是 Unity 自绘的 ErrorWindow");
                ImGui::BulletText("   v0.23 枚举窗口的做法对它无效 —— 已经用反编译证实");
                ImGui::BulletText("真凶 = ErrorManager._hasCrashed: 置 true 后 GameUIRoot 每帧 return");
                ImGui::BulletText("   于是 UI 全停, 可 AI / 物理 / 召唤照跑 (看着崩了其实还活着)");
                ImGui::BulletText("新功能每 250ms 把标志清回 false + Hide() 掉堆栈面板, 界面立刻复活");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.23 更新  ★ 过场功能修好 + 崩溃弹窗杀手");
                ImGui::BulletText("过场自由行动 / 脱离剧情相机: 修真了 (v0.20 那两个是类查找静默失败)");
                ImGui::BulletText("   现在改成翻元数据表硬查类名 + 从玩家祖先反查导演, 双保险");
                ImGui::BulletText("崩溃弹窗杀手: 一键按掉 UnityCrashHandler / WerFault / 本进程异常框");
                ImGui::BulletText("   Windows 错误报告 UI 也一起压制 (只动 HKCU)");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.22 更新  ★ 激光敌人可移动 + 武器加长");
                ImGui::BulletText("激光敌人也能移动: 皇帝 / 紫色舰长放激光时不再站桩");
                ImGui::BulletText("   每 50ms 替它补一次官方迈腿指令, 边扫射边朝你压过来");
                ImGui::BulletText("武器加长: 缩放武器根, 刀身和判定一起变长 (真的看得见)");
                ImGui::BulletText("   拉长轴由刃线反推, 关开关 / 换武器原样还原");
                ImGui::BulletText("   和 v0.13「判定范围」能叠着用");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.18 更新  ★ 高级玩法 III + 斗蛐蛐 3.0");
                ImGui::BulletText("镜头俯仰解锁 ±89° (抬头看天 / 低头看脚)");
                ImGui::BulletText("敌人视野: 变瞎 (只咬 2 米内) / 疯狗 (无限追击)");
                ImGui::BulletText("全场点燃 / 全场眩晕 / 全场无敌 (打不死模式)");
                ImGui::BulletText("斗蛐蛐 3.0: 环形布阵 + 比赛准备 + 押注 + 冠军播报");
                ImGui::BulletText("  随机武器大乱斗 + 实时红蓝比分");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.15 / v0.16 / v0.17 更新");
                ImGui::BulletText("快捷键页: 任意功能绑键, 同一对开/关可共用一个键");
                ImGui::BulletText("高级页: 招安 / 体型 / 统一武器 / 秒杀最近 / 技能点 / 开发者开关");
                ImGui::BulletText("斗蛐蛐 2.0: 红蓝分队对抗 / 指定单挑 (其他人围观) / 战报");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.12 更新  ★ 关卡编辑器属性面板中文化");
                ImGui::BulletText("编辑器右边那一堆看不懂的英文, 现在能变中文了");
                ImGui::BulletText("那些标签是代码里的字段名, 语言包管不到");
                ImGui::BulletText("开「编辑器」页里的开关即可, 只改界面文字");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.11 更新  ★ 变装修好了 + 清掉一堆隐形崩溃");
                ImGui::BulletText("修复 变装没用: 传进去的类型错了");
                ImGui::BulletText("   旧代码把兵种预制体的 Transform 当成 CharacterModel 传");
                ImGui::BulletText("   给了 CreateCharacterModel -> 内部强转直接访问违例");
                ImGui::BulletText("   现在按官方 SpawnEnemy 的取法, 从预制体的 FPM 上取");
                ImGui::BulletText("   真正的 CharacterModelPrefab, 并且先建新的再销毁旧的");
                ImGui::BulletText("修复 击杀不认账 (真正的根因)");
                ImGui::BulletText("   Kill 内部只有三步: 判活 -> 标记死亡 -> onDeath()");
                ImGui::BulletText("   而 onDeath 会抛异常, 导致「死亡登记」没做");
                ImGui::BulletText("   现在 Kill 之后自己补一遍 CharacterTracker.OnCharacterDied");
                ImGui::BulletText("修复 内讧每 0.15 秒崩一次 (日志刷了 21 条 SEH)");
                ImGui::BulletText("修复 幽灵对象导致的随机崩溃");
                ImGui::BulletText("   物体已销毁但引用还在 -> 一碰就是访问违例");
                ImGui::BulletText("   现在点名前先过滤, 玩家/目标/列表都过这道筛子");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.10 更新  ★ 夺舍 / 变装 两个卡死都修了");
                ImGui::BulletText("修复 夺舍卡死: 不再调游戏自带的意识转移");
                ImGui::BulletText("   它中途会把「玩家」从记录里抹掉、再装回来;");
                ImGui::BulletText("   只要中间崩一句, 玩家就永远被抹掉了 -> 全游戏卡住");
                ImGui::BulletText("   现在手动执行同样的步骤, 每句单独隔离 + 最后必定修补状态");
                ImGui::BulletText("修复 变装卡住: 改成就地换模型");
                ImGui::BulletText("   不再销毁重建角色 (那条路走网络销毁, 会挂住主线程)");
                ImGui::BulletText("修复 骑乘崩溃: 骑之前先确认坐骑真的有模型");
                ImGui::BulletText("修复 一击必杀 / 敌人攻速: 字段找错了类, 导致静默失效");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.9 更新  ★ 全是修 bug");
                ImGui::BulletText("修复 击杀不认账: 改走官方 Character.Kill");
                ImGui::BulletText("   以前用 DestroyIfAlive 只销毁物体");
                ImGui::BulletText("   → 游戏不记死亡, 全杀光也不通关");
                ImGui::BulletText("修复 骑乘崩溃: 只会挑能骑的角色");
                ImGui::BulletText("   以前可能选中战舰/陨石, 类型不对直接崩");
                ImGui::BulletText("修复 夺舍无效/卡: 同上, 目标必须是可夺舍角色");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.8 更新");
                ImGui::BulletText("新增「点名台」: 列出场上全部角色 (兵种 + 阵营)");
                ImGui::BulletText("   逐个 击杀 / 收编为友军 / 缴械 / 穿脱护甲");
                ImGui::BulletText("新增 内讧 (让它痛击自己队友)");
                ImGui::BulletText("新增 斗蛐蛐: 强制改 AI 目标当成你的傀儡");
                ImGui::BulletText("   换武器 (剑士也能拿矛) / 原地变身其它兵种");
                ImGui::BulletText("   体型 / 移动速度 / 攻击速度 单个调节");
                ImGui::BulletText("新增 全场乱斗 (所有 AI 互相打, 你躲旁边看)");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.7 更新");
                ImGui::BulletText("修复 胜利动作崩溃 (改走官方表情索引)");
                ImGui::BulletText("重做 夺舍 (补满次数 + 自己挑最近敌人)");
                ImGui::BulletText("新增 骑乘任意角色 / 下车");
                ImGui::BulletText("新增 敌人一击必杀 / 攻速归零");
                ImGui::BulletText("新增 磁力靴 / 喷气背包");
                ImGui::BulletText("日志不再每次清空, 带毫秒时间戳");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.6 更新");
                ImGui::BulletText("新增 夺舍敌人 / 敌人缴械 / 胜利动作");
                ImGui::BulletText("飞天键位改成 方向键 + PageUp/PageDown");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.5 更新");
                ImGui::BulletText("新增 飞天 / 平面穿墙");
                ImGui::BulletText("新增 移动速度 / 跳跃力 / 任意武器");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.4 更新");
                ImGui::BulletText("修复中文乱码 (字体字形表失效)");
                ImGui::BulletText("新增 官方调试菜单 / 实验分支模式");
                ImGui::BulletText("新增 官方: 杀光敌人 / 加技能点");
                ImGui::Spacing();
                ImGui::TextColored(COL_OK, "v0.3");
                ImGui::BulletText("无限能量 / 无限意识转移 / 克隆 +1");
                ImGui::BulletText("召唤列表全部翻成中文");
                ImGui::BulletText("子弹时间改成持续锁定");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(COL_WARN, "注意");
                ImGui::TextWrapped("必须在关卡里 (玩家活着) 才有效。主菜单按了没反应是正常的。");
                ImGui::Spacing();
                ImGui::TextColored(COL_DIM, "改动全部在内存里, 游戏文件没动过。");
                ImGui::TextColored(COL_DIM, "删掉 JQRMod 文件夹 + version.dll 即完全还原。");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

#define WM_JQR_TICK    (WM_APP + 211)
static volatile LONG g_tickPosted = 0;

static void RequestTick(void)
{
    if (!g_hwnd) return;
    if (InterlockedExchange(&g_tickPosted, 1) == 0)
        PostMessageW(g_hwnd, WM_JQR_TICK, 0, 0);
}

typedef HRESULT (WINAPI *tPresent)(IDXGISwapChain*, UINT, UINT);
static tPresent             oPresent = NULL;
static ID3D11Device*        g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dContext = NULL;
static IDXGISwapChain*      g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRTV = NULL;
static volatile LONG g_imguiState = 0;
static HWND g_menuHwnd = NULL;
static UINT g_lastW = 0, g_lastH = 0;

static void CreateRTV(void)
{
    ID3D11Texture2D* back = NULL;
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = NULL; }
    if (!g_pSwapChain || !g_pd3dDevice) return;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) && back) {
        g_pd3dDevice->CreateRenderTargetView(back, NULL, &g_mainRTV);
        back->Release();
    }
}

static ImVector<ImWchar> g_glyphRanges;

static const char* kUICN =

    "机器人角斗场修改器已连接正在游戏隐藏显示调试官方"

    "召唤玩家战场说明"

    "选择类型中文然后按下面按钮当前选中盟友敌人帮你打来生成位置你前方偏右约米"
    "克隆多一条命点一下数死了还能接着"

    "无敌打不死友军召唤出来的也战舰后期开时用不理不打但会队"
    "无限能量挥砍技术再消耗意识转移把次补满子弹间持续锁定启关速度"
    "杀过关都顶回原"

    "消灭场上所有一键清场含和击败只摧毁方战机停电除以外全部静止"
    "抖动你可以随便走"

    "热键需菜单锤兵锯片独轮车爬虫装甲行者弓箭手剑士更新列表翻成注意"
    "必须在关卡里活着才效主没反应是正常改动内存文件过删掉夹即完全还原"

    "正式版默认锁着这里帮开实验分支模式前置开关等效于自己按右"
    "调用方法光所有加技能点"

    "机动飞天开启小键盘控制前后左右升降速度米秒落地松开重力让它自己掉"
    "穿墙平面撞不到但会去高度锁那一刻只想水平过要上跟角色状态移动跳跃"
    "武器装备选中先加进你持有列表再换"

    "修复字形失效新增内置直接会先自动点击确认取消关闭返回上一页设置个"

    "变装就地换模型为目标兵种角色不重建卡了步骤身体救场"

    "变装修好了清掉一堆隐形崩溃没用传进去类错误旧代码把兵种预制体当成给内"
    "部强转访问违例按官方取法从上真正并且先建新再销毁击杀不认账根因只三步判"
    "活标记死亡而会抛异常导致登记没做之后自己补一遍秒次日志刷条幽灵对象随机"
    "体已但引用还碰就是点名过滤玩家目标列都过道筛子"

    "编辑器属性面板中文化原版字段名是代码里的英文标识符例如语言包管不到"
    "它们所以简中直刷一次重建会自动补只改界文字碰逻辑和存档覆盖范围"
    "敌人玩家身组件生命速度跳跃护甲踢击视角触发器灯光场地颜色生成"
    "认出来词保留不会乱翻还有哪些看懂跟我说加表"
    "右侧那一堆现在能变页里开关就行了签"

    "敌人行为激光也能移动发射时照样追着你走原版放皇帝紫色舰长会钉在站桩"
    "原理每替它补次官方迈腿指令一停就收手武器加长刀身变已开启只对剑矛生效"
    "度整把连模型带判定起拉这回是真看得见和上面范围叠用那个加取消勾选立刻还原成"
    "朝压过来缩根由刃线反推轴样"

    "与世东丢两串举么义乐乘买事互交什仅仍他仰任众伤低住何余作使依信俯倍倒候值假傀像儡儿允元充免入公共其具写冠冲决冷冻准几凭凶函切刚创初利别刺剥剧剩副办功励匹区半占卫却卸厂又双受句另台号吃各合同向吗吸告员呼咬品响喷嘛嘴图圈坊坏坐块垂域基填境壁声处够大太头夸夺奔奖套始宁安客害容宽寸封少尖尘尚尺尾局层屏巡工差币布帧幕干序库延弃弊张归录影往径待循心快忽怎怪总恒恢息恶悬情慢戈或截戳户房扑扔托执扫找承抓抗抢报抬抱抹押拆拍拖招拟拥拦拼拽拿挂挑挖挡挪据捷探描搜搬摆摔摸撑撕播操攻敏教施旁旋星晕普景智最木未本杂权材束构枚果某染查栈校格框械检概残殴比毫气永汉沉泛波洋流测浮淡添渡渲源滑滚演火灰炸焰燃爆爪父物特狗猛猜率王环甩画畜略疯痛盒相盾省眩眼睁瞄瞎瞬矢知石破硬碎磁祖禁离积称程空竞章端第筑答算篇粒精系素索累红级纯纹终经绑结绕绘络统继绪缓缘缝缴缺群老联肉肩背胜胸脚脱脸臂舍航艘节花获营蓝蔓蛐蛛蜘街被西观规览解计讧议许证译话该请读谁负质购贴费资赏赛超越足跑距路踹蹬躯躲躺载较辈输边达迅运近远迟述退送适逃逐途通造逼配野金针钻链锚镜门闪阈阵附陨险陷隔难集零雷非靠靴靶音项顺频额风飘驱骑鬼鼠龙"

    "份册堵废梯"

    "五俩厚很排短粗细荐"

    "击杀敌军战舰注原联机自毁保护已移除它会导致游戏启动闪退正版号请自己意别把修改器带进间补官方结算默认只炸船才会播胜利音推进剧情"

    "我自己变成这个兵种把本就地换上面选想皇帝排除垃圾器战舰航空闲悠趴竖姿导骨架立恢复"
    "身体动作游戏自带取消挥手坐下朝天由了立刻此类完全义势要节点名看志"

    "骼兜播放选中停止重新取列表原生保持秒刷舞蹈势"
    ;

static void LoadFont(void)
{
    ImGuiIO& io = ImGui::GetIO();

    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    builder.AddText(kUICN);
    for (int i = 0; i < JQR_ENEMYTYPE_COUNT; i++) builder.AddText(g_typeNamesCN[i]);

    {
        static const unsigned int kUISym[] = {
            0x2605,   0x2606,   0x25CF,   0x25CB,
            0x25A0,   0x25B2,   0x25BC,   0x25C6,
            0x2190,   0x2191,   0x2192,   0x2193,
            0x21D0,   0x21D1,   0x21D2,   0x21D3,
            0x2665,   0x2666,   0x2660,   0x2663,
            0x221A,   0x00D7,   0x2261,
            0x515C,
        };
        for (int i = 0; i < (int)(sizeof(kUISym) / sizeof(kUISym[0])); i++)
            builder.AddChar((ImWchar)kUISym[i]);
    }
    builder.BuildRanges(&g_glyphRanges);

    io.Fonts->TexDesiredWidth = 2048;

    const char* cands[] = {
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyhbd.ttc",
        "C:\\Windows\\Fonts\\simsun.ttc",
    };
    for (int i = 0; i < 4; i++) {
        if (GetFileAttributesA(cands[i]) == INVALID_FILE_ATTRIBUTES) continue;
        ImFontConfig cfg;
        cfg.FontNo = 0;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        cfg.PixelSnapH = true;
        ImFont* f = io.Fonts->AddFontFromFileTTF(cands[i], 17.0f, &cfg, g_glyphRanges.Data);
        if (!f) {
            Log("字体打不开或格式不对, 试下一个: %s", cands[i]);
            continue;
        }

        io.FontDefault = f;
        io.Fonts->Build();

        int ok = 0;
        if (f->FindGlyphNoFallback(0x4E2D)) ok++;
        if (f->FindGlyphNoFallback(0x53EC)) ok++;
        if (f->FindGlyphNoFallback(0x5524)) ok++;

        if (ok > 0) {
            Log("字体已加载: %s  (字形区间 %d 段, 中文校验 %d/3)", cands[i],
                g_glyphRanges.Size - 1, ok);
            return;
        }
        Log("  %s 里查不到中文字形, 试下一个", cands[i]);
    }
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    Log("!! 中文字体全失败, 用默认字体 (中文会显示成方块)");
}

static void SetupStyle(void)
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.FrameRounding  = 5.0f;
    s.GrabRounding   = 5.0f;
    s.TabRounding    = 5.0f;
    s.ScrollbarRounding = 6.0f;
    s.WindowPadding  = ImVec2(14, 12);
    s.FramePadding   = ImVec2(10, 6);
    s.ItemSpacing    = ImVec2(10, 8);
    s.WindowBorderSize = 1.0f;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.086f, 0.090f, 0.110f, 0.96f);
    c[ImGuiCol_ChildBg]         = ImVec4(0.120f, 0.126f, 0.150f, 1.00f);
    c[ImGuiCol_Border]          = ImVec4(0.220f, 0.230f, 0.270f, 0.80f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.160f, 0.168f, 0.196f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.220f, 0.232f, 0.280f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.260f, 0.276f, 0.330f, 1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.200f, 0.208f, 0.244f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.300f, 0.352f, 0.560f, 1.00f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.352f, 0.450f, 0.760f, 1.00f);
    c[ImGuiCol_Header]          = ImVec4(0.240f, 0.280f, 0.450f, 1.00f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.300f, 0.360f, 0.600f, 1.00f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.352f, 0.420f, 0.700f, 1.00f);
    c[ImGuiCol_Tab]             = ImVec4(0.160f, 0.168f, 0.196f, 1.00f);
    c[ImGuiCol_TabHovered]      = ImVec4(0.300f, 0.360f, 0.600f, 1.00f);
    c[ImGuiCol_TabActive]       = ImVec4(0.264f, 0.320f, 0.550f, 1.00f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.450f, 0.650f, 1.000f, 1.00f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.450f, 0.600f, 0.950f, 1.00f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.550f, 0.700f, 1.000f, 1.00f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0.100f, 0.104f, 0.126f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.260f, 0.276f, 0.330f, 1.00f);
    c[ImGuiCol_Text]            = ImVec4(0.920f, 0.930f, 0.960f, 1.00f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.520f, 0.540f, 0.590f, 1.00f);
    c[ImGuiCol_Separator]       = ImVec4(0.280f, 0.290f, 0.350f, 1.00f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.110f, 0.116f, 0.136f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.170f, 0.200f, 0.320f, 1.00f);
    c[ImGuiCol_PopupBg]         = ImVec4(0.110f, 0.116f, 0.136f, 0.98f);
}

static HRESULT WINAPI hkPresentBody(IDXGISwapChain* sc, UINT sync, UINT flags)
{

    if (InterlockedCompareExchange(&g_renderPaused, 0, 0)) {

        RequestTick();
        return oPresent(sc, sync, flags);
    }
    if (InterlockedCompareExchange(&g_imguiState, 0, 0) == 0) {

        if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice)) && g_pd3dDevice) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
            g_pSwapChain = sc;
            DXGI_SWAP_CHAIN_DESC sd;
            if (SUCCEEDED(sc->GetDesc(&sd))) {
                g_menuHwnd = sd.OutputWindow;
                g_lastW = sd.BufferDesc.Width;
                g_lastH = sd.BufferDesc.Height;
            }
            CreateRTV();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            LoadFont();
            SetupStyle();
            ImGui_ImplWin32_Init(g_menuHwnd);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
            InterlockedExchange(&g_imguiState, 1);
            Log("ImGui 初始化完成  hwnd=%p  %ux%u", (void*)g_menuHwnd, g_lastW, g_lastH);
        } else {
            InterlockedExchange(&g_imguiState, 2);
            Log("!! ImGui 初始化失败: GetDevice 失败");
        }
    }

    if (InterlockedCompareExchange(&g_imguiState, 0, 0) == 1) {

        if (sc != g_pSwapChain) {
            ID3D11Device* nd = NULL;
            if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&nd)) && nd) {
                g_pSwapChain = sc;
                if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = NULL; }
                DXGI_SWAP_CHAIN_DESC sd2;
                if (SUCCEEDED(sc->GetDesc(&sd2))) {
                    g_lastW = sd2.BufferDesc.Width;
                    g_lastH = sd2.BufferDesc.Height;
                    g_menuHwnd = sd2.OutputWindow;
                }
                CreateRTV();
                if (nd != g_pd3dDevice) {

                    ImGui_ImplDX11_Shutdown();
                    if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = NULL; }
                    if (g_pd3dDevice)  { g_pd3dDevice->Release();  g_pd3dDevice  = NULL; }
                    g_pd3dDevice = nd;
                    g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
                    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
                    Log("!! swapchain+device 都变了, ImGui D3D 后端已重建");
                } else {
                    nd->Release();
                    Log("!! swapchain 已重建: RTV 重挂成功 (%ux%u)", g_lastW, g_lastH);
                }
            }
        }

        DXGI_SWAP_CHAIN_DESC sd;
        if (SUCCEEDED(sc->GetDesc(&sd))) {
            if (sd.BufferDesc.Width != g_lastW || sd.BufferDesc.Height != g_lastH) {
                g_lastW = sd.BufferDesc.Width;
                g_lastH = sd.BufferDesc.Height;
                CreateRTV();
            }
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawMenu();
        ImGui::Render();

        __try {
            if (g_mainRTV) g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, NULL);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("!! SEH in Present 绘制 code=%08X (丢这一帧, 下帧重建 RTV)", GetExceptionCode());
            if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = NULL; }
            g_pSwapChain = NULL;
        }
    }
    RequestTick();
    return oPresent(sc, sync, flags);
}

static HRESULT WINAPI hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags)
{
    static __declspec(thread) int t_inPresent = 0;
    if (t_inPresent) return oPresent(sc, sync, flags);
    t_inPresent = 1;
    HRESULT hr = hkPresentBody(sc, sync, flags);
    t_inPresent = 0;
    return hr;
}

static BOOL InstallPresentHook(void)
{

    if (oPresent) { Log("Present hook 已存在, 跳过重复安装"); return TRUE; }
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.lpszClassName = L"JQRModDummyWnd";
    RegisterClassExW(&wc);
    HWND hDummy = CreateWindowExW(0, L"JQRModDummyWnd", L"", WS_OVERLAPPEDWINDOW,
                                  0, 0, 120, 80, NULL, NULL, wc.hInstance, NULL);
    if (!hDummy) { Log("!! 创建 dummy 窗口失败, err=%lu", GetLastError()); return FALSE; }

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width  = 2;
    sd.BufferDesc.Height = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hDummy;
    sd.SampleDesc.Count = 1;
    sd.Windowed     = TRUE;
    sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* dev = NULL;
    ID3D11DeviceContext* ctx = NULL;
    IDXGISwapChain* sc = NULL;
    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL want[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
                    want, 3, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
    if (FAILED(hr) || !sc) {
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0,
                    want, 3, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
    }
    if (FAILED(hr) || !sc) {
        Log("!! D3D11 创建失败 hr=%08X (无法装 Present hook)", hr);
        DestroyWindow(hDummy);
        UnregisterClassW(L"JQRModDummyWnd", wc.hInstance);
        return FALSE;
    }

    void** vt = *(void***)sc;
    oPresent = (tPresent)vt[8];
    DWORD old = 0;
    if (VirtualProtect(&vt[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
        InterlockedExchangePointer((PVOID*)&vt[8], (PVOID)hkPresent);
        VirtualProtect(&vt[8], sizeof(void*), old, &old);
        Log("Present hook 已安装: 原函数=%p 新函数=%p", (void*)oPresent, (void*)hkPresent);
    } else {
        Log("!! VirtualProtect 失败, 无法 hook");
    }

    sc->Release();
    if (dev) dev->Release();
    if (ctx) ctx->Release();
    DestroyWindow(hDummy);
    UnregisterClassW(L"JQRModDummyWnd", wc.hInstance);
    return TRUE;
}

#define WM_JQR_SUMMON  (WM_APP + 201)
#define WM_JQR_SELFTEST (WM_APP + 202)
#define WM_JQR_ACTION  (WM_APP + 210)
#define WM_JQR_HOTFUNC (WM_APP + 213)

static BOOL IsInputMsg(UINT m)
{
    switch (m) {
    case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
    case WM_INPUT: case WM_SETCURSOR:
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_CHAR:
        return TRUE;
    }
    return FALSE;
}

typedef struct { int vk; int type; int ally; const char* label; } HotKey;
static const HotKey g_hotkeys[] = {
    { VK_F1,   1, 1, "盟友 剑士 I" },
    { VK_F2,   1, 0, "敌人 剑士 I" },
    { VK_F3,  16, 1, "盟友 锤兵 I" },
    { VK_F4,   8, 1, "盟友 蜘蛛龙5000" },
    { VK_F5,   2, 1, "盟友 弓箭手 I" },
    { VK_F6,  29, 1, "盟友 装甲机器人" },
    { VK_F7,   3, 1, "盟友 召唤行者" },
    { VK_F8,  88, 1, "盟友 锯片独轮车" },
    { VK_F9,  89, 1, "盟友 可骑射弹爬虫" },
    { VK_F10, 21, 1, "盟友 剑士 IV" },
};
#define NHK (sizeof(g_hotkeys)/sizeof(g_hotkeys[0]))

typedef struct {
    const char* name;
    int defVk;
    int kind;
    int a, b;
} HotFunc;

static const HotFunc g_hf[] = {

    { "召唤·盟友 剑士 I",       VK_F1,  0,  1, 1 },
    { "召唤·敌人 剑士 I",       VK_F2,  0,  1, 0 },
    { "召唤·盟友 锤兵 I",       VK_F3,  0, 16, 1 },
    { "召唤·盟友 蜘蛛龙5000",   VK_F4,  0,  8, 1 },
    { "召唤·盟友 弓箭手 I",     VK_F5,  0,  2, 1 },
    { "召唤·盟友 装甲机器人",   VK_F6,  0, 29, 1 },
    { "召唤·盟友 召唤行者",     VK_F7,  0,  3, 1 },
    { "召唤·盟友 锯片独轮车",   VK_F8,  0, 88, 1 },
    { "召唤·盟友 可骑射弹爬虫", VK_F9,  0, 89, 1 },
    { "召唤·盟友 剑士 IV",      VK_F10, 0, 21, 1 },

    { "一击必杀 开",      0, 1, ACT_ONESHOT_ON,   0 },
    { "一击必杀 关",      0, 1, ACT_ONESHOT_OFF,  0 },
    { "玩家无敌 开",      0, 1, ACT_IMMUNE_ON,    0 },
    { "玩家无敌 关",      0, 1, ACT_IMMUNE_OFF,   0 },
    { "无限能量 开",      0, 1, ACT_ENERGY_ON,    0 },
    { "无限能量 关",      0, 1, ACT_ENERGY_OFF,   0 },
    { "飞天 开",          0, 1, ACT_FLY_ON,       0 },
    { "飞天 关",          0, 1, ACT_FLY_OFF,      0 },
    { "穿墙 开",          0, 1, ACT_WALL_ON,      0 },
    { "穿墙 关",          0, 1, ACT_WALL_OFF,     0 },
    { "AI停电 开",        0, 1, ACT_AIOFF_ON,     0 },
    { "AI停电 关",        0, 1, ACT_AIOFF_OFF,    0 },
    { "慢动作 开",        0, 1, ACT_SLOWMO_ON,    0 },
    { "慢动作 关",        0, 1, ACT_SLOWMO_OFF,   0 },
    { "AI 无视我 开",     0, 1, ACT_NEUTRAL_ON,   0 },
    { "AI 无视我 关",     0, 1, ACT_NEUTRAL_OFF,  0 },
    { "磁力靴 开",        0, 1, ACT_MAGBOOT_ON,   0 },
    { "磁力靴 关",        0, 1, ACT_MAGBOOT_OFF,  0 },
    { "喷气背包 开",      0, 1, ACT_JETPACK_ON,   0 },
    { "喷气背包 关",      0, 1, ACT_JETPACK_OFF,  0 },
    { "自动瞄准 开",      0, 1, ACT_AIM_ON,       0 },
    { "自动瞄准 关",      0, 1, ACT_AIM_OFF,      0 },
    { "自动攻击 开",      0, 1, ACT_AUTOATK_ON,   0 },
    { "自动攻击 关",      0, 1, ACT_AUTOATK_OFF,  0 },
    { "超级踢 开",        0, 1, ACT_EP_SUPERKICK_ON,  0 },
    { "超级踢 关",        0, 1, ACT_EP_SUPERKICK_OFF, 0 },
    { "永不摔倒 开",      0, 1, ACT_EP_NOFALL_ON,  0 },
    { "永不摔倒 关",      0, 1, ACT_EP_NOFALL_OFF, 0 },
    { "敌人反重力 开",    0, 1, ACT_EP_LOWGRAV_ON,  0 },
    { "敌人反重力 关",    0, 1, ACT_EP_LOWGRAV_OFF, 0 },
    { "全场乱斗 开",      0, 1, ACT_RUMBLE_ON,     0 },
    { "全场乱斗 关",      0, 1, ACT_RUMBLE_OFF,    0 },
    { "红蓝分队对抗",     0, 1, ACT_RB_TEAMS,      0 },
    { "全员恢复为敌",     0, 1, ACT_RB_TEAMOFF,    0 },
    { "单挑:VS 最近的敌人", 0, 1, ACT_RB_DUEL_NEAR, 0 },
    { "单挑:VS 我",       0, 1, ACT_RB_DUEL_ME,    0 },
    { "结束单挑",         0, 1, ACT_RB_DUEL_STOP,  0 },

    { "全场无敌 开",      0, 1, ACT_GOD_ALL_ON,   0 },
    { "全场无敌 关",      0, 1, ACT_GOD_ALL_OFF,  0 },
    { "镜头俯仰 开",      0, 1, ACT_TILT_ON,      0 },
    { "镜头俯仰 关",      0, 1, ACT_TILT_OFF,     0 },
    { "敌人变瞎",         0, 1, ACT_FOE_BLIND,    0 },
    { "敌人疯狗",         0, 1, ACT_FOE_FRENZY,   0 },
    { "押红队",           0, 1, ACT_RB_BET_RED,   0 },
    { "押蓝队",           0, 1, ACT_RB_BET_BLUE,  0 },
    { "取消押注",         0, 1, ACT_RB_BET_OFF,   0 },
    { "环形布阵",         0, 1, ACT_RB_FORMATION, 120 },
    { "比赛准备",         0, 1, ACT_RB_PREP,      1 },
    { "随机武器大乱斗",   0, 1, ACT_RB_RANDWEAPON, 0 },
    { "全场点燃",         0, 1, ACT_BURN_ALL,     0 },
    { "全场眩晕 3秒",     0, 1, ACT_STUN_ALL,     30 },
    { "敌人不计胜利 开",  0, 1, ACT_NOCOUNT_ON,    0 },
    { "敌人不计胜利 关",  0, 1, ACT_NOCOUNT_OFF,   0 },

    { "秒杀全场敌人",       0, 1, ACT_KILL_ALL,        0 },
    { "夺舍最近的敌人",     0, 1, ACT_POSSESS,         0 },
    { "全场缴械",           0, 1, ACT_DISARM,          0 },
    { "全场炸成零件",       0, 1, ACT_EP_EXPLODE_ALL,  0 },
    { "全场扔上天",         0, 1, ACT_EP_FLING_ALL,    0 },
    { "全场集体扑街",       0, 1, ACT_EP_FALL_ALL,     0 },
    { "全场眩晕 3 秒",      0, 1, ACT_EP_STUN_ALL,     3 },
    { "全场碎甲 EMP",       0, 1, ACT_EMP,             0 },
    { "闪现·贴敌人背后",    0, 1, ACT_BLINK,           0 },
    { "超级跳",             0, 1, ACT_SUPERJUMP,       0 },
    { "敌人全拽到面前",     0, 1, ACT_TELEPORT_ALL,    0 },
    { "骑上最近的坐骑",     0, 1, ACT_RIDE,            0 },
    { "下车",               0, 1, ACT_DISMOUNT,        0 },
    { "凭空拉弓",           0, 1, ACT_EP_BOW,          0 },
    { "多送一条命",         0, 1, ACT_CLONE_PLUS,      0 },
    { "游戏速度 50%",       0, 1, ACT_TS,             50 },
    { "游戏速度 100%",      0, 1, ACT_TS,            100 },
    { "游戏速度 200%",      0, 1, ACT_TS,            200 },
    { "游戏速度 400%",      0, 1, ACT_TS,            400 },

    { "全场招安 (敌人变你的兵)", 0, 1, ACT_RECRUIT_ALL,     0 },
    { "全体变小 50%",       0, 1, ACT_SCALE_ALL,       50 },
    { "全体变大 150%",      0, 1, ACT_SCALE_ALL,      150 },
    { "体型还原 100%",      0, 1, ACT_SCALE_ALL,      100 },
    { "全场换武器→剑",      0, 1, ACT_FORCE_WEAPON,     1 },
    { "全场换武器→弓",      0, 1, ACT_FORCE_WEAPON,     2 },
    { "全场换武器→锤",      0, 1, ACT_FORCE_WEAPON,     3 },
    { "全场换武器→矛",      0, 1, ACT_FORCE_WEAPON,     5 },
    { "全场换武器→空手",    0, 1, ACT_FORCE_WEAPON,     0 },
    { "秒杀最近敌人",       0, 1, ACT_KILL_NEAREST,      0 },
    { "自杀重生",           0, 1, ACT_SUICIDE,           0 },
    { "技能点给满 99",      0, 1, ACT_MAXSKILL,          0 },
    { "抢光武器 (缴械+换剑)",0, 1, ACT_TAKE_ALL_WEAPON,  0 },
    { "拍照模式 开 (藏HUD)", 0, 1, ACT_DBG_HIDEUI_ON,    0 },
    { "拍照模式 关",        0, 1, ACT_DBG_HIDEUI_OFF,    0 },
    { "看身体接缝 开",      0, 1, ACT_DBG_SEAMS_ON,      0 },
    { "看身体接缝 关",      0, 1, ACT_DBG_SEAMS_OFF,     0 },
    { "关镜头平滑 开",      0, 1, ACT_DBG_NOSMOOTH_ON,   0 },
    { "关镜头平滑 关",      0, 1, ACT_DBG_NOSMOOTH_OFF,  0 },

    { "过场自由行动 开",    0, 1, ACT_CUT_FREE_ON,   0 },
    { "过场自由行动 关",    0, 1, ACT_CUT_FREE_OFF,  0 },
    { "脱离剧情相机 开",    0, 1, ACT_FREE_CAM_ON,   0 },
    { "脱离剧情相机 关",    0, 1, ACT_FREE_CAM_OFF,  0 },

    { "无差别攻击 开",      0, 1, ACT_FREEALL_ON,    0 },
    { "无差别攻击 关",      0, 1, ACT_FREEALL_OFF,   0 },

    { "激光敌人可移动 开",  0, 1, ACT_LASERMOVE_ON,  0 },
    { "激光敌人可移动 关",  0, 1, ACT_LASERMOVE_OFF, 0 },
    { "武器加长 开",        0, 1, ACT_WLEN_ON,       0 },
    { "武器加长 关",        0, 1, ACT_WLEN_OFF,      0 },

    { "崩溃弹窗杀手 开",    0, 1, ACT_CRASHKILL_ON,  0 },
    { "崩溃弹窗杀手 关",    0, 1, ACT_CRASHKILL_OFF, 0 },

    { "崩溃无视 开",        0, 1, ACT_CRASHCLEAN_ON, 0 },
    { "崩溃无视 关",        0, 1, ACT_CRASHCLEAN_OFF,0 },

    { "克隆体收编 开",      0, 1, ACT_CLONEALLY_ON,  0 },
    { "克隆体收编 关",      0, 1, ACT_CLONEALLY_OFF, 0 },
    { "废除电梯拦截墙 开",  0, 1, ACT_FENCEOFF_ON,   0 },
    { "废除电梯拦截墙 关",  0, 1, ACT_FENCEOFF_OFF,  0 },

    { "弓箭放大 开",        0, 1, ACT_ARROWBIG_ON,   0 },
    { "弓箭放大 关",        0, 1, ACT_ARROWBIG_OFF,  0 },

    { "击杀后补结算 开",    0, 1, ACT_SHIPSETTLE_ON,   0 },
    { "击杀后补结算 关",    0, 1, ACT_SHIPSETTLE_OFF,  0 },

    { "斗兽场意识转移 开",  0, 1, ACT_CTFORCE_ON,  0 },
    { "斗兽场意识转移 关",  0, 1, ACT_CTFORCE_OFF, 0 },
    { "立即意识转移",       0, 1, ACT_CT_NOW,      0 },
    { "AI变聪明 开",        0, 1, ACT_AISMART_ON,  0 },
    { "AI变聪明 关",        0, 1, ACT_AISMART_OFF, 0 },
};
#define NHF ((int)(sizeof(g_hf)/sizeof(g_hf[0])))

#define HFC_MAX 32
static HotFunc       g_hfCustom[HFC_MAX];
static int           g_hfCBind[HFC_MAX];
static char          g_hfCName[HFC_MAX][64];
static int           g_hfCustomN = 0;

static int  g_hfBind[NHF];
static volatile LONG g_hfCapture = -1;
static int           g_hfSwallow = 0;
static volatile LONG g_hfVer     = 0;

static int HfTotal(void) { return NHF + g_hfCustomN; }
static const HotFunc* HfAt(int i)
{
    if (i >= 0 && i < NHF) return &g_hf[i];
    if (i >= NHF && i < NHF + g_hfCustomN) return &g_hfCustom[i - NHF];
    return NULL;
}
static int* HfBindRef(int i)
{
    if (i >= 0 && i < NHF) return &g_hfBind[i];
    if (i >= NHF && i < NHF + g_hfCustomN) return &g_hfCBind[i - NHF];
    return NULL;
}
static int HfBindVal(int i) { int* p = HfBindRef(i); return p ? *p : 0; }
static void HfBindSet(int i, int vk) { int* p = HfBindRef(i); if (p) *p = vk; }

static const int g_hfPair[][2] = {
    { ACT_ONESHOT_ON,      ACT_ONESHOT_OFF      },
    { ACT_IMMUNE_ON,       ACT_IMMUNE_OFF       },
    { ACT_ENERGY_ON,       ACT_ENERGY_OFF       },
    { ACT_FLY_ON,          ACT_FLY_OFF          },
    { ACT_WALL_ON,         ACT_WALL_OFF         },
    { ACT_AIOFF_ON,        ACT_AIOFF_OFF        },
    { ACT_AISMART_ON,      ACT_AISMART_OFF      },
    { ACT_SLOWMO_ON,       ACT_SLOWMO_OFF       },
    { ACT_NEUTRAL_ON,      ACT_NEUTRAL_OFF      },
    { ACT_MAGBOOT_ON,      ACT_MAGBOOT_OFF      },
    { ACT_JETPACK_ON,      ACT_JETPACK_OFF      },
    { ACT_AIM_ON,          ACT_AIM_OFF          },
    { ACT_AUTOATK_ON,      ACT_AUTOATK_OFF      },
    { ACT_EP_SUPERKICK_ON, ACT_EP_SUPERKICK_OFF },
    { ACT_EP_NOFALL_ON,    ACT_EP_NOFALL_OFF    },
    { ACT_EP_LOWGRAV_ON,   ACT_EP_LOWGRAV_OFF   },
    { ACT_RUMBLE_ON,       ACT_RUMBLE_OFF       },
    { ACT_NOCOUNT_ON,      ACT_NOCOUNT_OFF      },

    { ACT_DBG_HIDEUI_ON,   ACT_DBG_HIDEUI_OFF   },
    { ACT_DBG_SEAMS_ON,    ACT_DBG_SEAMS_OFF    },
    { ACT_DBG_NOSMOOTH_ON, ACT_DBG_NOSMOOTH_OFF },

    { ACT_TILT_ON,         ACT_TILT_OFF         },
    { ACT_GOD_ALL_ON,      ACT_GOD_ALL_OFF      },

    { ACT_CUT_FREE_ON,     ACT_CUT_FREE_OFF     },
    { ACT_FREE_CAM_ON,     ACT_FREE_CAM_OFF     },

    { ACT_FREEALL_ON,      ACT_FREEALL_OFF      },

    { ACT_LASERMOVE_ON,    ACT_LASERMOVE_OFF    },
    { ACT_WLEN_ON,         ACT_WLEN_OFF         },

    { ACT_CRASHKILL_ON,    ACT_CRASHKILL_OFF    },

    { ACT_CRASHCLEAN_ON,   ACT_CRASHCLEAN_OFF   },
};
#define NHFPAIR ((int)(sizeof(g_hfPair)/sizeof(g_hfPair[0])))

static int HfPairNo(int i, int* isOn)
{
    if (isOn) *isOn = 0;
    const HotFunc* h = HfAt(i);
    if (!h || h->kind != 1) return -1;
    int id = h->a;
    for (int k = 0; k < NHFPAIR; k++) {
        if (g_hfPair[k][0] == id) { if (isOn) *isOn = 1; return k; }
        if (g_hfPair[k][1] == id) { if (isOn) *isOn = 0; return k; }
    }
    return -1;
}

static int HfPartnerIdx(int i)
{
    int isOn = 0;
    int k = HfPairNo(i, &isOn);
    if (k < 0) return -1;
    int other = isOn ? g_hfPair[k][1] : g_hfPair[k][0];
    for (int j = 0; j < NHF; j++)
        if (g_hf[j].kind == 1 && g_hf[j].a == other) return j;
    return -1;
}

static int           g_hfFlip[NHF];

static const char* VkName(int vk, char* buf, int cap)
{
    if (vk <= 0) { _snprintf_s(buf, cap, _TRUNCATE, "—"); return buf; }
    switch (vk) {
    case VK_SPACE:  _snprintf_s(buf, cap, _TRUNCATE, "空格");  return buf;
    case VK_TAB:    _snprintf_s(buf, cap, _TRUNCATE, "Tab");   return buf;
    case VK_RETURN: _snprintf_s(buf, cap, _TRUNCATE, "回车");  return buf;
    case VK_ESCAPE: _snprintf_s(buf, cap, _TRUNCATE, "Esc");   return buf;
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
                    _snprintf_s(buf, cap, _TRUNCATE, "Shift"); return buf;
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
                    _snprintf_s(buf, cap, _TRUNCATE, "Ctrl");  return buf;
    case VK_MENU: case VK_LMENU: case VK_RMENU:
                    _snprintf_s(buf, cap, _TRUNCATE, "Alt");   return buf;
    case VK_INSERT: _snprintf_s(buf, cap, _TRUNCATE, "Insert"); return buf;
    case VK_DELETE: _snprintf_s(buf, cap, _TRUNCATE, "Delete"); return buf;
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        _snprintf_s(buf, cap, _TRUNCATE, "F%d", vk - VK_F1 + 1);
        return buf;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        _snprintf_s(buf, cap, _TRUNCATE, "小键盘 %c", (char)('0' + vk - VK_NUMPAD0));
        return buf;
    }
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        _snprintf_s(buf, cap, _TRUNCATE, "%c", (char)vk);
        return buf;
    }
    {
        UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
        if (sc && GetKeyNameTextA((LONG)(sc << 16), buf, cap) > 0) return buf;
    }
    _snprintf_s(buf, cap, _TRUNCATE, "键%02X", vk);
    return buf;
}

static int HfCount(void) { return HfTotal(); }
static const char* HfName(int i) { const HotFunc* h = HfAt(i); return h ? h->name : "?"; }
static int HfBindOf(int i) { return HfBindVal(i); }
static BOOL HfIsCustom(int i) { return (i >= NHF && i < NHF + g_hfCustomN) ? TRUE : FALSE; }
static int HfCustomTypeId(int i) { const HotFunc* h = HfAt(i); return h ? h->a : -1; }
static int HfCustomAlly(int i)   { const HotFunc* h = HfAt(i); return h ? h->b : 0; }

static const char* HfCustomTypeName(int i)
{
    const HotFunc* h = HfAt(i);
    if (!h) return "?";
    for (int k = 0; k < JQR_ENEMYTYPE_COUNT; k++)
        if (g_typeIds[k] == h->a) return g_typeNamesCN[k];
    return h->name;
}
static const char* HfKeyName(int i, char* buf, int cap)
{
    return VkName(HfBindVal(i), buf, cap);
}
static int HfCapturing(void)
{
    return (int)InterlockedCompareExchange(&g_hfCapture, -1, -1);
}
static void HfBeginCapture(int i)
{
    if (i >= 0 && i < HfTotal()) InterlockedExchange(&g_hfCapture, i);
}

static void HfSave(void);
static void HfLoad(void);
static int  HfCustomCount(void) { return g_hfCustomN; }

static int HfCustomAddEx(int typeId, int ally, int autoCapture)
{
    if (g_hfCustomN >= HFC_MAX) { Log("!! 自定义召唤项已达上限 %d", HFC_MAX); return -1; }
    int al = ally ? 1 : 0;

    for (int i = 0; i < g_hfCustomN; i++) {
        if (g_hfCustom[i].a == typeId && g_hfCustom[i].b == al) {
            Log("召唤快捷: [id%d %s] 已在列表中 (索引 %d), 不重复添加",
                typeId, al ? "盟友" : "敌人", NHF + i);
            if (autoCapture) HfBeginCapture(NHF + i);
            return NHF + i;
        }
    }
    int k = g_hfCustomN;
    const char* cn = "未知";
    for (int i = 0; i < JQR_ENEMYTYPE_COUNT; i++)
        if (g_typeIds[i] == typeId) { cn = g_typeNamesCN[i]; break; }
    _snprintf_s(g_hfCName[k], sizeof(g_hfCName[k]), _TRUNCATE,
                "召唤·%s %s [id%d]", ally ? "盟友" : "敌人", cn, typeId);
    g_hfCustom[k].name = g_hfCName[k];
    g_hfCustom[k].defVk = 0;
    g_hfCustom[k].kind = 0;
    g_hfCustom[k].a = typeId;
    g_hfCustom[k].b = ally ? 1 : 0;
    g_hfCBind[k] = 0;
    g_hfCustomN++;
    HfSave();
    InterlockedIncrement(&g_hfVer);
    Log("召唤快捷: 已添加 [%s] (索引 %d)", g_hfCName[k], NHF + k);

    if (autoCapture) HfBeginCapture(NHF + k);
    return NHF + k;
}

static int HfCustomAdd(int typeId, int ally) { return HfCustomAddEx(typeId, ally, 1); }

static void HfCustomDel(int idx)
{
    if (!HfIsCustom(idx)) return;
    int k = idx - NHF;

    for (int i = k; i < g_hfCustomN - 1; i++) {
        g_hfCustom[i] = g_hfCustom[i + 1];
        memcpy(g_hfCName[i], g_hfCName[i + 1], sizeof(g_hfCName[i]));
        g_hfCustom[i].name = g_hfCName[i];
        g_hfCBind[i] = g_hfCBind[i + 1];
    }
    g_hfCustomN--;
    if (HfCapturing() >= g_hfCustomN + NHF) InterlockedExchange(&g_hfCapture, -1);
    HfSave();
    InterlockedIncrement(&g_hfVer);
    Log("召唤快捷: 已删除 1 项 (剩 %d 项)", g_hfCustomN);
}

#define HFC_FILE L"jqrmod_hotkeys.txt"

static void HfPath(wchar_t* out, int cap)
{
    out[0] = 0;

    HMODULE hm = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&HfPath, &hm) && hm)
    {
        wchar_t p[MAX_PATH] = {0};
        if (GetModuleFileNameW(hm, p, MAX_PATH))
        {

            for (int i = (int)wcslen(p) - 1; i >= 0; i--) {
                if (p[i] == L'\\' || p[i] == L'/') { p[i + 1] = 0; break; }
            }
            _snwprintf_s(out, cap, _TRUNCATE, L"%s%s", p, HFC_FILE);
            return;
        }
    }

    wchar_t t[MAX_PATH] = {0};
    GetTempPathW(MAX_PATH, t);
    _snwprintf_s(out, cap, _TRUNCATE, L"%s%s", t, HFC_FILE);
}

static void HfSave(void)
{
    wchar_t path[MAX_PATH];
    HfPath(path, MAX_PATH);

    FILE* f = NULL;
    if (_wfopen_s(&f, path, L"wb") != 0 || !f) return;
    fputs("# jqrmod custom summon hotkeys (fmt: vk<TAB>typeId<TAB>ally<TAB>name, UTF-8)\n", f);
    fputs("# vk=0 means unbound; delete a line to remove the item\n", f);
    for (int i = 0; i < g_hfCustomN; i++) {
        char buf[128];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%d\t%d\t%d\t",
                    g_hfCBind[i], g_hfCustom[i].a, g_hfCustom[i].b);
        fputs(buf, f);
        fputs(g_hfCName[i], f);
        fputs("\n", f);
    }
    fclose(f);
}

static void HfLoad(void)
{
    wchar_t path[MAX_PATH];
    HfPath(path, MAX_PATH);

    FILE* f = NULL;
    if (_wfopen_s(&f, path, L"rb") != 0 || !f) return;

    g_hfCustomN = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        char* p = line;
        long vals[3] = { 0, 0, 0 };
        int  got = 0;
        for (int fld = 0; fld < 3; fld++) {
            while (*p == ' ' || *p == '\t') p++;
            if (!(*p == '-' || *p == '+' || (*p >= '0' && *p <= '9'))) break;
            char* end = NULL;
            long v = strtol(p, &end, 10);
            if (end == p) break;
            vals[fld] = v;
            got++;
            p = end;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\t') p++;
        }
        if (got < 3) continue;
        while (*p == '\t' || *p == ' ') p++;

        char nm[64] = "";
        if (*p) {
            int n = 0;
            while (p[n] && p[n] != '\r' && p[n] != '\n' && n < 63) { nm[n] = p[n]; n++; }
            nm[n] = 0;
        }
        int vk  = (int)vals[0];
        int tid = (int)vals[1];
        int ally = (int)vals[2];
        if (g_hfCustomN >= HFC_MAX) break;
        int k = g_hfCustomN;
        if (nm[0])
            _snprintf_s(g_hfCName[k], sizeof(g_hfCName[k]), _TRUNCATE, "%s", nm);
        else
            _snprintf_s(g_hfCName[k], sizeof(g_hfCName[k]), _TRUNCATE, "召唤 id%d", tid);
        g_hfCustom[k].name  = g_hfCName[k];
        g_hfCustom[k].defVk = 0;
        g_hfCustom[k].kind  = 0;
        g_hfCustom[k].a     = tid;
        g_hfCustom[k].b     = ally ? 1 : 0;
        g_hfCBind[k]        = (vk == VK_INSERT) ? 0 : vk;
        g_hfCustomN++;
    }
    fclose(f);
    Log("召唤快捷: 读入 %d 个自定义项", g_hfCustomN);
}

static void HfSetBind(int idx, int vk)
{
    char kb[64];
    if (idx < 0 || idx >= HfTotal()) return;
    if (vk == VK_INSERT) { Log("快捷键: Insert 是菜单键, 不能占用"); return; }

    if (vk > 0) {
        for (int i = 0; i < HfTotal(); i++) {
            if (i == idx || HfBindVal(i) != vk) continue;
            if (HfPartnerIdx(idx) == i) {
                g_hfFlip[idx] = 0;
                continue;
            }
            HfBindSet(i, 0);
            VkName(vk, kb, sizeof(kb));
            Log("快捷键: [%s] 原本占着 %s, 已被踢出", HfAt(i)->name, kb);
        }
    }
    HfBindSet(idx, vk);
    VkName(vk, kb, sizeof(kb));
    Log("快捷键: [%s] <- %s%s", HfAt(idx)->name, kb, (vk > 0) ? "" : " (清空)");
    HfSave();
    InterlockedIncrement(&g_hfVer);
}

static void HfResetAll(void)
{
    for (int i = 0; i < NHF; i++) g_hfBind[i] = g_hf[i].defVk;
    for (int i = 0; i < g_hfCustomN; i++) g_hfCBind[i] = 0;
    InterlockedExchange(&g_hfCapture, -1);
    InterlockedIncrement(&g_hfVer);
    HfSave();
    Log("快捷键: 已还原出厂 (F1~F10 召唤回来了; 自定义召唤项保留, 绑定清空)");
}

static void HfClearAll(void)
{
    for (int i = 0; i < NHF; i++) g_hfBind[i] = 0;
    for (int i = 0; i < g_hfCustomN; i++) g_hfCBind[i] = 0;
    InterlockedExchange(&g_hfCapture, -1);
    InterlockedIncrement(&g_hfVer);
    HfSave();
    Log("快捷键: 全部绑定已清空");
}

static void DoHotFunc(int idx)
{
    const HotFunc* h = HfAt(idx);
    if (!h) return;

    int isOn = 0;
    if (HfPairNo(idx, &isOn) >= 0) {
        int p = HfPartnerIdx(idx);
        int share = (p >= 0 && HfBindVal(p) == HfBindVal(idx) && HfBindVal(idx) > 0);
        if (share) {
            if (!isOn) return;
            const HotFunc* off = &g_hf[p];
            if (g_hfFlip[idx]) {
                Log("[热键] %s", off->name);
                DoAction(off->a, off->b);
            } else {
                Log("[热键] %s", h->name);
                DoAction(h->a, h->b);
            }
            g_hfFlip[idx] = !g_hfFlip[idx];
            return;
        }
    }

    Log("[热键] %s", h->name);
    if (h->kind == 0) { ActSummon(h->a, h->b); return; }
    DoAction(h->a, h->b);
}

static LRESULT CALLBACK JqrProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{

    if (msg == WM_JQR_ACTION) {
        __try { DoAction((int)w, (int)l); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("!! SEH in DoAction id=%d code=%08X", (int)w, GetExceptionCode());
        }
        return 0;
    }
    if (msg == WM_JQR_ACTION_PTR) {

        __try { DoActionPtr((int)w, (void*)l); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("!! SEH in DoActionPtr id=%d code=%08X", (int)w, GetExceptionCode());
        }
        return 0;
    }
    if (msg == WM_JQR_HOTFUNC) {
        __try { DoHotFunc((int)w); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("!! SEH in DoHotFunc idx=%d code=%08X", (int)w, GetExceptionCode());
        }
        return 0;
    }

    {
        LONG cap = InterlockedCompareExchange(&g_hfCapture, -1, -1);
        if (cap >= 0) {
            switch (msg) {
            case WM_KEYDOWN: case WM_SYSKEYDOWN:
                {
                    int vk = (int)w;
                    if (vk == VK_ESCAPE) {
                        InterlockedExchange(&g_hfCapture, -1);
                        Log("快捷键: 绑定已取消");
                    } else if (vk == VK_DELETE || vk == VK_BACK) {
                        HfSetBind((int)cap, 0);
                        InterlockedExchange(&g_hfCapture, -1);
                    } else {
                        HfSetBind((int)cap, vk);
                        g_hfSwallow = vk;
                        InterlockedExchange(&g_hfCapture, -1);
                    }
                }
                return 0;
            case WM_KEYUP: case WM_SYSKEYUP: case WM_CHAR:
                return 0;
            }

        }
        if (g_hfSwallow && (msg == WM_KEYUP || msg == WM_SYSKEYUP) &&
            (int)w == g_hfSwallow) {
            g_hfSwallow = 0;
            return 0;
        }
    }
    if (msg == WM_JQR_TICK) {
        InterlockedExchange(&g_tickPosted, 0);
        __try { DoTick(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            LONG st = g_tickStage;
            Log("!! SEH in DoTick code=%08X (崩溃时正在跑第 %d 步: %s)",
                GetExceptionCode(), (int)st, TickStageName(st));
        }
        return 0;
    }
    if (msg == WM_JQR_SUMMON) {
        __try {
            int idx = (int)w;
            if (idx >= 0 && idx < (int)NHK) {
                Log("[热键] %s", g_hotkeys[idx].label);
                ActSummon(g_hotkeys[idx].type, g_hotkeys[idx].ally);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("!! SEH in summon, code=%08X", GetExceptionCode());
        }
        return 0;
    }
    if (msg == WM_JQR_SELFTEST) {
        __try {
            Log("--- 自检 ---");
            if (InitRefs()) {
                Vec3 v;
                if (GetPlayerPosition(&v)) Log("  玩家位置 = (%.2f, %.2f, %.2f)", v.x, v.y, v.z);
                else Log("  拿不到玩家位置");
                InterlockedExchange(&g_ready, 1);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("!! SEH in selftest, code=%08X", GetExceptionCode());
        }
        return 0;
    }

    if (ImGui::GetCurrentContext()) {
        if (msg == WM_MOUSEMOVE && g_showMenu) {

            int mx = (int)(short)LOWORD(l);
            int my = (int)(short)HIWORD(l);
            ImGui::GetIO().AddMousePosEvent((float)mx, (float)my);
        }
        LRESULT r = ImGui_ImplWin32_WndProcHandler(h, msg, w, l);
        if (r) return r;

        if (g_showMenu && IsInputMsg(msg)) {
            bool isRctrl = ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN ||
                             msg == WM_KEYUP   || msg == WM_SYSKEYUP) &&
                            (w == VK_RCONTROL || w == VK_RMENU));
            if (!isRctrl) return 0;
        }
    }

    return CallWindowProcW(g_oldProc, h, msg, w, l);
}

typedef struct { HWND best; DWORD pid; } FindWndCtx;

static BOOL CALLBACK EnumWinProc(HWND h, LPARAM lp)
{
    FindWndCtx* ctx = (FindWndCtx*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != ctx->pid || !IsWindowVisible(h) || GetWindow(h, GW_OWNER) != NULL)
        return TRUE;
    wchar_t cls[128] = {0};
    GetClassNameW(h, cls, 128);
    if (wcscmp(cls, L"UnityWndClass") == 0) { ctx->best = h; return FALSE; }
    if (!ctx->best) ctx->best = h;
    return TRUE;
}

static const char* StrIStr(const char* h, const char* n)
{
    if (!h || !n || !*n) return NULL;
    size_t nl = strlen(n);
    for (; *h; h++) {
        size_t i = 0;
        while (i < nl && h[i] &&
               tolower((unsigned char)h[i]) == tolower((unsigned char)n[i])) i++;
        if (i == nl) return h;
    }
    return NULL;
}

static void GetGameManagedPath(char* out, int outSize)
{
    wchar_t self[MAX_PATH] = {0};
    out[0] = 0;
    if (!g_hInst || !GetModuleFileNameW(g_hInst, self, MAX_PATH)) return;
    wchar_t* p = wcsrchr(self, L'\\');
    if (!p) return;
    *p = 0;
    p = wcsrchr(self, L'\\');
    if (!p) return;
    *p = 0;
    lstrcatW(self, L"\\Clone Drone in the Danger Zone_Data\\Managed\\Assembly-CSharp.dll");
    WideCharToMultiByte(CP_UTF8, 0, self, -1, out, outSize, NULL, NULL);
}

static int  g_asmCount = 0;
static int  g_asmLogLimit = 0;
static char g_asmList[6144];

static void AsmEnumCb(MonoAssembly* a, void* user)
{
    (void)user;
    MonoImage* img = a ? p_assembly_get_image(a) : NULL;
    if (!img) return;
    const char* nm = p_image_get_name ? p_image_get_name(img) : NULL;
    if (!nm) nm = "(无名)";
    g_asmCount++;
    if (g_asmCount <= g_asmLogLimit) {
        char line[200];
        _snprintf_s(line, sizeof(line), _TRUNCATE, "  asm[%03d] %s", g_asmCount, nm);
        strncat_s(g_asmList, sizeof(g_asmList), line, _TRUNCATE);
        strncat_s(g_asmList, sizeof(g_asmList), "\r\n", _TRUNCATE);
    }
    if (!g_image && StrIStr(nm, "assembly-csharp")) {
        g_image = img;
        Log("[+] 命中 Assembly-CSharp : image=%p  name=%s", (void*)img, nm);
    }
}

static DWORD WINAPI WorkerBody(LPVOID p)
{
    (void)p;

    { static volatile LONG s_workerOnce = 0;
      if (InterlockedExchange(&s_workerOnce, 1)) {
          Log("!! 初始化线程已在运行, 本线程直接退出");
          return 0;
      } }
    LogInit();

    __try {
        HANDLE hck = CreateThread(NULL, 0, CrashKillerBody, NULL, 0, NULL);
        if (hck) CloseHandle(hck);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("!! 崩溃杀手线程创建失败");
    }
    Log("==========================================");
    Log(" JQRMod v0.41.2 - Clone Drone in the Danger Zone");
    Log(" 进程 pid = %lu", GetCurrentProcessId());
    Log("==========================================");

    __try { InstallPresentHook(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("!! InstallPresentHook 异常 code=%08X", GetExceptionCode());
    }

    for (int i = 0; i < 240 && !g_hMono; i++) {
        g_hMono = GetModuleHandleW(L"mono-2.0-bdwgc.dll");
        if (!g_hMono) Sleep(500);
    }
    if (!g_hMono) { Log("!! mono 未加载, 放弃"); return 1; }
    Log("mono = %p", (void*)g_hMono);

    GETSYM(g_hMono, get_root_domain); GETSYM(g_hMono, domain_get);  GETSYM(g_hMono, thread_attach);
    GETSYM(g_hMono, assembly_foreach); GETSYM(g_hMono, assembly_get_image);
    GETSYM(g_hMono, image_get_name); GETSYM(g_hMono, class_from_name);
    GETSYM(g_hMono, class_get_method_from_name); GETSYM(g_hMono, class_get_parent);
    GETSYM(g_hMono, class_get_methods); GETSYM(g_hMono, method_signature);
    GETSYM(g_hMono, signature_get_param_count); GETSYM(g_hMono, signature_get_params);
    GETSYM(g_hMono, type_get_name); GETSYM(g_hMono, method_get_name);
    GETSYM(g_hMono, class_get_name); GETSYM(g_hMono, runtime_invoke);
    GETSYM(g_hMono, object_get_class); GETSYM(g_hMono, object_unbox);
    GETSYM(g_hMono, class_get_type); GETSYM(g_hMono, type_get_object);
    GETSYM(g_hMono, class_get_image); GETSYM(g_hMono, string_new);
    GETSYM(g_hMono, free);
    GETSYM(g_hMono, domain_assembly_open); GETSYM(g_hMono, image_loaded);
    GETSYM(g_hMono, image_get_filename);
    GETSYM(g_hMono, class_get_field_from_name); GETSYM(g_hMono, field_set_value);
    GETSYM(g_hMono, field_get_value);
    GETSYM(g_hMono, image_get_table_info); GETSYM(g_hMono, table_info_get_rows);
    GETSYM(g_hMono, class_get); GETSYM(g_hMono, class_get_namespace);
    GETSYM(g_hMono, array_length); GETSYM(g_hMono, array_addr_with_size);
    GETSYM(g_hMono, string_to_utf8);

    g_domain = p_get_root_domain();
    if (!g_domain) { Log("!! 拿不到 root domain"); return 1; }
    p_thread_attach(g_domain);

    g_asmLogLimit = 60;
    for (int i = 0; i < 60 && !g_image; i++) {
        if (!p_assembly_foreach) break;
        g_asmCount = 0;
        g_asmList[0] = 0;
        p_assembly_foreach(AsmEnumCb, NULL);
        if (i == 0) {
            Log("--- 已加载程序集 %d 个 ---", g_asmCount);
            if (g_asmList[0]) Log("%s", g_asmList);
            g_asmLogLimit = 0;
        }
        if (!g_image) Sleep(1000);
    }
    if (g_image) Log("[+] 路线1 (assembly_foreach) 成功");

    if (!g_image && p_image_loaded) {
        const char* cand[3] = { "Assembly-CSharp.dll", "Assembly-CSharp", "Assembly-CSharp, Version=0.0.0.0" };
        for (int i = 0; i < 3 && !g_image; i++) {
            MonoImage* im = p_image_loaded(cand[i]);
            if (im) g_image = im;
        }
        if (g_image) Log("[+] 路线2 (image_loaded) 成功");
    }

    if (!g_image && p_domain_assembly_open) {
        char path[MAX_PATH * 3];
        GetGameManagedPath(path, sizeof(path));
        if (path[0]) {
            MonoAssembly* a2 = p_domain_assembly_open(g_domain, path);
            if (a2) {
                g_image = p_assembly_get_image(a2);
                Log("[+] 路线3 (domain_assembly_open) 成功");
            }
        }
    }

    if (!g_image) { Log("!! 三条路线全失败, 退出 (游戏本身没受影响)"); return 1; }

    for (int i = 0; i < 240 && !g_hwnd; i++) {
        FindWndCtx ctx; ctx.best = NULL; ctx.pid = GetCurrentProcessId();
        EnumWindows(EnumWinProc, (LPARAM)&ctx);
        g_hwnd = ctx.best;
        if (!g_hwnd) Sleep(500);
    }
    if (!g_hwnd) { Log("!! 找不到游戏窗口"); return 1; }
    g_oldProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)JqrProc);
    Log("hwnd = %p  oldProc = %p", (void*)g_hwnd, (void*)g_oldProc);

    Sleep(1500);
    PostMessageW(g_hwnd, WM_JQR_SELFTEST, 0, 0);
    Sleep(2500);
    PostMessageW(g_hwnd, WM_JQR_SELFTEST, 0, 0);

    HfResetAll();
    HfLoad();

    Log("");
    Log("=== 就绪: Insert 呼出菜单; 快捷键在菜单的「快捷键」页里改 ===");
    Log("");

    static BOOL down[NHF + HFC_MAX + 1];
    for (;;) {
        int total = HfTotal();
        for (int i = 0; i < total; i++) {
            int vk = HfBindVal(i);
            if (vk <= 0) { down[i] = FALSE; continue; }
            BOOL now = (GetAsyncKeyState(vk) & 0x8000) ? TRUE : FALSE;
            if (now && !down[i]) PostMessageW(g_hwnd, WM_JQR_HOTFUNC, (WPARAM)i, 0);
            down[i] = now;
        }

        BOOL ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) ? TRUE : FALSE;
        int insSlot = total;
        if (ins && !down[insSlot]) {
            LONG v = InterlockedCompareExchange(&g_showMenu, 0, 0);
            InterlockedExchange(&g_showMenu, v ? 0 : 1);
            ClipCursor(NULL);
        }
        down[insSlot] = ins;
        Sleep(40);
    }
    return 0;
}

static DWORD WINAPI Worker(LPVOID p)
{
    __try {
        return WorkerBody(p);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("!! FATAL: worker 线程异常 code=%08X (已拦截, 游戏继续存活)",
            GetExceptionCode());
        return 0;
    }
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        g_hInst = hInst;

        HANDLE mtx = CreateMutexW(NULL, TRUE, L"Local\\JQRModInitOnce");
        if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
            return TRUE;
        }

        HANDLE h = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
