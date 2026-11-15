#ifndef _sawGenericInput_Configuration_h
#define _sawGenericInput_Configuration_h
#include <string>
#include <vector>
#include <sawGenericInput/sawGenericInputExport.h>
namespace sawGenericInput {
struct ButtonConfiguration {
    unsigned int Code;
    std::vector<std::string> Interfaces; // Empty for aggregate input only.
    unsigned int QuickTapMs;
};
struct CISST_EXPORT AxisConfiguration {
    unsigned int Code;
    bool Calibrated = false;
    double Minimum = 0.0, Center = 0.0, Maximum = 0.0, Deadzone = 0.0;
    double OutputMinimum = -32767.0, OutputCenter = 0.0, OutputMaximum = 32767.0;
    double Convert(double value) const;
};
struct Configuration {
    std::string Path;
    std::string Name;
    int Vendor = -1;
    int Product = -1;
    bool Exclusive = true;
    std::vector<ButtonConfiguration> Buttons;
    std::vector<AxisConfiguration> Axes;
};
CISST_EXPORT Configuration LoadConfiguration(const std::string & filename);
}
#endif
