/*
(C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty. The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/
#include "ConfigWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <iostream>

int main(int argc, char ** argv) {
    QApplication application(argc,argv);
    application.setApplicationName("sawGenericInputConfigQt");
    QCommandLineParser parser;
    parser.setApplicationDescription("Observe an evdev input node and generate sawGenericInput configuration JSON.");
    parser.addHelpOption();
    QCommandLineOption device({"d","device"},"Preselect an absolute evdev node or stable symlink; observation starts only when requested.","path");
    parser.addOption(device);
    parser.process(application);
    if (parser.isSet(device) && !QDir::isAbsolutePath(parser.value(device))) {
        std::cerr << "The device path must be absolute.\n";
        return 1;
    }
    sawGenericInputConfigurator::ConfigWindow window(parser.value(device));
    window.show();
    return application.exec();
}
