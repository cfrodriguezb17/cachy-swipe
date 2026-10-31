/*
    Cachy Swipe — cambia de ventana con gestos del touchpad usando el
    conmutador nativo de KDE (el mismo de Alt+Tab).

    El gesto de 3 (o 4) dedos no sintetiza ninguna tecla: maneja el TabBox
    directamente. Al deslizar se muestra el conmutador nativo y la selección
    avanza según la distancia recorrida, con topes en la primera y última
    ventana. Al soltar se confirma la ventana resaltada; al cancelar se
    restaura la original.
*/
#include <config-kwin.h>
#include <effect/effect.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <input.h>
#include <input_event.h>
#include <keyboard_input.h>
#include <workspace.h>
#include "swipe_index.h"
#include "tabbox_bridge.h"
#include <KConfigGroup>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QScopedValueRollback>
#include <algorithm>
#include <cmath>
#ifdef CACHY_SWIPE_TESTING
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#endif

namespace KWin
{
class CachySwipe final : public Effect, public InputEventFilter
{
    Q_OBJECT

public:
    CachySwipe()
        : InputEventFilter(InputFilterOrder::WindowSelector)
    {
        reconfigure(ReconfigureAll);
        // Al bloquear, el filtro de pantalla bloqueada toma los eventos finales;
        // este efecto debe liberar también el flujo para funcionar al volver.
        connect(effects, &EffectsHandler::screenAboutToLock, this, [this] { finish(true); });
        connect(effects, &EffectsHandler::desktopChanged, this, [this] { finish(true, false); });
        connect(effects, &EffectsHandler::currentActivityChanged, this, [this] { finish(true, false); });
        connect(effects, &EffectsHandler::windowActivated, this, [this](EffectWindow *window) {
            if (m_shown && window != m_origin) finish(true, false);
        });
        // Visibilidad del conmutador por vías ajenas (Alt+Tab normal, acciones
        // del sistema). currentTabBoxWindow() no es fiable: mantiene la última
        // ventana aunque el conmutador esté oculto.
        connect(effects, &EffectsHandler::tabBoxAdded, this, [this](int) {
            m_tabBoxSignaledVisible = true;
            if (!m_openingSwitcher) {
                m_foreignTabBox = true;
                m_shown = false;
                m_swipe.cancel();
                m_windowList.clear();
                m_origin.clear();
            }
        });
        // Si el conmutador se cierra por otra vía (Esc, acción del sistema,
        // timeout), abandonamos el gesto sin volver a tocarlo.
        connect(effects, &EffectsHandler::tabBoxClosed, this, [this] {
            m_tabBoxSignaledVisible = false;
            m_foreignTabBox = false;
            // Mantener la propiedad del flujo hasta su evento final. Otros
            // reconocedores no recibieron Begin y no deben recibir su cola.
            m_shown = false;
            m_swipe.cancel();
            m_windowList.clear();
            m_origin.clear();
        });
        connect(effects, &EffectsHandler::tabBoxUpdated, this, [this] {
            auto *tabBox = switcher();
            if (m_shown && (!tabBox || tabBox->currentClientList() != m_windowList)) {
                finish(true, false);
            }
        });
        input()->installInputEventFilter(this);
    }

    ~CachySwipe() override
    {
        finish(true);
        input()->uninstallInputEventFilter(this);
    }

    void reconfigure(ReconfigureFlags) override
    {
        if (m_ownedStream) finish(true, false);
        const KConfigGroup config(effects->config(), QStringLiteral("Effect-cachyswipe"));
        m_fingers = std::clamp(config.readEntry("FingerCount", 3), 3, 4);
        const double distance = config.readEntry("SwipeDistance", 120.0);
        m_distance = std::isfinite(distance) ? std::clamp(distance, 80.0, 4000.0) : 120.0;
        m_reverse = config.readEntry("ReverseDirection", true);
        const KConfigGroup tabBoxConfig(effects->config(), QStringLiteral("TabBox"));
        m_gridNavigation = config.readEntry("GridNavigation", true)
            && tabBoxConfig.readEntry("LayoutName", QStringLiteral("breeze")) == QStringLiteral("cachy-swipe-grid");
    }

