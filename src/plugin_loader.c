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
typedef void (*Ts3CurrentServerConnectionChangedFn)(uint64);
typedef void (*Ts3OnConnectStatusChangeEventFn)(uint64, int, unsigned int);
typedef void (*Ts3OnUpdateClientEventFn)(uint64, anyID, anyID, const char*, const char*);
typedef void (*Ts3OnTalkStatusChangeEventFn)(uint64, int, int, anyID);
typedef void (*Ts3OnHotkeyEventFn)(const char*);
typedef void (*Ts3OnEditCapturedVoiceDataEventFn)(uint64, short*, int, int, int*);
typedef void (*Ts3OnEditMixedPlaybackVoiceDataEventFn)(uint64, short*, int, int, const unsigned int*, unsigned int*);

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
  Ts3CurrentServerConnectionChangedFn currentServerConnectionChanged;
  Ts3OnConnectStatusChangeEventFn onConnectStatusChangeEvent;
  Ts3OnUpdateClientEventFn onUpdateClientEvent;
  Ts3OnTalkStatusChangeEventFn onTalkStatusChangeEvent;
  Ts3OnHotkeyEventFn onHotkeyEvent;
  Ts3OnEditCapturedVoiceDataEventFn onEditCapturedVoiceDataEvent;
  Ts3OnEditMixedPlaybackVoiceDataEventFn onEditMixedPlaybackVoiceDataEvent;
} RuntimeApi;

static RuntimeApi g_runtime;
static char* g_registeredPluginId = NULL;
static int g_runtimeLoadState = 0; /* 0=unknown, 1=loaded, -1=failed */

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
  g_runtimeLoadState = 0;
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

  if (g_runtimeLoadState == 1 && g_runtime.module) {
    return 1;
  }
  if (g_runtimeLoadState == -1) {
    return 0;
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
    g_runtimeLoadState = -1;
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
  g_runtime.currentServerConnectionChanged = (Ts3CurrentServerConnectionChangedFn)GetProcAddress(
    g_runtime.module, "ts3plugin_currentServerConnectionChanged"
  );
  g_runtime.onConnectStatusChangeEvent = (Ts3OnConnectStatusChangeEventFn)GetProcAddress(
    g_runtime.module, "ts3plugin_onConnectStatusChangeEvent"
  );
  g_runtime.onUpdateClientEvent = (Ts3OnUpdateClientEventFn)GetProcAddress(
    g_runtime.module, "ts3plugin_onUpdateClientEvent"
  );
  g_runtime.onTalkStatusChangeEvent = (Ts3OnTalkStatusChangeEventFn)GetProcAddress(
    g_runtime.module, "ts3plugin_onTalkStatusChangeEvent"
  );
  g_runtime.onHotkeyEvent = (Ts3OnHotkeyEventFn)GetProcAddress(g_runtime.module, "ts3plugin_onHotkeyEvent");
  g_runtime.onEditCapturedVoiceDataEvent = (Ts3OnEditCapturedVoiceDataEventFn)GetProcAddress(g_runtime.module, "ts3plugin_onEditCapturedVoiceDataEvent");
  g_runtime.onEditMixedPlaybackVoiceDataEvent = (Ts3OnEditMixedPlaybackVoiceDataEventFn)GetProcAddress(
    g_runtime.module, "ts3plugin_onEditMixedPlaybackVoiceDataEvent"
  );

  if (!g_runtime.name || !g_runtime.version || !g_runtime.apiVersion || !g_runtime.author ||
      !g_runtime.description || !g_runtime.setFunctionPointers || !g_runtime.init ||
      !g_runtime.shutdown || !g_runtime.offersConfigure || !g_runtime.configure ||
      !g_runtime.registerPluginID || !g_runtime.commandKeyword || !g_runtime.processCommand ||
      !g_runtime.freeMemory || !g_runtime.requestAutoload || !g_runtime.initMenus ||
      !g_runtime.initHotkeys || !g_runtime.onMenuItemEvent || !g_runtime.onHotkeyEvent ||
      !g_runtime.onEditCapturedVoiceDataEvent) {
    append_debug_log("load_runtime: missing one or more required exports");
    clear_runtime();
    g_runtimeLoadState = -1;
    return 0;
  }

  append_debug_log("load_runtime: runtime exports resolved");
  g_runtimeLoadState = 1;
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
  if (load_runtime() && g_runtime.name) {
    return g_runtime.name();
  }
  return kFallbackName;
}

