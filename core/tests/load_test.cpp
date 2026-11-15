#include <cisstMultiTask/mtsManagerLocal.h>
#include <cisstMultiTask/mtsComponent.h>
#include <cisstCommon/cmnPath.h>
#include <json/json.h>
#include <iostream>

int main(int argc, char ** argv) {
    if (argc != 3) {
        return 1;
    }
    Json::Value configuration;
    auto & component = configuration["components"][0];
    component["shared-library"] = argv[1];
    component["class-name"] = "mtsEvdev";
    component["constructor-arg"]["Name"] = "Evdev";
    component["constructor-arg"]["Period"] = 0.002;
    component["configure-parameter"] = argv[2];
    auto * manager = mtsManagerLocal::GetInstance();
    cmnPath path;
    if (!manager->ConfigureJSON(configuration, path)) {
        return 1;
    }
    auto * device = manager->GetComponent("Evdev");
    if (!device || !device->GetInterfaceProvided("Clutch")
        || !device->GetInterfaceProvided("Camera")
        || !device->GetInterfaceProvided("OperatorPresent")
        || !device->GetInterfaceProvided("Device")
        || !device->GetInterfaceProvided("Input")) {
        return 1;
    }
    manager->RemoveComponent("Evdev");
    delete device;
    std::cout << "Dynamic library loading and JSON constructor/configuration passed\n";
}
