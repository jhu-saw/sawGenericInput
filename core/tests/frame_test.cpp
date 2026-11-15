#include <sawGenericInput/mtsEvdev.h>
#include <cisstMultiTask/mtsManagerLocal.h>
#include <cisstMultiTask/mtsInterfaceRequired.h>
#include <linux/input-event-codes.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <cstdio>
#include <unistd.h>

static void Check(bool ok, const char * message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

class Device : public mtsEvdev {
public:

    Device(void) : mtsEvdev("InputDevice") {
    }

    using mtsEvdev::HandleEvent;
    using mtsEvdev::BeginSynchronization;
    using mtsEvdev::CompleteSynchronization;
    using mtsEvdev::InvalidateInput;
    using mtsEvdev::ReleaseAll;

    void Frame(void) {
        StateTable.Start();
        HandleEvent(EV_SYN, SYN_REPORT, 0, 0);
        StateTable.Advance();
    }

};
class Listener : public mtsComponent {
public:
    std::vector<prmInputData> Frames;
    std::vector<prmEventButton::EventType> Buttons;
    mtsFunctionRead Read;

    Listener(void) : mtsComponent("Listener") {
        auto * input = AddInterfaceRequired("Input");
        input->AddEventHandlerWrite(&Listener::OnData, this, "input_data", MTS_EVENT_NOT_QUEUED);
        input->AddFunction("input_data", Read);
        AddInterfaceRequired("Clutch")->AddEventHandlerWrite(&Listener::OnButton, this, "Button", MTS_EVENT_NOT_QUEUED);
    }

    void OnData(const prmInputData & data) {
        Frames.push_back(data);
    }

    void OnButton(const prmEventButton & data) {
        Buttons.push_back(data.Type());
    }

};

int main(int argc, char ** argv) {
    try {
        Check(argc == 2, "expected joystick config");
        const auto config = sawGenericInput::LoadConfiguration(argv[1]);
        Check(config.Axes.size() == 2 && config.Buttons.size() == 2, "config ordering");
        const auto & calibrated = config.Axes[1];
        Check(calibrated.Convert(-32768) == -32767 && calibrated.Convert(32767) == 32767
              && calibrated.Convert(0) == 0 && config.Axes[0].Convert(123) == 123, "raw/calibrated ranges");
        auto axis = calibrated;
        axis.Minimum = -100;
        axis.Center = 10;
        axis.Maximum = 210;
        axis.Deadzone = 10;
        axis.OutputMinimum = -1;
        axis.OutputMaximum = 1;
        Check(axis.Convert(-200) == -1 && axis.Convert(220) == 1 && axis.Convert(0) == 0
              && axis.Convert(20) == 0 && std::abs(axis.Convert(115) - .5) < 1e-10
              && std::abs(axis.Convert(-50) + .5) < 1e-10, "asymmetric calibration and deadzone");
        axis.OutputMinimum = 1;
        axis.OutputMaximum = -1;
        Check(axis.Convert(-100) == 1 && axis.Convert(210) == -1, "explicit axis inversion");
        Device device;
        device.Configure(argv[1]);
        Listener listener;
        auto * manager = mtsManagerLocal::GetInstance();
        Check(manager->AddComponent(&device) && manager->AddComponent(&listener), "add components");
        Check(manager->Connect("Listener", "Input", "InputDevice", "Input")
              && manager->Connect("Listener", "Clutch", "InputDevice", "Clutch"), "connect input");
        device.HandleEvent(EV_ABS, ABS_Y, 32767, 1);
        device.HandleEvent(EV_ABS, ABS_X, 1234, 2);
        device.HandleEvent(EV_KEY, BTN_THUMB, 1, 3);
        device.HandleEvent(EV_KEY, BTN_TRIGGER, 1, 4);
        Check(listener.Frames.empty() && listener.Buttons.size() == 1, "buttons immediate, aggregate waits");
        device.Frame();
        Check(listener.Frames.size() == 1, "one snapshot per changed report");
        const auto & data = listener.Frames.back();
        Check(data.Valid() && data.AnalogInputs().at(0) == 1234 && data.AnalogInputs().at(1) == 32767
              && data.DigitalInputs().at(0) && data.DigitalInputs().at(1), "ordered coherent snapshot");
        device.HandleEvent(EV_ABS, ABS_X, 9999, 5);
        prmInputData read;
        listener.Read(read);
        Check(read.AnalogInputs().at(0) == 1234, "read cannot observe partial frame");
        device.Frame();
        Check(listener.Frames.back().AnalogInputs().at(0) == 9999, "second report");
        device.HandleEvent(EV_KEY, BTN_TRIGGER, 2, 6);
        device.Frame();
        Check(listener.Frames.size() == 2 && listener.Buttons.size() == 1, "repeat ignored");
        device.HandleEvent(EV_ABS, ABS_X, 77, 7);
        device.BeginSynchronization();
        Check(!listener.Frames.back().Valid() && !listener.Frames.back().DigitalInputs().at(0)
              && listener.Buttons.back() == prmEventButton::RELEASED, "overflow invalidates and releases");
        device.HandleEvent(EV_ABS, ABS_X, 555, 8);
        device.Frame();
        Check(!listener.Frames.back().Valid(), "ignore sync deltas");
        device.CompleteSynchronization();
        Check(listener.Frames.back().Valid() && listener.Frames.back().AnalogInputs().at(0) == 0,
              "discard unfinished frame on recovery");
        const auto beforeTap = listener.Buttons.size();
        device.HandleEvent(EV_KEY, BTN_TRIGGER, 1, 100);
        device.HandleEvent(EV_KEY, BTN_TRIGGER, 0, 101);
        Check(listener.Buttons.size() == beforeTap + 2, "preserve transitions within one report");
        device.Frame();
        Check(!listener.Frames.back().DigitalInputs().at(0), "aggregate contains final report state");
        device.CloseDevice();
        Check(!listener.Frames.back().Valid(), "disconnect invalidates input");
        const std::string temp = "/tmp/sawGenericInput-axes-" + std::to_string(getpid()) + ".json";
        for (const auto & bad : {
            R"({"axes":[{"code":"ABS_NOT_REAL"}]})",
            R"({"axes":[{"code":"ABS_MT_POSITION_X"}]})",
            R"({"axes":[{"code":"ABS_X"},{"code":0}]})",
            R"({"axes":[{"code":"ABS_X","calibration":{"minimum":0,"center":0,"maximum":1}}]})",
            R"({"axes":[{"code":"ABS_X","calibration":{"minimum":-1,"center":0,"maximum":1,"deadzone":1}}]})",
            R"({"axes":null})",
            R"({"buttons":[{"key":"KEY_A","interface":"Input"}]})"
        }) {
            std::string json = bad;
            json.insert(1, "\"device\":\"/dev/input/event0\",");
            {
                std::ofstream out(temp);
                out << json;
            }
            bool rejected = false;
            try {
                sawGenericInput::LoadConfiguration(temp);
            }
            catch (const std::exception &) {
                rejected = true;
            }
            std::remove(temp.c_str());
            Check(rejected, "invalid axes accepted");
        }
        {
            std::ofstream out(temp);
            out << R"({"device":"/dev/input/event0","axes":[{"code":"ABS_X"}]})";
        }
        const auto axisOnly = sawGenericInput::LoadConfiguration(temp);
        Check(axisOnly.Buttons.empty() && axisOnly.Axes.size() == 1, "axis-only configuration");
        std::remove(temp.c_str());
        manager->Disconnect("Listener", "Input", "InputDevice", "Input");
        manager->Disconnect("Listener", "Clutch", "InputDevice", "Clutch");
        manager->RemoveComponent("Listener");
        manager->RemoveComponent("InputDevice");
        std::cout << "Axis calibration, frame snapshots, recovery and validation passed\n";
    }
    catch (const std::exception & error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
