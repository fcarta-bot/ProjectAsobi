#pragma once
#include <memory>
#include <string>
#include <vector>

// Ricerca e abbinamento di nuovi controller direttamente dal gioco.
// Ogni piattaforma ha la sua implementazione: Windows (ControllerLink_Windows.cpp) ora,
// Android in futuro. Dove il sistema non lo permette (console, iOS) si usa
// ControllerLink_Unsupported.cpp e il gioco offre solo cavo e pennetta.

struct NearbyController {
    std::string id;        // identificativo del sistema operativo
    std::string name;
    bool lowEnergy;        // Bluetooth LE (es. Xbox Series) oppure classico (PlayStation, Switch Pro)
    bool looksLikeGamepad; // dal nome sembra un controller: viene mostrato in cima alla lista
};

enum class LinkStatus { Idle, Scanning, Pairing, Paired, Failed };

class ControllerLink {
public:
    virtual ~ControllerLink() = default;
    static std::unique_ptr<ControllerLink> Create(); // sceglie l'implementazione della piattaforma

    virtual bool PairingSupported() const = 0; // la piattaforma permette di abbinare dal gioco
    virtual bool BluetoothAvailable() const = 0; // c'e' un adattatore Bluetooth acceso
    virtual void StartScan() = 0;
    virtual void Pair(int foundIndex) = 0;
    virtual void Cancel() = 0;
    virtual void Update() = 0; // da chiamare ogni frame: fa avanzare le operazioni in corso

    LinkStatus Status() const { return status; }
    const std::vector<NearbyController>& Found() const { return found; }
    const std::string& Message() const { return message; }

protected:
    LinkStatus status = LinkStatus::Idle;
    std::vector<NearbyController> found;
    std::string message;
};
