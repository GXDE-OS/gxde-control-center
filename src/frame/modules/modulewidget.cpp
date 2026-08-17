/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "modules/modulewidget.h"
#include "modulewidgetheader.h"

#include <QEvent>
#include <QIcon>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QSvgRenderer>

using namespace dcc::widgets;

namespace dcc {

static const char *ObjectNameTitle = "ModuleHeaderTitle";
static const char *ObjectNameTemplateIcon = "ModuleHeaderIcon%1";

// 设置页模块头部图标。不缓存按 DPR 预渲染的位图，而是在 paintEvent 里
// 按当前绘制设备的像素密度重新渲染 SVG，保证任何 DPR 下都是 1:1 清晰。
//
// 用 DImageButton 时的问题：wlcom 上 wl_output.scale 上报的是 1.25 取整后的
// 2，fractional-scale 事件才把窗口 DPR 修正为 1.25。窗口 DPR 变化后，按旧
// DPR 缓存的位图在绘制时被缩放，产生锯齿；模块在 DPR 事件之后才创建的
// 情况（模块是延迟加载的）则完全收不到该事件，缓存位图永远按 2x 渲染。
class ModuleHeaderIconWidget : public QWidget
{
public:
    explicit ModuleHeaderIconWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(24, 24);
        setDisabled(true);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    void setIconPath(const QString &path)
    {
        if (m_path == path)
            return;

        m_path = path;
        m_renderer.load(path);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (!m_renderer.isValid())
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 仅在尺寸或绘制 DPR 变化时重新渲染，避免滚动时反复光栅化
        const qreal dpr = p.device()->devicePixelRatioF();
        if (m_cachedPixmap.isNull()
            || !qFuzzyCompare(m_cachedPixmap.devicePixelRatio(), dpr)
            || m_cachedPixmap.size() != size() * dpr) {
            m_cachedPixmap = renderIcon(dpr);
        }

        QStyleOption opt;
        opt.initFrom(this);
        style()->drawItemPixmap(&p, rect(), Qt::AlignCenter, m_cachedPixmap);
    }

private:
    QPixmap renderIcon(qreal dpr)
    {
        QPixmap pm(qRound(width() * dpr), qRound(height() * dpr));
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);

        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            m_renderer.render(&p, QRectF(QPointF(0, 0), QSizeF(width(), height())));
        }

        // 与 QLabel/DImageButton 对禁用位图的处理保持一致（置灰），
        // generatedIconPixmap 内部 toImage() 会丢掉 DPR，这里补回去
        if (!isEnabled()) {
            QStyleOption opt;
            opt.initFrom(this);
            pm = style()->generatedIconPixmap(QIcon::Disabled, pm, &opt);
            pm.setDevicePixelRatio(dpr);
        }

        return pm;
    }

    QString m_path;
    QSvgRenderer m_renderer;
    QPixmap m_cachedPixmap;
};

ModuleWidget::ModuleWidget()
    : QWidget(nullptr)
{
    m_moduleIcon = new ModuleHeaderIconWidget;

    m_moduleTitle = new LargeLabel;
    m_moduleTitle->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_moduleTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_moduleTitle->setObjectName(ObjectNameTitle);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    titleLayout->setSpacing(0);
    titleLayout->setContentsMargins(11, 0, 0, 0);
    titleLayout->addWidget(m_moduleIcon);
    titleLayout->setAlignment(m_moduleIcon, Qt::AlignCenter);
    titleLayout->addWidget(m_moduleTitle);

    ModuleWidgetHeader *headerWidget = new ModuleWidgetHeader;
    headerWidget->setLayout(titleLayout);

    m_centralLayout = new QVBoxLayout;
    m_centralLayout->addWidget(headerWidget);
    m_centralLayout->setSpacing(10);
    m_centralLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(m_centralLayout);

    connect(this, &ModuleWidget::objectNameChanged, [this] {
        m_moduleIcon->setObjectName(QString(ObjectNameTemplateIcon).arg(objectName()));
        QString moduleName = objectName().toLower();
        if (moduleName == "sysinfo") {
            moduleName = "systeminfo";
        }
        m_moduleIcon->setIconPath(
            QString(":/%1/themes/dark/icons/nav_%1.svg").arg(moduleName));
    });
}

const QString ModuleWidget::title() const
{
    return m_moduleTitle->text();
}

void ModuleWidget::setTitle(const QString &title)
{
    m_moduleTitle->setText(title);

    setAccessibleName(title);
}

bool ModuleWidget::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest)
        setFixedHeight(m_centralLayout->sizeHint().height());

    return QWidget::event(event);
}

}
