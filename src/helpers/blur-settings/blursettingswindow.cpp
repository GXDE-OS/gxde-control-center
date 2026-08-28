/*
 * Copyright (C) 2026 CharOfString <charofstring.cc>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 */

#include "blursettingswindow.h"

#include <QCloseEvent>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace dcc {
namespace personalization {

namespace {

const QString EffectService = QStringLiteral("top.gxde.Wlcom");
const QString EffectPath = QStringLiteral("/top/gxde/Wlcom/Effect");
const QString EffectInterface = QStringLiteral("top.gxde.Wlcom.Effect");
const QString EffectName = QStringLiteral("blur");

constexpr int DefaultBlurStrength = 3;
constexpr int DefaultNoiseStrength = 0;

QDBusMessage call(QDBusInterface *iface, const QString &method,
        const QVariantList &args = QVariantList()) {
    if (!iface || !iface->isValid()) {
        return QDBusMessage::createError(QDBusError::UnknownObject,
            QStringLiteral("invalid interface"));
    }
    return iface->callWithArgumentList(QDBus::Block, method, args);
}

class SliderRow : public QWidget {
public:
    SliderRow(const QString &title, const QString &light, const QString &strong,
                int minimum, int maximum, QWidget *parent)
            : QWidget(parent)
            , slider(new QSlider(Qt::Horizontal)) {
        auto *titleLabel = new QLabel(title);
        titleLabel->setObjectName(QStringLiteral("BlurSettingTitle"));

        auto *lightLabel = new QLabel(light);
        auto *strongLabel = new QLabel(strong);
        lightLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        strongLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lightLabel->setMinimumWidth(55);
        strongLabel->setMinimumWidth(55);

        slider->setRange(minimum, maximum);
        slider->setTracking(true);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(1);
        slider->setPageStep(1);
        slider->setAccessibleName(title);

        auto *sliderLayout = new QHBoxLayout;
        sliderLayout->setContentsMargins(0, 0, 0, 0);
        sliderLayout->setSpacing(8);
        sliderLayout->addWidget(lightLabel);
        sliderLayout->addWidget(slider, 1);
        sliderLayout->addWidget(strongLabel);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        layout->addWidget(titleLabel);
        layout->addLayout(sliderLayout);
    }