    bool swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override
    {
        m_gesturesSeen++;
        if (m_ownedStream) {
            m_lastGesture = QStringLiteral("ignored: busy");
            return false;
        }
        if (event->fingerCount != m_fingers) {
            m_lastGesture = QStringLiteral("ignored: %1 fingers").arg(event->fingerCount);
            return false;
        }
        if (effects->isScreenLocked()) {
            m_lastGesture = QStringLiteral("ignored: locked");
            return false;
        }
        if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this) {
            m_lastGesture = QStringLiteral("ignored: fullscreen effect");
            return false;
        }
        if (input()->keyboard()->modifiers() != Qt::NoModifier) {
            m_lastGesture = QStringLiteral("ignored: modifiers");
            return false;
        }
        if (m_foreignTabBox) {
            m_lastGesture = QStringLiteral("ignored: switcher visible");
            return false;
        }
        if (!switcher()) {
            m_lastGesture = QStringLiteral("ignored: no tabbox");
            return false;
        }
        if (candidateCount() < 2) {
            m_lastGesture = QStringLiteral("ignored: <2 windows");
            return false;
        }
        if (!m_swipe.begin(event->fingerCount, m_fingers, m_distance, m_reverse, m_gridNavigation ? 4 : 0)) {
            m_lastGesture = QStringLiteral("ignored: config");
            return false;
        }
        m_ownedStream = true;
        m_lastGesture = QStringLiteral("owned");
        return true;
    }

    bool swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override
    {
        if (!m_ownedStream) {
            return false;
        }
        if (!m_swipe.active()) {
            return true; // gesto ya descartado: se consume en silencio
        }
        m_swipe.update(event->delta.x(), event->delta.y());
        if (!m_swipe.attached()) {
            if (!m_swipe.ready()) {
                return true; // aún ambiguo o descartado
            }
            if (!showSwitcher()) {
                m_swipe.finish();
                return true;
            }
        }
        applySelection();
        return true;
    }

    bool swipeGestureEnd(PointerSwipeGestureEndEvent *event) override
    {
        Q_UNUSED(event)
        if (!m_ownedStream) {
            return false;
        }
        finish(false);
        return true;
    }

    bool swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override
    {
        Q_UNUSED(event)
        if (!m_ownedStream) {
            return false;
        }
        finish(true);
        return true;
    }

