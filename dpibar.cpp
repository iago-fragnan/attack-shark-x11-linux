#include "dpibar.h"
#include "dpiscale.h"

#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QVector>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// Resolution handed to QStyle when it paints the groove and the handles. The bar
// only has six stages, so this is just enough precision for the fill to land on
// the pixel it should.
constexpr int kStyleResolution = 10000;

constexpr int kLabelGap = 6;      // px between a row of text and the groove
constexpr int kHitPadding = 4;    // px of slack around a handle for hit-testing
constexpr int kHitMinRadius = 6;
constexpr int kEditorPadding = 8; // px of slack inside the inline editor
constexpr int kEditorMinWidth = 90;
constexpr int kWheelNotch = 120;  // eighths of a degree in one wheel notch

} // namespace

DpiBarWidget::DpiBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    // The bar is a fixed stack of rows, so it asks for its sizeHint height and
    // does not stretch inside its container.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

int DpiBarWidget::handleLength() const
{
    return style()->pixelMetric(QStyle::PM_SliderControlThickness, nullptr, this);
}

int DpiBarWidget::grooveThickness() const
{
    return style()->pixelMetric(QStyle::PM_SliderThickness, nullptr, this);
}

int DpiBarWidget::handleHitRadius() const
{
    return std::max(handleLength() / 2, kHitMinRadius) + kHitPadding;
}

int DpiBarWidget::mainLabelRowTop() const
{
    // Row 0 is reserved for the labels that get staggered upwards when the
    // stages bunch up on the left of the logarithmic scale, row 1 carries the
    // labels that fit.
    return kLabelGap + fontMetrics().height() + kLabelGap;
}

QRect DpiBarWidget::grooveRowRect() const
{
    // Stacked rows: staggered stage labels, stage labels, groove, range labels.
    const int labelHeight = fontMetrics().height();
    const int top = mainLabelRowTop() + labelHeight + kLabelGap;
    const int height = std::max(0, this->height() - top - labelHeight - kLabelGap);
    return QRect(0, top, this->width(), height);
}

QRect DpiBarWidget::grooveRect() const
{
    QStyleOptionSlider opt;
    opt.initFrom(this);
    opt.orientation = Qt::Horizontal;
    opt.rect = grooveRowRect();
    opt.minimum = 0;
    opt.maximum = kStyleResolution;
    opt.sliderPosition = 0;
    opt.sliderValue = 0;
    return style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove);
}

QStyleOptionSlider DpiBarWidget::sliderOption(int sliderPosition) const
{
    QStyleOptionSlider opt;
    opt.initFrom(this);
    opt.orientation = Qt::Horizontal;
    opt.rect = grooveRowRect();
    opt.minimum = 0;
    opt.maximum = kStyleResolution;
    opt.pageStep = 0;
    opt.singleStep = 0;
    opt.sliderPosition = sliderPosition;
    opt.sliderValue = sliderPosition;
    return opt;
}

int DpiBarWidget::stylePositionForStage(int stage) const
{
    const QRect groove = grooveRect();
    if (groove.width() <= 0)
        return 0;
    const int offset = std::clamp(pointCenter(stage).x() - groove.left(), 0, groove.width());
    return static_cast<int>(std::llround(
        static_cast<double>(offset) * kStyleResolution / groove.width()));
}

QPoint DpiBarWidget::pointCenter(int stage) const
{
    const QRect groove = grooveRect();
    const double pos = dpiscale::dpiToPos(m_values[stage]);
    const int x = groove.left()
        + static_cast<int>(std::llround(pos * std::max(0, groove.width())));
    return QPoint(x, groove.center().y());
}

int DpiBarWidget::hitTestPoint(const QPoint &p) const
{
    const int radius = handleHitRadius();
    const int radiusSq = radius * radius;
    int best = -1;
    int bestSq = radiusSq;
    for (int i = 0; i < dpiscale::kNumStages; ++i) {
        const QPoint center = pointCenter(i);
        const int dx = center.x() - p.x();
        const int dy = center.y() - p.y();
        const int distance = dx * dx + dy * dy;
        if (distance <= bestSq) {
            bestSq = distance;
            best = i;
        }
    }
    return best;
}

QSize DpiBarWidget::sizeHint() const
{
    const QFontMetrics fm = fontMetrics();
    const int height = kLabelGap * 4 + fm.height() * 3 + grooveThickness();
    const int width = handleLength() * 2 + fm.horizontalAdvance(QStringLiteral("26000")) * 3;
    return QSize(std::max(width, 320), height);
}

