/*
    Cachy Swipe — matemática pura del gesto

    Convierte un deslizamiento horizontal en un índice dentro de la lista de
    ventanas del conmutador nativo. Admite una lista horizontal o una
    cuadrícula de columnas fijas, con topes por fila y columna y sin
    seleccionar celdas vacías. Ambos modos siguen la distancia del dedo.
*/
#pragma once

#include <algorithm>
#include <cmath>

class SwipeIndex
{
public:
    bool begin(int fingers, int required, double stepDistance, bool reverse = false, int columns = 0)
    {
        cancel();
        if (fingers != required || !std::isfinite(stepDistance) || stepDistance <= 0) {
            return false;
        }
        m_active = true;
        m_stepDistance = stepDistance;
        m_reverse = reverse;
        m_columns = std::max(0, columns);
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
        if (m_columns > 0) {
            m_columns = std::min(m_columns, itemCount);
            m_row = baseIndex / m_columns;
            m_column = baseIndex % m_columns;
            m_rowPosition = m_row;
            m_columnPosition = rowOffset(m_row) + m_column;
            updateGrid(m_x, m_y);
            return true;
        }
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
                m_axisChosen = true;
                if (m_columns == 0) {
                    m_declined = true;
                    return m_index;
                }
            } else {
                return m_index; // ambiguo: seguir esperando
            }
        }
        if (m_declined || !m_attached) {
            return m_index;
        }
        if (m_columns > 0) {
            updateGrid(dx, dy);
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
    bool ready() const { return m_axisChosen && !m_declined; }
    int columns() const { return m_columns; }
    int row() const { return m_columns ? m_index / m_columns : 0; }
    int column() const { return m_columns ? m_index % m_columns : m_index; }
    int index() const { return m_index; }
    int base() const { return m_base; }
    int count() const { return m_count; }
    int steps() const { return m_index - m_base; }
    double progress() const
    {
        if (m_attached && m_columns > 0) return m_index - m_base;
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
        m_columns = 0;
        m_row = m_column = 0;
        m_rowPosition = m_columnPosition = 0;
    }

private:
    int rowItems(int row) const
    {
        return std::min(m_columns, m_count - row * m_columns);
    }

    double rowOffset(int row) const
    {
        // Match the centered incomplete row in the native QML layout.
        return (m_columns - rowItems(row)) / 2.0;
    }

    void updateGrid(double dx, double dy)
    {
        if (!ready()) return;
        const int lastRow = (m_count - 1) / m_columns;
        const int previousRow = m_row;
        const double previousY = m_rowPosition;
        m_rowPosition = std::clamp(m_rowPosition + dy / m_stepDistance, 0.0, double(lastRow));
        const bool verticalTravel = m_rowPosition != previousY;
        while (m_row < lastRow && m_rowPosition >= m_row + 0.56) ++m_row;
        while (m_row > 0 && m_rowPosition <= m_row - 0.56) --m_row;

        const double offset = rowOffset(m_row);
        const int lastColumn = rowItems(m_row) - 1;
        if (!verticalTravel) {
            // No invisible horizontal debt at a partial row's boundaries.
            m_columnPosition = std::clamp(m_columnPosition, offset, offset + lastColumn);
        }
        m_columnPosition = std::clamp(m_columnPosition + (m_reverse ? dx : -dx) / m_stepDistance,
                                      0.0, double(m_columns - 1));
        if (previousRow != m_row || !verticalTravel) {
            m_columnPosition = std::clamp(m_columnPosition, offset, offset + lastColumn);
        }
        if (previousRow != m_row) {
            // Use the actual horizontal trace, not a remembered local column.
            // Keep sub-cell travel so curves and diagonals remain continuous.
            m_column = std::clamp(int(std::round(m_columnPosition - offset)), 0, lastColumn);
        } else {
            while (m_column < lastColumn && m_columnPosition >= offset + m_column + 0.56) ++m_column;
            while (m_column > 0 && m_columnPosition <= offset + m_column - 0.56) --m_column;
        }
        m_index = m_row * m_columns + m_column;
    }

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
    int m_columns = 0;
    int m_row = 0;
    int m_column = 0;
    double m_rowPosition = 0;
    double m_columnPosition = 0;
};