    QString debug(const QString &parameter) const override
    {
#ifdef CACHY_SWIPE_TESTING
        auto *self = const_cast<CachySwipe *>(this);
        if (parameter == QStringLiteral("test-begin")) {
            PointerSwipeGestureBeginEvent event{m_fingers, {}};
            self->swipeGestureBegin(&event);
        } else if (parameter == QStringLiteral("test-wobble")) {
            PointerSwipeGestureUpdateEvent event{QPointF(-6, -2), {}};
            self->swipeGestureUpdate(&event);
        } else if (parameter == QStringLiteral("test-next")) {
            PointerSwipeGestureUpdateEvent event{QPointF((m_reverse ? 1 : -1) * m_distance * 0.6, 0), {}};
            self->swipeGestureUpdate(&event);
        } else if (parameter == QStringLiteral("test-prev")) {
            PointerSwipeGestureUpdateEvent event{QPointF((m_reverse ? -1 : 1) * m_distance * 0.6, 0), {}};
            self->swipeGestureUpdate(&event);
        } else if (parameter == QStringLiteral("test-vertical")) {
            PointerSwipeGestureUpdateEvent event{QPointF(2, m_distance * 0.6), {}};
            self->swipeGestureUpdate(&event);
        } else if (parameter.startsWith(QStringLiteral("test-move:"))) {
            const auto values = parameter.mid(10).split(QLatin1Char(','));
            if (values.size() == 2) {
                bool xOk, yOk;
                const double x = values[0].toDouble(&xOk);
                const double y = values[1].toDouble(&yOk);
                if (xOk && yOk) {
                    PointerSwipeGestureUpdateEvent event{QPointF(x * m_distance, y * m_distance), {}};
                    self->swipeGestureUpdate(&event);
                }
            }
        } else if (parameter == QStringLiteral("test-end")) {
            PointerSwipeGestureEndEvent event{};
            self->swipeGestureEnd(&event);
        } else if (parameter == QStringLiteral("test-cancel")) {
            PointerSwipeGestureCancelEvent event{};
            self->swipeGestureCancelled(&event);
        } else if (parameter == QStringLiteral("test-escape")) {
            KeyboardKeyEvent event{};
            event.key = Qt::Key_Escape;
            event.state = KeyboardKeyState::Pressed;
            input()->processFilters(&InputEventFilter::keyboardKey, &event);
        }
#else
        Q_UNUSED(parameter)
#endif
        QJsonObject state{
            {QStringLiteral("gestureActive"), m_ownedStream},
            {QStringLiteral("shown"), m_shown},
            {QStringLiteral("attached"), m_swipe.attached()},
            {QStringLiteral("horizontal"), m_swipe.horizontal()},
            {QStringLiteral("gridNavigation"), m_gridNavigation},
            {QStringLiteral("columns"), m_swipe.columns()},
            {QStringLiteral("row"), m_swipe.row()},
            {QStringLiteral("column"), m_swipe.column()},
            {QStringLiteral("base"), m_swipe.base()},
            {QStringLiteral("index"), m_swipe.index()},
            {QStringLiteral("steps"), m_swipe.steps()},
            {QStringLiteral("count"), m_swipe.count()},
            {QStringLiteral("distance"), m_distance},
            {QStringLiteral("reverse"), m_reverse},
            {QStringLiteral("fingers"), m_fingers},
            {QStringLiteral("visible"), m_tabBoxSignaledVisible},
            {QStringLiteral("gesturesSeen"), m_gesturesSeen},
            {QStringLiteral("lastGesture"), m_lastGesture},
            {QStringLiteral("activeCaption"), effects->activeWindow() ? effects->activeWindow()->caption() : QString()},
            {QStringLiteral("selectedCaption"), tabBoxSelectionCaption()},
        };
#ifdef CACHY_SWIPE_TESTING
        for (auto *window : QGuiApplication::allWindows()) {
            auto *quickWindow = qobject_cast<QQuickWindow *>(window);
            if (!quickWindow) continue;
            // PlasmaQuick::Dialog reparents the visual item, while its QObject
            // parent can remain in the switcher's QML component hierarchy.
            auto *panel = window->property("mainItem").value<QQuickItem *>();
            auto *grid = panel ? panel->findChild<QObject *>(QStringLiteral("cachy-swipe-grid-view")) : nullptr;
            if (panel && grid) {
                if (parameter == QStringLiteral("test-screenshot")) {
                    quickWindow->grabWindow().save(qEnvironmentVariable("CACHY_TEST_RUNTIME") + QStringLiteral("/grid.png"));
                }
                state.insert(QStringLiteral("uiColumns"), panel->property("columns").toInt());
                state.insert(QStringLiteral("uiRows"), panel->property("rows").toInt());
                state.insert(QStringLiteral("uiIndex"), grid->property("currentIndex").toInt());
                if (auto *item = grid->property("currentItem").value<QQuickItem *>()) {
                    auto *icon = item->objectName() == QStringLiteral("cachy-swipe-grid-icon")
                        ? item : item->findChild<QQuickItem *>(QStringLiteral("cachy-swipe-grid-icon"));
                    if (icon) {
                        state.insert(QStringLiteral("uiIconReady"), icon->property("valid").toBool());
                        state.insert(QStringLiteral("uiIconWidth"), icon->width());
                    }
                }
            }
        }
#endif
        return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
    }

private:
    static KWin::TabBox::TabBox *switcher()
    {
        return KWin::workspace() ? KWin::workspace()->tabbox() : nullptr;
    }

