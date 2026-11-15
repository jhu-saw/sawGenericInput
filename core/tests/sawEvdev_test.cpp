#include <sawGenericInput/mtsEvdev.h>
#include <cisstMultiTask/mtsManagerLocal.h>
#include <cisstMultiTask/mtsInterfaceRequired.h>
#include <linux/input-event-codes.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

void Check(bool condition, const std::string & message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

using Events = std::vector<prmEventButton::EventType>;

class TestDevice : public mtsEvdev {
public:

    TestDevice(void) : mtsEvdev("Evdev") {
    }

    using mtsEvdev::HandleKey;
    using mtsEvdev::HandleEvent;
    using mtsEvdev::BeginSynchronization;
    using mtsEvdev::CompleteSynchronization;
    using mtsEvdev::ReleaseAll;

    void Frame(void) {
        StateTable.Start();
        HandleEvent(EV_SYN, SYN_REPORT, 0, 0);
        StateTable.Advance();
    }

};
class Listener : public mtsComponent {
public:
    Events Clutch, Camera, Operator;

    Listener(void) : mtsComponent("Listener") {
        AddInterfaceRequired("Clutch")->AddEventHandlerWrite(&Listener::OnClutch, this, "Button", MTS_EVENT_NOT_QUEUED);
        AddInterfaceRequired("Camera")->AddEventHandlerWrite(&Listener::OnCamera, this, "Button", MTS_EVENT_NOT_QUEUED);
        AddInterfaceRequired("Operator")->AddEventHandlerWrite(&Listener::OnOperator, this, "Button", MTS_EVENT_NOT_QUEUED);
    }

    void OnClutch(const prmEventButton & event) {
        Check(event.Valid(), "invalid event");
        Clutch.push_back(event.Type());
    }

    void OnCamera(const prmEventButton & event) {
        Camera.push_back(event.Type());
    }

    void OnOperator(const prmEventButton & event) {
        Operator.push_back(event.Type());
    }

};

class FanoutListener : public mtsComponent {
public:
    std::vector<prmEventButton> Primary, Secondary;
    std::vector<prmInputData> Frames;

    FanoutListener(void) : mtsComponent("FanoutListener") {
        AddInterfaceRequired("Primary")->AddEventHandlerWrite(&FanoutListener::OnPrimary, this, "Button", MTS_EVENT_NOT_QUEUED);
        AddInterfaceRequired("Secondary")->AddEventHandlerWrite(&FanoutListener::OnSecondary, this, "Button", MTS_EVENT_NOT_QUEUED);
        AddInterfaceRequired("Input")->AddEventHandlerWrite(&FanoutListener::OnInput, this, "input_data", MTS_EVENT_NOT_QUEUED);
    }

    void OnPrimary(const prmEventButton & event) {
        Primary.push_back(event);
    }

    void OnSecondary(const prmEventButton & event) {
        Secondary.push_back(event);
    }

    void OnInput(const prmInputData & data) {
        Frames.push_back(data);
    }

};

void TestFanout(const std::string & filename) {
    {
        std::ofstream file(filename);
        file << R"({"device":"/dev/input/event0","buttons":[
            {"key":"KEY_A","interfaces":["Primary","Secondary"],"quick_tap_ms":300},
            {"key":"KEY_B"}]})";
    }
    const auto config = sawGenericInput::LoadConfiguration(filename);
    Check(config.Buttons.size() == 2
          && config.Buttons[0].Interfaces == std::vector<std::string>({"Primary", "Secondary"})
          && config.Buttons[1].Interfaces.empty(), "named and aggregate-only button mappings");
    TestDevice device;
    device.Configure(filename);
    unlink(filename.c_str());
    FanoutListener listener;
    auto * manager = mtsManagerLocal::GetInstance();
    Check(manager->AddComponent(&device) && manager->AddComponent(&listener), "add fanout components");
    for (const auto & name : {"Primary", "Secondary", "Input"}) {
        Check(manager->Connect("FanoutListener", name, "Evdev", name), "connect fanout interface");
    }
    device.HandleKey(KEY_A, 1, 1000);
    device.HandleKey(KEY_A, 1, 2000);
    device.HandleKey(KEY_A, 2, 3000);
    device.HandleKey(KEY_B, 1, 4000);
    Check(listener.Primary.size() == 1 && listener.Secondary.size() == 1 && listener.Frames.empty(),
          "fanout ignores duplicate presses and repeats, aggregate waits for report");
    device.Frame();
    const auto & pressed = listener.Frames.back().DigitalInputs();
    Check(pressed.size() == 2 && pressed.at(0) && pressed.at(1), "one digital slot per physical button");
    device.HandleKey(KEY_A, 0, 101000);
    Check(listener.Primary.size() == 3
          && listener.Primary[0].Type() == prmEventButton::PRESSED
          && listener.Primary[1].Type() == prmEventButton::RELEASED
          && listener.Primary[2].Type() == prmEventButton::CLICKED, "fanout quick-tap event order");
    device.Frame();
    Check(!listener.Frames.back().DigitalInputs().at(0) && listener.Frames.back().DigitalInputs().at(1),
          "fanout keeps aggregate-only button independent");
    device.HandleKey(KEY_A, 1, 200000);
    device.BeginSynchronization();
    Check(!listener.Frames.back().Valid() && listener.Primary.size() == 5
          && listener.Primary.back().Type() == prmEventButton::RELEASED,
          "recovery releases every interface without clicking");
    device.HandleEvent(EV_KEY, KEY_A, 1, 210000);
    Check(listener.Primary.size() == 5 && listener.Secondary.size() == 5, "ignore input during synchronization");
    device.CompleteSynchronization();
    Check(listener.Frames.back().Valid(), "fanout recovers valid aggregate state");
    device.HandleKey(KEY_A, 0, 220000);
    Check(listener.Primary.size() == 5, "recovery does not emit a click for an unmatched release");
    device.HandleKey(KEY_A, 1, 300000);
    device.CloseDevice();
    Check(!listener.Frames.back().Valid() && listener.Primary.size() == 7
          && listener.Primary.back().Type() == prmEventButton::RELEASED, "close releases every interface");
    Check(listener.Primary.size() == listener.Secondary.size(), "same number of events on every interface");
    for (size_t index = 0; index < listener.Primary.size(); ++index) {
        const auto & first = listener.Primary[index];
        const auto & second = listener.Secondary[index];
        Check(first.Valid() && second.Valid() && first.Type() == second.Type()
              && first.Timestamp() == second.Timestamp(), "identical fanout payloads and timestamps");
    }
    manager->Disconnect("FanoutListener", "Secondary", "Evdev", "Secondary");
    device.HandleKey(KEY_A, 1, 400000);
    Check(listener.Primary.size() == 8 && listener.Secondary.size() == 7,
          "unconnected output does not suppress connected output");
    manager->Disconnect("FanoutListener", "Primary", "Evdev", "Primary");
    manager->Disconnect("FanoutListener", "Input", "Evdev", "Input");
    manager->RemoveComponent("FanoutListener");
    manager->RemoveComponent("Evdev");
}

