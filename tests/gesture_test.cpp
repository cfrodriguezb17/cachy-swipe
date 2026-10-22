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

    std::cout << "swipe_index: all checks passed\n";
    return 0;
}
