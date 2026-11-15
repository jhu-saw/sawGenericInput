#include <sawGenericInput/mtsEvdev.h>
#include <cisstMultiTask/mtsInterfaceProvided.h>
#include <cisstMultiTask/mtsManagerLocal.h>
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <utility>

CMN_IMPLEMENT_SERVICES_DERIVED_ONEARG(mtsEvdev, mtsTaskPeriodic, mtsTaskPeriodicConstructorArg);

namespace {
// Temporary candidates own their resources until selection succeeds.
struct Candidate {
    int Fd = -1;
    libevdev * Device = nullptr;
    std::string Path;

    ~Candidate(void) {
        if (Device) {
            libevdev_free(Device);
        }
        if (Fd >= 0) {
            close(Fd);
        }
    }

};

std::string Failure(const std::string & action, int error) {
    return action + ": " + std::strerror(error);
}

}

mtsEvdev::mtsEvdev(const std::string & name, double period) : mtsTaskPeriodic(name, period, false) {
    Init();
}

mtsEvdev::mtsEvdev(const mtsTaskPeriodicConstructorArg & arg) : mtsTaskPeriodic(arg) {
    Init();
}

mtsEvdev::~mtsEvdev(void) {
    Close(false);
}

void mtsEvdev::Init(void) {
    mConnected = false;
    StateTable.AddData(mConnected, "connected");
    mStatus = AddInterfaceProvided("Device");
    if (!mStatus) {
        throw std::runtime_error("sawGenericInput: cannot create Device interface");
    }
    mStatus->AddMessageEvents();
    mStatus->AddCommandReadState(StateTable, mConnected, "connected");
    mStatus->AddCommandReadState(StateTable, StateTable.PeriodStats, "period_statistics");
    mStatus->AddCommandWrite(&mtsEvdev::SetDevice, this, "set_device", std::string());
    mStatus->AddCommandVoid(&mtsEvdev::OpenDevice, this, "open_device");
    mStatus->AddCommandVoid(&mtsEvdev::CloseDevice, this, "close_device");
    mInputData.SetValid(false);
    StateTable.AddData(mInputData, "input_data");
    auto * input = AddInterfaceProvided("Input");
    if (!input) {
        throw std::runtime_error("sawGenericInput: cannot create Input interface");
    }
    input->AddCommandReadState(StateTable, mInputData, "input_data");
    input->AddEventWrite(mInputEvent, "input_data", mInputData);
}

void mtsEvdev::Configure(const std::string & filename) {
    if (mConfigured) {
        throw std::runtime_error("sawGenericInput: Configure can only be called once");
    }
    mConfig = sawGenericInput::LoadConfiguration(filename);
    if (!mPathOverride.empty()) {
        mConfig.Path = mPathOverride;
        mConfig.Name.clear();
    }
    mPendingData.DigitalInputs().SetSize(mConfig.Buttons.size());
    mPendingData.DigitalInputs().SetAll(false);
    mPendingData.AnalogInputs().SetSize(mConfig.Axes.size());
    mPendingData.AnalogInputs().SetAll(0.0);
    mPendingData.SetValid(false);
    mInputData = mPendingData;
    size_t index = 0;
    for (const auto & config : mConfig.Buttons) {
        auto button = std::make_unique<Button>();
        button->State.QuickTapMs = config.QuickTapMs;
        button->Index = index++;
        button->Events.resize(config.Interfaces.size());
        for (size_t eventIndex = 0; eventIndex < config.Interfaces.size(); ++eventIndex) {
            const auto & name = config.Interfaces[eventIndex];
            auto * provided = AddInterfaceProvided(name);
            if (!provided) {
                throw std::runtime_error("sawGenericInput: cannot create interface " + name);
            }
            button->Events[eventIndex].Bind(provided->AddEventWrite("Button", prmEventButton()));
        }
        mButtons.emplace(config.Code, std::move(button));
    }
    mConfigured = true;
}

void mtsEvdev::Emit(Button & button, prmEventButton::EventType type) {
    prmEventButton event(type);
    event.SetValid(true);
    // cisst timestamps use the manager's relative time, independently of
    // the kernel monotonic timestamps used to measure tap duration.
    event.SetTimestamp(mtsManagerLocal::GetInstance()->GetTimeServer().GetRelativeTime());
    for (const auto & output : button.Events) {
        if (output.IsValid()) {
            output(event);
        }
    }
}

void mtsEvdev::HandleKey(unsigned int code, int value, std::uint64_t time) {
    const auto found = mButtons.find(code);
    if (found == mButtons.end()) {
        return;
    }
    auto & button = *found->second;
    const bool previous = button.State.Down;
    for (const auto type : button.State.Process(value, time)) Emit(button, type);
    mPendingData.DigitalInputs().at(button.Index) = button.State.Down;
    mFrameDirty = mFrameDirty || previous != button.State.Down;
}

