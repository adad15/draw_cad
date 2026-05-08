#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QPushButton>
#include <QString>

class QWidget;
class QEnterEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;

namespace AntDesignStyle {

struct Palette {
    QColor primary = QColor("#1677ff");
    QColor primaryHover = QColor("#4096ff");
    QColor primaryActive = QColor("#0958d9");
    QColor text = QColor("#1f2937");
    QColor textSecondary = QColor("#5f6f89");
    QColor border = QColor("#d9e2ef");
    QColor surface = QColor("#ffffff");
    QColor surfaceMuted = QColor("#f5f8fc");
};

const Palette& palette();
QString styleSheet();
void applyCardShadow(QWidget* widget, int blurRadius = 34, int yOffset = 10, int alpha = 22);
void applyTopBarShadow(QWidget* widget);

} // namespace AntDesignStyle

class AntPrimaryButton final : public QPushButton {
public:
    explicit AntPrimaryButton(const QString& text, QWidget* parent = nullptr);
    ~AntPrimaryButton() override;

    void setAccentColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct RippleState {
        QPointF center;
        qreal progress = 0.0;
        qreal opacity = 0.0;
    };

    QColor currentFillColor() const;
    void startRipple(const QPointF& center);

    QColor accentColor_;
    bool hovered_ = false;
    bool pressed_ = false;
    QList<RippleState*> ripples_;
    QElapsedTimer clickTimer_;
};
