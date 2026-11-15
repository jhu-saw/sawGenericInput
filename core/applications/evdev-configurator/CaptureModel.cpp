/*
(C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty. The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/
#include "CaptureModel.h"
#include <sawGenericInput/Configuration.h>
#include <libevdev/libevdev.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>
#include <algorithm>

namespace sawGenericInputConfigurator {

bool Observation::Exportable(void) const {
    return (Type == EV_KEY && Code <= KEY_MAX) || (Type == EV_ABS && Code < ABS_MT_SLOT);
}

QString CaptureModel::CodeName(unsigned int type, unsigned int code) {
    const auto * name = libevdev_event_code_get_name(type, code);
    return name ? QString::fromLatin1(name) : QString::number(code);
}

QString CaptureModel::TypeName(unsigned int type) {
    const auto * name = libevdev_event_type_get_name(type);
    return name ? QString::fromLatin1(name) : QString::number(type);
}

void CaptureModel::Observe(unsigned int type, unsigned int code, int value, std::uint64_t time) {
    for (auto & event : Events) {
        if (event.Type == type && event.Code == code) {
            event.Value = value;
            event.Minimum = std::min(event.Minimum, value);
            event.Maximum = std::max(event.Maximum, value);
            ++event.Count;
            if (type == EV_KEY) {
                event.Presses += value == 1;
                event.Releases += value == 0;
                event.Repeats += value == 2;
                if (value == 1 && !event.Down) {
                    event.Down = true;
                    event.PressTime = time;
                }
                if (value == 0 && event.Down) {
                    if (time >= event.PressTime) {
                        event.HoldMs = double(time - event.PressTime) / 1000.0;
                    }
                    event.Down = false;
                }
            }
            return;
        }
    }
    Observation event;
    event.Type = type;
    event.Code = code;
    event.Minimum = event.Maximum = event.Value = value;
    Events.push_back(event);
    Observe(type, code, value, time);
}

QJsonObject CaptureModel::Fragment(void) const {
    QJsonArray buttons, axes;
    for (const auto & mapping : Mappings) {
        if (mapping.Type == EV_KEY) {
            const auto * symbol = libevdev_event_code_get_name(EV_KEY,mapping.Code);
            QJsonObject entry{{"key", symbol ? QJsonValue(QString::fromLatin1(symbol)) : QJsonValue(int(mapping.Code))}};
            if (mapping.Interfaces.size() == 1) {
                entry["interface"] = mapping.Interfaces.front();
            } else if (!mapping.Interfaces.isEmpty()) {
                entry["interfaces"] = QJsonArray::fromStringList(mapping.Interfaces);
            }
            if (mapping.QuickTapMs) {
                entry["quick_tap_ms"] = double(mapping.QuickTapMs);
            }
            buttons.append(entry);
        } else if (mapping.Type == EV_ABS) {
            const auto * symbol = libevdev_event_code_get_name(EV_ABS,mapping.Code);
            QJsonObject entry{{"code", symbol ? QJsonValue(QString::fromLatin1(symbol)) : QJsonValue(int(mapping.Code))}};
            if (mapping.Calibrated) {
                entry["calibration"] = QJsonObject{
                    {"minimum", mapping.Minimum}, {"center", mapping.Center},
                    {"maximum", mapping.Maximum}, {"deadzone", mapping.Deadzone},
                    {"output_minimum", mapping.OutputMinimum}, {"output_center", mapping.OutputCenter},
                    {"output_maximum", mapping.OutputMaximum}};
            }
            axes.append(entry);
        }
    }
    QJsonObject result;
    if (!buttons.empty()) {
        result["buttons"] = buttons;
    }
    if (!axes.empty()) {
        result["axes"] = axes;
    }
    return result;
}

QJsonObject CaptureModel::Configuration(void) const {
    auto result = Fragment();
    if (MatchByName) {
        QJsonObject selector{{"name", Device.Name}};
        if (Device.Vendor >= 0) {
            selector["vendor_id"] = QString("%1").arg(Device.Vendor, 4, 16, QLatin1Char('0'));
        }
        if (Device.Product >= 0) {
            selector["product_id"] = QString("%1").arg(Device.Product, 4, 16, QLatin1Char('0'));
        }
        result["device"] = selector;
    } else {
        result["device"] = DevicePath;
    }
    result["exclusive"] = Exclusive;
    return result;
}

QString CaptureModel::Validate(const QJsonObject & object) {
    QTemporaryFile file;
    if (!file.open()) {
        return "Unable to create a temporary file for configuration validation.";
    }
    const auto data = QJsonDocument(object).toJson();
    if (file.write(data) != data.size() || !file.flush()) {
        return "Unable to write the validation file.";
    }
    try {
        sawGenericInput::LoadConfiguration(file.fileName().toStdString());
    }
    catch (const std::exception & error) {
        return QString::fromUtf8(error.what());
    }
    return {};
}

QString CaptureModel::ValidationError(void) const {
    return Validate(Configuration());
}

}
