#include <QApplication>

#include "src/plugin_context.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("RP Soundboard Ultimate"));
  QCoreApplication::setApplicationName(QStringLiteral("RP Soundboard Ultimate"));
  rpsu::PluginContext::instance().initialize();
  rpsu::PluginContext::instance().showWindow();
  return app.exec();
}