PLUGINS_EXPORTDLL const char* ts3plugin_version(void) {
  if (load_runtime() && g_runtime.version) {
    return g_runtime.version();
  }
  return kFallbackVersion;
}

PLUGINS_EXPORTDLL int ts3plugin_apiVersion(void) {
  if (load_runtime() && g_runtime.apiVersion) {
    return g_runtime.apiVersion();
  }
  return 26;
}

PLUGINS_EXPORTDLL const char* ts3plugin_author(void) {
  if (load_runtime() && g_runtime.author) {
    return g_runtime.author();
  }
  return kFallbackAuthor;
}

PLUGINS_EXPORTDLL const char* ts3plugin_description(void) {
  if (load_runtime() && g_runtime.description) {
    return g_runtime.description();
  }
  return kFallbackDescription;
}

PLUGINS_EXPORTDLL void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
  ts3Functions = funcs;
  if (load_runtime() && g_runtime.setFunctionPointers) {
    g_runtime.setFunctionPointers(funcs);
  }
}

PLUGINS_EXPORTDLL int ts3plugin_init(void) {
  append_debug_log("ts3plugin_init: loader init");
  if (load_runtime()) {
    if (g_runtime.setFunctionPointers) {
      g_runtime.setFunctionPointers(ts3Functions);
    }
    if (g_registeredPluginId && g_runtime.registerPluginID) {
      g_runtime.registerPluginID(g_registeredPluginId);
    }
    if (g_runtime.init && g_runtime.init() != 0) {
      append_debug_log("ts3plugin_init: runtime init failed");
      clear_runtime();
      log_ts_message("RP Soundboard Ultimate runtime init failed, falling back to safe mode");
      return 0;
    }
    append_debug_log("ts3plugin_init: runtime initialized");
    return 0;
  }

  log_ts_message("RP Soundboard Ultimate loaded in safe mode");
  return 0;
}

PLUGINS_EXPORTDLL void ts3plugin_shutdown(void) {
  append_debug_log("ts3plugin_shutdown: loader shutdown");
  if (g_runtime.shutdown) {
    g_runtime.shutdown();
  }
  clear_runtime();
  if (g_registeredPluginId) {
    free(g_registeredPluginId);
    g_registeredPluginId = NULL;
  }
}

PLUGINS_EXPORTDLL int ts3plugin_offersConfigure(void) {
  if (load_runtime() && g_runtime.offersConfigure) {
    return g_runtime.offersConfigure();
  }
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
  if (g_registeredPluginId) {
    free(g_registeredPluginId);
    g_registeredPluginId = NULL;
  }
  if (id) {
    const size_t size = strlen(id) + 1;
    g_registeredPluginId = (char*)malloc(size);
    if (g_registeredPluginId) {
      memcpy(g_registeredPluginId, id, size);
    }
  }

  if (load_runtime() && g_runtime.registerPluginID) {
    g_runtime.registerPluginID(id);
  }
}

PLUGINS_EXPORTDLL const char* ts3plugin_commandKeyword(void) {
  if (load_runtime() && g_runtime.commandKeyword) {
    const char* keyword = g_runtime.commandKeyword();
    if (keyword && keyword[0] != '\0') {
      return keyword;
    }
  }
  return kFallbackCommandKeyword;
}

PLUGINS_EXPORTDLL int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
  if (load_runtime() && g_runtime.processCommand) {
    return g_runtime.processCommand(serverConnectionHandlerID, command);
  }

  (void)serverConnectionHandlerID;
  if (command && (_stricmp(command, "show") == 0 || _stricmp(command, "reload") == 0)) {
    return launch_dashboard_process() ? 0 : 1;
  }
  return 1;
}

PLUGINS_EXPORTDLL void ts3plugin_freeMemory(void* data) {
  if (g_runtime.module && g_runtime.freeMemory) {
    g_runtime.freeMemory(data);
  } else {
    free(data);
  }
}

