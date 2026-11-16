#ifndef _mtsEvdev_h
#define _mtsEvdev_h
#include <cisstMultiTask/mtsTaskPeriodic.h>
#include <cisstMultiTask/mtsFunctionWrite.h>
#include <cisstMultiTask/mtsGenericObjectProxy.h>
#include <sawGenericInput/ButtonState.h>
#include <sawGenericInput/Configuration.h>
#include <chrono>
#include <cisstParameterTypes/prmInputData.h>
#include <map>
#include <memory>
#include <sawGenericInput/sawGenericInputExport.h> // Always include last.

struct libevdev;
class CISST_EXPORT mtsEvdev : public mtsTaskPeriodic {
    CMN_DECLARE_SERVICES(CMN_DYNAMIC_CREATION_ONEARG, CMN_LOG_ALLOW_DEFAULT)
public:
    explicit mtsEvdev(const std::string & name = "Evdev", double period = 0.002);
    explicit mtsEvdev(const mtsTaskPeriodicConstructorArg & arg);
    ~mtsEvdev(void) override;
    void Configure(const std::string & filename = "") override;
    void Configure(const sawGenericInput::Configuration & config);
    void Startup(void) override;
    void Run(void) override;
    void Cleanup(void) override;
    // A path override set before Configure wins over the JSON device selector.
    void SetDevice(const std::string & path);
    void OpenDevice(void);
    void CloseDevice(void);
protected:
    // Kept independent of device IO to test the actual provided input events.
    void HandleKey(unsigned int code, int value, std::uint64_t time);
    void ReleaseAll(void);
    void HandleAbsolute(unsigned int code, int value);
    void HandleReport(void);
    void InvalidateInput(void);
    void BeginSynchronization(void);
    void CompleteSynchronization(void);
    void HandleEvent(unsigned int type, unsigned int code, int value, std::uint64_t time);
private:
    struct Button {
        sawGenericInput::ButtonState State;
        std::vector<mtsFunctionWrite> Events;
        size_t Index = 0;
    };
    void Init(void);
    void Open(void);
    void Close(bool notify = true);
    void InhibitHeld(void);
    void RefreshAxes(void);
    void PublishInput(bool valid);
    void Emit(Button & button, prmEventButton::EventType type);
    void Error(const std::string & message);
    sawGenericInput::Configuration mConfig;
    std::map<unsigned int, std::unique_ptr<Button>> mButtons;
    mtsInterfaceProvided * mStatus = nullptr;
    mtsBool mConnected;
    prmInputData mInputData, mPendingData;
    mtsFunctionWrite mInputEvent;
    std::string mPathOverride;
    bool mFrameDirty = false;
    bool mEnabled = true;
    bool mConfigured = false;
    bool mSync = false;
    int mFd = -1;
    libevdev * mDevice = nullptr;
    std::string mLastError;
    std::chrono::steady_clock::time_point mNextOpen;
};
CMN_DECLARE_SERVICES_INSTANTIATION(mtsEvdev);
#endif
