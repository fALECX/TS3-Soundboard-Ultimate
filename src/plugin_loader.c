#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

#include "pluginsdk/include/plugin_definitions.h"
#include "pluginsdk/include/ts3_functions.h"

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#define PLUGINS_EXPORTDLL __declspec(dllexport)
#else
#define PLUGINS_EXPORTDLL
#endif

struct TS3Functions ts3Functions;

static const char* kFallbackName = "RP Soundboard Ultimate";
static const char* kFallbackVersion = "0.2.0";
static const char* kFallbackAuthor = "OpenAI Codex";
static const char* kFallbackDescription = "RP Soundboard Ultimate TeamSpeak plugin with external dashboard fallback.";
static const char* kFallbackCommandKeyword = "rpsu";
static const char* kFallbackMenuText = "Open RP Soundboard Ultimate";
static const char* kDashboardLaunchErrorMessage =
  "RP Soundboard Ultimate could not launch the dashboard UI.\n\n"
  "Check that the packaged dashboard files are installed under the TeamSpeak plugins folder.";
static const int kMenuOpenWindow = 1;

typedef const char* (*Ts3NameFn)(void);
typedef const char* (*Ts3VersionFn)(void);
typedef int (*Ts3ApiVersionFn)(void);
typedef const char* (*Ts3AuthorFn)(void);
typedef const char* (*Ts3DescriptionFn)(void);
typedef void (*Ts3SetFunctionPointersFn)(const struct TS3Functions);
typedef int (*Ts3InitFn)(void);
typedef void (*Ts3ShutdownFn)(void);
typedef int (*Ts3OffersConfigureFn)(void);
typedef void (*Ts3ConfigureFn)(void*, void*);
typedef void (*Ts3RegisterPluginIdFn)(const char*);
typedef const char* (*Ts3CommandKeywordFn)(void);
typedef int (*Ts3ProcessCommandFn)(uint64, const char*);
typedef void (*Ts3FreeMemoryFn)(void*);
typedef int (*Ts3RequestAutoloadFn)(void);
typedef void (*Ts3InitMenusFn)(struct PluginMenuItem***, char**);
typedef void (*Ts3InitHotkeysFn)(struct PluginHotkey***);
typedef void (*Ts3OnMenuItemEventFn)(uint64, enum PluginMenuType, int, uint64);
typedef void (*Ts3OnHotkeyEventFn)(const char*);
typedef void (*Ts3OnEditCapturedVoiceDataEventFn)(uint64, short*, int, int, int*);

typedef struct RuntimeApi {
  HMODULE module;
  Ts3NameFn name;
  Ts3VersionFn version;
  Ts3ApiVersionFn apiVersion;
  Ts3AuthorFn author;
  Ts3DescriptionFn description;
  Ts3SetFunctionPointersFn setFunctionPointers;
  Ts3InitFn init;
  Ts3ShutdownFn shutdown;
  Ts3OffersConfigureFn offersConfigure;
  Ts3ConfigureFn configure;
  Ts3RegisterPluginIdFn registerPluginID;
  Ts3CommandKeywordFn commandKeyword;
  Ts3ProcessCommandFn processCommand;
  Ts3FreeMemoryFn freeMemory;
  Ts3RequestAutoloadFn requestAutoload;
  Ts3InitMenusFn initMenus;
  Ts3InitHotkeysFn initHotkeys;
  Ts3OnMenuItemEventFn onMenuItemEvent;
  Ts3OnHotkeyEventFn onHotkeyEvent;
  Ts3OnEditCapturedVoiceDataEventFn onEditCapturedVoiceDataEvent;
} RuntimeApi;

static RuntimeApi g_runtime;

static void append_debug_log(const char* message) {
  wchar_t appData[MAX_PATH];
  wchar_t logPath[MAX_PATH];
  FILE* file;

  if (!GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
    return;
  }

  if (swprintf(logPath, MAX_PATH, L"%ls\\TS3Client\\rpsu_loader.log", appData) < 0) {
    return;
  }

  file = _wfopen(logPath, L"a+b");
  if (!file) {
    return;
  }

  fputs(message, file);
  fputs("\n", file);
  fclose(file);
}

