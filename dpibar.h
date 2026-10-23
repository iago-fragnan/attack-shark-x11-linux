#ifndef DPIBAR_H
#define DPIBAR_H

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QWidget>

class QLineEdit;
class QPainter;
class QStyleOptionSlider;

class DpiBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DpiBarWidget(QWidget *parent = nullptr);

    void setValues(const int values[6]);
    void setActiveStage(int stage);
    int activeStage() const { return m_activeStage; }
    const int *values() const { return m_values; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // Where each stage label ends up for the current values and width. Stages
    // bunch up on the left of the logarithmic scale, so a label that has no room
    // is moved to the row above and one that still has no room is left out
    // rather than drawn on top of its neighbour. Exposed so the tests can assert
    // that no two labels overlap.
    struct StageLabel {
        QRect rect;
        int stage = 0;
        bool active = false;
    };
    QVector<StageLabel> stageLabelLayout() const;

signals:
    void valueChanged(int stage, int dpi);
    void stageActivated(int stage);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // Geometry comes from the style and from the font metrics rather than from
    // hardcoded pixels, so the bar tracks the platform theme, the HiDPI scale and
    // the user's font size the same way the rest of the window does.
    QRect grooveRowRect() const;
    QRect grooveRect() const;
    int mainLabelRowTop() const;
    QStyleOptionSlider sliderOption(int sliderPosition) const;
    int stylePositionForStage(int stage) const;
    QPoint pointCenter(int stage) const;
    int hitTestPoint(const QPoint &p) const;
    int handleHitRadius() const;
    int grooveThickness() const;
    int handleLength() const;

    void paintGroove(QPainter &p);
    void paintStageLabels(QPainter &p);
    void paintPoints(QPainter &p);
    void paintRangeLabels(QPainter &p);

    void stepValue(int stage, int delta);
    void endDrag();
    void openEditor(int stage);
    void closeEditor();
    void applyEditedValue();

    int m_values[6] = {800, 1600, 2400, 3200, 5000, 22000};
    int m_activeStage = 1;
    // A press arms the drag but only commits to it once the pointer travels
    // further than the platform drag distance, so a plain click or a double
    // click on a handle cannot nudge the value.
    int m_pressStage = -1;
    QPoint m_pressPos;
    int m_dragStage = -1;
    int m_dragStartValue = 0;
    // Unconsumed wheel movement, so one notch always means one step.
    int m_wheelRemainder = 0;
    QLineEdit *m_editor = nullptr;
    int m_editStage = 0;
};

#endif // DPIBAR_H