    // Aproximación rápida (sin filtrado de escritorio/actividad) para no
    // mostrar el conmutador cuando claramente no hay entre qué elegir.
    int candidateCount() const
    {
        const auto windows = effects->stackingOrder();
        return int(std::count_if(windows.cbegin(), windows.cend(), [](EffectWindow *w) {
            return w && !w->isDeleted() && !w->isSkipSwitcher()
                && (w->isNormalWindow() || w->isDialog());
        }));
    }

    bool showSwitcher()
    {
        auto *tabBox = switcher();
        if (!tabBox) {
            return false;
        }
        // La entrada modal nativa hace reset(false), actualiza el orden MRU y
        // captura la entrada. show() solo dibuja y no prepara esa sesión.
        {
            QScopedValueRollback opening(m_openingSwitcher, true);
            if (!tabBox->toggleMode(TabBoxWindowsMode)) return false;
        }
        m_shown = true;
        auto list = tabBox->currentClientList();
        if (list.size() < 2) {
            m_shown = false;
            tabBox->close(true);
            return false;
        }
        const EffectWindow *active = effects->activeWindow();
        int base = active ? list.indexOf(active->window()) : -1;
        if (base < 0) {
            base = 0;
        }
        if (!m_swipe.attach(list.size(), base)) {
            m_shown = false;
            tabBox->close(true);
            return false;
        }
        m_windowList = list;
        m_origin = effects->activeWindow();
        // Selección inicial explícita: la ventana activa.
        tabBox->setCurrentClient(list[base]);
        m_lastGesture = QStringLiteral("shown");
        return true;
    }

    void applySelection()
    {
        if (!m_shown || !m_swipe.attached()) {
            return;
        }
        auto *tabBox = switcher();
        if (!tabBox) {
            return;
        }
        // Cancelar si cambia la identidad o el orden de la lista: rebajar un
        // índice numérico podría terminar activando otra ventana al soltar.
        auto list = tabBox->currentClientList();
        if (list != m_windowList) {
            finish(true, false);
            return;
        }
        const int index = std::clamp(m_swipe.index(), 0, int(list.size()) - 1);
        if (KWin::Window *target = list[index]; target && tabBox->currentClient() != target) {
            tabBox->setCurrentClient(target);
        }
    }

    void finish(bool abort, bool releaseStream = true)
    {
        const bool wasOwned = m_ownedStream;
        const bool wasShown = m_shown;
        if (releaseStream) m_ownedStream = false;
        m_shown = false;
        m_windowList.clear();
        m_origin.clear();
        m_swipe.cancel();
        if (wasOwned && !wasShown) {
            m_lastGesture = QStringLiteral("no-op (declined)");
        }
        if (!wasOwned || !wasShown) {
            return;
        }
        m_lastGesture = abort ? QStringLiteral("cancelled") : QStringLiteral("committed");
        if (auto *tabBox = switcher()) {
            if (abort) {
                tabBox->close(true);
            } else {
                tabBox->accept(true);
            }
        }
    }

    QString tabBoxSelectionCaption() const
    {
        auto *tabBox = switcher();
        if (!tabBox) {
            return {};
        }
        KWin::Window *selected = tabBox->currentClient();
        if (!selected) {
            return {};
        }
        const auto windows = effects->stackingOrder();
        for (EffectWindow *w : windows) {
            if (w->window() == selected) {
                return w->caption();
            }
        }
        return {};
    }

    int m_fingers = 3;
    double m_distance = 120;
    bool m_reverse = true;
    bool m_gridNavigation = true;
    bool m_ownedStream = false;
    bool m_shown = false;
    bool m_tabBoxSignaledVisible = false;
    bool m_foreignTabBox = false;
    bool m_openingSwitcher = false;
    QList<KWin::Window *> m_windowList;
    QPointer<EffectWindow> m_origin;
    int m_gesturesSeen = 0;
    QString m_lastGesture = QStringLiteral("none");
    SwipeIndex m_swipe;
};

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(CachySwipe, "cachyswipe.json",
    return true;, return false;)
}
#include "cachyswipe.moc"
