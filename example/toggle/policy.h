#pragma once

#include <string>
#include <map>

// state policy class must have `STATE` and `EVENT` enum type.
struct Policy {
    enum STATE {
        INIT = 0,
        ON,
        OFF,
        FIN,
    };

    enum EVENT {
        INIT_COMP = 0,
        TOGGLE,
        END,
    };
};

// `STATE` and `EVENT` must have to_string().
std::string to_string(Policy::STATE st) {
    static std::map<Policy::STATE, std::string> state_to_string = {
        {Policy::INIT, "INIT"},
        {Policy::ON, "ON"},
        {Policy::OFF, "OFF"},
        {Policy::FIN, "FIN"}};

    return state_to_string[st];
}

std::string to_string(Policy::EVENT ev) {
    static std::map<Policy::EVENT, std::string> event_to_string = {
        {Policy::INIT_COMP, "INIT_COMP"},
        {Policy::TOGGLE, "TOGGLE"},
        {Policy::END, "END"}};

    return event_to_string[ev];
}

// `DEFINE_EVENT()` or `DEFINE_EVENT_WITH_DATA()` macros must be called to use the event.
// second argument of `DEFINE_EVENT_WITH_DATA()` macro is argument type of function sending the event.
DEFINE_EVENT(Policy::INIT_COMP);
DEFINE_EVENT_WITH_DATA(Policy::TOGGLE, std::string);
DEFINE_EVENT(Policy::END);
