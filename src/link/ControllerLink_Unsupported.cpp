// Piattaforme dove il gioco non puo' abbinare controller da solo (console, iOS, ...):
// li abbina il sistema e il gioco li rileva quando compaiono.
#ifndef _WIN32
#include "ControllerLink.hpp"

class UnsupportedLink : public ControllerLink {
public:
    UnsupportedLink() { message = "Su questo dispositivo i controller si abbinano dalle impostazioni di sistema"; }
    bool PairingSupported() const override { return false; }
    bool BluetoothAvailable() const override { return false; }
    void StartScan() override {}
    void Pair(int) override {}
    void Cancel() override {}
    void Update() override {}
};

std::unique_ptr<ControllerLink> ControllerLink::Create() {
    return std::make_unique<UnsupportedLink>();
}
#endif
