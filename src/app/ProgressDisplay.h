#pragma once

#include "geolib/ProgressClass.h"
#include <QProgressDialog>

namespace app {

class ProgressDisplay : public geo::ProgressClass {
public:
    ProgressDisplay(QWidget* parent) {
        m_progressDialog = std::make_shared<QProgressDialog>("progress...", QString(), 0, 0, parent);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setMinimumDuration(0);
        m_progressDialog->setWindowTitle("SolarSim");
        m_progressDialog->setRange(0, 100);
        m_progressDialog->show();
    }

    ~ProgressDisplay() {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }

    virtual void progress(int percent, std::string msg) {
        if (percent < 1) {
            m_progressDialog->setValue(0);
            return;
        }
        if (percent > 100) {
            m_progressDialog->setValue(0);
            return;
        }
        m_progressDialog->setValue(percent);
        if (msg.empty()) {
            return;
        }
        m_progressDialog->setLabelText(msg.c_str());
    }

private:
    std::shared_ptr<QProgressDialog> m_progressDialog;
};

} // namespace
