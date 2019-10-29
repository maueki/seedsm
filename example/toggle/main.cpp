
#include <iostream>
#include <future>

#include "seedsm.h"
#include "policy.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

using namespace seedsm;

using ST = Policy::STATE;
using EV = Policy::EVENT;

struct MyStateMachine : public StateMachine<Policy> {
    MyStateMachine(QCoreApplication& app)
        : StateMachine("Root") {
        create_states({ST::INIT, ST::ON, ST::OFF, ST::FIN});

        add_transition<EV::INIT_COMP>(ST::INIT, ST::OFF);
        add_transition<EV::TOGGLE>(ST::OFF, ST::ON);
        add_transition<EV::TOGGLE>(ST::ON, ST::OFF);
        add_transition<EV::END>(ST::ON, ST::FIN);
        add_transition<EV::END>(ST::OFF, ST::FIN);

        on_state_entered(ST::INIT,
                         [this] {
                             send_high<EV::INIT_COMP>();
                         });
        on_state_entered(ST::FIN, [this, &app] { app.quit(); });

        on_transition<EV::TOGGLE>(ST::ON, [this](const std::string& ev_msg) {
            std::cout << "ST::ON receive EV::TOGGLE: msg = " << ev_msg
                      << std::endl;
        });

        start();
    }
};

int main(int argc, char* argv[]) {

    QCoreApplication app(argc, argv);

    MyStateMachine sm(app);

    QTimer::singleShot(0, [&sm] {
                              sm.send<EV::TOGGLE>("toggle1");
                              sm.send<EV::TOGGLE>("toggle2");
                              sm.send<EV::TOGGLE>("toggle3");
                              sm.send<EV::END>();
                          });

    app.exec();

    return 0;
}
