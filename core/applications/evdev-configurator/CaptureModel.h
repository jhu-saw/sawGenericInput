/*
(C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty. The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/
#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <vector>
#include <map>

namespace sawGenericInputConfigurator {
struct DeviceInfo {
    QString Path, Name;
    int Vendor = -1, Product = -1;
};
struct Observation {
    unsigned int Type = 0, Code = 0;
    int Value = 0, Minimum = 0, Maximum = 0;
    std::uint64_t Count = 0, Presses = 0, Releases = 0, Repeats = 0;
    bool Down = false;
    std::uint64_t PressTime = 0;
    double HoldMs = -1;
    bool Exportable(void) const;
};
struct Mapping {
    unsigned int Type = 0, Code = 0;
    QStringList Interfaces;
    unsigned int QuickTapMs = 0;
    bool Calibrated = false;
    double Minimum = -32767, Center = 0, Maximum = 32767, Deadzone = 0;
    double OutputMinimum = -32767, OutputCenter = 0, OutputMaximum = 32767;
};
class CaptureModel {
public:
    DeviceInfo Device;
    std::vector<Observation> Events;
    std::vector<Mapping> Mappings;
    std::map<unsigned int, std::pair<int, int>> AxisRanges;
    bool Exclusive = false; // Export setting; observation itself never grabs.
    bool MatchByName = true;
    QString DevicePath;
    void Observe(unsigned int type, unsigned int code, int value, std::uint64_t time = 0);
    QJsonObject Configuration(void) const;
    QJsonObject Fragment(void) const;
    QString ValidationError(void) const;
    static QString Validate(const QJsonObject & object);
    static QString CodeName(unsigned int type, unsigned int code);
    static QString TypeName(unsigned int type);
};
}
