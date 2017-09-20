
#include <ev++.h>
#include <list>
#include <string>
#include <map>
#include <cassert>
#include <mutex>
#include <queue>
#include <memory>

#include "event.h"
#include "log.h"

namespace seeds {

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
        if (parent_) parent_->enter_child(event, this);

        log("enter state: %s", name_.c_str());
        is_active_ = true;

        do_enter_callback(event);

        if (!children_.empty()) {
            active_child_ = children_.front();
            active_child_->enter(event);
        }
    }

    void exit(EventBase* event) {
        if (active_child_) {
            active_child_->exit(event);
            active_child_ = nullptr;
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
    State* active_child_ = nullptr;

    std::list<std::function<void()>> on_entered_callbacks_;
    std::list<std::function<void()>> on_exited_callbacks_;

};

}  // namespace seeds
