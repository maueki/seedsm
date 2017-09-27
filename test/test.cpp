
#include "statemachine.h"
#include "gtest/gtest.h"

#include <ev++.h>

#include <string>

#include "util.h"

struct Policy1 {
    enum state_id_t { A, B, C };
    enum event_id_t { TO_A, TO_B, TO_C };
};

DEFINE_EVENT(Policy1::TO_A);
DEFINE_EVENT(Policy1::TO_B);
DEFINE_EVENT_WITH_DATA(Policy1::TO_C, std::string);

namespace {
class Test : public testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};
}

namespace {

struct SM1 : public seeds::StateMachine<Policy1> {
    using ST = Policy1::state_id_t;
    using EV = Policy1::event_id_t;

    SM1(ev::loop_ref loop)
        : StateMachine("Root", loop) {
        create_states({ST::A, ST::B, ST::C});
        add_transition<EV::TO_A>(ST::A);
        add_transition<EV::TO_B>(ST::A, ST::B);
        add_transition<EV::TO_B>(ST::B, ST::B);
        add_transition<EV::TO_C>(ST::B, ST::C);

        on_state_entered(ST::C, [this] { stop(); });
        on_transition<EV::TO_A>(ST::A, [this] { a_recv_to_a = true; });
        on_transition<EV::TO_B>(ST::B, [this] { send<EV::TO_C>("msg"); });
        on_transition<EV::TO_C>(
            ST::B, [this](const std::string& msg) { to_c_msg = msg; });
        on_state_exited(ST::B, [this] { exit_b_cnt++; });
    }

    bool a_recv_to_a = false;
    int exit_b_cnt = 0;
    std::string to_c_msg = "";
};

TEST_F(Test, Test1) {
    using ST = Policy1::state_id_t;
    using EV = Policy1::event_id_t;

    ev::dynamic_loop loop;
    SM1 sm(loop);

    sm.start();
    sm.send<EV::TO_A>();
    sm.send<EV::TO_B>();
    sm.send<EV::TO_B>();

    loop.run(0);

    EXPECT_EQ(true, sm.a_recv_to_a);
    EXPECT_EQ(2, sm.exit_b_cnt);
    EXPECT_EQ("msg", sm.to_c_msg);
}
}

struct PolicyPri {
    enum state_id_t { A, B, C };
    enum event_id_t { TO_B, TO_C };
};

DEFINE_EVENT(PolicyPri::TO_B);
DEFINE_EVENT(PolicyPri::TO_C);

namespace {

struct SMPri : public seeds::StateMachine<PolicyPri> {
    using ST = PolicyPri::state_id_t;
    using EV = PolicyPri::event_id_t;

    SMPri(ev::loop_ref loop)
        : StateMachine("Root", loop) {
        create_states({ST::A, ST::B, ST::C});
        add_transition<EV::TO_B>(ST::A, ST::B);
        add_transition<EV::TO_C>(ST::A, ST::C);
        add_transition<EV::TO_C>(ST::B, ST::C);
        add_transition<EV::TO_B>(ST::C, ST::B);

        on_state_entered(ST::C, [this] { enter_c = true; });
        on_state_entered(ST::B, [this] { stop(); });
    }

    bool enter_c = false;
};

TEST_F(Test, TestPriority) {
    using ST = PolicyPri::state_id_t;
    using EV = PolicyPri::event_id_t;

    ev::dynamic_loop loop;
    SMPri sm(loop);

    sm.start();
    sm.send<EV::TO_B>();
    sm.send_high<EV::TO_C>();

    loop.run(0);

    EXPECT_EQ(true, sm.enter_c);
}
}

struct PolicyPar {
    enum state_id_t { A, A1, A2, B, B1, B2, C };
    enum event_id_t { TO_A, TO_B, TO_C };
};

DEFINE_EVENT(PolicyPar::TO_A);
DEFINE_EVENT(PolicyPar::TO_B);
DEFINE_EVENT(PolicyPar::TO_C);

namespace {

struct SMPar : public seeds::StateMachine<PolicyPar> {
    using ST = PolicyPar::state_id_t;
    using EV = PolicyPar::event_id_t;

    SMPar(ev::loop_ref loop)
        : StateMachine("Root", loop) {
        create_states({ST::A, ST::B, ST::C});

        create_states(ST::A, {ST::A1, ST::A2});
        set_parallel(ST::A, true);

        create_states(ST::B, {ST::B1, ST::B2});

        add_transition<EV::TO_B>(ST::A, ST::B);
        add_transition<EV::TO_C>(ST::B, ST::C);
        add_transition<EV::TO_A>(ST::B, ST::A);

        on_state_entered(ST::C, [this] { stop(); });

        for (auto&& st : {ST::A1, ST::A2, ST::B1, ST::B2}) {
            on_state_entered(st, [this, st] { enter_cnt[st]++; });
        }
    }

    std::map<ST, int> enter_cnt = {
        {ST::A1, 0}, {ST::A2, 0}, {ST::B1, 0}, {ST::B2, 0}};
};

TEST_F(Test, TestParallel) {
    using ST = PolicyPar::state_id_t;
    using EV = PolicyPar::event_id_t;

    ev::dynamic_loop loop;
    SMPar sm(loop);

    sm.start();
    sm.send<EV::TO_B>();
    sm.send<EV::TO_A>();
    sm.send<EV::TO_B>();
    sm.send<EV::TO_C>();

    loop.run(0);

    EXPECT_EQ(2, sm.enter_cnt[ST::A1]);
    EXPECT_EQ(2, sm.enter_cnt[ST::A2]);
    EXPECT_EQ(2, sm.enter_cnt[ST::B1]);
    EXPECT_EQ(0, sm.enter_cnt[ST::B2]);
}
}
