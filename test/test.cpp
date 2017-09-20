
#include "statemachine.h"
#include "gtest/gtest.h"

#include <ev++.h>

template <typename T>
std::string to_string(T st) {
    return std::to_string(static_cast<int>(st));
}

struct Policy1 {
    enum state_id_t { A, B, C};
    enum event_id_t { TO_A, TO_B, TO_C };
};

DEFINE_EVENT(Policy1::TO_A);
DEFINE_EVENT(Policy1::TO_B);
DEFINE_EVENT(Policy1::TO_C);

namespace {

using ST = Policy1::state_id_t;
using EV = Policy1::event_id_t;

struct SM1 : public seeds::StateMachine<Policy1> {
    SM1(ev::loop_ref loop)
        : StateMachine("Root", loop) {
        create_states({ST::A, ST::B, ST::C});
        add_transition<EV::TO_A>(ST::A);
        add_transition<EV::TO_B>(ST::A, ST::B);
        add_transition<EV::TO_B>(ST::B, ST::B);
        add_transition<EV::TO_C>(ST::B, ST::C);

        on_state_entered(ST::C, [this] { stop(); });
        on_transition<EV::TO_A>(ST::A, [this](EV) { a_recv_to_a = true; });
        on_transition<EV::TO_B>(ST::B, [this](EV) { send<EV::TO_C>(); });
        on_state_exited(ST::B, [this] { exit_b_cnt++; });
    }

    bool a_recv_to_a = false;
    int exit_b_cnt = 0;
};

class Test : public testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(Test, Test1) {
    ev::dynamic_loop loop;
    SM1 sm(loop);

    sm.start();
    sm.send<EV::TO_A>();
    sm.send<EV::TO_B>();
    sm.send<EV::TO_B>();

    loop.run(0);

    EXPECT_EQ(true, sm.a_recv_to_a);
    EXPECT_EQ(2, sm.exit_b_cnt);
}
}
