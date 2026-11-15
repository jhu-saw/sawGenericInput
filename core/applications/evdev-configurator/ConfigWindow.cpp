/*
(C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty. The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/
#include "ConfigWindow.h"
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace sawGenericInputConfigurator {
namespace {

QString DeviceKey(const DeviceInfo & info) {
    return info.Path + '\n' + info.Name + '\n' + QString::number(info.Vendor) + ':' + QString::number(info.Product);
}

QString Read(const QString & path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

QTableWidget * Table(const QStringList & labels) {
    auto * table = new QTableWidget(0, labels.size());
    table->setHorizontalHeaderLabels(labels);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
}

}

QDoubleSpinBox * ConfigWindow::Number(double initial) {
    auto * field = new QDoubleSpinBox;
    field->setRange(-1e12, 1e12);
    field->setDecimals(6);
    field->setValue(initial);
    return field;
}

ConfigWindow::ConfigWindow(const QString & initialPath, QWidget * parent) : QWidget(parent) {
    setWindowTitle("sawGenericInput — evdev configuration");
    resize(1180, 1040);
    auto * outer = new QVBoxLayout(this);
    auto * scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto * content = new QWidget;
    auto * layout = new QVBoxLayout(content);
    scroll->setWidget(content);
    outer->addWidget(scroll);
    auto * deviceRow = new QHBoxLayout;
    mDevices = new QComboBox;
    mDevices->setMinimumContentsLength(28);
    mRefresh = new QPushButton("Refresh devices");
    mStart = new QPushButton("Start observing");
    mStop = new QPushButton("Stop");
    mStop->setEnabled(false);
    deviceRow->addWidget(new QLabel("Device:"));
    deviceRow->addWidget(mDevices, 1);
    deviceRow->addWidget(mRefresh);
    deviceRow->addWidget(mStart);
    deviceRow->addWidget(mStop);
    layout->addLayout(deviceRow);
    auto * hint = new QLabel("Choose one input node, start observing, and operate its controls. Observation is shared; it never grabs the device.");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    mStatus = new QLabel;
    mStatus->setWordWrap(true);
    layout->addWidget(mStatus);

    auto * splitter = new QSplitter(Qt::Vertical);
    auto * observed = new QGroupBox("1. Observe and isolate events");
    observed->setMinimumHeight(340);
    observed->setMaximumHeight(350);
    auto * observedLayout = new QVBoxLayout(observed);
    auto * filterRow = new QHBoxLayout;
    mTypeFilter = new QComboBox;
    mTypeFilter->addItems({"All events", "Keys / buttons", "Absolute axes", "Other events"});
    mFilter = new QLineEdit;
    mFilter->setPlaceholderText("Filter by event code, e.g. KEY_VOLUME or ABS_X");
    auto * clear = new QPushButton("Clear observations");
    filterRow->addWidget(mTypeFilter);
    filterRow->addWidget(mFilter, 1);
    filterRow->addWidget(clear);
    observedLayout->addLayout(filterRow);
    mEvents = Table({"Type", "Code", "Latest", "Min", "Max", "Count", "Press / release / repeat", "Last hold (ms)"});
    mEvents->setMinimumHeight(140);
    observedLayout->addWidget(mEvents);
    auto * note = new QLabel("Select a row to configure it. Synchronization packets are omitted. Unsupported event types remain visible for diagnosis.");
    note->setWordWrap(true);
    observedLayout->addWidget(note);
    mLog = new QPlainTextEdit;
    mLog->setReadOnly(true);
    mLog->setMaximumBlockCount(300);
    mLog->setFixedHeight(60);
    mLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    observedLayout->addWidget(mLog);
    splitter->addWidget(observed);

    auto * mappingBox = new QGroupBox("2. Name and configure selected events");
    mappingBox->setMinimumHeight(280);
    mappingBox->setMaximumHeight(320);
    auto * mappingLayout = new QHBoxLayout(mappingBox);
    mMappings = Table({"Event", "Provided interfaces", "Quick tap (ms)", "Axis calibration"});
    mMappings->setMinimumHeight(100);
    auto * mappingList = new QVBoxLayout;
    mappingList->addWidget(mMappings);
    auto * mappingButtons = new QHBoxLayout;
    auto * remove = new QPushButton("Remove mapping"), * up = new QPushButton("Move up"), * down = new QPushButton("Move down");
    mappingButtons->addWidget(remove);
    mappingButtons->addWidget(up);
    mappingButtons->addWidget(down);
    mappingList->addLayout(mappingButtons);
    mappingList->addWidget(new QLabel("Array order defines the digital and analog input indices."));
    mappingLayout->addLayout(mappingList, 1);
    auto * editorWidget = new QWidget;
    auto * editor = new QVBoxLayout(editorWidget);
    mEventLabel = new QLabel("Select an observed event.");
    mEventLabel->setWordWrap(true);
    editor->addWidget(mEventLabel);
    auto * buttonForm = new QFormLayout;
    mInterface = new QLineEdit;
    mInterface->setPlaceholderText("e.g. Clutch, Camera; blank = aggregate only");
    mInterface->setToolTip("Separate interface names with commas. Each interface receives the same Button events.");
    mQuickTap = new QSpinBox;
    mQuickTap->setRange(0, std::numeric_limits<int>::max());
    mQuickTap->setSuffix(" ms");
    buttonForm->addRow("Provided interfaces (comma-separated)", mInterface);
    buttonForm->addRow("Quick tap (0 = disabled)", mQuickTap);
    editor->addLayout(buttonForm);
    mCalibration = new QGroupBox("Calibrate absolute axis (unchecked = raw values)");
    mCalibration->setCheckable(true);
    mCalibration->setChecked(false);
    auto * calForm = new QFormLayout(mCalibration);
    mMinimum = Number(-32767);
    mCenter = Number(0);
    mMaximum = Number(32767);
    mDeadzone = Number(0);
    mDeadzone->setMinimum(0);
    mOutputMinimum = Number(-32767);
    mOutputCenter = Number(0);
    mOutputMaximum = Number(32767);
    calForm->addRow("Input minimum", mMinimum);
    calForm->addRow("Input center", mCenter);
    calForm->addRow("Input maximum", mMaximum);
    calForm->addRow("Deadzone (raw units)", mDeadzone);
    calForm->addRow("Output minimum", mOutputMinimum);
    calForm->addRow("Output center", mOutputCenter);
    calForm->addRow("Output maximum", mOutputMaximum);
    auto * sampleButtons = new QHBoxLayout;
    auto * range = new QPushButton("Use observed range"), * center = new QPushButton("Use latest as center");
    sampleButtons->addWidget(range);
    sampleButtons->addWidget(center);
    calForm->addRow(sampleButtons);
    editor->addWidget(mCalibration);
    mApply = new QPushButton("Add / update mapping");
    mApply->setEnabled(false);
    auto * editorScroll = new QScrollArea;
    editorScroll->setWidgetResizable(true);
    editorScroll->setWidget(editorWidget);
    editorScroll->setMinimumHeight(180);
    auto * editorColumn = new QVBoxLayout;
    editorColumn->addWidget(editorScroll);
    editorColumn->addWidget(mApply);
    mappingLayout->addLayout(editorColumn, 1);
    splitter->addWidget(mappingBox);
    splitter->setChildrenCollapsible(false);
    splitter->setMaximumHeight(674);
    splitter->setSizes({340,280});
    layout->addWidget(splitter, 2);

    auto * exportBox = new QGroupBox("3. Copy or save configuration JSON");
    auto * exportLayout = new QVBoxLayout(exportBox);
    auto * selectorRow = new QHBoxLayout;
    mSelector = new QComboBox;
    mSelector->addItems({"Match exact name and IDs", "Use device path / stable symlink"});
    mPath = new QLineEdit;
    mPath->setPlaceholderText("/dev/input/by-id/... or /dev/input/eventN");
    mPath->setEnabled(false);
    mExclusive = new QCheckBox("Exclusive when used by sawGenericInput");
    selectorRow->addWidget(mSelector);
    selectorRow->addWidget(mPath, 1);
    selectorRow->addWidget(mExclusive);
    exportLayout->addLayout(selectorRow);
    mOutputMode = new QComboBox;
    mOutputMode->addItems({"Complete device configuration", "Mappings fragment (merge into an existing configuration)"});
    exportLayout->addWidget(mOutputMode);
    mPreview = new QPlainTextEdit;
    mPreview->setReadOnly(true);
    mPreview->setMinimumHeight(100);
    mPreview->setMaximumHeight(180);
    exportLayout->addWidget(mPreview);
    mPreview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    mValidation = new QLabel;
    mValidation->setWordWrap(true);
    exportLayout->addWidget(mValidation);
    auto * exportButtons = new QHBoxLayout;
    mCopy = new QPushButton("Copy displayed JSON");
    mSave = new QPushButton("Save full configuration…");
    exportButtons->addWidget(mCopy);
    exportButtons->addWidget(mSave);
    exportButtons->addStretch();
    exportLayout->addLayout(exportButtons);
    layout->addWidget(exportBox, 1);

    connect(mRefresh, &QPushButton::clicked, this, [this] {
            RefreshDevices(mDevices->currentData().toString());
        });
    connect(mDevices, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
            SelectDevice();
        });
    connect(mStart, &QPushButton::clicked, this, [this] {
            Start();
        });
    connect(mStop, &QPushButton::clicked, this, [this] {
            Stop();
        });
    connect(&mTimer, &QTimer::timeout, this, [this] {
            Poll();
        });
    connect(mTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
            RefreshEvents();
        });
    connect(mFilter, &QLineEdit::textChanged, this, [this] {
            RefreshEvents();
        });
    connect(clear, &QPushButton::clicked, this, [this] {
            if (auto * model = Model()) {
                model->Events.clear();
                mLog->clear();
                mHasFormEvent = false;
                mApply->setEnabled(false);
                RefreshEvents();
                mEventLabel->setText("Select an observed event.");
            }
        });
    connect(mEvents, &QTableWidget::itemSelectionChanged, this, [this] {
            SelectEvent();
        });
    connect(mMappings, &QTableWidget::itemSelectionChanged, this, [this] {
            SelectMapping();
        });
    connect(mApply, &QPushButton::clicked, this, [this] {
            ApplyMapping();
        });
    connect(remove, &QPushButton::clicked, this, [this] {
            auto * model = Model();
            const int row = mMappings->currentRow();
            if (model && row >= 0) {
                model->Mappings.erase(model->Mappings.begin()+row);
                RefreshMappings();
                UpdatePreview();
            }
        });
    connect(up, &QPushButton::clicked, this, [this] {
            MoveMapping(-1);
        });
    connect(down, &QPushButton::clicked, this, [this] {
            MoveMapping(1);
        });
    connect(range, &QPushButton::clicked, this, [this] {
            auto * model = Model();
            if (!model) {
                return;
            }
            for (const auto & event : model->Events) if (event.Type == mFormType && event.Code == mFormCode) {
                    mMinimum->setValue(event.Minimum);
                    mMaximum->setValue(event.Maximum);
                    mCenter->setValue((double(event.Minimum)+event.Maximum)/2.0);
                    break;
                }
        });
    connect(center, &QPushButton::clicked, this, [this] {
            auto * model = Model();
            if (!model) {
                return;
            }
            for (const auto & event : model->Events) if (event.Type == mFormType && event.Code == mFormCode) {
                    mCenter->setValue(event.Value);
                    break;
                }
        });
    connect(mSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
            UpdatePreview();
        });
    connect(mPath, &QLineEdit::textChanged, this, [this] {
            UpdatePreview();
        });
    connect(mExclusive, &QCheckBox::toggled, this, [this] {
            UpdatePreview();
        });
    connect(mOutputMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
            UpdatePreview();
        });
    connect(mCopy, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(mExport);
            Status("Displayed JSON copied to clipboard.");
        });
    connect(mSave, &QPushButton::clicked, this, [this] {
            Save();
        });
    RefreshDevices(initialPath);
}

ConfigWindow::~ConfigWindow(void) {
    Stop();
}

void ConfigWindow::Status(const QString & message, bool error) {
    mStatus->setText(message);
    mStatus->setStyleSheet(error ? "color: #b03030" : "");
}

CaptureModel * ConfigWindow::Model(void) {
    const auto path = mDevices->currentData().toString();
    if (path.isEmpty()) {
        return nullptr;
    }
    return &mSessions.at(path);
}

void ConfigWindow::RefreshDevices(const QString & prefer) {
    if (mFd >= 0) {
        return;
    }
    const auto previous = mSessions.find(prefer);
    const auto preferPath = previous == mSessions.end() ? prefer : previous->second.Device.Path;
    mDeviceInfos.clear();
    const QDir directory("/dev/input");
    for (const auto & entry : directory.entryList({"event*"}, QDir::System | QDir::Files, QDir::Name)) {
        DeviceInfo info;
        info.Path = directory.absoluteFilePath(entry);
        const auto sys = "/sys/class/input/" + entry + "/device/";
        info.Name = Read(sys+"name");
        if (info.Name.isEmpty()) {
            info.Name = entry;
        }
        bool ok = false;
        info.Vendor = Read(sys+"id/vendor").toInt(&ok,16);
        if (!ok) {
            info.Vendor = -1;
        }
        info.Product = Read(sys+"id/product").toInt(&ok,16);
        if (!ok) {
            info.Product = -1;
        }
        mDeviceInfos.push_back(info);
    }
    if (!preferPath.isEmpty() && std::none_of(mDeviceInfos.begin(), mDeviceInfos.end(), [&](const auto & info) {
            return info.Path == preferPath;
        })) {
        mDeviceInfos.push_back(previous == mSessions.end() ? DeviceInfo{preferPath,preferPath,-1,-1} : previous->second.Device);
    }
    {
        QSignalBlocker blocker(mDevices);
        mDevices->clear();
        for (const auto & info : mDeviceInfos) {
            const auto key = DeviceKey(info);
            mDevices->addItem(info.Name + " — " + info.Path, key);
            auto found = mSessions.find(key);
            if (found == mSessions.end()) {
                CaptureModel session;
                session.Device = info;
                session.DevicePath = info.Path;
                mSessions.emplace(key, session);
            }
        }
        for (int index = 0; index < int(mDeviceInfos.size()); ++index)
            if (mDeviceInfos[index].Path == preferPath) {
                mDevices->setCurrentIndex(index);
                break;
            }
    }
    SelectDevice();
    if (mDeviceInfos.empty()) {
        Status("No evdev nodes found in /dev/input. Connect a device and refresh, or check device access in this environment.", true);
    }
}

void ConfigWindow::SelectDevice(void) {
    Stop();
    mLoading = true;
    const auto * model = Model();
    mSelector->setCurrentIndex(model && !model->MatchByName ? 1 : 0);
    mPath->setText(model ? model->DevicePath : QString());
    mExclusive->setChecked(model && model->Exclusive);
    mVisibleEvents.clear();
    mEvents->clearSelection();
    mEvents->setRowCount(0);
    mLoading = false;
    mHasFormEvent = false;
    mApply->setEnabled(false);
    mEventLabel->setText("Select an observed event.");
    mLog->clear();
    RefreshEvents();
    RefreshMappings();
    UpdatePreview();
}

void ConfigWindow::Start(void) {
    auto * model = Model();
    if (!model || mFd >= 0) {
        return;
    }
    mFd = open(model->Device.Path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (mFd < 0) {
        Status("Cannot open " + model->Device.Path + ": " + QString::fromLocal8Bit(std::strerror(errno))
               + ". Check device-specific input permissions.", true);
        return;
    }
    const int result = libevdev_new_from_fd(mFd, &mDevice);
    if (result < 0) {
        Stop();
        Status("Unable to read evdev metadata: " + QString::fromLocal8Bit(std::strerror(-result)), true);
        return;
    }
    DeviceInfo actual{model->Device.Path, QString::fromUtf8(libevdev_get_name(mDevice)), libevdev_get_id_vendor(mDevice),libevdev_get_id_product(mDevice)};
    if (model->Device.Vendor >= 0 && DeviceKey(actual) != DeviceKey(model->Device)) {
        Stop();
        Status("This event node now belongs to another device. Refresh and select it again; the previous device's mappings are retained separately.",true);
        return;
    }
    model->Device = actual;
    const auto key = DeviceKey(actual);
    if (key != mDevices->currentData().toString()) {
        model = &mSessions.try_emplace(key,*model).first->second;
        QSignalBlocker blocker(mDevices);
        mDevices->setItemData(mDevices->currentIndex(),key);
    }
    mDevices->setItemText(mDevices->currentIndex(),actual.Name + " — " + actual.Path);
    model->AxisRanges.clear();
    for (unsigned int code = 0; code < ABS_MT_SLOT; ++code) {
        if (const auto * info = libevdev_get_abs_info(mDevice, code)) {
            model->AxisRanges[code] = {info->minimum, info->maximum};
        }
    }
    // Never merge a hold duration across stopped sessions, even for a held key.
    for (auto & event : model->Events) event.Down = false;
    mDevices->setEnabled(false);
    mRefresh->setEnabled(false);
    mStart->setEnabled(false);
    mStop->setEnabled(true);
    mTimer.start(10);
    UpdatePreview();
    Status("Observing " + model->Device.Name + " (shared, no exclusive grab).");
}

void ConfigWindow::Stop(void) {
    mTimer.stop();
    if (mDevice) {
        libevdev_free(mDevice);
        mDevice = nullptr;
    }
    if (mFd >= 0) {
        ::close(mFd);
        mFd = -1;
    }
    mDevices->setEnabled(true);
    mRefresh->setEnabled(true);
    mStart->setEnabled(mDevices->count()>0);
    mStop->setEnabled(false);
    Status("Observation stopped. Captured events and mappings are retained per device.");
}

void ConfigWindow::Poll(void) {
    bool changed = false;
    for (unsigned int count = 0; mDevice && count < 1024; ++count) {
        input_event event{};
        const int result = libevdev_next_event(mDevice, LIBEVDEV_READ_FLAG_NORMAL, &event);
        if (result == -EAGAIN) {
            break;
        }
        if (result == -EINTR) {
            continue;
        }
        if (result == LIBEVDEV_READ_STATUS_SYNC) {
            Stop();
            Status("Events were dropped. Observation stopped; restart to capture a fresh sequence.", true);
            break;
        }
        if (result < 0) {
            Stop();
            Status("Device disconnected or read failed: " + QString::fromLocal8Bit(std::strerror(-result)), true);
            break;
        }
        Receive(event.type, event.code, event.value, std::uint64_t(event.time.tv_sec)*1000000+event.time.tv_usec);
        changed = true;
    }
    if (changed) {
        RefreshEvents();
    }
}

void ConfigWindow::Receive(unsigned int type, unsigned int code, int value, std::uint64_t time) {
    if (type == EV_SYN) {
        return;
    }
    auto * model = Model();
    if (!model) {
        return;
    }
    model->Observe(type, code, value, time);
    const auto text = QString("%1  %2  %3  %4").arg(double(time)/1000000.0,0,'f',6)
                      .arg(CaptureModel::TypeName(type), CaptureModel::CodeName(type,code)).arg(value);
    if (MatchesFilter(type,code)) {
        mLog->appendPlainText(text);
    }
}

bool ConfigWindow::MatchesFilter(unsigned int type, unsigned int code) const {
    const int filter = mTypeFilter->currentIndex();
    if ((filter == 1 && type != EV_KEY) || (filter == 2 && type != EV_ABS)
        || (filter == 3 && (type == EV_KEY || type == EV_ABS))) {
        return false;
    }
    return CaptureModel::CodeName(type,code).contains(mFilter->text().trimmed(),Qt::CaseInsensitive);
}

int ConfigWindow::SelectedObservation(void) const {
    const int row = mEvents->currentRow();
    return mEvents->selectionModel()->hasSelection() && row >= 0 && row < int(mVisibleEvents.size()) ? mVisibleEvents[row] : -1;
}

void ConfigWindow::RefreshEvents(void) {
    const auto * model = Model();
    const int selected = SelectedObservation();
    QSignalBlocker blocker(mEvents);
    mVisibleEvents.clear();
    if (model) {
        for (size_t index = 0; index < model->Events.size(); ++index) {
            const auto & event = model->Events[index];
            if (!MatchesFilter(event.Type,event.Code)) {
                continue;
            }
            mVisibleEvents.push_back(int(index));
        }
    }
    mEvents->setRowCount(int(mVisibleEvents.size()));
    int selectedRow = -1;
    for (size_t row = 0; model && row < mVisibleEvents.size(); ++row) {
        const auto & event = model->Events[mVisibleEvents[row]];
        const QStringList values{CaptureModel::TypeName(event.Type),CaptureModel::CodeName(event.Type,event.Code),
                                 QString::number(event.Value),QString::number(event.Minimum),QString::number(event.Maximum),QString::number(qulonglong(event.Count)),
                                 QString("%1 / %2 / %3").arg(qulonglong(event.Presses)).arg(qulonglong(event.Releases)).arg(qulonglong(event.Repeats)),
                                 event.HoldMs < 0 ? "—" : QString::number(event.HoldMs,'f',1)};
        for (int column = 0; column < values.size(); ++column) mEvents->setItem(int(row),column,new QTableWidgetItem(values[column]));
        if (mVisibleEvents[row] == selected) {
            selectedRow = int(row);
        }
    }
    if (selectedRow >= 0) {
        mEvents->selectRow(selectedRow);
    } else {
        mEvents->clearSelection();
        mEvents->setCurrentCell(-1,-1);
    }
}

void ConfigWindow::SelectEvent(void) {
    const auto * model = Model();
    const int index = SelectedObservation();
    if (mLoading || !model || index < 0 || index >= int(model->Events.size())) {
        return;
    }
    const auto & event = model->Events[index];
    if (!event.Exportable()) {
        mHasFormEvent = false;
        mApply->setEnabled(false);
        mEventLabel->setText(CaptureModel::CodeName(event.Type,event.Code)
                             + ": visible for diagnosis; sawGenericInput currently exports EV_KEY and non-multitouch EV_ABS mappings only.");
        return;
    }
    for (const auto & mapping : model->Mappings) {
        if (mapping.Type == event.Type && mapping.Code == event.Code) {
            LoadForm(mapping);
            return;
        }
    }
    Mapping mapping;
    mapping.Type = event.Type;
    mapping.Code = event.Code;
    if (event.Type == EV_ABS) {
        auto range = model->AxisRanges.find(event.Code);
        mapping.Minimum = range == model->AxisRanges.end() ? event.Minimum : range->second.first;
        mapping.Maximum = range == model->AxisRanges.end() ? event.Maximum : range->second.second;
        mapping.Center = (mapping.Minimum+mapping.Maximum)/2.0;
    }
    LoadForm(mapping);
}

void ConfigWindow::SelectMapping(void) {
    const auto * model = Model();
    const int row = mMappings->currentRow();
    if (model && row >= 0 && row < int(model->Mappings.size())) {
        LoadForm(model->Mappings[row]);
    }
}

void ConfigWindow::LoadForm(const Mapping & mapping) {
    mFormType = mapping.Type;
    mFormCode = mapping.Code;
    mHasFormEvent = true;
    const bool key = mapping.Type == EV_KEY;
    mEventLabel->setText(CaptureModel::CodeName(mapping.Type,mapping.Code)
                         + (key ? " — key / button" : " — absolute axis; channel names are the event codes, with indices set by array order"));
    mInterface->setEnabled(key);
    mInterface->setText(mapping.Interfaces.join(", "));
    mQuickTap->setEnabled(key);
    mQuickTap->setValue(int(mapping.QuickTapMs));
    mCalibration->setVisible(!key);
    mCalibration->setEnabled(!key);
    mCalibration->setChecked(mapping.Calibrated);
    mMinimum->setValue(mapping.Minimum);
    mCenter->setValue(mapping.Center);
    mMaximum->setValue(mapping.Maximum);
    mDeadzone->setValue(mapping.Deadzone);
    mOutputMinimum->setValue(mapping.OutputMinimum);
    mOutputCenter->setValue(mapping.OutputCenter);
    mOutputMaximum->setValue(mapping.OutputMaximum);
    mApply->setEnabled(true);
}

void ConfigWindow::ApplyMapping(void) {
    auto * model = Model();
    if (!model || !mHasFormEvent) {
        return;
    }
    Mapping mapping;
    mapping.Type = mFormType;
    mapping.Code = mFormCode;
    if (mapping.Type == EV_KEY) {
        const auto names = mInterface->text().trimmed();
        if (!names.isEmpty()) {
            for (const auto & name : names.split(',')) mapping.Interfaces.push_back(name.trimmed());
        }
        mapping.QuickTapMs = unsigned(mQuickTap->value());
    } else {
        mapping.Calibrated = mCalibration->isChecked();
        mapping.Minimum = mMinimum->value();
        mapping.Center = mCenter->value();
        mapping.Maximum = mMaximum->value();
        mapping.Deadzone = mDeadzone->value();
        mapping.OutputMinimum = mOutputMinimum->value();
        mapping.OutputCenter = mOutputCenter->value();
        mapping.OutputMaximum = mOutputMaximum->value();
    }
    auto candidate = *model;
    auto found = std::find_if(candidate.Mappings.begin(),candidate.Mappings.end(),[&](const auto & item) {
            return item.Type == mapping.Type && item.Code == mapping.Code;
        });
    int index = int(candidate.Mappings.size());
    if (found == candidate.Mappings.end()) {
        candidate.Mappings.push_back(mapping);
    } else {
        index = int(found-candidate.Mappings.begin());
        *found = mapping;
    }
    const auto error = candidate.ValidationError();
    if (!error.isEmpty()) {
        Status("Mapping not applied: " + error, true);
        return;
    }
    model->Mappings = candidate.Mappings;
    RefreshMappings();
    mMappings->selectRow(index);
    UpdatePreview();
    Status("Mapping applied. JSON is ready to copy or save.");
}

void ConfigWindow::RefreshMappings(void) {
    const auto * model = Model();
    QSignalBlocker blocker(mMappings);
    mMappings->setRowCount(model ? int(model->Mappings.size()) : 0);
    for (size_t row = 0; model && row < model->Mappings.size(); ++row) {
        const auto & mapping = model->Mappings[row];
        const QStringList values{CaptureModel::CodeName(mapping.Type,mapping.Code),mapping.Interfaces.join(", "),
                                 mapping.Type == EV_KEY ? QString::number(mapping.QuickTapMs) : "—",
                                 mapping.Type == EV_ABS ? (mapping.Calibrated ? "Calibrated" : "Raw") : "—"};
        for (int column = 0; column < values.size(); ++column) mMappings->setItem(int(row),column,new QTableWidgetItem(values[column]));
    }
    mMappings->clearSelection();
}

void ConfigWindow::MoveMapping(int direction) {
    auto * model = Model();
    const int row = mMappings->currentRow();
    const int target = row+direction;
    if (!model || row < 0 || target < 0 || target >= int(model->Mappings.size())) {
        return;
    }
    std::swap(model->Mappings[row], model->Mappings[target]);
    RefreshMappings();
    mMappings->selectRow(target);
    UpdatePreview();
}

void ConfigWindow::UpdatePreview(void) {
    if (mLoading) {
        return;
    }
    auto * model = Model();
    mCopy->setEnabled(false);
    mSave->setEnabled(false);
    if (!model) {
        mPreview->clear();
        mValidation->setText("Select a device to begin.");
        return;
    }
    model->MatchByName = mSelector->currentIndex() == 0;
    model->DevicePath = mPath->text().trimmed();
    model->Exclusive = mExclusive->isChecked();
    mPath->setEnabled(!model->MatchByName);
    const auto object = mOutputMode->currentIndex() == 0 ? model->Configuration() : model->Fragment();
    mExport = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
    mPreview->setPlainText(mExport);
    const auto error = model->ValidationError();
    if (!error.isEmpty()) {
        mValidation->setText(model->Mappings.empty() ? "Add at least one supported event mapping to enable export." : error);
        return;
    }
    mValidation->setText(model->MatchByName
        ? "Validated by sawGenericInput. Exact name and IDs must identify one input node."
        : "Validated by sawGenericInput. Event numbers can change; prefer a stable device symlink.");
    mCopy->setEnabled(true);
    mSave->setEnabled(true);
}

void ConfigWindow::Save(void) {
    const auto * model = Model();
    if (!model) {
        return;
    }
    const auto error = model->ValidationError();
    if (!error.isEmpty()) {
        Status(error,true);
        return;
    }
    const auto path = QFileDialog::getSaveFileName(this,"Save full sawGenericInput configuration",QString(),"JSON files (*.json)");
    if (path.isEmpty()) {
        return;
    }
    QSaveFile file(path);
    const auto data = QJsonDocument(model->Configuration()).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        Status("Unable to save configuration: " + file.errorString(),true);
        return;
    }
    Status("Saved " + path);
}

}