int main(int argc, char ** argv) {
    try {
        Check(argc == 2, "expected sample config path");
        sawGenericInput::ButtonState state;
        state.QuickTapMs = 300;
        Check(state.Process(0, 0).empty(), "unmatched release");
        Check(state.Process(1, 1000) == Events{prmEventButton::PRESSED}, "press");
        Check(state.Process(1, 2000).empty(), "duplicate press");
        Check(state.Process(2, 200000).empty(), "repeat");
        Check(state.Process(0, 301000) == Events({prmEventButton::RELEASED, prmEventButton::CLICKED}), "tap threshold and ordering");
        state.Process(1, 1000000);
        Check(state.Process(0, 1300001) == Events{prmEventButton::RELEASED}, "hold must not click");
        state.Process(1, 2000000);
        Check(state.Process(0, 1000000) == Events{prmEventButton::RELEASED}, "backward clock must not click");
        state.QuickTapMs = 0;
        state.Process(1, 3000000);
        Check(state.Process(0, 3000001) == Events{prmEventButton::RELEASED}, "click disabled");
        state.Reset(true);
        Check(state.Process(1, 4000000).empty() && state.Process(2, 4100000).empty(), "held at connect must be inhibited");
        Check(state.Process(0, 4200000).empty(), "inhibited release");
        Check(state.Process(1, 4300000) == Events{prmEventButton::PRESSED}, "release must rearm");
        state.Reset();
        Check(state.Process(0, 4400000).empty(), "disconnect reset must not click");

        const auto config = sawGenericInput::LoadConfiguration(argv[1]);
        Check(config.Exclusive && config.Vendor == 0x3553 && config.Product == 0xb001, "device config");
        Check(config.Buttons.size() == 3 && config.Buttons[0].Code == KEY_A && config.Buttons[1].Code == KEY_B
              && config.Buttons[2].Code == KEY_C, "key names");
        Check(config.Buttons[0].Interfaces == std::vector<std::string>{"Clutch"}, "single interface remains supported");
        const std::string temp = "/tmp/sawEvdev-config-" + std::to_string(getpid()) + ".json";
        const std::vector<std::string> invalid = {
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interface":"Primary","interfaces":["Secondary"]}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":[]}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":null}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":"Primary"}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":["Primary",42]}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":[""]}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":["Primary","Input"]}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":["Device"]}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":["Primary","Primary"]}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interfaces":["Primary","Secondary"]},{"key":"KEY_B","interface":"Secondary"}]})",
            R"({"device":"/dev/input/event0","buttons":[]})",
            R"({"device":"relative","buttons":[{"key":"KEY_A","interface":"Clutch"}]})",
            R"({"device":"/dev/input/event0","exclusive":"true","buttons":[{"key":"KEY_A","interface":"Clutch"}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_NOT_A_REAL_KEY","interface":"Clutch"}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interface":"Device"}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interface":"Clutch","quick_tap_ms":-1}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interface":"Clutch"},{"key":30,"interface":"Other"}]})",
            R"({"device":"/dev/input/event0","buttons":[{"key":"KEY_A","interface":"Same"},{"key":"KEY_B","interface":"Same"}]})",
            R"({"device":{"name":"pedal","vendor_id":"oops"},"buttons":[{"key":30,"interface":"Clutch"}]})",
            R"({"device":"/dev/input/event0","typo":true,"buttons":[{"key":30,"interface":"Clutch"}]})",
            R"({"device":"/dev/input/event0","exclusive":true,"exclusive":false,"buttons":[{"key":30,"interface":"Clutch"}]})"
        };
        for (const auto & json : invalid) {
            {
                std::ofstream file(temp);
                file << json;
            }
            bool rejected = false;
            try {
                sawGenericInput::LoadConfiguration(temp);
            }
            catch (const std::exception &) {
                rejected = true;
            }
            unlink(temp.c_str());
            Check(rejected, "invalid configuration accepted: " + json);
        }

        TestDevice device;
        device.Configure(argv[1]);
        Listener listener;
        auto * manager = mtsManagerLocal::GetInstance();
        Check(manager->AddComponent(&device) && manager->AddComponent(&listener), "add components");
        Check(manager->Connect("Listener", "Clutch", "Evdev", "Clutch"), "connect clutch");
        Check(manager->Connect("Listener", "Camera", "Evdev", "Camera"), "connect camera");
        Check(manager->Connect("Listener", "Operator", "Evdev", "OperatorPresent"), "connect operator");
        device.HandleKey(KEY_A, 1, 1000);
        device.HandleKey(KEY_B, 1, 1000);
        device.HandleKey(KEY_C, 1, 1000);
        device.HandleKey(KEY_A, 2, 100000);
        device.HandleKey(KEY_A, 0, 200000);
        device.HandleKey(KEY_C, 0, 200000);
        device.HandleKey(KEY_B, 0, 500000);
        Check(listener.Clutch == Events({prmEventButton::PRESSED, prmEventButton::RELEASED, prmEventButton::CLICKED}), "provided clutch events");
        Check(listener.Camera == Events({prmEventButton::PRESSED, prmEventButton::RELEASED}), "independent camera hold");
        Check(listener.Operator == Events({prmEventButton::PRESSED, prmEventButton::RELEASED}), "operator must not click");
        device.HandleKey(KEY_C, 1, 600000);
        device.ReleaseAll();
        Check(listener.Operator.back() == prmEventButton::RELEASED, "disconnect must release operator");
        device.HandleKey(KEY_A, 0, 610000);
        Check(listener.Clutch.back() == prmEventButton::RELEASED, "reset must not click");
        manager->Disconnect("Listener", "Clutch", "Evdev", "Clutch");
        manager->Disconnect("Listener", "Camera", "Evdev", "Camera");
        manager->Disconnect("Listener", "Operator", "Evdev", "OperatorPresent");
        manager->RemoveComponent("Listener");
        manager->RemoveComponent("Evdev");
        TestFanout(temp);
        std::cout << "Button timing, repeat filtering, recovery, fanout, config validation and cisst connections passed\n";
    }
    catch (const std::exception & error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
