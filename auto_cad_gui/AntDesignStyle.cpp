#include "AntDesignStyle.h"

#include <QEvent>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QVariantAnimation>
#include <QtAlgorithms>
#include <QtMath>

namespace AntDesignStyle {

const Palette& palette() {
    static const Palette instance;
    return instance;
}

QString styleSheet() {
    return QStringLiteral(R"(
        QWidget#AppShell {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f7f1ff, stop:0.46 #fbfdff, stop:1 #effdf9);
        }
        QWidget#TopBar {
            background: #ffffff;
            border-bottom: 1px solid #edf1f7;
        }
        QLabel#BrandIcon {
            background: #1677ff;
            border-radius: 11px;
            color: #ffffff;
        }
        QPushButton#ActiveNavButton {
            background: #e6f4ff;
            border: 1px solid #91caff;
            color: #1677ff;
            font-weight: 600;
        }
        QPushButton#NavButton:hover {
            background: #f5f8fc;
            color: #1f2937;
        }
        QLineEdit#SearchEdit {
            background: #f5f8fc;
            border: 1px solid transparent;
            border-radius: 21px;
        }
        QLineEdit#SearchEdit:focus {
            background: #ffffff;
            border-color: #1677ff;
        }
        QWidget#GeneratorCard,
        QWidget#QueueCard,
        QWidget#GenerationControlCard,
        QWidget#BoardLengthDataSourceCard,
        QWidget#OutputPathCard,
        QWidget#AiSettingsCard,
        QWidget#ModuleCard {
            background: #ffffff;
            border: 1px solid #f0f3f8;
            border-radius: 18px;
        }
        QLabel#PageTitle {
            font-size: 29px;
            font-weight: 800;
            color: #0f172a;
        }
        QLabel#PageSubtitle {
            font-size: 16px;
            color: #52678a;
        }
        QLabel#CardBigTitle,
        QLabel#QueueTitle,
        QLabel#GenerationTitle,
        QLabel#AiCardTitle {
            color: #111827;
            font-weight: 800;
        }
        QLabel#CardBigTitle,
        QLabel#QueueTitle,
        QLabel#GenerationTitle {
            font-size: 24px;
        }
        QLabel#GenerationSectionTitle {
            color: #111827;
            font-size: 15px;
            font-weight: 750;
            margin-top: 2px;
        }
        QLabel#GenerationStatusBadge {
            border-radius: 13px;
            padding: 0 10px;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#GenerationStatusBadge[state="pending"] {
            background: #f1f5f9;
            color: #64748b;
        }
        QLabel#GenerationStatusBadge[state="ready"] {
            background: #e6f4ff;
            color: #1677ff;
        }
        QLabel#GenerationStatusBadge[state="running"] {
            background: #fff7e6;
            color: #d46b08;
        }
        QLabel#GenerationStatusBadge[state="done"] {
            background: #f6ffed;
            color: #389e0d;
        }
        QLabel#GenerationStatusBadge[state="error"] {
            background: #fff1f0;
            color: #cf1322;
        }
        QLabel#GenerationCompactNote {
            background: #f8fbff;
            border: 1px solid #eef2f7;
            border-radius: 10px;
            color: #74859c;
            padding: 10px 12px;
            font-size: 13px;
            font-weight: 550;
        }
        QFrame#GenerationDivider {
            background: #edf2f7;
            border: none;
        }
        QWidget#GenerationCheckRow {
            background: transparent;
            border: none;
        }
        QLabel#GenerationCheckIcon {
            border-radius: 12px;
            font-size: 12px;
            font-weight: 900;
        }
        QLabel#GenerationCheckIcon[state="ok"] {
            background: #f6ffed;
            color: #389e0d;
        }
        QLabel#GenerationCheckIcon[state="warning"] {
            background: #fff7e6;
            color: #d46b08;
        }
        QLabel#GenerationCheckText {
            color: #52678a;
            font-size: 14px;
            font-weight: 550;
        }
        QLabel#GenerationCheckText[state="ok"] {
            color: #2f5d3a;
        }
        QLabel#GenerationCheckText[state="warning"] {
            color: #8a5a16;
        }
        QLabel#GenerationRuntimeText {
            background: #f8fbff;
            border: 1px solid #e6edf6;
            border-radius: 10px;
            color: #52678a;
            padding: 10px 12px;
            font-size: 14px;
            font-weight: 600;
        }
        QLabel#AiCardTitle {
            font-size: 23px;
        }
        QWidget#TaskFilePanel {
            background: #ffffff;
            border: none;
            border-radius: 14px;
        }
        QPushButton#UploadPromptPage {
            background: #f8fbff;
            border: 2px dashed #d9e2ef;
            border-radius: 14px;
        }
        QPushButton#UploadPromptPage:hover {
            background: #f0f7ff;
            border-color: #91caff;
        }
        QLabel#UploadTitle {
            color: #1f2937;
            font-size: 15px;
            font-weight: 650;
        }
        QWidget#SelectedFileRow,
        QWidget#DataSourcePathBox {
            background: #f8fbff;
            border: 1px solid #d9e2ef;
            border-radius: 12px;
        }
        QLabel#SelectedFileType,
        QLabel#DataSourceIcon {
            background: #e6f4ff;
            border: 1px solid #bae0ff;
            border-radius: 12px;
            color: #1677ff;
        }
        QLabel#SelectedFileName {
            color: #111827;
            font-size: 15px;
            font-weight: 600;
        }
        QLabel#SelectedFileCategory,
        QLabel#MutedText,
        QLabel#DataSourceDescription,
        QLabel#BoardLengthSourceMeta,
        QLabel#OutputPathHint {
            color: #5f6f89;
            font-size: 14px;
        }
        QComboBox#AiComboBox,
        QLineEdit#SettingsInput,
        QLineEdit#PathEdit {
            background: #f8fbff;
            border: 1px solid #d9e2ef;
            border-radius: 9px;
            color: #1f2937;
            selection-background-color: #1677ff;
        }
        QComboBox#AiComboBox:focus,
        QLineEdit#SettingsInput:focus,
        QLineEdit#PathEdit:focus {
            background: #ffffff;
            border-color: #1677ff;
        }
        QPushButton#AiGhostButton,
        QPushButton#SecondaryButton,
        QPushButton#ChangeDataSourceButton,
        QPushButton#DefaultSourceButton,
        QPushButton#SmallActionButton {
            background: #ffffff;
            border: 1px solid #d9e2ef;
            border-radius: 8px;
            color: #3d4d63;
            font-weight: 600;
        }
        QPushButton#AiGhostButton:hover,
        QPushButton#SecondaryButton:hover,
        QPushButton#ChangeDataSourceButton:hover,
        QPushButton#DefaultSourceButton:hover,
        QPushButton#SmallActionButton:hover {
            background: #f5f8fc;
            border-color: #1677ff;
            color: #1677ff;
        }
        QLabel#TaskCount {
            background: #e6f4ff;
            color: #1677ff;
        }
        QLabel#QueueStepNumber {
            background: #f1f5f9;
            color: #8aa0bc;
        }
        QLabel#QueueStepText {
            color: #607590;
            font-size: 16px;
        }
        QLabel#QueueStepText[state="running"] {
            color: #1677ff;
        }
        QLabel#QueueStepText[state="done"] {
            color: #08979c;
        }
        QPushButton#QueuePrimaryButton,
        QPushButton#PrimaryButton,
        QPushButton#AiBlueButton,
        QPushButton#AiPurpleButton {
            background: transparent;
            border: none;
            color: #ffffff;
        }
    )");
}

