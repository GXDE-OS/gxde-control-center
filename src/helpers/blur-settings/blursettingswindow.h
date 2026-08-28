/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 */

#ifndef SRC_HELPERS_BLUR_SETTINGS_BLURSETTINGSWINDOW_H_
#define SRC_HELPERS_BLUR_SETTINGS_BLURSETTINGSWINDOW_H_

#include <QWidget>

class QCloseEvent;
class QDBusInterface;
class QSlider;

namespace dcc {
namespace personalization {

class BlurSettingsWindow : public QWidget {
    Q_OBJECT

public:
    explicit BlurSettingsWindow();

public Q_SLOTS:
    void Show();

protected:
    void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
    void onBlurStrengthChanged(int value);
    void onNoiseStrengthChanged(int value);
    void restoreDefaults();
    void confirm();

private:
    struct Values {
        int blurStrength = 3;
        int noiseStrength = 0;
    };

    Values readValues() const;
    bool setOption(const QString &option, int value) const;
    bool setOptionAndKeep(QSlider *slider, const QString &option, int value,
                          int *storedValue);
    void setSliderValues(const Values &values);
    void updateSliderToolTips();
    void restoreOriginalValues();

    QDBusInterface *m_effectIface;
    QSlider *m_blurSlider;
    QSlider *m_noiseSlider;
    Values m_originalValues;
    Values m_currentValues;
    bool m_confirmed = false;
};

}  // namespace personalization
}  // namespace dcc

#endif  // SRC_HELPERS_BLUR_SETTINGS_BLURSETTINGSWINDOW_H_
