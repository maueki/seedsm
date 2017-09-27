
#include <iostream>
#include <future>
#include <ev++.h>
#include <signal.h>

#include "statemachine.h"
#include "policy.h"

using namespace seedsm;

struct MyStateMachine : public StateMachine<Policy> {
    using ST = Policy::state_id_t;
    using EV = Policy::event_id_t;

    MyStateMachine(ev::loop_ref loop)
        : StateMachine("Root", loop) {
        create_states({ST::INIT, ST::ON, ST::OFF, ST::FIN});

        add_transition<EV::INIT_COMP>(ST::INIT, ST::OFF);
        add_transition<EV::TOGGLE>(ST::OFF, ST::ON);
        add_transition<EV::TOGGLE>(ST::ON, ST::OFF);
        add_transition<EV::END>(ST::ON, ST::FIN);
        add_transition<EV::END>(ST::OFF, ST::FIN);

        on_state_entered(ST::INIT, [this] { send<EV::INIT_COMP>(); });
        on_state_entered(ST::OFF,
                         [] { std::cout << "ST::OFF entered" << std::endl; });
        on_state_entered(ST::ON,
                         [] { std::cout << "ST::ON entered" << std::endl; });
        on_state_entered(ST::FIN, [this] { stop(); });

        on_transition<EV::TOGGLE>(ST::ON,
                                  [this](const std::string& ev_msg) {
            std::cout << "ST::ON receive EV::TOGGLE: msg = " << ev_msg.c_str()
                      << std::endl;
        });
    }
};

int main(int argc, char* argv[]) {
    ev::dynamic_loop main_loop;

    MyStateMachine sm(main_loop);

    sm.start();

    auto f = std::async(std::launch::async, [&sm] {
        using EV = Policy::event_id_t;
        using namespace std::chrono_literals;

        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(500ms);
            sm.send<EV::TOGGLE>("toggle");
        }

        sm.send<EV::END>();
    });

    main_loop.run(0);

    return 0;
}
