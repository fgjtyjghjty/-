# 机器人决斗修改器

Clone Drone in the Danger Zone（机器人决斗场）单人修改器，无注释版源码。

## 构成

| 文件 | 作用 |
|---|---|
| `jqrmod.cpp` | 主体，D3D11 Present hook + ImGui 菜单 |
| `version_proxy.cpp` | version.dll 代理，开游戏自动加载 jqrmod.dll |
| `injector.cpp` | 手动注入器 |
| `jqr_probe_dll.cpp` | 探针，验证 mono 导出可用 |
| `test_proxy.cpp` | 代理加载测试 |
| `jqr_enemytypes.h` | 敌人类型表 |

## 用法

1. 编译 `jqrmod.cpp` 为 x64 DLL。
2. 丢进游戏根目录 `JQRMod\jqrmod.dll`。
3. 把 `version.dll` 放进游戏根目录。
4. 开游戏，进关卡，按 `Insert` 开菜单。

## 说明

Unity Mono 游戏，PE32+ x64，ImGui + D3D11。
所有游戏内操作走 mono 反射，不碰进程内存读写。
仅供单人离线使用。