void mtsEvdev::ReleaseAll(void) {
    for (auto & item : mButtons) {
        mPendingData.DigitalInputs().at(item.second->Index) = false;
        item.second->State.Reset();
        Emit(*item.second, prmEventButton::RELEASED);
    }
}

void mtsEvdev::HandleAbsolute(unsigned int code, int value) {
    for (size_t index = 0; index < mConfig.Axes.size(); ++index) {
        const auto & axis = mConfig.Axes[index];
        if (axis.Code == code) {
            const double converted = axis.Convert(value);
            mFrameDirty = mFrameDirty || mPendingData.AnalogInputs().at(index) != converted;
            mPendingData.AnalogInputs().at(index) = converted;
            return;
        }
    }
}

void mtsEvdev::PublishInput(bool valid) {
    mPendingData.SetValid(valid);
    mPendingData.SetTimestamp(mtsManagerLocal::GetInstance()->GetTimeServer().GetRelativeTime());
    mInputData = mPendingData;
    mInputEvent(mInputData);
    mFrameDirty = false;
}

void mtsEvdev::HandleReport(void) {
    if (!mSync && mFrameDirty) {
        PublishInput(true);
    }
}

void mtsEvdev::InvalidateInput(void) {
    mPendingData.DigitalInputs().SetAll(false);
    mPendingData.AnalogInputs().SetAll(0.0);
    PublishInput(false);
}

void mtsEvdev::BeginSynchronization(void) {
    ReleaseAll();
    mSync = true;
    InvalidateInput();
}

void mtsEvdev::CompleteSynchronization(void) {
    if (mDevice) {
        InhibitHeld();
        RefreshAxes();
    }
    mSync = false;
    PublishInput(true);
}

void mtsEvdev::HandleEvent(unsigned int type, unsigned int code, int value, std::uint64_t time) {
    if (mSync) {
        return;
    }
    if (type == EV_KEY) {
        HandleKey(code, value, time);
    } else if (type == EV_ABS) {
        HandleAbsolute(code, value);
    } else if (type == EV_SYN && code == SYN_REPORT) {
        HandleReport();
    }
}

void mtsEvdev::RefreshAxes(void) {
    for (const auto & axis : mConfig.Axes) {
        HandleAbsolute(axis.Code, libevdev_get_event_value(mDevice, EV_ABS, axis.Code));
    }
}

void mtsEvdev::SetDevice(const std::string & path) {
    if (path.empty() || path.front() != '/') {
        Error("device override must be an absolute evdev path");
        return;
    }
    mPathOverride = path;
    if (mConfigured) {
        CloseDevice();
        mConfig.Path = path;
        mConfig.Name.clear();
        OpenDevice();
    }
}

void mtsEvdev::OpenDevice(void) {
    mEnabled = true;
    mNextOpen = std::chrono::steady_clock::now();
}

void mtsEvdev::CloseDevice(void) {
    mEnabled = false;
    ReleaseAll();
    Close();
}

void mtsEvdev::InhibitHeld(void) {
    for (auto & item : mButtons) {
        item.second->State.Reset(libevdev_get_event_value(mDevice, EV_KEY, item.first) != 0);
    }
}

void mtsEvdev::Error(const std::string & message) {
    if (message != mLastError) {
        CMN_LOG_CLASS_RUN_ERROR << message << std::endl;
        mStatus->SendError(message);
        mLastError = message;
    }
}

