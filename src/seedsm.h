/*
 * Copyright (c) 2019 Ueki Masaru <masaru.ueki@itage.co.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 *all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <cassert>
#include <functional>
#include <list>
#include <string>
#include <map>

#include <QStateMachine>
#include <QAbstractTransition>
#include <QState>
#include <QEvent>
#include <QDebug>

#define DEFINE_EVENT(EVENT)                                                    \
    template <>                                                                \
    struct _EventCreator<decltype(EVENT), static_cast<int>(EVENT)> {           \
        using EVENT_CLASS = seedsm::_inner::EventImpl<decltype(EVENT), EVENT>; \
        static QEvent* create() {                                              \
            return seedsm::_inner::EventImpl<decltype(EVENT),                  \
                                             EVENT>::create();                 \
        }                                                                      \
    };

#define DEFINE_EVENT_WITH_DATA(EVENT, DATATYPE)                      \
    template <>                                                      \
    struct _EventCreator<decltype(EVENT), static_cast<int>(EVENT)> { \
        using EVENT_CLASS = seedsm::_inner::EventImplWithData<       \
            decltype(EVENT), EVENT, DATATYPE>;                       \
        static QEvent* create(const DATATYPE& arg) {                 \
            return EVENT_CLASS::create(arg);                         \
        }                                                            \
    };

// workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480
template <typename EVENT, int EVENT_NO>
struct _EventCreator {};

namespace seedsm {

namespace _inner {

// FIXME: user defined EventKind number.
static const QEvent::Type TheEventType =
    static_cast<QEvent::Type>(static_cast<int>(QEvent::User) + 1);

template <typename EVENT_ENUM>
class Event : public QEvent {
    EVENT_ENUM event_type_;
    std::function<void()> on_delete_fn_;

public:
    Event(EVENT_ENUM event_type)
        : QEvent(TheEventType), event_type_(event_type) {}

    ~Event() {
        if (on_delete_fn_) on_delete_fn_();
    }

    EVENT_ENUM kind() const { return event_type_; };

    void on_delete(std::function<void()> fn) { on_delete_fn_ = fn; }
};

template <typename EVENT_ENUM, EVENT_ENUM EVENT>
class EventImpl : public Event<EVENT_ENUM> {
    EventImpl() : Event<EVENT_ENUM>(EVENT) {}

public:
    using callback_type = std::function<void()>;
    static const EVENT_ENUM event_type = EVENT;

    static EventImpl* create() { return new EventImpl(); }

    void exec(callback_type fn) { fn(); }
};

template <typename EVENT_ENUM, EVENT_ENUM EVENT, typename DATATYPE>
class EventImplWithData : public Event<EVENT_ENUM> {
    EventImplWithData(const DATATYPE& data)
        : Event<EVENT_ENUM>(EVENT), data(data) {}

public:
    const DATATYPE data;
    using callback_type = std::function<void(DATATYPE)>;
    static const EVENT_ENUM event_type = EVENT;

    static EventImplWithData* create(const DATATYPE& data) {
        return new EventImplWithData(data);
    }

    void exec(callback_type fn) { fn(data); }
};

template <typename EVENT, typename EVENT_ENUM>
inline EVENT* event_cast(Event<EVENT_ENUM>* ev) {
    if (ev->EventType() == static_cast<EVENT_ENUM>(EVENT::event_type)) {
        return static_cast<EVENT*>(ev);
    }

    return nullptr;
}

struct State : public QState {
    State(const std::string& name, QState* parent = nullptr)
        : QState(parent), name_(name) {}

    virtual ~State() {}

    const std::string name() { return name_; }

    void on_entered(std::function<void()> fn) {
        on_entered_callbacks_.push_back(fn);
    }

    void on_exited(std::function<void()> fn) {
        on_exited_callbacks_.push_back(fn);
    }

private:
    void onEntry(QEvent*) override {
        for (auto& fn : on_entered_callbacks_) {
            fn();
        }
    }

    void onExit(QEvent*) override {
        for (auto& fn : on_exited_callbacks_) {
            fn();
        }
    }

private:
    std::string name_;

    std::list<std::function<void()>> on_entered_callbacks_;
    std::list<std::function<void()>> on_exited_callbacks_;
};

template <typename EVENT_CLASS>
struct TransitionImpl : public QAbstractTransition {
    explicit TransitionImpl(QState* source = nullptr,
                            QAbstractState* target = nullptr)
        : QAbstractTransition(source), func_list_() {
        if (target) {
            setTargetState(target);
        }
    }

    void on_transition(typename EVENT_CLASS::callback_type fn) {
        func_list_.push_back(fn);
    }

    void on_transition_failed(typename EVENT_CLASS::callback_type fn) {
        failed_func_list_.push_back(fn);
    }

protected:
    // bool event_test(Event* ev) override {
    //     auto event = static_cast<EVENT_CLASS*>(ev);
    //     if (event->type() != EVENT_CLASS::event_type) return false;

    //     return true;
    // }

    bool eventTest(QEvent* ev) override {
        if (ev->type() != TheEventType) {
            return false;
        }

        // TODO
        /*        if (disable) {
                    qDebug() << "Transition Disable";
                    return false;
                }
        */

        auto event = static_cast<EVENT_CLASS*>(ev);
        if (event->kind() != EVENT_CLASS::event_type) {
            return false;
        }

        //        auto st = dynamic_cast<_inner::State*>(targetState());

        // TODO
        /*
                if (st) {
                    const bool target_enabled = st->isEnabled();
                    if (!target_enabled) {
                        qDebug() << "[State] " << qPrintable(st->name()) << " is
           disable";
                        for(auto fn: failed_func_list_) {
                            auto event = static_cast<EVENT_CLASS*>(ev);
                            event->Exec(fn);
                        }
                        return false;
                    }
                }
        */
        return true;
    }

    void onTransition(QEvent* ev) override {
        for (auto fn : func_list_) {
            auto event = static_cast<EVENT_CLASS*>(ev);
            event->exec(fn);
        }
    }

