#include <QApplication>
#include <QWidget>
#include <QIcon>
#include <memory>
#include <vector>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    // Give the virtual Wayland session a real desktop identity for icon lookup.
    QApplication::setDesktopFileName(QStringLiteral("org.kde.konsole"));
    QIcon::setThemeName(QStringLiteral("breeze"));
    const int count = qEnvironmentVariableIntValue("CACHY_TEST_WINDOWS");
    std::vector<std::unique_ptr<QWidget>> windows;
    for (int i = 0; i < (count > 0 ? count : 3); ++i) {
        auto window = std::make_unique<QWidget>();
        window->setWindowTitle(QStringLiteral("Test window %1").arg(i + 1));
        window->setWindowIcon(QIcon::fromTheme(i % 2 ? "utilities-terminal" : "applications-development"));
        window->setStyleSheet(QStringLiteral("background: %1").arg(i % 2 ? "#3d5a48" : "#274060"));
        window->resize(650, 420);
        window->show();
        windows.push_back(std::move(window));
    }
    windows.back()->activateWindow();
    return app.exec();
}