QSize DpiBarWidget::minimumSizeHint() const
{
    const QFontMetrics fm = fontMetrics();
    return QSize(handleLength() * 4 + fm.horizontalAdvance(QStringLiteral("26000")),
                 kLabelGap * 4 + fm.height() * 3 + grooveThickness());
}

void DpiBarWidget::paintGroove(QPainter &p)
{
    // Let the style paint the groove so it is the exact same colour and
    // thickness as the QSlider controls elsewhere in the window. The fill stops
    // at the active stage, which the style derives from sliderPosition.
    QStyleOptionSlider opt = sliderOption(stylePositionForStage(m_activeStage - 1));
    opt.subControls = QStyle::SC_SliderGroove;
    style()->drawComplexControl(QStyle::CC_Slider, &opt, &p, this);
}

QVector<DpiBarWidget::StageLabel> DpiBarWidget::stageLabelLayout() const
{
    const int labelHeight = fontMetrics().height();
    const int mainRowTop = mainLabelRowTop();
    const int activeIndex = m_activeStage - 1;

    QVector<StageLabel> placed;
    // Right edge of the labels already placed on each row. Starts below zero so a
    // label flush against the left edge still fits.
    int rightEdge[2] = { std::numeric_limits<int>::min(), std::numeric_limits<int>::min() };

    for (int i = 0; i < dpiscale::kNumStages; ++i) {
        QFont font = this->font();
        font.setBold(i == activeIndex);
        const QFontMetrics stageFm(font);
        const int width = stageFm.horizontalAdvance(QString::number(m_values[i])) + 6;
        const int left = std::clamp(pointCenter(i).x() - width / 2,
                                    0, std::max(0, this->width() - width));

        // Prefer the main row, fall back to the row above.
        int row = -1;
        for (int r = 1; r >= 0; --r) {
            if (left > rightEdge[r]) {
                row = r;
                break;
            }
        }
        // The active stage is the one the user is looking at, so it is drawn even
        // when it has to share a row with its neighbour.
        if (row < 0 && i == activeIndex)
            row = (rightEdge[0] <= rightEdge[1]) ? 0 : 1;
        if (row < 0)
            continue;

        rightEdge[row] = left + width;
        placed.append({ QRect(left, row == 0 ? mainRowTop - labelHeight : mainRowTop,
                              width, labelHeight),
                        i, i == activeIndex });
    }

    // The active label wins the space it needs: a neighbour it would sit on top of
    // is dropped instead, which keeps every visible label whole. The row above is
    // tried first above, so in practice this only bites at narrow widths.
    const auto active = std::find_if(placed.begin(), placed.end(),
                                     [](const StageLabel &label) { return label.active; });
    if (active != placed.end()) {
        const QRect activeRect = active->rect;
        placed.erase(std::remove_if(placed.begin(), placed.end(),
                                    [&activeRect](const StageLabel &label) {
                                        return !label.active && label.rect.intersects(activeRect);
                                    }),
                     placed.end());
    }

    std::sort(placed.begin(), placed.end(),
              [](const StageLabel &a, const StageLabel &b) { return a.active < b.active; });
    return placed;
}

void DpiBarWidget::paintStageLabels(QPainter &p)
{
    for (const StageLabel &label : stageLabelLayout()) {
        if (label.active) {
            p.fillRect(label.rect, palette().color(QPalette::Window));
            p.setPen(palette().color(QPalette::Highlight));
        } else {
            p.setPen(palette().color(QPalette::Disabled, QPalette::WindowText));
        }
        QFont font = p.font();
        font.setBold(label.active);
        p.setFont(font);
        p.drawText(label.rect, Qt::AlignCenter, QString::number(m_values[label.stage]));
    }
}

void DpiBarWidget::paintPoints(QPainter &p)
{
    for (int i = 0; i < dpiscale::kNumStages; ++i) {
        const bool active = (i + 1 == m_activeStage);
        QStyleOptionSlider opt = sliderOption(stylePositionForStage(i));
        opt.subControls = QStyle::SC_SliderHandle;
        // Fusion paints the handle in the highlight colour only while the handle
        // is the active sub-control, which is what tells the selected stage apart.
        opt.activeSubControls = active ? QStyle::SC_SliderHandle : QStyle::SC_None;
        style()->drawComplexControl(QStyle::CC_Slider, &opt, &p, this);
    }
}

