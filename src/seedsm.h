#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>

#include <memory>
#include <functional>
#include <list>
#include <string>
#include <map>
#include <mutex>
#include <deque>

#include <ev++.h>

#ifndef SEEDSM_LOG_HANDLER
#define SEEDSM_LOG_HANDLER(fmt, arg) \
    {                               \
        vfprintf(stderr, fmt, arg); \
        fprintf(stderr, "\n");      \
    }
#endif

// workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480
template <typename EVENT, int EVENT_NO>
struct _EventCreator {};

namespace seedsm {

__attribute__((format(printf, 1, 2)))
static void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    SEEDSM_LOG_HANDLER(fmt, ap);
    va_end(ap);
}

__attribute__((format(printf, 1, 2)))
static void abort(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    SEEDSM_LOG_HANDLER(fmt, ap);
    va_end(ap);
    ::abort();
}

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
    using callback_type = std::function<void()>;
    static const EVENT_ENUM event_type = EVENT;

    static EventImpl* create() { return new EventImpl(); }

    void exec(callback_type fn) { fn(); }
};

#define DEFINE_EVENT(EVENT)                                                \
    template <>                                                            \
    struct _EventCreator<decltype(EVENT), static_cast<int>(EVENT)> { \
        using EVENT_CLASS = seedsm::EventImpl<decltype(EVENT), EVENT>;      \
        static seedsm::EventBase* create() {                                \
            return seedsm::EventImpl<decltype(EVENT), EVENT>::create();     \
        }                                                                  \
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

#define DEFINE_EVENT_WITH_DATA(EVENT, DATATYPE)                            \
    template <>                                                            \
    struct _EventCreator<decltype(EVENT), static_cast<int>(EVENT)> { \
        using EVENT_CLASS =                                                \
            seedsm::EventImplWithData<decltype(EVENT), EVENT, DATATYPE>;    \
        static seedsm::EventBase* create(const DATATYPE& arg) {             \
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

struct State {
    State(const std::string& name, State* parent = nullptr)
        : name_(name), parent_(parent) {
        if (parent) {
            parent->add_child(this);
        }
    }

    virtual ~State() {
        for (auto&& child : children_) {
            delete child;
        }
    }

    void enter(EventBase* event) {
        assert(!is_active_);

        if (parent_) parent_->enter_child(event, this);

        log("enter state: %s", name_.c_str());
        is_active_ = true;

        do_enter_callback(event);

        if (is_parallel_) {
            for (auto&& child: children_) {
                child->enter(event);
            }
            active_child_ = nullptr;
        } else {
            if (!children_.empty()) {
                active_child_ = children_.front();
                active_child_->enter(event);
            }
        }
    }

    void exit(EventBase* event) {
        assert(is_active_);

        if (active_child_) {
            active_child_->exit(event);
            active_child_ = nullptr;
        }

        if (is_parallel_) {
            for(auto&& child: children_) {
                assert(child->is_active());
                child->exit(event);
            }
        }

        log("exit state: %s", name_.c_str());
        is_active_ = false;

        do_exit_callback(event);
    }

    void walk(std::function<void(State*)> fn) {
        if (!is_active_) {
            return;
        }

        if (active_child_) {
            active_child_->walk(fn);
        }

        fn(this);
    }

    const bool is_active() const { return is_active_; }

    const State* parent() const { return parent_; }
    State* parent() { return parent_; }

    const std::string name() { return name_; }

    void on_entered(std::function<void()> fn) {
        on_entered_callbacks_.push_back(fn);
    }

    void on_exited(std::function<void()> fn) {
        on_exited_callbacks_.push_back(fn);
    }

    void set_parallel(bool is_par) {
        assert(!is_active_);
        is_parallel_ = is_par;
    }

    bool is_parallel() const {
        return is_parallel_;
    }

private:
    void add_child(State* child) {
        if (!child) return;

        children_.push_back(child);
    }

    void do_enter_callback(EventBase* event) {
        for (auto& fn : on_entered_callbacks_) {
            fn();
        }
    }

    void do_exit_callback(EventBase* event) {
        for (auto& fn : on_exited_callbacks_) {
            fn();
        }
    }

    void enter_child(EventBase* event, State* child) {
        active_child_ = child;

        if (is_active_) return;

        // FIXME: Error when having parallel states.

        if (parent_) parent_->enter_child(event, this);

        log("enter state: %s", name_.c_str());

        is_active_ = true;
        do_enter_callback(event);
    }

private:
    std::string name_;
    State* parent_;
    bool is_active_ = false;
    std::list<State*> children_;
    State* active_child_ = nullptr; // not used in parallel state
    bool is_parallel_ = false;

    std::list<std::function<void()>> on_entered_callbacks_;
    std::list<std::function<void()>> on_exited_callbacks_;

};

struct Transition {
    Transition(State* source = nullptr, State* target = nullptr)
        : source_(source), target_(target) {}

    // virtual bool event_test(Event* ev) = 0;

    virtual ~Transition(){}

    State* source_state() { return source_; }
    const State* source_state() const { return source_; }

    State* target_state() { return target_; }
    const State* target_state() const { return target_; }

    virtual void do_callback(EventBase* ev) = 0;

private:
    State* source_;
    State* target_;
};

template <typename EVENT_CLASS>
struct TransitionImpl : public Transition {
    explicit TransitionImpl(State* source = nullptr, State* target = nullptr)
        : Transition(source, target), func_list_() {}

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

    void do_callback(EventBase* ev) override {
        for (auto fn : func_list_) {
            auto event = static_cast<EVENT_CLASS*>(ev);
            event->exec(fn);
        }
    }

private:
    std::list<typename EVENT_CLASS::callback_type> func_list_;
    std::list<typename EVENT_CLASS::callback_type> failed_func_list_;
};

template <typename STATE_POLICY>
struct StateMachine : protected State {
    using STATE_ID = typename STATE_POLICY::STATE;
    using EVENT_ID = typename STATE_POLICY::EVENT;

    template <EVENT_ID EVENT>
    using event_class = typename _EventCreator<
        decltype(EVENT), static_cast<int>(EVENT)>::EVENT_CLASS;

    StateMachine(const std::string& name, ev::loop_ref loop)
        : State(name)
        , loop_(loop)
        , states_()
        , send_event_(std::unique_ptr<ev::async>(new ev::async(loop)))
        , init_event_(std::unique_ptr<ev::async>(new ev::async(loop))) {
        send_event_->set<StateMachine, &StateMachine::received>(this);
        init_event_->set<StateMachine, &StateMachine::initialize>(this);
    }

    ~StateMachine() {
        for (auto&& trans: transitions_) {
            delete trans.second;
        }

        for (auto&& ev: event_queue_) {
            delete ev;
        }

        for (auto&& ev: high_event_queue_) {
            delete ev;
        }
    }

    void create_states(STATE_ID parent, const std::list<STATE_ID>& states) {
        for (auto&& s : states) {
            create_state(id_to_state(parent), s);
        }
    }

    void create_states(const std::list<STATE_ID>& states) {
        for (auto&& s : states) {
            create_state(this, s);
        }
    }

    void set_parallel(bool is_par) {
        this->set_parallel(is_par);
    }

    void set_parallel(STATE_ID st, bool is_par) {
        state(st)->set_parallel(is_par);
    }

    void start() {
        init_event_->start();
        init_event_->send();

        send_event_->start();
    }

    void stop() {
        send_event_->stop();
        init_event_->stop();
    }

    template <EVENT_ID E, typename... Args>
    void send(Args&&... args) {
        auto event = event_class<E>::create(std::forward<Args>(args)...);
        post_event(event);
    }

    void post_event(EventBase* ev) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        event_queue_.push_back(ev);
        send_event_->send();
    }

    template <EVENT_ID E, typename... Args>
    void send_high(Args&&... args) {
        auto event = event_class<E>::create(std::forward<Args>(args)...);
        post_high_event(event);
    }

    void post_high_event(EventBase* ev) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        high_event_queue_.push_back(ev);
        send_event_->send();
    }

    EventBase* pop_event() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (!high_event_queue_.empty()) {
            auto ev = high_event_queue_.front();
            high_event_queue_.pop_front();
            return ev;
        }

        if (event_queue_.empty()) return nullptr;

        auto ev = event_queue_.front();
        event_queue_.pop_front();
        return ev;
    }

    const std::list<Transition*> collect_transition(EVENT_ID ev) {
        std::list<Transition*> trans;

        walk([this, ev, &trans](State* st) {
            if (transitions_.count({st, ev}) > 0) {
                trans.push_back(transitions_[{st, ev}]);
            }
        });

        return trans;
    }

    bool is_active(State* state) {
        bool active = false;
        walk([this, &active, state](State* st) {
            if (state == st) {
                active = true;
            }
        });

        return active;
    }

    void do_transition(EventBase* ev, State* source, State* target) {
        assert(source);
        if (source == target) {
            source->exit(ev);
            auto event = static_cast<Event<EVENT_ID>*>(ev);
            auto trans = transitions_[{source, event->type()}];
            trans->do_callback(ev);

            target->enter(ev);
            return;
        }

        State* prev_s = nullptr;
        for (State* s = source; s != nullptr; s = s->parent()) {
            for (State* t = target; t != nullptr; t = t->parent()) {
                if (s == t) {
                    if (prev_s) {
                        prev_s->exit(ev);
                    }

                    auto event = static_cast<Event<EVENT_ID>*>(ev);
                    auto trans = transitions_[{source, event->type()}];
                    trans->do_callback(ev);

                    target->enter(ev);
                    return;
                }
            }
            prev_s = s;
        }

        return;  // don't reach
    }

    void received() {
        for (;;) {
            auto ev = std::unique_ptr<Event<EVENT_ID>>(
                static_cast<Event<EVENT_ID>*>(pop_event()));
            if (!ev) return;

            auto ev_type = ev->type();
            auto trans = collect_transition(ev_type);

            for (auto&& tr : trans) {
                auto source = tr->source_state();
                auto target = tr->target_state();

                if (target) {
                    if (is_active(source)) {
                        do_transition(ev.get(), source, target);
                    }
                } else {
                    tr->do_callback(ev.get());
                }
            }
        }
    }

    void initialize() {
        log("initialize");

        enter(nullptr);
    }

    void on_state_entered(STATE_ID st, std::function<void()> fn) {
        state(st)->on_entered(fn);
    }

    void on_state_exited(STATE_ID st, std::function<void()> fn) {
        state(st)->on_exited(fn);
    }

    State* state(STATE_ID st) {
        // TODO: return const_cast<State*>(static_cast<const
        // StateMachine*>(this)->state(st));
        assert(states_.count(st) == 1);
        return states_[st];
    }

    const State* state(STATE_ID st) const {
        // TODO: return state_impl(st);
        assert(states_.count(st) == 1);
        return states_[st];
    }

    template <EVENT_ID EVENT>
    void add_transition(STATE_ID source) {
        assert(transitions_.count({state(source), EVENT}) == 0);

        auto tran = new TransitionImpl<event_class<EVENT>>(state(source),
                                                           nullptr);
        transitions_[{state(source), EVENT}] = tran;
    }

    template <EVENT_ID EVENT>
    void add_transition(STATE_ID source, STATE_ID target) {
        assert(transitions_.count({state(source), EVENT}) == 0);

        auto tran = new TransitionImpl<event_class<EVENT>>(state(source),
                                                           state(target));
        transitions_[{state(source), EVENT}] = tran;
    }

    template <EVENT_ID EVENT>
    void on_transition(STATE_ID source,
                       typename event_class<EVENT>::callback_type fn) {
        assert(transitions_.count({state(source), EVENT}) == 1);

        auto trans = static_cast<TransitionImpl<event_class<EVENT>>*>(
            transitions_[{state(source), EVENT}]);
        trans->on_transition(fn);
    }

private:
    ev::loop_ref loop_;
    std::map<STATE_ID, State*> states_;
    std::map<std::pair<State*, EVENT_ID>, Transition*> transitions_;

    std::unique_ptr<ev::async> init_event_;
    std::unique_ptr<ev::async> send_event_;
    std::mutex queue_mutex_;
    std::deque<EventBase*> event_queue_;
    std::deque<EventBase*> high_event_queue_;

    void create_state(State* parent, STATE_ID child) {
        assert(states_.count(child) == 0);

        states_[child] = new State(to_string(child), parent);
    }

    State* id_to_state(STATE_ID st) {
        assert(states_.count(st) == 1);
        return states_[st];
    }
};

}  // namespace seedsm
