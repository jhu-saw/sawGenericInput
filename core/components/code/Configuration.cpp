#include <sawGenericInput/Configuration.h>
#include <json/json.h>
#include <libevdev/libevdev.h>
#include <fstream>
#include <set>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

void Require(bool condition, const std::string & message) {
    if (!condition) {
        throw std::runtime_error("sawGenericInput configuration: " + message);
    }
}

void Members(const Json::Value & object, const std::set<std::string> & allowed) {
    Require(object.isObject(), "expected an object");
    for (const auto & key : object.getMemberNames()) {
        Require(allowed.count(key), "unknown field '" + key + "'");
    }
}

std::string Text(const Json::Value & value, const std::string & field) {
    Require(value.isString() && !value.asString().empty(), field + " must be a nonempty string");
    return value.asString();
}

int UsbId(const Json::Value & value) {
    const auto text = Text(value, "USB ID");
    Require(text.size() == 4 && text.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos,
            "USB IDs must be four hexadecimal digits");
    return std::stoi(text, nullptr, 16);
}

}

sawGenericInput::Configuration sawGenericInput::LoadConfiguration(const std::string & filename) {
    std::ifstream stream(filename);
    Require(stream.good(), "cannot open " + filename);
    Json::CharReaderBuilder reader;
    reader["rejectDupKeys"] = true;
    reader["failIfExtra"] = true;
    Json::Value parsed;
    std::string error;
    Require(Json::parseFromStream(reader, stream, &parsed, &error), error);
    const Json::Value & root = parsed;
    Members(root, {"device", "exclusive", "buttons", "axes"});
    Configuration config;
    const auto & device = root["device"];
    if (device.isString()) {
        config.Path = Text(device, "device");
        Require(config.Path.front() == '/', "device path must be absolute");
    } else {
        Members(device, {"name", "vendor_id", "product_id"});
        config.Name = Text(device["name"], "device.name");
        if (device.isMember("vendor_id")) {
            config.Vendor = UsbId(device["vendor_id"]);
        }
        if (device.isMember("product_id")) {
            config.Product = UsbId(device["product_id"]);
        }
    }
    if (root.isMember("exclusive")) {
        Require(root["exclusive"].isBool(), "exclusive must be boolean");
        config.Exclusive = root["exclusive"].asBool();
    }
    const auto & buttons = root["buttons"];
    Require(!root.isMember("buttons") || buttons.isArray(), "buttons must be an array");
    std::set<unsigned int> codes;
    std::set<std::string> interfaces;
    for (const auto & button : buttons) {
        Members(button, {"key", "interface", "interfaces", "quick_tap_ms"});
        int code = -1;
        if (button["key"].isString()) {
            const auto key = button["key"].asString();
            code = libevdev_event_code_from_name(EV_KEY, key.c_str());
        } else if (button["key"].isUInt() && button["key"].asUInt() <= KEY_MAX) {
            code = static_cast<int>(button["key"].asUInt());
        }
        Require(code >= 0 && code <= KEY_MAX, "key must name an EV_KEY code or be a numeric key code");
        Require(!(button.isMember("interface") && button.isMember("interfaces")),
                "use either interface or interfaces, not both");
        std::vector<std::string> names;
        if (button.isMember("interface")) {
            names.push_back(Text(button["interface"], "interface"));
        }
        if (button.isMember("interfaces")) {
            const auto & list = button["interfaces"];
            Require(list.isArray() && !list.empty(), "interfaces must be a nonempty array");
            for (const auto & name : list) names.push_back(Text(name, "interface"));
        }
        for (const auto & name : names) {
            Require(name != "Device" && name != "Input", "Device and Input interfaces are reserved");
            Require(interfaces.insert(name).second, "duplicate interface");
        }
        Require(codes.insert(static_cast<unsigned int>(code)).second, "duplicate key");
        unsigned int tap = 0;
        if (button.isMember("quick_tap_ms")) {
            Require(button["quick_tap_ms"].isUInt(), "quick_tap_ms must be a nonnegative integer");
            tap = button["quick_tap_ms"].asUInt();
        }
        config.Buttons.push_back({static_cast<unsigned int>(code), names, tap});
    }
    const auto & axes = root["axes"];
    Require(!root.isMember("axes") || axes.isArray(), "axes must be an array");
    std::set<unsigned int> axisCodes;
    for (const auto & axis : axes) {
        Members(axis, {"code", "calibration"});
        int code = -1;
        if (axis["code"].isString()) {
            const auto text = axis["code"].asString();
            code = libevdev_event_code_from_name(EV_ABS, text.c_str());
        } else if (axis["code"].isUInt() && axis["code"].asUInt() <= ABS_MAX) {
            code = static_cast<int>(axis["code"].asUInt());
        }
        Require(code >= 0 && code <= ABS_MAX, "axis code must name an EV_ABS code");
        Require(code < ABS_MT_SLOT, "multitouch axes are not supported");
        Require(axisCodes.insert(code).second, "duplicate axis");
        sawGenericInput::AxisConfiguration entry;
        entry.Code = static_cast<unsigned int>(code);
        if (axis.isMember("calibration")) {
            const auto & cal = axis["calibration"];
            Members(cal, {"minimum", "center", "maximum", "deadzone", "output_minimum", "output_center", "output_maximum"});
            const auto number = [&](const char * key, double fallback, bool required) {
                                    if (!cal.isMember(key) && !required) {
                                        return fallback;
                                    }
                                    Require(cal[key].isNumeric() && std::isfinite(cal[key].asDouble()), std::string("invalid calibration ") + key);
                                    return cal[key].asDouble();
                                };
            entry.Calibrated = true;
            entry.Minimum = number("minimum", 0, true);
            entry.Center = number("center", 0, true);
            entry.Maximum = number("maximum", 0, true);
            entry.Deadzone = number("deadzone", 0, false);
            entry.OutputMinimum = number("output_minimum", -32767, false);
            entry.OutputCenter = number("output_center", 0, false);
            entry.OutputMaximum = number("output_maximum", 32767, false);
            Require(entry.Minimum < entry.Center && entry.Center < entry.Maximum, "calibration requires minimum < center < maximum");
            Require(entry.Deadzone >= 0 && entry.Center - entry.Deadzone > entry.Minimum
                    && entry.Center + entry.Deadzone < entry.Maximum, "invalid calibration deadzone");
            Require((entry.OutputMinimum < entry.OutputCenter && entry.OutputCenter < entry.OutputMaximum)
                    || (entry.OutputMinimum > entry.OutputCenter && entry.OutputCenter > entry.OutputMaximum), "invalid output range");
            Require(std::isfinite(entry.Maximum - entry.Minimum)
                    && std::isfinite(entry.OutputMaximum - entry.OutputMinimum), "calibration range is too large");
        }
        config.Axes.push_back(entry);
    }
    Require(!config.Buttons.empty() || !config.Axes.empty(), "configure at least one button or axis");
    return config;
}

double sawGenericInput::AxisConfiguration::Convert(double value) const {
    if (!Calibrated) {
        return value;
    }
    value = std::clamp(value, Minimum, Maximum);
    if (value < Center - Deadzone) {
        return OutputCenter + ((value - (Center - Deadzone)) / (Center - Deadzone - Minimum))
               * (OutputCenter - OutputMinimum);
    }
    if (value > Center + Deadzone) {
        return OutputCenter + ((value - (Center + Deadzone)) / (Maximum - Center - Deadzone))
               * (OutputMaximum - OutputCenter);
    }
    return OutputCenter;
}