static void append_last_error(const char* prefix) {
  char buffer[512];
  DWORD error = GetLastError();
  _snprintf(buffer, sizeof(buffer), "%s (GetLastError=%lu)", prefix, (unsigned long)error);
  buffer[sizeof(buffer) - 1] = '\0';
  append_debug_log(buffer);
}

static void log_ts_message(const char* message) {
  if (ts3Functions.logMessage) {
    ts3Functions.logMessage(message, LogLevel_INFO, "RP Soundboard Ultimate", 0);
  }
}

static void clear_runtime(void) {
  if (g_runtime.module) {
    FreeLibrary(g_runtime.module);
  }
  memset(&g_runtime, 0, sizeof(g_runtime));
}

static int path_from_module(wchar_t* out, size_t outCount) {
  HMODULE module = NULL;
  wchar_t filePath[MAX_PATH];
  wchar_t* slash;
  DWORD len;

  if (!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&path_from_module,
        &module)) {
    return 0;
  }

  len = GetModuleFileNameW(module, filePath, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return 0;
  }

  slash = wcsrchr(filePath, L'\\');
  if (!slash) {
    slash = wcsrchr(filePath, L'/');
  }
  if (!slash) {
    return 0;
  }
  *slash = L'\0';

  if (wcslen(filePath) + 1 > outCount) {
    return 0;
  }

  wcscpy(out, filePath);
  return 1;
}

static int load_runtime(void) {
  wchar_t dir[MAX_PATH];
  wchar_t runtimeDir[MAX_PATH];
  wchar_t runtimePath[MAX_PATH];

  if (g_runtime.module) {
    append_debug_log("load_runtime: runtime already loaded");
    return 1;
  }

  if (!path_from_module(dir, MAX_PATH)) {
    append_debug_log("load_runtime: failed to resolve loader directory");
    return 0;
  }

  SetDllDirectoryW(dir);
  append_debug_log("load_runtime: SetDllDirectoryW applied");

  if (swprintf(runtimeDir, MAX_PATH, L"%ls\\rp_soundboard_ultimate", dir) < 0) {
    append_debug_log("load_runtime: failed to build runtime directory");
    return 0;
  }

  if (swprintf(runtimePath, MAX_PATH, L"%ls\\rp_soundboard_ultimate_runtime.bin", runtimeDir) < 0) {
    append_debug_log("load_runtime: failed to build runtime path");
    return 0;
  }

  g_runtime.module = LoadLibraryExW(runtimePath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!g_runtime.module) {
    append_last_error("load_runtime: LoadLibraryExW failed");
    return 0;
  }

  append_debug_log("load_runtime: LoadLibraryExW succeeded");

  g_runtime.name = (Ts3NameFn)GetProcAddress(g_runtime.module, "ts3plugin_name");
  g_runtime.version = (Ts3VersionFn)GetProcAddress(g_runtime.module, "ts3plugin_version");
  g_runtime.apiVersion = (Ts3ApiVersionFn)GetProcAddress(g_runtime.module, "ts3plugin_apiVersion");
  g_runtime.author = (Ts3AuthorFn)GetProcAddress(g_runtime.module, "ts3plugin_author");
  g_runtime.description = (Ts3DescriptionFn)GetProcAddress(g_runtime.module, "ts3plugin_description");
  g_runtime.setFunctionPointers = (Ts3SetFunctionPointersFn)GetProcAddress(g_runtime.module, "ts3plugin_setFunctionPointers");
  g_runtime.init = (Ts3InitFn)GetProcAddress(g_runtime.module, "ts3plugin_init");
  g_runtime.shutdown = (Ts3ShutdownFn)GetProcAddress(g_runtime.module, "ts3plugin_shutdown");
  g_runtime.offersConfigure = (Ts3OffersConfigureFn)GetProcAddress(g_runtime.module, "ts3plugin_offersConfigure");
  g_runtime.configure = (Ts3ConfigureFn)GetProcAddress(g_runtime.module, "ts3plugin_configure");
  g_runtime.registerPluginID = (Ts3RegisterPluginIdFn)GetProcAddress(g_runtime.module, "ts3plugin_registerPluginID");
  g_runtime.commandKeyword = (Ts3CommandKeywordFn)GetProcAddress(g_runtime.module, "ts3plugin_commandKeyword");
  g_runtime.processCommand = (Ts3ProcessCommandFn)GetProcAddress(g_runtime.module, "ts3plugin_processCommand");
  g_runtime.freeMemory = (Ts3FreeMemoryFn)GetProcAddress(g_runtime.module, "ts3plugin_freeMemory");
  g_runtime.requestAutoload = (Ts3RequestAutoloadFn)GetProcAddress(g_runtime.module, "ts3plugin_requestAutoload");
  g_runtime.initMenus = (Ts3InitMenusFn)GetProcAddress(g_runtime.module, "ts3plugin_initMenus");
  g_runtime.initHotkeys = (Ts3InitHotkeysFn)GetProcAddress(g_runtime.module, "ts3plugin_initHotkeys");
  g_runtime.onMenuItemEvent = (Ts3OnMenuItemEventFn)GetProcAddress(g_runtime.module, "ts3plugin_onMenuItemEvent");
  g_runtime.onHotkeyEvent = (Ts3OnHotkeyEventFn)GetProcAddress(g_runtime.module, "ts3plugin_onHotkeyEvent");
  g_runtime.onEditCapturedVoiceDataEvent = (Ts3OnEditCapturedVoiceDataEventFn)GetProcAddress(g_runtime.module, "ts3plugin_onEditCapturedVoiceDataEvent");

  if (!g_runtime.name || !g_runtime.version || !g_runtime.apiVersion || !g_runtime.author ||
      !g_runtime.description || !g_runtime.setFunctionPointers || !g_runtime.init ||
      !g_runtime.shutdown || !g_runtime.offersConfigure || !g_runtime.configure ||
      !g_runtime.registerPluginID || !g_runtime.commandKeyword || !g_runtime.processCommand ||
      !g_runtime.freeMemory || !g_runtime.requestAutoload || !g_runtime.initMenus ||
      !g_runtime.initHotkeys || !g_runtime.onMenuItemEvent || !g_runtime.onHotkeyEvent ||
      !g_runtime.onEditCapturedVoiceDataEvent) {
    append_debug_log("load_runtime: missing one or more required exports");
    clear_runtime();
    return 0;
  }

  append_debug_log("load_runtime: runtime exports resolved");
  return 1;
}

