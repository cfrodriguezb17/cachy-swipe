/*
    Cachy Swipe — matemática pura del gesto

    Convierte un deslizamiento horizontal en un índice dentro de la lista de
    ventanas del conmutador nativo. El índice sigue al dedo de forma
    proporcional, tiene topes duros en ambos extremos (sin dar la vuelta) y
    no cambia mientras el gesto no sea claramente horizontal.
*/
#pragma once

#include <algorithm>
#include <cmath>

class SwipeIndex
{
public:
    bool begin(int fingers, int required, double stepDistance, bool reverse = false)
    {
        cancel();
        if (fingers != required || !std::isfinite(stepDistance) || stepDistance <= 0) {
            return false;
        }
        m_active = true;
        m_stepDistance = stepDistance;
        m_reverse = reverse;
        return true;
    }

    // Enlaza la lista del conmutador: cuántos elementos tiene y qué índice
    // ocupa la ventana activa. A partir de aquí index() es válido.
    bool attach(int itemCount, int baseIndex)
    {
        if (!m_active || itemCount < 2 || baseIndex < 0 || baseIndex >= itemCount) {
            return false;
        }
        const double offset = progress();
        m_count = itemCount;
        m_base = baseIndex;
        m_attached = true;
        m_index = baseIndex;
        m_position = std::clamp(m_base + offset, 0.0, double(m_count - 1));
        updateIndex();
        return true;
    }

    // Acumula el movimiento y clasifica el eje. Devuelve el índice absoluto
    // actual; no cambia hasta que attach() fija la lista.
    int update(double dx, double dy)
    {
        if (!m_active || !std::isfinite(dx) || !std::isfinite(dy)) {
            return m_index;
        }
        if (!m_axisChosen || !m_attached) {
            m_x += dx;
            m_y += dy;
        }
        if (!m_axisChosen) {
            if (std::hypot(m_x, m_y) < 8) {
                return m_index; // temblor inicial
            }
            if (std::abs(m_x) > std::abs(m_y) * 1.5) {
                m_horizontal = true;
                m_axisChosen = true;
            } else if (std::abs(m_y) >= std::abs(m_x) * 1.5) {
                m_declined = true;
                m_axisChosen = true;
                return m_index;
            } else {
                return m_index; // ambiguo: seguir esperando
            }
        }
        if (m_declined || !m_attached) {
            return m_index;
        }
        // No acumular recorrido invisible al empujar contra un extremo.
        m_position = std::clamp(m_position + (m_reverse ? dx : -dx) / m_stepDistance,
                                0.0, double(m_count - 1));
        updateIndex();
        return m_index;
    }

    bool active() const { return m_active; }
    bool attached() const { return m_attached; }
    bool horizontal() const { return m_horizontal; }
    int index() const { return m_index; }
    int base() const { return m_base; }
    int count() const { return m_count; }
    int steps() const { return m_index - m_base; }
    double progress() const
    {
        return m_attached ? m_position - m_base : (m_reverse ? m_x : -m_x) / m_stepDistance;
    }

    void finish() { m_active = false; }

    void cancel()
    {
        m_active = false;
        m_attached = false;
        m_horizontal = false;
        m_axisChosen = false;
        m_declined = false;
        m_reverse = false;
        m_count = 0;
        m_base = 0;
        m_index = 0;
        m_stepDistance = 120;
        m_x = 0;
        m_y = 0;
        m_position = 0;
    }

private:
    void updateIndex()
    {
        if (!m_attached) {
            return;
        }
        if (!m_horizontal) return;
        // Dos umbrales distintos (56% al avanzar, 44% al volver) evitan
        // alternar de ventana por pequeñas oscilaciones alrededor del centro.
        while (m_index < m_count - 1 && m_position >= m_index + 0.56) ++m_index;
        while (m_index > 0 && m_position <= m_index - 0.56) --m_index;
    }

    bool m_active = false;
    bool m_attached = false;
    bool m_horizontal = false;
    bool m_axisChosen = false;
    bool m_declined = false;
    bool m_reverse = false;
    int m_count = 0;
    int m_base = 0;
    int m_index = 0;
    double m_stepDistance = 120;
    double m_x = 0;
    double m_y = 0;
    double m_position = 0;
};
