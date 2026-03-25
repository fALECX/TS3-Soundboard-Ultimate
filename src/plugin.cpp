#include "src/plugin.h"

#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstdlib>
#include <cstring>

#include "src/plugin_context.h"

struct TS3Functions ts3Functions;

namespace {

constexpr int kPluginApiVersion = 26;
constexpr int kMenuOpenWindow = 1;
char* g_pluginId = nullptr;

void logMessage(const char* text, LogLevel level = LogLevel_INFO) {
  if (ts3Functions.logMessage) {
    ts3Functions.logMessage(text, level, "RP Soundboard Ultimate", 0);
  }
}

PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text) {
  auto* item = static_cast<PluginMenuItem*>(std::malloc(sizeof(PluginMenuItem)));
  std::memset(item, 0, sizeof(PluginMenuItem));
  item->type = type;
  item->id = id;
#ifdef _WIN32
  strcpy_s(item->text, PLUGIN_MENU_BUFSZ, text);
#else
  std::strncpy(item->text, text, PLUGIN_MENU_BUFSZ - 1);
#endif
  return item;
}

PluginHotkey* createHotkey(const rpsu::HotkeyBinding& binding) {
  auto* hotkey = static_cast<PluginHotkey*>(std::malloc(sizeof(PluginHotkey)));
  std::memset(hotkey, 0, sizeof(PluginHotkey));
#ifdef _WIN32
  strcpy_s(hotkey->keyword, PLUGIN_HOTKEY_BUFSZ, binding.keyword.toUtf8().constData());
  strcpy_s(hotkey->description, PLUGIN_HOTKEY_BUFSZ, binding.displayValue.toUtf8().constData());
#else
  std::strncpy(hotkey->keyword, binding.keyword.toUtf8().constData(), PLUGIN_HOTKEY_BUFSZ - 1);
  std::strncpy(hotkey->description, binding.displayValue.toUtf8().constData(), PLUGIN_HOTKEY_BUFSZ - 1);
#endif
  return hotkey;
}

}  // namespace

extern "C" {

const char* ts3plugin_name() {
  return "RP Soundboard Ultimate";
}

const char* ts3plugin_version() {
  return "0.2.0";
}

int ts3plugin_apiVersion() {
  return kPluginApiVersion;
}

const char* ts3plugin_author() {
  return "OpenAI Codex";
}

const char* ts3plugin_description() {
  return "Native TeamSpeak 3 soundboard plugin with direct captured-voice injection.";
}

void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
  ts3Functions = funcs;
}

int ts3plugin_init() {
  QCoreApplication::setOrganizationName(QStringLiteral("RP Soundboard Ultimate"));
  QCoreApplication::setApplicationName(QStringLiteral("RP Soundboard Ultimate"));
  logMessage("Initializing RP Soundboard Ultimate plugin");
  return rpsu::PluginContext::instance().initialize() ? 0 : 1;
}

void ts3plugin_shutdown() {
  rpsu::PluginContext::instance().shutdown();
  if (g_pluginId) {
    std::free(g_pluginId);
    g_pluginId = nullptr;
  }
}

int ts3plugin_offersConfigure() {
  return PLUGIN_OFFERS_CONFIGURE_QT_THREAD;
}

void ts3plugin_configure(void* handle, void* qParentWidget) {
  Q_UNUSED(handle);
  rpsu::PluginContext::instance().showWindow(static_cast<QWidget*>(qParentWidget));
}

void ts3plugin_registerPluginID(const char* id) {
  if (!id) {
    return;
  }

  const size_t size = std::strlen(id) + 1;
  g_pluginId = static_cast<char*>(std::malloc(size));
  std::memcpy(g_pluginId, id, size);
}

const char* ts3plugin_commandKeyword() {
  return "rpsu";
}

int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
  Q_UNUSED(serverConnectionHandlerID);
  const QString text = QString::fromUtf8(command ? command : "");
  if (text == QStringLiteral("show") || text == QStringLiteral("reload")) {
    rpsu::PluginContext::instance().showWindow();
    return 0;
  }
  return 1;
}

void ts3plugin_freeMemory(void* data) {
  std::free(data);
}

int ts3plugin_requestAutoload() {
  return 1;
}

void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
  Q_UNUSED(menuIcon);
  *menuItems = static_cast<PluginMenuItem**>(std::malloc(sizeof(PluginMenuItem*) * 2));
  (*menuItems)[0] = createMenuItem(PLUGIN_MENU_TYPE_GLOBAL, kMenuOpenWindow, "Open RP Soundboard Ultimate");
  (*menuItems)[1] = nullptr;
}

void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys) {
  const QVector<rpsu::HotkeyBinding> bindings = rpsu::PluginContext::instance().hotkeys();
  *hotkeys = static_cast<PluginHotkey**>(std::malloc(sizeof(PluginHotkey*) * (bindings.size() + 1)));
  for (int index = 0; index < bindings.size(); ++index) {
    (*hotkeys)[index] = createHotkey(bindings[index]);
  }
  (*hotkeys)[bindings.size()] = nullptr;
}

void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
  Q_UNUSED(serverConnectionHandlerID);
  Q_UNUSED(type);
  Q_UNUSED(selectedItemID);
  if (menuItemID == kMenuOpenWindow) {
    rpsu::PluginContext::instance().showWindow();
  }
}

void ts3plugin_onHotkeyEvent(const char* keyword) {
  rpsu::PluginContext::instance().playHotkey(QString::fromUtf8(keyword ? keyword : ""));
}

void ts3plugin_onEditCapturedVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, int* edited) {
  Q_UNUSED(serverConnectionHandlerID);
  if (rpsu::PluginContext::instance().mixCaptured(samples, sampleCount, channels) && edited) {
    *edited = 1;
  }
}

}  // extern "C"