void mtsEvdev::Open(void) {
    std::vector<std::string> paths;
    if (!mConfig.Path.empty()) {
        paths.push_back(mConfig.Path);
    } else {
        std::error_code error;
        std::filesystem::directory_iterator directory("/dev/input", error);
        if (error) {
            throw std::runtime_error("cannot enumerate /dev/input: " + error.message());
        }
        for (const auto & entry : directory) {
            if (entry.path().filename().string().rfind("event", 0) == 0) {
                paths.push_back(entry.path().string());
            }
        }
    }
    std::unique_ptr<Candidate> selected;
    bool denied = false;
    for (const auto & path : paths) {
        auto candidate = std::make_unique<Candidate>();
        candidate->Path = path;
        candidate->Fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (candidate->Fd < 0) {
            const int error = errno;
            if (!mConfig.Path.empty()) {
                throw std::runtime_error(Failure("open " + path, error));
            }
            denied = denied || error == EACCES || error == EPERM;
            continue;
        }
        const int result = libevdev_new_from_fd(candidate->Fd, &candidate->Device);
        if (result < 0) {
            if (!mConfig.Path.empty()) {
                throw std::runtime_error(Failure("read device " + path, -result));
            }
            continue;
        }
        if (mConfig.Path.empty()) {
            const char * name = libevdev_get_name(candidate->Device);
            if (!name || mConfig.Name != name) {
                continue;
            }
            if (mConfig.Vendor >= 0 && libevdev_get_id_vendor(candidate->Device) != mConfig.Vendor) {
                continue;
            }
            if (mConfig.Product >= 0 && libevdev_get_id_product(candidate->Device) != mConfig.Product) {
                continue;
            }
        }
        bool supportsKeys = true;
        for (const auto & button : mConfig.Buttons) {
            supportsKeys = supportsKeys && libevdev_has_event_code(candidate->Device, EV_KEY, button.Code);
        }
        for (const auto & axis : mConfig.Axes) {
            supportsKeys = supportsKeys && libevdev_has_event_code(candidate->Device, EV_ABS, axis.Code);
        }
        if (!supportsKeys) {
            if (!mConfig.Path.empty()) {
                throw std::runtime_error(path + " does not support every configured key/axis");
            }
            continue;
        }
        if (selected) {
            throw std::runtime_error("multiple matching input devices; configure a stable device path");
        }
        selected = std::move(candidate);
    }
    if (!selected) {
        throw std::runtime_error("no matching input device" + std::string(denied ? "; some input nodes denied access (check udev permissions)" : ""));
    }
    int result = libevdev_set_clock_id(selected->Device, CLOCK_MONOTONIC);
    if (result < 0) {
        throw std::runtime_error(Failure("set monotonic input clock", -result));
    }
    if (mConfig.Exclusive) {
        result = libevdev_grab(selected->Device, LIBEVDEV_GRAB);
        if (result < 0) {
            throw std::runtime_error(Failure("exclusive grab " + selected->Path, -result));
        }
    }
    mFd = std::exchange(selected->Fd, -1);
    mDevice = std::exchange(selected->Device, nullptr);
    mConnected = true;
    mSync = false;
    // Publish a known released state; held pedals must be lifted before reuse.
    ReleaseAll();
    InhibitHeld();
    RefreshAxes();
    PublishInput(true);
    mLastError.clear();
    mStatus->SendStatus("connected to " + selected->Path + (mConfig.Exclusive ? " (exclusive)" : " (shared)"));
}

void mtsEvdev::Close(bool notify) {
    if (mDevice) {
        if (mConfig.Exclusive) {
            libevdev_grab(mDevice, LIBEVDEV_UNGRAB);
        }
        libevdev_free(mDevice);
        mDevice = nullptr;
    }
    if (mFd >= 0) {
        close(mFd);
        mFd = -1;
    }
    mConnected = false;
    mSync = false;
    if (notify) {
        InvalidateInput();
    }
}

void mtsEvdev::Startup(void) {
    ReleaseAll();
    mNextOpen = std::chrono::steady_clock::now();
}

void mtsEvdev::Run(void) {
    ProcessQueuedCommands();
    ProcessQueuedEvents();
    if (!mConfigured || !mEnabled) {
        return;
    }
    if (!mDevice) {
        const auto now = std::chrono::steady_clock::now();
        if (now < mNextOpen) {
            return;
        }
        mNextOpen = now + std::chrono::seconds(1);
        try {
            Open();
        }
        catch (const std::exception & error) {
            Error(error.what());
        }
        if (!mDevice) {
            return;
        }
    }
    // Bound work even if the device is noisy; never block the cisst thread.
    for (unsigned int count = 0; count < 256; ++count) {
        input_event event{};
        const int result = libevdev_next_event(mDevice,
                                               mSync ? LIBEVDEV_READ_FLAG_SYNC : LIBEVDEV_READ_FLAG_NORMAL, &event);
        if (result == -EAGAIN) {
            if (mSync) {
                CompleteSynchronization();
                continue;
            }
            break;
        }
        if (result < 0) {
            if (result == -EINTR) {
                continue;
            }
            ReleaseAll();
            Close();
            mNextOpen = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            Error(Failure("input disconnected/read failed", -result));
            break;
        }
        if (result == LIBEVDEV_READ_STATUS_SYNC) {
            if (!mSync) {
                BeginSynchronization();
                mStatus->SendWarning("input events dropped; resetting buttons and resynchronizing");
            }
            continue;
        }
        const auto time = std::uint64_t(event.time.tv_sec) * 1000000 + event.time.tv_usec;
        HandleEvent(event.type, event.code, event.value, time);
    }
}

void mtsEvdev::Cleanup(void) {
    ReleaseAll();
    Close();
}
