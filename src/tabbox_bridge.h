/*
    Cachy Swipe — puente ABI hacia el conmutador nativo de KWin (TabBox)

    KWin instala workspace.h pero no tabbox.h, así que esta clase refleja el
    ABI *exportado* de KWin::TabBox::TabBox (verificado con `nm -D` sobre
    libkwin 6.8). Todos los métodos listados son símbolos exportados, no
    virtuales y fuera de línea; la clase solo se usa a través de punteros
    obtenidos con workspace()->tabbox() y nunca se instancia aquí.

    Gracias a esto el efecto maneja el conmutador nativo (el mismo de
    Alt+Tab) sin sintetizar ninguna pulsación de teclado.
*/
#pragma once

#include <effect/globals.h> // define KWin::TabBoxMode
#include <QList>

namespace KWin
{
class Window;

namespace TabBox
{
class TabBox
{
public:
    TabBox() = delete; // nunca se instancia desde el efecto

    // Entrada modal nativa: reconstruye el modelo y captura teclado/puntero.
    // Método privado de KWin, exportado y comprobado para la serie 6.8.
    bool toggleMode(KWin::TabBoxMode mode);
    void close(bool abort);
    void accept(bool closeTabBox);
    void setCurrentClient(KWin::Window *client);
    KWin::Window *currentClient();
    QList<KWin::Window *> currentClientList();
};
} // namespace TabBox
} // namespace KWin