void DpiBarWidget::paintRangeLabels(QPainter &p)
{
    const QFontMetrics fm = fontMetrics();
    const QRect groove = grooveRect();
    const QRect row(groove.left(), groove.bottom() + kLabelGap,
                    groove.width(), fm.height());

    p.setPen(palette().color(QPalette::Disabled, QPalette::WindowText));
    p.drawText(QRect(row.left(), row.top(), row.width() / 2, row.height()),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("50"));
    p.drawText(QRect(row.center().x(), row.top(), row.width() / 2, row.height()),
               Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("26000"));
}

void DpiBarWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    paintStageLabels(p);
    paintGroove(p);
    paintPoints(p);
    paintRangeLabels(p);
}

void DpiBarWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int stage = hitTestPoint(event->pos());
    if (stage < 0) {
        QWidget::mousePressEvent(event);
        return;
    }
    // Qt already grabs the mouse implicitly for the duration of the press, and
    // that implicit grab is what keeps the move and release events coming when
    // the pointer leaves the window. A manual grabMouse() on top of it is what
    // breaks under Wayland and XWayland, which is what Plasma runs, so the drag
    // relies on the implicit one only now.
    m_pressStage = stage;
    m_pressPos = event->pos();
    m_dragStage = -1;
    setActiveStage(stage + 1);
    event->accept();
}

void DpiBarWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressStage < 0) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (m_dragStage < 0) {
        // Not a drag yet: wait for the pointer to travel past the platform drag
        // distance before touching any value, so a plain click or a double click
        // on a handle cannot nudge it.
        if ((event->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
            return;
        m_dragStage = m_pressStage;
        m_dragStartValue = m_values[m_dragStage];
        setCursor(Qt::ClosedHandCursor);
    }

    const QRect groove = grooveRect();
    if (groove.width() <= 0) {
        event->accept();
        return;
    }
    const double pos = static_cast<double>(event->pos().x() - groove.left()) / groove.width();
    m_values[m_dragStage] = dpiscale::posToDpi(pos, true);
    dpiscale::clampOrdered(m_values);
    update();
    event->accept();
}

void DpiBarWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_pressStage = -1;
    endDrag();
    event->accept();
}

void DpiBarWidget::endDrag()
{
    if (m_dragStage < 0) {
        unsetCursor();
        return;
    }
    const int stage = m_dragStage;
    const int previous = m_dragStartValue;
    m_dragStage = -1;
    unsetCursor();
    update();
    if (m_values[stage] != previous)
        emit valueChanged(stage + 1, m_values[stage]);
}

void DpiBarWidget::focusOutEvent(QFocusEvent *event)
{
    // Losing focus mid drag means the release event will never arrive. Without
    // this the widget stays stuck with a closed-hand cursor and a half applied
    // value, which is the "stuck drag" reported on Plasma.
    m_pressStage = -1;
    endDrag();
    QWidget::focusOutEvent(event);
}

void DpiBarWidget::hideEvent(QHideEvent *event)
{
    m_pressStage = -1;
    endDrag();
    QWidget::hideEvent(event);
}

bool DpiBarWidget::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate) {
        m_pressStage = -1;
        endDrag();
    }
    return QWidget::event(event);
}

void DpiBarWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int stage = hitTestPoint(event->pos());
    if (stage >= 0)
        openEditor(stage);
}

void DpiBarWidget::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint wheelPos = event->position().toPoint();
#else
    const QPoint wheelPos = event->pos();
#endif
    const int deltaY = event->angleDelta().y();
    if (deltaY == 0) {
        event->ignore();
        return;
    }

    // High resolution wheels and trackpads report a stream of small deltas, so
    // stepping once per event makes the value jump on every pixel of movement.
    // Accumulate until there is a full notch worth of travel and step once per
    // notch, which is what the rest of the desktop does.
    m_wheelRemainder += deltaY;
    const int notches = m_wheelRemainder / kWheelNotch;
    if (notches == 0) {
        event->ignore();
        return;
    }
    m_wheelRemainder -= notches * kWheelNotch;

    int stage = hitTestPoint(wheelPos);
    if (stage < 0) {
        // On the track but not on a point: adjust the active stage.
        const QRect band = grooveRect().adjusted(-40, -16, 40, 16);
        if (!band.contains(wheelPos)) {
            event->ignore();
            return;
        }
        stage = m_activeStage - 1;
    }

    const int dir = (notches > 0) ? dpiscale::kStep : -dpiscale::kStep;
    for (int i = 0; i < std::abs(notches); ++i)
        stepValue(stage, dir);
    event->accept();
}