private:
    std::list<typename EVENT_CLASS::callback_type> func_list_;
    std::list<typename EVENT_CLASS::callback_type> failed_func_list_;
};
}  // _inner

template <typename STATE_POLICY>
struct StateMachine : public QStateMachine {
    using STATE_ID = typename STATE_POLICY::STATE;
    using EVENT_ID = typename STATE_POLICY::EVENT;

    template <EVENT_ID EVENT>
    using event_class = typename _EventCreator<
        decltype(EVENT), static_cast<int>(EVENT)>::EVENT_CLASS;

    StateMachine(const std::string& name)
        : QStateMachine(nullptr /*parent*/), name_(name), states_() {}

    ~StateMachine() {
        for (auto&& trans : transitions_) {
            delete trans.second;
        }
    }

    void set_parallel(STATE_ID st, bool is_parallel) {
        state(st)->setChildMode(is_parallel ? QState::ParallelStates
                                            : QState::ExclusiveStates);
    }

    void create_states(STATE_ID parent, const std::list<STATE_ID>& states) {
        for (auto&& s : states) {
            create_state(id_to_state(parent), s);
        }

        const STATE_ID s = states.front();
        state(parent)->setInitialState(state(s));
    }

    void create_states(const std::list<STATE_ID>& states) {
        for (auto&& s : states) {
            create_state(this, s);
        }
        const STATE_ID s = states.front();
        setInitialState(state(s));
    }

    template <EVENT_ID E, typename... Args>
    void send(Args&&... args) {
        auto event = event_class<E>::create(std::forward<Args>(args)...);
        postEvent(event);
    }

    template <EVENT_ID E, typename... Args>
    void send_high(Args&&... args) {
        auto event = event_class<E>::create(std::forward<Args>(args)...);
        postEvent(event, QStateMachine::HighPriority);
    }

    void on_state_entered(STATE_ID st, std::function<void()> fn) {
        state(st)->on_entered(fn);
    }

    void on_state_exited(STATE_ID st, std::function<void()> fn) {
        state(st)->on_exited(fn);
    }

    template <EVENT_ID EVENT>
    void add_transition(STATE_ID source) {
        assert(transitions_.count({state(source), EVENT}) == 0);

        auto tran = new _inner::TransitionImpl<event_class<EVENT>>(
            state(source), nullptr);
        transitions_[{state(source), EVENT}] = tran;
    }

    template <EVENT_ID EVENT>
    void add_transition(STATE_ID source, STATE_ID target) {
        assert(transitions_.count({state(source), EVENT}) == 0);

        auto tran = new _inner::TransitionImpl<event_class<EVENT>>(
            state(source), state(target));
        transitions_[{state(source), EVENT}] = tran;
    }

    template <EVENT_ID EVENT>
    void on_transition(STATE_ID source,
                       typename event_class<EVENT>::callback_type fn) {
        assert(transitions_.count({state(source), EVENT}) == 1);

        auto trans = static_cast<_inner::TransitionImpl<event_class<EVENT>>*>(
            transitions_[{state(source), EVENT}]);
        trans->on_transition(fn);
    }

private:
    bool is_active(_inner::State* state) {
        bool active = false;
        walk([this, &active, state](_inner::State* st) {
            if (state == st) {
                active = true;
            }
        });

        return active;
    }

    _inner::State* state(STATE_ID st) {
        // TODO: return const_cast<_inner::State*>(static_cast<const
        // StateMachine*>(this)->state(st));
        assert(states_.count(st) == 1);
        return states_[st];
    }

    const _inner::State* state(STATE_ID st) const {
        // TODO: return state_impl(st);
        assert(states_.count(st) == 1);
        return states_[st];
    }

private:
    const std::string name_;
    std::map<STATE_ID, _inner::State*> states_;
    std::map<std::pair<_inner::State*, EVENT_ID>, QAbstractTransition*>
        transitions_;

    void create_state(QState* parent, STATE_ID child) {
        assert(states_.count(child) == 0);

        states_[child] = new _inner::State(to_string(child), parent);
    }

    _inner::State* id_to_state(STATE_ID st) {
        assert(states_.count(st) == 1);
        return states_[st];
    }
};

}  // namespace seedsm
