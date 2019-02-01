
#include <iostream>

#include "seedsm.h"
#include "policy.h"

using namespace seedsm;

using ST = Policy::STATE;
using EV = Policy::EVENT;

struct MyStateMachine : public StateMachine<Policy> {
    MyStateMachine()
        : StateMachine("Root") {
        create_states({ST::INIT, ST::ON, ST::OFF, ST::FIN});

        add_transition<EV::INIT_COMP>(ST::INIT, ST::OFF);
        add_transition<EV::TOGGLE>(ST::OFF, ST::ON);
        add_transition<EV::TOGGLE>(ST::ON, ST::OFF);
        add_transition<EV::END>(ST::ON, ST::FIN);
        add_transition<EV::END>(ST::OFF, ST::FIN);

        on_state_entered(ST::INIT, [this] { send_high<EV::INIT_COMP>(); });
        on_state_entered(ST::FIN, [this] { std::cout << "ST::FIN" << std::endl; });

        on_transition<EV::TOGGLE>(ST::ON, [this](const std::string& ev_msg) {
            std::cout << "ST::ON receive EV::TOGGLE: msg = " << ev_msg
                      << std::endl;
        });
    }
};

int main(int argc, char* argv[]) {
    MyStateMachine sm{};

    sm.start();

    sm.send<EV::TOGGLE>("toggle1");
    sm.send<EV::TOGGLE>("toggle2");
    sm.send<EV::TOGGLE>("toggle3");
    sm.send<EV::END>();

    return 0;
}