static int build_support_path(wchar_t* out, size_t outCount, const wchar_t* fileName) {
  wchar_t dir[MAX_PATH];
  int written;

  if (!path_from_module(dir, MAX_PATH)) {
    append_debug_log("build_support_path: failed to resolve loader directory");
    return 0;
  }

  written = swprintf(out, outCount, L"%ls\\rp_soundboard_ultimate\\%ls", dir, fileName);
  if (written < 0 || (size_t)written >= outCount) {
    append_debug_log("build_support_path: failed to build support path");
    return 0;
  }

  return 1;
}

static struct PluginMenuItem* create_menu_item(enum PluginMenuType type, int id, const char* text) {
  struct PluginMenuItem* item = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
  if (!item) {
    return NULL;
  }

  memset(item, 0, sizeof(struct PluginMenuItem));
  item->type = type;
  item->id = id;
#ifdef _WIN32
  strcpy_s(item->text, PLUGIN_MENU_BUFSZ, text);
#else
  strncpy(item->text, text, PLUGIN_MENU_BUFSZ - 1);
#endif
  return item;
}

static int launch_dashboard_process(void) {
  wchar_t exePath[MAX_PATH];
  wchar_t workingDir[MAX_PATH];
  STARTUPINFOW startupInfo;
  PROCESS_INFORMATION processInfo;

  if (!build_support_path(exePath, MAX_PATH, L"rpsu_ui_preview.exe")) {
    return 0;
  }

  if (!build_support_path(workingDir, MAX_PATH, L".")) {
    return 0;
  }

  memset(&startupInfo, 0, sizeof(startupInfo));
  memset(&processInfo, 0, sizeof(processInfo));
  startupInfo.cb = sizeof(startupInfo);

  if (!CreateProcessW(exePath, NULL, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, workingDir, &startupInfo, &processInfo)) {
    append_last_error("launch_dashboard_process: CreateProcessW failed");
    return 0;
  }

  CloseHandle(processInfo.hThread);
  CloseHandle(processInfo.hProcess);
  append_debug_log("launch_dashboard_process: dashboard launched");
  return 1;
}