void DpiBarWidget::keyPressEvent(QKeyEvent *event)
{
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    int delta = 0;
    switch (event->key()) {
    case Qt::Key_Left:
        delta = shift ? -250 : -50;
        break;
    case Qt::Key_Right:
        delta = shift ? 250 : 50;
        break;
    case Qt::Key_Up:
        setActiveStage(m_activeStage > 1 ? m_activeStage - 1 : dpiscale::kNumStages);
        event->accept();
        return;
    case Qt::Key_Down:
        setActiveStage(m_activeStage < dpiscale::kNumStages ? m_activeStage + 1 : 1);
        event->accept();
        return;
    case Qt::Key_F2:
        if (m_editor == nullptr)
            openEditor(m_activeStage - 1);
        event->accept();
        return;
    default:
        // Tab and Backtab are deliberately not handled: swallowing them traps
        // keyboard focus inside the bar and makes it impossible to reach the
        // rest of the window with the keyboard.
        QWidget::keyPressEvent(event);
        return;
    }
    stepValue(m_activeStage - 1, delta);
    event->accept();
}

bool DpiBarWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_editor && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            closeEditor();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DpiBarWidget::stepValue(int stage, int delta)
{
    const int next = dpiscale::roundToStep(m_values[stage] + delta);
    if (next == m_values[stage])
        return;
    m_values[stage] = next;
    dpiscale::clampOrdered(m_values);
    update();
    emit valueChanged(stage + 1, m_values[stage]);
}

void DpiBarWidget::openEditor(int stage)
{
    closeEditor();
    m_editStage = stage;

    const QFontMetrics fm = fontMetrics();
    const int editorWidth = std::max(
        kEditorMinWidth, fm.horizontalAdvance(QStringLiteral("26000")) + kEditorPadding * 3);
    const int editorHeight = fm.height() + kEditorPadding * 2;

    m_editor = new QLineEdit(this);
    m_editor->setText(QString::number(m_values[stage]));
    m_editor->selectAll();
    m_editor->setAlignment(Qt::AlignCenter);
    m_editor->setFixedSize(editorWidth, editorHeight);

    const QPoint c = pointCenter(stage);
    const int x = std::clamp(c.x() - editorWidth / 2, 0, width() - editorWidth);
    const int y = std::clamp(c.y() - editorHeight / 2, kLabelGap, height() - editorHeight - kLabelGap);
    m_editor->move(x, y);

    m_editor->installEventFilter(this);
    connect(m_editor, &QLineEdit::editingFinished,
            this, &DpiBarWidget::applyEditedValue);
    m_editor->show();
    m_editor->setFocus();
    update();
}

void DpiBarWidget::closeEditor()
{
    if (!m_editor)
        return;
    m_editor->disconnect(this);
    m_editor->removeEventFilter(this);
    m_editor->deleteLater();
    m_editor = nullptr;
    setFocus();
    update();
}

void DpiBarWidget::applyEditedValue()
{
    if (!m_editor)
        return;
    bool ok = false;
    const int raw = m_editor->text().toInt(&ok);
    closeEditor();
    if (!ok)
        return;
    const int val = dpiscale::roundToStep(raw);
    m_values[m_editStage] = val;
    dpiscale::clampOrdered(m_values);
    update();
    emit valueChanged(m_editStage + 1, m_values[m_editStage]);
}

void DpiBarWidget::setValues(const int *values)
{
    int old[dpiscale::kNumStages];
    for (int i = 0; i < dpiscale::kNumStages; ++i)
        old[i] = m_values[i];
    for (int i = 0; i < dpiscale::kNumStages; ++i)
        m_values[i] = values[i];
    dpiscale::clampOrdered(m_values);
    for (int i = 0; i < dpiscale::kNumStages; ++i) {
        if (m_values[i] != old[i])
            emit valueChanged(i + 1, m_values[i]);
    }
    update();
}

void DpiBarWidget::setActiveStage(int stage)
{
    const int s = std::clamp(stage, 1, dpiscale::kNumStages);
    if (m_activeStage == s)
        return;
    m_activeStage = s;
    update();
    emit stageActivated(s);
}