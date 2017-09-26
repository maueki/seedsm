#pragma once

#include "state.h"
#include "event.h"
#include "transition.h"
#include "log.h"

#include <future>
#include <memory>

namespace seeds {

template <typename STATE_POLICY>
struct StateMachine : protected State {
    using STATE_ID = typename STATE_POLICY::state_id_t;
    using EVENT_ID = typename STATE_POLICY::event_id_t;

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
        event_queue_.push(ev);
        send_event_->send();
    }

    template <EVENT_ID E, typename... Args>
    void send_high(Args&&... args) {
        auto event = event_class<E>::create(std::forward<Args>(args)...);
        post_high_event(event);
    }

    void post_high_event(EventBase* ev) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        high_event_queue_.push(ev);
        send_event_->send();
    }

    EventBase* pop_event() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (!high_event_queue_.empty()) {
            auto ev = high_event_queue_.front();
            high_event_queue_.pop();
            return ev;
        }

        if (event_queue_.empty()) return nullptr;

        auto ev = event_queue_.front();
        event_queue_.pop();
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
    std::queue<EventBase*> event_queue_;
    std::queue<EventBase*> high_event_queue_;

    void create_state(State* parent, STATE_ID child) {
        assert(states_.count(child) == 0);

        states_[child] = new State(to_string(child), parent);
    }

    State* id_to_state(STATE_ID st) {
        assert(states_.count(st) == 1);
        return states_[st];
    }
};

}  // seeds
