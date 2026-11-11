#include "Animation.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}
}

int main() {
    AnimationPoint position{};
    Animation linear = Animation::linear({0.0f, 0.0f}, {10.0f, 20.0f}, 2.0f, 2);
    assert(linear.active());
    assert(linear.repeatCount() == 2);
    assert(linear.advance(1.0f, position));
    assert(near(position.x, 5.0f) && near(position.y, 10.0f));
    assert(linear.advance(1.0f, position)); // second play restarts at the start
    assert(near(position.x, 0.0f) && near(position.y, 0.0f));
    assert(!linear.advance(2.0f, position));
    assert(near(position.x, 10.0f) && near(position.y, 20.0f));
    assert(!linear.active());

    Animation bezier = Animation::bezier(
        {0.0f, 0.0f}, {0.0f, 10.0f}, {10.0f, 10.0f}, {10.0f, 0.0f}, 1.0f);
    assert(bezier.advance(0.5f, position));
    assert(near(position.x, 5.0f) && near(position.y, 7.5f));
    assert(!bezier.advance(0.5f, position));
    assert(near(position.x, 10.0f) && near(position.y, 0.0f));

    Animation forever = Animation::linear({0.0f, 0.0f}, {1.0f, 0.0f}, 1.0f, 0);
    assert(forever.advance(3.25f, position));
    assert(near(position.x, 0.25f));
    forever.setRepeatCount(1);
    assert(forever.active() && forever.repeatCount() == 1);

    std::cout << "Animation presets and repeat counts passed\n";
    return 0;
}
