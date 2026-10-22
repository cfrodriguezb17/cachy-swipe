#include <QApplication>
#include <QWidget>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QWidget a, b, c;
    a.setWindowTitle("Editor — proyecto"); b.setWindowTitle("Navegador — documentación"); c.setWindowTitle("Terminal — desarrollo");
    a.setStyleSheet("background: #274060"); b.setStyleSheet("background: #3d5a48"); c.setStyleSheet("background: #593f66");
    for (auto w : {&a, &b, &c}) { w->resize(650,420); w->show(); }
    c.activateWindow();
    return app.exec();
}
