#pragma once

#include "geolib/ProgressClass.h"
#include <QProgressDialog>

namespace app {

class ProgressDisplay : public geo::ProgressClass {
public:
    ProgressDisplay(QWidget* parent) {
        m_progressDialog = std::make_shared<QProgressDialog>("progress...", "Cancel", 0, 0, parent);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->setMinimumDuration(0);
        m_progressDialog->setWindowTitle("SolarSim");
        m_progressDialog->setRange(0, 100);
        QObject::connect(m_progressDialog.get(), &QProgressDialog::canceled, [this]() {
            cancel();
        });
        m_progressDialog->show();
    }

    ~ProgressDisplay() {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }

    virtual void progress(int percent, std::string msg) {
        if (percent < 1) {
            m_progressDialog->setValue(0);
        } else if (percent > 100) {
            m_progressDialog->setValue(100);
        }
        else {
            m_progressDialog->setValue(percent);
        }
        if (!msg.empty()) {
            m_progressDialog->setLabelText(msg.c_str());
        }
    }

private:
    std::shared_ptr<QProgressDialog> m_progressDialog;
};

} // namespace