static void show_dashboard_launch_error(void) {
  MessageBoxA(
    NULL,
    kDashboardLaunchErrorMessage,
    "RP Soundboard Ultimate",
    MB_OK | MB_ICONINFORMATION
  );
}

PLUGINS_EXPORTDLL const char* ts3plugin_name(void) {
  return kFallbackName;
}

PLUGINS_EXPORTDLL const char* ts3plugin_version(void) {
  return kFallbackVersion;
}

PLUGINS_EXPORTDLL int ts3plugin_apiVersion(void) {
  return 26;
}

PLUGINS_EXPORTDLL const char* ts3plugin_author(void) {
  return kFallbackAuthor;
}

PLUGINS_EXPORTDLL const char* ts3plugin_description(void) {
  return kFallbackDescription;
}

PLUGINS_EXPORTDLL void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
  ts3Functions = funcs;
}

PLUGINS_EXPORTDLL int ts3plugin_init(void) {
  append_debug_log("ts3plugin_init: safe mode init");
  log_ts_message("RP Soundboard Ultimate loaded in safe mode");
  return 0;
}

PLUGINS_EXPORTDLL void ts3plugin_shutdown(void) {
  append_debug_log("ts3plugin_shutdown: safe mode shutdown");
}

PLUGINS_EXPORTDLL int ts3plugin_offersConfigure(void) {
  return PLUGIN_OFFERS_CONFIGURE_NEW_THREAD;
}

PLUGINS_EXPORTDLL void ts3plugin_configure(void* handle, void* qParentWidget) {
  if (load_runtime() && g_runtime.configure) {
    g_runtime.configure(handle, qParentWidget);
    return;
  }

  (void)handle;
  (void)qParentWidget;
  if (!launch_dashboard_process()) {
    show_dashboard_launch_error();
  }
}

PLUGINS_EXPORTDLL void ts3plugin_registerPluginID(const char* id) {
  (void)id;
}

PLUGINS_EXPORTDLL const char* ts3plugin_commandKeyword(void) {
  return kFallbackCommandKeyword;
}

PLUGINS_EXPORTDLL int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
  (void)serverConnectionHandlerID;
  if (command && (_stricmp(command, "show") == 0 || _stricmp(command, "reload") == 0)) {
    return launch_dashboard_process() ? 0 : 1;
  }
  return 1;
}

PLUGINS_EXPORTDLL void ts3plugin_freeMemory(void* data) {
  free(data);
}

PLUGINS_EXPORTDLL int ts3plugin_requestAutoload(void) {
  return 1;
}

PLUGINS_EXPORTDLL void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
  (void)menuIcon;
  *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * 2);
  if (!*menuItems) {
    return;
  }
  (*menuItems)[0] = create_menu_item(PLUGIN_MENU_TYPE_GLOBAL, kMenuOpenWindow, kFallbackMenuText);
  (*menuItems)[1] = NULL;
}

PLUGINS_EXPORTDLL void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys) {
  *hotkeys = (struct PluginHotkey**)malloc(sizeof(struct PluginHotkey*));
  (*hotkeys)[0] = NULL;
}

PLUGINS_EXPORTDLL void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
  (void)serverConnectionHandlerID;
  (void)type;
  (void)selectedItemID;
  if (menuItemID == kMenuOpenWindow) {
    if (!launch_dashboard_process()) {
      show_dashboard_launch_error();
    }
  }
}

PLUGINS_EXPORTDLL void ts3plugin_onHotkeyEvent(const char* keyword) {
  (void)keyword;
}

PLUGINS_EXPORTDLL void ts3plugin_onEditCapturedVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, int* edited) {
  (void)serverConnectionHandlerID;
  (void)samples;
  (void)sampleCount;
  (void)channels;
  (void)edited;
}
