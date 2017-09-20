#pragma once

#include <functional>

// workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480
template <typename EVENT, int EVENT_NO>
struct _EventCreator {};

namespace seeds {

class EventBase {};

template <typename EVENT_ENUM>
class Event : public EventBase {
    EVENT_ENUM event_type_;
    std::function<void()> on_delete_fn_;

public:
    Event(EVENT_ENUM event_type) : event_type_(event_type) {}

    ~Event() {
        if (on_delete_fn_) on_delete_fn_();
    }

    EVENT_ENUM type() const { return event_type_; };

    void on_delete(std::function<void()> fn) { on_delete_fn_ = fn; }
};

template <typename EVENT_ENUM, EVENT_ENUM EVENT>
class EventImpl : public Event<EVENT_ENUM> {
    EventImpl() : Event<EVENT_ENUM>(EVENT) {}

public:
    using callback_type = std::function<void(EVENT_ENUM)>;
    static const EVENT_ENUM event_type = EVENT;

    static EventImpl* create() { return new EventImpl(); }

    void exec(callback_type fn) { fn(event_type); }
};

#define DEFINE_EVENT(EVENT)                                                \
    template <>                                                            \
    struct _EventCreator<decltype(EVENT), static_cast<int>(EVENT)> { \
        using EVENT_CLASS = seeds::EventImpl<decltype(EVENT), EVENT>;      \
        static seeds::EventBase* create() {                                \
            return seeds::EventImpl<decltype(EVENT), EVENT>::create();     \
        }                                                                  \
    };

template <typename EVENT_ENUM, EVENT_ENUM EVENT, typename DATATYPE>
class EventImplWithData : public Event<EVENT_ENUM> {
    EventImplWithData(const DATATYPE& data)
        : Event<EVENT_ENUM>(EVENT), data(data) {}

public:
    const DATATYPE data;
    using callback_type = std::function<void(EVENT_ENUM, DATATYPE)>;
    static const EVENT_ENUM event_type = EVENT;

    static EventImplWithData* create(const DATATYPE& data) {
        return new EventImplWithData(data);
    }

    void exec(callback_type fn) { fn(event_type, data); }
};

#define DEFINE_EVENT_WITH_DATA(EVENT, DATATYPE)                            \
    template <>                                                            \
    struct _EventCreator<decltype(EVENT), static_cast<int>(EVENT)> { \
        using EVENT_CLASS =                                                \
            seeds::EventImplWithData<decltype(EVENT), EVENT, DATATYPE>;    \
        static seeds::EventBase* create(const DATATYPE& arg) {             \
            return EVENT_CLASS::create(arg);                               \
        }                                                                  \
    };

template <typename EVENT, typename EVENT_ENUM>
inline EVENT* event_cast(Event<EVENT_ENUM>* ev) {
    if (ev->EventType() == static_cast<EVENT_ENUM>(EVENT::event_type)) {
        return static_cast<EVENT*>(ev);
    }

    return nullptr;
}

}  // seeds
