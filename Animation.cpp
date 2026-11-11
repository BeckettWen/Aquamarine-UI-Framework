#include "Animation.hpp"

#include <algorithm>
#include <cmath>

Animation::Animation(Preset preset, AnimationPoint from,
                     AnimationPoint control1, AnimationPoint control2,
                     AnimationPoint to, float durationSeconds,
                     uint32_t repeatCount)
    : m_preset(preset),
      m_from(from),
      m_control1(control1),
      m_control2(control2),
      m_to(to),
      m_durationSeconds(durationSeconds),
      m_repeatCount(repeatCount),
      m_active(std::isfinite(durationSeconds) && durationSeconds > 0.0f) {}

Animation Animation::linear(AnimationPoint from, AnimationPoint to,
                            float durationSeconds, uint32_t repeatCount) {
    return Animation(Preset::Linear, from, from, to, to,
                     durationSeconds, repeatCount);
}

Animation Animation::bezier(AnimationPoint from, AnimationPoint control1,
                            AnimationPoint control2, AnimationPoint to,
                            float durationSeconds, uint32_t repeatCount) {
    return Animation(Preset::Bezier, from, control1, control2, to,
                     durationSeconds, repeatCount);
}

void Animation::setRepeatCount(uint32_t repeatCount) {
    m_repeatCount = repeatCount;
    m_completedPlays = 0;
    m_elapsedSeconds = 0.0;
    m_active = std::isfinite(m_durationSeconds) && m_durationSeconds > 0.0f;
}

AnimationPoint Animation::sample(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    if (m_preset == Preset::Linear) {
        return {
            m_from.x + (m_to.x - m_from.x) * t,
            m_from.y + (m_to.y - m_from.y) * t,
        };
    }

    const float oneMinusT = 1.0f - t;
    const float a = oneMinusT * oneMinusT * oneMinusT;
    const float b = 3.0f * oneMinusT * oneMinusT * t;
    const float c = 3.0f * oneMinusT * t * t;
    const float d = t * t * t;
    return {
        a * m_from.x + b * m_control1.x + c * m_control2.x + d * m_to.x,
        a * m_from.y + b * m_control1.y + c * m_control2.y + d * m_to.y,
    };
}

bool Animation::advance(float deltaSeconds, AnimationPoint& position) {
    if (!m_active) {
        position = m_to;
        return false;
    }
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) deltaSeconds = 0.0f;

    const double totalElapsed = m_elapsedSeconds + deltaSeconds;
    const uint64_t completedThisUpdate = static_cast<uint64_t>(
        totalElapsed / static_cast<double>(m_durationSeconds));
    if (m_repeatCount != 0 &&
        m_completedPlays + completedThisUpdate >= m_repeatCount) {
        m_completedPlays = m_repeatCount;
        m_elapsedSeconds = m_durationSeconds;
        m_active = false;
        position = m_to;
        return false;
    }

    m_completedPlays += completedThisUpdate;
    m_elapsedSeconds = std::fmod(totalElapsed, static_cast<double>(m_durationSeconds));
    const float progress = static_cast<float>(
        m_elapsedSeconds / static_cast<double>(m_durationSeconds));
    position = sample(progress);
    return true;
}