void applyCardShadow(QWidget* widget, int blurRadius, int yOffset, int alpha) {
    if (!widget) {
        return;
    }

    auto* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(blurRadius);
    shadow->setOffset(0, yOffset);
    QColor color("#0f172a");
    color.setAlpha(alpha);
    shadow->setColor(color);
    widget->setGraphicsEffect(shadow);
}

void applyTopBarShadow(QWidget* widget) {
    applyCardShadow(widget, 20, 3, 16);
}

} // namespace AntDesignStyle

AntPrimaryButton::AntPrimaryButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent),
      accentColor_(AntDesignStyle::palette().primary) {
    setCursor(Qt::PointingHandCursor);
    setFlat(true);
    setMouseTracking(true);

    QFont buttonFont = font();
    buttonFont.setPointSize(15);
    buttonFont.setWeight(QFont::DemiBold);
    setFont(buttonFont);
}

AntPrimaryButton::~AntPrimaryButton() {
    qDeleteAll(ripples_);
    ripples_.clear();
}

void AntPrimaryButton::setAccentColor(const QColor& color) {
    accentColor_ = color;
    update();
}

QColor AntPrimaryButton::currentFillColor() const {
    if (!isEnabled()) {
        return QColor("#d9dee8");
    }
    if (pressed_) {
        return accentColor_.darker(118);
    }
    if (hovered_) {
        return accentColor_.lighter(112);
    }
    return accentColor_;
}

void AntPrimaryButton::startRipple(const QPointF& center) {
    if (clickTimer_.isValid() && clickTimer_.elapsed() < 80) {
        return;
    }
    clickTimer_.restart();

    auto* ripple = new RippleState;
    ripple->center = center;
    ripples_.append(ripple);

    auto* animation = new QVariantAnimation(this);
    animation->setDuration(520);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, ripple](const QVariant& value) {
        ripple->progress = value.toReal();
        ripple->opacity = 0.28 * (1.0 - ripple->progress);
        update();
    });
    connect(animation, &QVariantAnimation::finished, this, [this, ripple, animation]() {
        ripples_.removeOne(ripple);
        delete ripple;
        animation->deleteLater();
        update();
    });
    animation->start();
}

void AntPrimaryButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF buttonRect = rect().adjusted(1, 1, -1, -1);
    QPainterPath clipPath;
    clipPath.addRoundedRect(buttonRect, 12, 12);

    painter.fillPath(clipPath, currentFillColor());
    painter.setClipPath(clipPath);

    const qreal maxRadius = qSqrt(width() * width() + height() * height());
    for (const RippleState* ripple : ripples_) {
        QColor rippleColor("#ffffff");
        rippleColor.setAlphaF(ripple->opacity);
        painter.setBrush(rippleColor);
        painter.setPen(Qt::NoPen);
        const qreal radius = maxRadius * ripple->progress;
        painter.drawEllipse(ripple->center, radius, radius);
    }

    painter.setClipping(false);
    painter.setPen(isEnabled() ? QColor("#ffffff") : QColor("#8b96a8"));
    painter.setFont(font());
    painter.drawText(buttonRect, Qt::AlignCenter, text());
}

void AntPrimaryButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && isEnabled()) {
        pressed_ = true;
        startRipple(event->position());
        update();
    }
    QPushButton::mousePressEvent(event);
}

void AntPrimaryButton::mouseReleaseEvent(QMouseEvent* event) {
    pressed_ = false;
    update();
    QPushButton::mouseReleaseEvent(event);
}

void AntPrimaryButton::enterEvent(QEnterEvent* event) {
    hovered_ = true;
    update();
    QPushButton::enterEvent(event);
}

void AntPrimaryButton::leaveEvent(QEvent* event) {
    hovered_ = false;
    pressed_ = false;
    update();
    QPushButton::leaveEvent(event);
}
