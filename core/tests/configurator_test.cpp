/*
(C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty. The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/
#include "ConfigWindow.h"
#include <sawGenericInput/Configuration.h>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryFile>
#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <unistd.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace sawGenericInputConfigurator {

static void Check(bool condition, const char * message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class ConfiguratorTest {
public:

    static void Run(const QString & screenshot) {
        CaptureModel timing;
        timing.Observe(EV_KEY,KEY_A,1,1000);
        timing.Observe(EV_KEY,KEY_A,2,2000);
        timing.Observe(EV_KEY,KEY_A,1,3000);
        timing.Observe(EV_KEY,KEY_A,0,301000);
        Check(timing.Events.size()==1 && timing.Events[0].Count==4 && timing.Events[0].Repeats==1, "group repeated events");
        Check(timing.Events[0].HoldMs==300, "hold timing must ignore repeat/duplicate down");
        CaptureModel numeric;
        numeric.Device.Name="Numeric code controller";
        Mapping unnamed;
        unnamed.Type=EV_KEY;
        unnamed.Code=KEY_MAX-1;
        numeric.Mappings.push_back(unnamed);
        Check(numeric.Fragment()["buttons"].toArray()[0].toObject()["key"].isDouble()
              && numeric.ValidationError().isEmpty(), "unnamed kernel codes remain numeric");
        ConfigWindow window("/dev/input/saw-test-controller");
        auto * model = window.Model();
        Check(model, "CLI device preselection");
        model->Device.Name="Synthetic Bluetooth Controller";
        model->Device.Vendor=0x05d6;
        model->Device.Product=0x022c;
        window.Receive(EV_KEY,KEY_A,1,1000);
        window.Receive(EV_KEY,KEY_A,0,201000);
        window.Receive(EV_ABS,ABS_X,-100,1);
        window.Receive(EV_ABS,ABS_X,100,2);
        window.Receive(EV_ABS,ABS_MT_POSITION_X,12,3);
        window.Receive(EV_REL,REL_WHEEL,1,4);
        window.RefreshEvents();
        Check(window.mEvents->rowCount()==4 && !window.mCopy->isEnabled(), "capture and initially disabled export");
        window.mEvents->selectRow(0);
        window.mInterface->setText("Clutch, ClutchMirror");
        window.mQuickTap->setValue(300);
        window.mApply->click();
        Check(model->Mappings.size()==1 && window.mCopy->isEnabled(), "button mapping and export enabled");
        const auto config=model->Configuration();
        Check(config["device"].toObject()["name"]==model->Device.Name, "exact device selector");
        Check(config["device"].toObject()["vendor_id"]=="05d6" && config["device"].toObject()["product_id"]=="022c", "hex IDs");
        Check(config["buttons"].toArray()[0].toObject()["quick_tap_ms"].toInt()==300, "tap timing output");
        Check(config["buttons"].toArray()[0].toObject()["interfaces"].toArray()==QJsonArray({"Clutch", "ClutchMirror"}),
              "multiple interfaces exported on one button");
        Check(window.mInterface->text()=="Clutch, ClutchMirror", "mapping selection restores all interface names");
        window.mInterface->setText("Clutch");
        window.mApply->click();
        const auto single=model->Fragment()["buttons"].toArray()[0].toObject();
        Check(model->Mappings.size()==1 && single["interface"]=="Clutch" && !single.contains("interfaces"),
              "editing a button down to one interface preserves its digital slot");
        window.mInterface->setText("Clutch, ClutchMirror");
        window.mApply->click();
        for (const auto & invalid : {"Clutch, Clutch", "Clutch,,ClutchMirror", "Clutch, Input"}) {
            window.mInterface->setText(invalid);
            window.mApply->click();
            Check(model->Mappings[0].Interfaces==QStringList({"Clutch", "ClutchMirror"})
                  && window.mStatus->text().contains("Mapping not applied"), "invalid interface list rejected without changing mapping");
        }
        window.mInterface->setText("Input");
        window.mApply->click();
        Check(model->Mappings[0].Interfaces==QStringList({"Clutch", "ClutchMirror"})
              && window.mStatus->text().contains("reserved"), "reserved name rejected");
        window.Receive(EV_KEY,KEY_B,1,5);
        window.RefreshEvents();
        window.mEvents->selectRow(4);
        window.mInterface->setText("Clutch");
        window.mApply->click();
        Check(model->Mappings.size()==1 && window.mStatus->text().contains("duplicate interface"), "duplicate name rejected");
        window.mInterface->setText("Camera");
        window.mApply->click();
        Check(model->Mappings.size()==2, "second mapping");
        window.mMappings->selectRow(1);
        window.MoveMapping(-1);
        Check(model->Fragment()["buttons"].toArray()[0].toObject()["key"]=="KEY_B", "mapping order preserved in vectors");
        window.mFilter->setText("ABS_X");
        Check(window.mEvents->rowCount()==1 && window.SelectedObservation()<0, "isolation filter clears hidden selection");
        window.mEvents->selectRow(0);
        window.mCalibration->setChecked(true);
        window.mMinimum->setValue(-100);
        window.mCenter->setValue(0);
        window.mMaximum->setValue(100);
        window.mDeadzone->setValue(10);
        window.mApply->click();
        Check(model->Mappings.size()==3 && model->Mappings.back().Calibrated, "axis calibration mapping");
        window.mMinimum->setValue(0);
        window.mApply->click();
        Check(model->Mappings.back().Minimum==-100 && window.mStatus->text().contains("minimum < center < maximum"), "invalid calibration rejected");
        window.mFilter->clear();
        window.mEvents->selectRow(2);
        Check(!window.mApply->isEnabled(), "multitouch export excluded");
        window.mEvents->selectRow(3);
        Check(!window.mApply->isEnabled(), "relative event export excluded");
        window.mSelector->setCurrentIndex(1);
        window.mPath->setText("relative");
        Check(!window.mCopy->isEnabled(), "relative path rejected");
        window.mPath->setText("/dev/input/by-id/example-controller");
        window.mExclusive->setChecked(true);
        Check(window.mCopy->isEnabled() && model->Configuration()["exclusive"].toBool(), "stable path and grab setting");
        window.mOutputMode->setCurrentIndex(1);
        window.mCopy->click();
        const auto fragment=QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8()).object();
        Check(!fragment.contains("device") && fragment.contains("buttons") && fragment.contains("axes"), "copy mappings fragment");
        QTemporaryFile generated;
        Check(generated.open(), "temporary configuration");
        generated.write(QJsonDocument(model->Configuration()).toJson());
        generated.flush();
        const auto loaded=sawGenericInput::LoadConfiguration(generated.fileName().toStdString());
        Check(loaded.Buttons.size()==2 && loaded.Buttons[0].Code==KEY_B && loaded.Axes.size()==1, "generated config loads into real component");
        Check(loaded.Buttons[1].Interfaces==std::vector<std::string>({"Clutch", "ClutchMirror"}), "multiple interfaces round trip");
        Check(loaded.Exclusive && loaded.Path=="/dev/input/by-id/example-controller", "path selector round trip");
        Check(std::abs(loaded.Axes[0].Convert(100)-32767)<1e-6 && loaded.Axes[0].Convert(0)==0, "calibration round trip");
        // Stopping closes the descriptor and timer without throwing captured data away.
        int pipefd[2];
        Check(pipe(pipefd)==0,"pipe");
        window.mFd=pipefd[0];
        window.mTimer.start(1000);
        window.mStop->setEnabled(true);
        window.mStop->click();
        Check(window.mFd==-1 && !window.mTimer.isActive() && fcntl(pipefd[0],F_GETFD)==-1, "stop releases resources");
        ::close(pipefd[1]);
        Check(model->Mappings.size()==3 && model->Events.size()==5,"stop retains capture");
        window.mStart->click();
        Check(window.mFd==-1 && !window.mTimer.isActive() && window.mStatus->text().contains("Cannot open"), "missing device error is recoverable");
        const auto originalPath=window.mDevices->currentData().toString();
        CaptureModel second;
        second.Device={"/dev/input/second-controller","Second Controller",1,2};
        second.DevicePath=second.Device.Path;
        window.mSessions.emplace(second.Device.Path,second);
        window.mDevices->addItem(second.Device.Name,second.Device.Path);
        window.mDevices->setCurrentIndex(window.mDevices->count()-1);
        Check(window.Model()->Mappings.empty() && !window.mCopy->isEnabled(),"devices have isolated sessions");
        window.mDevices->setCurrentIndex(window.mDevices->findData(originalPath));
        Check(window.Model()->Mappings.size()==3 && window.Model()->Events.size()==5,"switching device preserves work");
        window.mSelector->setCurrentIndex(0);
        window.mOutputMode->setCurrentIndex(0);
        window.mMappings->selectRow(2);
        window.show();
        QApplication::processEvents();
        if (!screenshot.isEmpty()) {
            Check(window.grab().save(screenshot),"screenshot saved");
        }
    }

};
}

int main(int argc,char ** argv) {
    QApplication application(argc,argv);
    try {
        sawGenericInputConfigurator::ConfiguratorTest::Run(argc>1?QString::fromLocal8Bit(argv[1]):QString());
    }
    catch (const std::exception & error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
    std::cout<<"Capture, UI mapping, filtering, validation, device isolation and JSON export passed\n";
}
