#ifndef _sawGenericInput_ButtonState_h
#define _sawGenericInput_ButtonState_h

#include <cisstParameterTypes/prmEventButton.h>
#include <cstdint>
#include <vector>

namespace sawGenericInput {
// Event times are monotonic microseconds from evdev, not polling-loop times.
struct ButtonState {
    unsigned int QuickTapMs = 0;
    bool Down = false;
    bool Inhibited = false;
    std::uint64_t PressTime = 0;

    void Reset(bool physicallyDown = false) {
        Down = false;
        Inhibited = physicallyDown;
        PressTime = 0;
    }

    std::vector<prmEventButton::EventType> Process(int value, std::uint64_t time) {
        if (value != 0 && value != 1) {
            return {}; // Ignore autorepeat.
        }
        if (Inhibited) {
            if (value == 0) {
                Inhibited = false;
            }
            return {};
        }
        if (value == 1 && !Down) {
            Down = true;
            PressTime = time;
            return {prmEventButton::PRESSED};
        }
        if (value == 0 && Down) {
            Down = false;
            if (QuickTapMs && time >= PressTime
                && time - PressTime <= std::uint64_t(QuickTapMs) * 1000) {
                return {prmEventButton::RELEASED, prmEventButton::CLICKED};
            }
            return {prmEventButton::RELEASED};
        }
        return {}; // Ignore duplicate transitions and unmatched releases.
    }

};
}
#endif