PLUGINS_EXPORTDLL int ts3plugin_requestAutoload(void) {
  if (load_runtime() && g_runtime.requestAutoload) {
    return g_runtime.requestAutoload();
  }
  return 1;
}

PLUGINS_EXPORTDLL void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
  if (load_runtime() && g_runtime.initMenus) {
    g_runtime.initMenus(menuItems, menuIcon);
    return;
  }

  (void)menuIcon;
  *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * 2);
  if (!*menuItems) {
    return;
  }
  (*menuItems)[0] = create_menu_item(PLUGIN_MENU_TYPE_GLOBAL, kMenuOpenWindow, kFallbackMenuText);
  (*menuItems)[1] = NULL;
}

PLUGINS_EXPORTDLL void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys) {
  if (load_runtime() && g_runtime.initHotkeys) {
    g_runtime.initHotkeys(hotkeys);
    return;
  }

  *hotkeys = (struct PluginHotkey**)malloc(sizeof(struct PluginHotkey*));
  (*hotkeys)[0] = NULL;
}

PLUGINS_EXPORTDLL void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
  if (load_runtime() && g_runtime.onMenuItemEvent) {
    g_runtime.onMenuItemEvent(serverConnectionHandlerID, type, menuItemID, selectedItemID);
    return;
  }

  (void)serverConnectionHandlerID;
  (void)type;
  (void)selectedItemID;
  if (menuItemID == kMenuOpenWindow) {
    if (!launch_dashboard_process()) {
      show_dashboard_launch_error();
    }
  }
}

PLUGINS_EXPORTDLL void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
  if (load_runtime() && g_runtime.currentServerConnectionChanged) {
    g_runtime.currentServerConnectionChanged(serverConnectionHandlerID);
  }
}

PLUGINS_EXPORTDLL void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
  if (load_runtime() && g_runtime.onConnectStatusChangeEvent) {
    g_runtime.onConnectStatusChangeEvent(serverConnectionHandlerID, newStatus, errorNumber);
  }
}

PLUGINS_EXPORTDLL void ts3plugin_onUpdateClientEvent(
  uint64 serverConnectionHandlerID, anyID clientID, anyID invokerID, const char* invokerName,
  const char* invokerUniqueIdentifier
) {
  if (load_runtime() && g_runtime.onUpdateClientEvent) {
    g_runtime.onUpdateClientEvent(
      serverConnectionHandlerID, clientID, invokerID, invokerName, invokerUniqueIdentifier
    );
  }
}

PLUGINS_EXPORTDLL void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
  if (load_runtime() && g_runtime.onTalkStatusChangeEvent) {
    g_runtime.onTalkStatusChangeEvent(serverConnectionHandlerID, status, isReceivedWhisper, clientID);
  }
}

PLUGINS_EXPORTDLL void ts3plugin_onHotkeyEvent(const char* keyword) {
  if (load_runtime() && g_runtime.onHotkeyEvent) {
    g_runtime.onHotkeyEvent(keyword);
    return;
  }

  (void)keyword;
}

PLUGINS_EXPORTDLL void ts3plugin_onEditCapturedVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, int* edited) {
  if (load_runtime() && g_runtime.onEditCapturedVoiceDataEvent) {
    g_runtime.onEditCapturedVoiceDataEvent(serverConnectionHandlerID, samples, sampleCount, channels, edited);
    return;
  }

  (void)serverConnectionHandlerID;
  (void)samples;
  (void)sampleCount;
  (void)channels;
  (void)edited;
}

PLUGINS_EXPORTDLL void ts3plugin_onEditMixedPlaybackVoiceDataEvent(
  uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels,
  const unsigned int* channelSpeakerArray, unsigned int* channelFillMask
) {
  if (load_runtime() && g_runtime.onEditMixedPlaybackVoiceDataEvent) {
    g_runtime.onEditMixedPlaybackVoiceDataEvent(
      serverConnectionHandlerID, samples, sampleCount, channels, channelSpeakerArray, channelFillMask
    );
    return;
  }

  (void)serverConnectionHandlerID;
  (void)samples;
  (void)sampleCount;
  (void)channels;
  (void)channelSpeakerArray;
  (void)channelFillMask;
}
