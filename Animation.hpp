#pragma once

#include <cstdint>

struct AnimationPoint {
    float x = 0.0f;
    float y = 0.0f;
};

// Position animation presets. Bezier uses a cubic path through two control
// points; Linear interpolates directly between the endpoints.
class Animation {
public:
    enum class Preset { Linear, Bezier };

    static Animation linear(AnimationPoint from, AnimationPoint to,
                            float durationSeconds, uint32_t repeatCount = 1);
    static Animation bezier(AnimationPoint from, AnimationPoint control1,
                            AnimationPoint control2, AnimationPoint to,
                            float durationSeconds, uint32_t repeatCount = 1);

    // Advances by deltaSeconds and writes the current position. Returns false
    // after the requested finite number of plays is complete. repeatCount=0
    // means repeat forever; otherwise it is the total number of plays.
    bool advance(float deltaSeconds, AnimationPoint& position);
    bool active() const { return m_active; }
    uint32_t repeatCount() const { return m_repeatCount; }
    void setRepeatCount(uint32_t repeatCount);

private:
    Animation(Preset preset, AnimationPoint from, AnimationPoint control1,
              AnimationPoint control2, AnimationPoint to,
              float durationSeconds, uint32_t repeatCount);

    AnimationPoint sample(float t) const;

    Preset m_preset = Preset::Linear;
    AnimationPoint m_from;
    AnimationPoint m_control1;
    AnimationPoint m_control2;
    AnimationPoint m_to;
    float m_durationSeconds = 0.0f;
    double m_elapsedSeconds = 0.0;
    uint64_t m_completedPlays = 0;
    uint32_t m_repeatCount = 1;
    bool m_active = false;
};
