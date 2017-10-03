
#include <iostream>
#include <future>
#include <ev++.h>
#include <signal.h>

#include "seedsm.h"
#include "policy.h"

using namespace seedsm;

using ST = Policy::STATE;
using EV = Policy::EVENT;

struct MyStateMachine : public StateMachine<Policy> {
    MyStateMachine(ev::loop_ref loop)
        : StateMachine("Root", loop) {
        create_states({ST::INIT, ST::ON, ST::OFF, ST::FIN});

        add_transition<EV::INIT_COMP>(ST::INIT, ST::OFF);
        add_transition<EV::TOGGLE>(ST::OFF, ST::ON);
        add_transition<EV::TOGGLE>(ST::ON, ST::OFF);
        add_transition<EV::END>(ST::ON, ST::FIN);
        add_transition<EV::END>(ST::OFF, ST::FIN);

        on_state_entered(ST::INIT, [this] { send_high<EV::INIT_COMP>(); });
        on_state_entered(ST::FIN, [this] { stop(); });

        on_transition<EV::TOGGLE>(ST::ON, [this](const std::string& ev_msg) {
            std::cout << "ST::ON receive EV::TOGGLE: msg = " << ev_msg
                      << std::endl;
        });
    }
};

int main(int argc, char* argv[]) {
    ev::dynamic_loop main_loop;

    MyStateMachine sm(main_loop);

    sm.start();

    sm.send<EV::TOGGLE>("toggle1");
    sm.send<EV::TOGGLE>("toggle2");
    sm.send<EV::TOGGLE>("toggle3");
    sm.send<EV::END>();

    main_loop.run(0);

    return 0;
}
