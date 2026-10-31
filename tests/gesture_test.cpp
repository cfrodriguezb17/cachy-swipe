#include "swipe_index.h"
#include <cstdlib>
#include <iostream>
#include <limits>

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main()
{
    SwipeIndex swipe;
    check(!swipe.begin(2, 3, 600), "Two-finger scroll passes through");
    check(!swipe.begin(3, 3, 0), "Invalid distance does not start a gesture");
    check(swipe.begin(3, 3, 600), "Three fingers start a gesture");
    check(!swipe.attached() && !swipe.horizontal(), "No axis or list before any movement");

    check(swipe.update(-30, 2) == 0, "Small movement keeps the index");
    check(swipe.horizontal(), "A dominant horizontal movement picks the axis");
    check(!swipe.attached(), "Still unattached until the window list is known");
    check(!swipe.attach(1, 0), "A single item cannot be attached");
    check(!swipe.attach(4, 4), "An out-of-range base cannot be attached");
    check(swipe.attach(4, 1), "Attaching to a four item list with base one");
    check(swipe.steps() == 0 && swipe.index() == 1, "A wobble below half a step keeps index");

    check(swipe.update(-330, 0) == 2 && swipe.steps() == 1, "A little over half a step advances once");
    check(swipe.update(100, 0) == 1 && swipe.steps() == 0, "Reversing below half a step returns to base");
    check(swipe.update(100, 0) == 1, "Further small movement stays at base");

    check(swipe.update(-2400, 0) == 3 && swipe.steps() == 2, "Clamped at the last item: hard stop");
    check(swipe.update(-900, 0) == 3, "Travel beyond the end stays at the last item");
    check(swipe.update(6800, 0) == 0 && swipe.steps() == -1, "Clamped at the first item: hard stop");
    check(swipe.update(900, 0) == 0, "Travel beyond the first item stays there");

    // Regresión: oscilar alrededor del punto medio no alterna la selección.
    SwipeIndex half;
    check(half.begin(3, 3, 600), "Half-step gesture begins");
    check(half.update(-300, 0) == 0 && !half.attached(), "Axis picked at half a step");
    check(half.attach(4, 1), "Attach at half a step");
    check(half.steps() == 0, "Half a step remains inside the stability margin");
    check(half.update(-2, 0) == 1, "Tiny movement across halfway keeps the base");
    check(half.update(2, 0) == 1, "Tiny reversal keeps the base too");
    check(half.update(-40, 0) == 2, "Intentional movement crosses the forward threshold");
    check(half.update(40, 0) == 2, "Returning to halfway preserves the new selection");
    check(half.update(-2, 0) == 2 && half.update(2, 0) == 2, "Jitter cannot flicker after a step");
    check(half.update(40, 0) == 1, "Intentional reversal crosses the return threshold");
    check(half.update(600, 0) == 0, "Reverse direction reaches the first item");

    // Regresión: no se acumula movimiento invisible más allá de un extremo.
    SwipeIndex edge;
    edge.begin(3, 3, 600);
    edge.update(-6000, 0);
    edge.attach(3, 0);
    check(edge.index() == 2, "Large movement stops at the last window");
    check(edge.update(-6000, 0) == 2, "Further pushing stays at the last window");
    check(edge.update(360, 0) == 1, "A normal reversal immediately leaves the final stop");
    check(edge.update(6000, 0) == 0, "Large reverse travel stops at the first window");
    check(edge.update(6000, 0) == 0, "Pushing at the first stop does not accumulate");
    check(edge.update(-360, 0) == 1, "A normal reversal immediately leaves the first stop");
    check(edge.update(std::numeric_limits<double>::quiet_NaN(), 0) == 1, "Invalid movement is ignored");
    check(edge.update(-std::numeric_limits<double>::max(), 0) == 2, "Huge finite input is clamped before rounding");

    // Dirección invertida.
    SwipeIndex reversed;
    check(reversed.begin(3, 3, 600, true), "Reverse direction begins");
    check(reversed.update(360, 0) == 0, "Reverse accumulates positively");
    check(reversed.attach(3, 0), "Reverse attach");
    check(reversed.steps() == 1 && reversed.index() == 1, "Reverse maps to the same proportional step");
    check(reversed.update(-100, 0) == 0 && reversed.steps() == 0, "Reverse returns below half a step");

    // Un gesto vertical se descarta y se mantiene descartado.
    SwipeIndex vertical;
    check(vertical.begin(3, 3, 600), "Vertical gesture begins");
    check(vertical.update(2, 40) == 0, "Vertical dominance declines the gesture");
    check(!vertical.horizontal(), "Declined gesture is not horizontal");
    check(vertical.attach(4, 1), "Declined gesture can still report state");
    check(vertical.update(-2000, 0) == vertical.base() && vertical.steps() == 0, "Declined gesture ignores later movement");
    check(vertical.update(0, -80) == vertical.base() && vertical.steps() == 0, "Declined gesture stays ignored on vertical movement");

    // Zona ambigua: espera antes de decidir.
    SwipeIndex ambiguous;
    check(ambiguous.begin(3, 3, 600), "Ambiguous gesture begins");
    check(ambiguous.update(40, 40) == 0 && !ambiguous.horizontal(), "Ambiguous movement waits");
    check(ambiguous.update(-400, 0) == 0, "Decisive horizontal movement after ambiguity");
    check(ambiguous.horizontal(), "Axis becomes horizontal afterwards");
    check(ambiguous.attach(4, 1), "Attach after ambiguity");
    check(ambiguous.steps() == 1, "Accumulated travel counts proportionally after ambiguity");

    // Cancelar y reiniciar.
    SwipeIndex again;
    check(again.begin(3, 3, 600), "Cancel test begins");
    again.update(-300, 0);
    again.cancel();
    check(!again.active() && !again.attached(), "Cancel resets the state");
    check(again.begin(3, 3, 600), "Gesture can begin again after cancellation");
    check(again.attach(3, 1), "Fresh attach after cancellation");
    check(again.steps() == 0, "Cancelled travel does not leak into the new gesture");

    // Cuatro dedos con distancia inválida.
    check(!again.begin(3, 3, -5), "Negative distance is rejected");
    check(!again.begin(4, 3, 600), "A different finger count is rejected");

    // Four-column navigation uses both axes within a single gesture.
    SwipeIndex grid;
    check(grid.begin(3, 3, 120, true, 4), "Grid gesture begins");
    grid.update(72, 0);
    check(grid.attach(12, 0) && grid.index() == 1, "Initial horizontal movement selects column one");
    check(grid.update(0, 120) == 5, "Down moves to the same column in the next row");
    check(grid.update(-120, 0) == 4, "Left moves within the current row");
    check(grid.update(0, -120) == 0, "Up returns to the first row");
    check(grid.update(12000, 0) == 3, "Right stops at column four without wrapping");
    check(grid.update(12000, 0) == 3, "Horizontal edge accumulates no travel");
    check(grid.update(-72, 0) == 2, "Reversing immediately leaves the right edge");
    check(grid.update(0, 12000) == 10, "Down stops at the final row");
    check(grid.update(0, 12000) == 10, "Bottom edge accumulates no travel");
    check(grid.update(0, -72) == 6, "Reversing immediately leaves the bottom edge");
    check(grid.update(0, -12000) == 2, "Up stops at the first row");
    check(grid.update(0, 72) == 6, "Reversing immediately leaves the top edge");

    SwipeIndex shortRow;
    shortRow.begin(3, 3, 120, true, 4);
    shortRow.update(12000, 0);
    check(shortRow.attach(6, 0) && shortRow.index() == 3, "Four full columns above a partial row");
    check(shortRow.update(0, 120) == 5, "Partial row selects its nearest real window");
    check(shortRow.update(0, -120) == 2, "Returning up follows the centered window's visual column");
    check(shortRow.update(0, 120) == 5, "Return to partial row");
    check(shortRow.update(12000, 0) == 5, "Cannot enter an empty column");
    check(shortRow.update(-72, 0) == 4, "Partial right edge has no hidden travel");
    check(shortRow.update(-12000, 0) == 4, "Left stops at the beginning of the current row");
    check(shortRow.update(72, 0) == 5, "Reversing immediately leaves the partial row's left edge");

    for (int base : {4, 5}) {
        SwipeIndex centered;
        centered.begin(3, 3, 120, true, 4);
        centered.update(0, -72);
        centered.attach(6, base);
        check(centered.index() == base - 3, "Up from a centered lower row selects the window visually above");
    }

    for (int direction : {-1, 1}) {
        SwipeIndex diagonal;
        diagonal.begin(3, 3, 120, true, 4);
        diagonal.update(0, -8);
        diagonal.attach(6, 4);
        check(diagonal.update(direction * 120, -120) == 1 + direction,
              "Diagonal upward movement follows the horizontal component of the trace");
        SwipeIndex curved;
        curved.begin(3, 3, 120, true, 4);
        curved.update(0, -8);
        curved.attach(6, 4);
        for (int i = 0; i < 10; ++i) curved.update(direction * 12, -12);
        check(curved.index() == diagonal.index(), "A diagonal split into small events follows the same target");
    }

    for (int count : {5, 7}) {
        for (int direction : {-1, 1}) {
            SwipeIndex oddRow;
            oddRow.begin(3, 3, 120, true, 4);
            oddRow.update(0, -8);
            oddRow.attach(count, 4);
            const int expected = (count == 5 ? 1 : 0) + (direction > 0 ? 1 : 0);
            check(oddRow.update(direction * 24, -120) == expected,
                  "An odd centered row uses the trace to choose between the two nearest upper windows");
        }
    }

    SwipeIndex verticalGrid;
    verticalGrid.begin(3, 3, 120, true, 4);
    verticalGrid.update(0, 72);
    check(verticalGrid.ready() && !verticalGrid.horizontal(), "Vertical movement opens grid navigation");
    check(verticalGrid.attach(12, 0) && verticalGrid.index() == 4, "Initial down selects the second row");
    check(verticalGrid.update(30, 30) == 4, "Small diagonal movement stays within the stability margins");
    check(verticalGrid.update(72, 0) == 5, "Gesture changes from vertical to horizontal without lifting");
    verticalGrid.cancel();
    check(!verticalGrid.active() && !verticalGrid.ready() && verticalGrid.columns() == 0, "Cancel clears grid state");

    SwipeIndex reversedGrid;
    reversedGrid.begin(3, 3, 120, false, 4);
    reversedGrid.update(-72, 0);
    check(reversedGrid.attach(12, 0) && reversedGrid.index() == 1, "Horizontal reversal remains configurable");
    check(reversedGrid.update(0, 120) == 5, "Horizontal reversal never inverts up and down");

    SwipeIndex rowJitter;
    rowJitter.begin(3, 3, 120, true, 4);
    rowJitter.update(0, 60);
    check(rowJitter.attach(12, 0) && rowJitter.index() == 0, "Half a vertical step stays within the margin");
    check(rowJitter.update(0, 1) == 0, "Vertical midpoint jitter keeps the original row");
    check(rowJitter.update(0, 10) == 4, "Intentional vertical movement changes row");
    check(rowJitter.update(0, -10) == 4, "Row hysteresis prevents flicker");
    check(rowJitter.update(0, -10) == 0, "Intentional upward reversal restores first row");

    for (int count : {2, 3, 4, 5, 6, 7, 8, 12, 13}) {
        SwipeIndex bounded;
        bounded.begin(3, 3, 120, true, 4);
        bounded.update(0, 8);
        bounded.attach(count, 0);
        for (int i = 0; i < 100; ++i) {
            bounded.update((i % 3 - 1) * 600, (i % 5 - 2) * 120);
            check(bounded.index() >= 0 && bounded.index() < count, "All grid shapes select only existing windows");
        }
    }

    std::cout << "swipe_index: all checks passed\n";
    return 0;
}
