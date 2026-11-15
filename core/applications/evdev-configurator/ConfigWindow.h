/*
(C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty. The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/
#pragma once
#include "CaptureModel.h"
#include <QWidget>
#include <QTimer>
#include <map>
#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
struct libevdev;

namespace sawGenericInputConfigurator {
class ConfiguratorTest;
class ConfigWindow : public QWidget {
public:
    explicit ConfigWindow(const QString & initialPath = {}, QWidget * parent = nullptr);
    ~ConfigWindow(void) override;
private:
    friend class ConfiguratorTest;
    void RefreshDevices(const QString & prefer = {});
    void SelectDevice(void);
    CaptureModel * Model(void);
    void Start(void);
    void Stop(void);
    void Poll(void);
    void Receive(unsigned int type, unsigned int code, int value, std::uint64_t time = 0);
    void RefreshEvents(void);
    void SelectEvent(void);
    void SelectMapping(void);
    void LoadForm(const Mapping & mapping);
    void ApplyMapping(void);
    void RefreshMappings(void);
    void MoveMapping(int direction);
    void UpdatePreview(void);
    void Save(void);
    void Status(const QString & message, bool error = false);
    int SelectedObservation(void) const;
    bool MatchesFilter(unsigned int type, unsigned int code) const;
    QDoubleSpinBox * Number(double initial);

    QComboBox * mDevices, * mTypeFilter, * mSelector, * mOutputMode;
    QLineEdit * mFilter, * mInterface, * mPath;
    QPushButton * mRefresh, * mStart, * mStop, * mApply, * mCopy, * mSave;
    QCheckBox * mExclusive;
    QSpinBox * mQuickTap;
    QGroupBox * mCalibration;
    QLabel * mStatus, * mEventLabel, * mValidation;
    QTableWidget * mEvents, * mMappings;
    QPlainTextEdit * mLog, * mPreview;
    QDoubleSpinBox * mMinimum, * mCenter, * mMaximum, * mDeadzone;
    QDoubleSpinBox * mOutputMinimum, * mOutputCenter, * mOutputMaximum;
    QTimer mTimer;
    std::vector<DeviceInfo> mDeviceInfos;
    std::map<QString, CaptureModel> mSessions;
    std::vector<int> mVisibleEvents;
    int mFd = -1;
    libevdev * mDevice = nullptr;
    bool mLoading = false;
    unsigned int mFormType = 0, mFormCode = 0;
    bool mHasFormEvent = false;
    QString mExport;
};
}