    QSlider *slider;
};

}  // namespace

BlurSettingsWindow::BlurSettingsWindow()
        : QWidget(nullptr, Qt::Window | Qt::WindowTitleHint
            | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
        , m_effectIface(new QDBusInterface(EffectService, EffectPath, EffectInterface,
            QDBusConnection::sessionBus(), this))
        , m_blurSlider(nullptr)
        , m_noiseSlider(nullptr) {
    setWindowTitle(tr("Window Background Blur - System Settings"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));
    setFixedSize(440, 300);

    auto *blurRow = new SliderRow(tr("Blur strength:"), tr("Light"), tr("Strong"),
                                  1, 15, this);
    auto *noiseRow = new SliderRow(tr("Noise strength:"), tr("Light"), tr("Strong"),
                                   0, 14, this);
    m_blurSlider = blurRow->slider;
    m_noiseSlider = noiseRow->slider;

    auto *restoreButton = new QPushButton(tr("Restore Defaults"));
    auto *confirmButton = new QPushButton(tr("Confirm"));
    auto *cancelButton = new QPushButton(tr("Cancel"));
    confirmButton->setDefault(true);
    confirmButton->setProperty("recommended", true);
    restoreButton->setMinimumWidth(120);
    confirmButton->setMinimumWidth(100);
    cancelButton->setMinimumWidth(100);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addWidget(restoreButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(confirmButton);
    buttonLayout->addWidget(cancelButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(16);
    layout->addWidget(blurRow);
    layout->addWidget(noiseRow);
    layout->addStretch();
    layout->addLayout(buttonLayout);

    m_originalValues = readValues();
    m_currentValues = m_originalValues;
    setSliderValues(m_currentValues);

    connect(m_blurSlider, &QSlider::valueChanged, this,
            &BlurSettingsWindow::onBlurStrengthChanged);
    connect(m_noiseSlider, &QSlider::valueChanged, this,
            &BlurSettingsWindow::onNoiseStrengthChanged);
    connect(restoreButton, &QPushButton::clicked, this,
            &BlurSettingsWindow::restoreDefaults);
    connect(confirmButton, &QPushButton::clicked, this,
            &BlurSettingsWindow::confirm);
    connect(cancelButton, &QPushButton::clicked, this, &QWidget::close);
}

void BlurSettingsWindow::Show() {
    showNormal();
    raise();
    activateWindow();
}

BlurSettingsWindow::Values BlurSettingsWindow::readValues() const {
    Values values;
    const QDBusMessage reply = call(m_effectIface, QStringLiteral("PrintEffectOptions"),
                                    QVariantList() << EffectName);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return values;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(
        reply.arguments().first().toString().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return values;
    }

    const QJsonObject object = document.object();
    values.blurStrength = qBound(1, object.value(QStringLiteral("blur_strength"))
                                      .toInt(DefaultBlurStrength), 15);
    values.noiseStrength = qBound(0, object.value(QStringLiteral("noise_strength"))
                                      .toInt(DefaultNoiseStrength), 14);
    return values;
}

bool BlurSettingsWindow::setOption(const QString &option, int value) const {
    const QVariant dbusValue = QVariant::fromValue(QDBusVariant(value));
    const QDBusMessage reply = call(m_effectIface, QStringLiteral("SetEffectOption"),
                                    QVariantList() << EffectName << option << dbusValue);
    return reply.type() == QDBusMessage::ReplyMessage;
}

bool BlurSettingsWindow::setOptionAndKeep(QSlider *slider, const QString &option, int value,
        int *storedValue) {
    if (setOption(option, value)) {
        *storedValue = value;
        return true;
    }

    const QSignalBlocker blocker(slider);
    slider->setValue(*storedValue);
    return false;
}

void BlurSettingsWindow::setSliderValues(const Values &values) {
    const QSignalBlocker blurBlocker(m_blurSlider);
    const QSignalBlocker noiseBlocker(m_noiseSlider);
    m_blurSlider->setValue(values.blurStrength);
    m_noiseSlider->setValue(values.noiseStrength);
    updateSliderToolTips();
}

void BlurSettingsWindow::updateSliderToolTips() {
    m_blurSlider->setToolTip(tr("Blur strength: %1").arg(m_blurSlider->value()));
    m_noiseSlider->setToolTip(tr("Noise strength: %1").arg(m_noiseSlider->value()));
}

void BlurSettingsWindow::onBlurStrengthChanged(int value) {
    setOptionAndKeep(m_blurSlider, QStringLiteral("blur_strength"), value,
        &m_currentValues.blurStrength);
    updateSliderToolTips();
}

void BlurSettingsWindow::onNoiseStrengthChanged(int value) {
    setOptionAndKeep(m_noiseSlider, QStringLiteral("noise_strength"), value,
        &m_currentValues.noiseStrength);
    updateSliderToolTips();
}

void BlurSettingsWindow::restoreDefaults() {
    if (setOption(QStringLiteral("blur_strength"), DefaultBlurStrength)) {
        m_currentValues.blurStrength = DefaultBlurStrength;
    }
    if (setOption(QStringLiteral("noise_strength"), DefaultNoiseStrength)) {
        m_currentValues.noiseStrength = DefaultNoiseStrength;
    }
    setSliderValues(m_currentValues);
}

void BlurSettingsWindow::confirm() {
    m_confirmed = true;
    close();
}

void BlurSettingsWindow::restoreOriginalValues() {
    if (m_currentValues.blurStrength != m_originalValues.blurStrength) {
        setOption(QStringLiteral("blur_strength"), m_originalValues.blurStrength);
    }
    if (m_currentValues.noiseStrength != m_originalValues.noiseStrength) {
        setOption(QStringLiteral("noise_strength"), m_originalValues.noiseStrength);
    }
    m_currentValues = m_originalValues;
}

void BlurSettingsWindow::closeEvent(QCloseEvent *event) {
    if (!m_confirmed) {
        restoreOriginalValues();
    }
    QWidget::closeEvent(event);
}

}  // namespace personalization
}  // namespace dcc
