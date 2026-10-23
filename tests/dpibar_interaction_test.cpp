#include "../dpibar.h"
#include "../dpiscale.h"

#include <QApplication>
#include <QFocusEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cstdio>

namespace {

// Explicit events rather than the QTest mouse/keyboard helpers: those go through
// the platform layer and can be consumed before they reach the widget, which
// would make these tests pass against the very bugs they are meant to catch.

void sendKeyPress(QWidget *target, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(target, &event);
}

void sendMouseMove(QWidget *target, const QPoint &pos, Qt::MouseButtons buttons = Qt::NoButton)
{
    QMouseEvent event(QEvent::MouseMove, pos, target->mapToGlobal(pos),
                      Qt::NoButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void sendMousePress(QWidget *target, const QPoint &pos, Qt::MouseButton button)
{
    QMouseEvent event(QEvent::MouseButtonPress, pos, target->mapToGlobal(pos),
                      button, button, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void sendMouseRelease(QWidget *target, const QPoint &pos, Qt::MouseButton button)
{
    QMouseEvent event(QEvent::MouseButtonRelease, pos, target->mapToGlobal(pos),
                      button, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void sendWheel(QWidget *target, int deltaY)
{
    const QPointF center(target->width() / 2.0, target->height() / 2.0);
    const QPoint pixel(0, deltaY / 4);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF global = target->mapToGlobal(center.toPoint());
    QWheelEvent event(center, global, deltaY >= 0 ? pixel : -pixel,
                      QPoint(0, deltaY),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
#else
    const QPoint global = target->mapToGlobal(center.toPoint());
    QWheelEvent event(global, deltaY >= 0 ? pixel : -pixel,
                      QPointF(0, deltaY), Qt::NoButton, Qt::NoModifier);
#endif
    QApplication::sendEvent(target, &event);
}

const int kDefaults[6] = {800, 1600, 2400, 3200, 5000, 22000};

// Finds an (x, y) that lands on one of the stage handles, so the drag tests do
// not need to duplicate the widget's own geometry maths.
bool findHandle(DpiBarWidget *bar, QPoint *out)
{
    for (int y = bar->height() / 3; y <= bar->height() - 4; y += 4) {
        for (int x = 8; x <= bar->width() - 8; x += 4) {
            const int before = bar->values()[0];
            sendMousePress(bar, QPoint(x, y), Qt::LeftButton);
            sendMouseMove(bar, QPoint(x + 60, y), Qt::LeftButton);
            sendMouseRelease(bar, QPoint(x + 60, y), Qt::LeftButton);
            if (bar->values()[0] != before) {
                *out = QPoint(x, y);
                return true;
            }
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    int failures = 0;
    auto check = [&](bool condition, const char *what) {
        if (condition) {
            std::printf("  ok:   %s\n", what);
        } else {
            std::printf("  FAIL: %s\n", what);
            ++failures;
        }
    };

    // ---------------------------------------------------------------- wheel ---
    std::printf("--- la rueda acumula hasta un notch completo ---\n");
    {
        DpiBarWidget bar;
        bar.resize(700, 90);
        bar.show();
        bar.setActiveStage(1);
        const int before = bar.values()[0];
        const QSignalSpy spy(&bar, &DpiBarWidget::valueChanged);

        // A high resolution wheel emits a stream of small deltas; none of them
        // on its own is a full notch, so the value must not move.
        for (int i = 0; i < 7; ++i)
            sendWheel(&bar, 15);
        check(bar.values()[0] == before, "7 deltas de 15 no mueven el valor");
        check(spy.count() == 0, "ningun valueChanged con deltas parciales");

        sendWheel(&bar, 15);
        check(bar.values()[0] == before + dpiscale::kStep,
              "al completar 120 unidades el valor avanza un paso");
        check(spy.count() == 1, "un solo valueChanged por notch");

        sendWheel(&bar, 240);
        check(bar.values()[0] == before + 3 * dpiscale::kStep,
              "un delta de 240 avanza dos pasos");
    }

    // ------------------------------------------------------------- keyboard ---
    std::printf("--- las flechas mueven valor y etapa por separado ---\n");
    {
        DpiBarWidget bar;
        bar.resize(700, 90);
        bar.show();
        bar.setActiveStage(2);
        const int valueBefore = bar.values()[1];
        const int stageBefore = bar.activeStage();

        sendKeyPress(&bar, Qt::Key_Right);
        check(bar.values()[1] == valueBefore + dpiscale::kStep,
              "flecha derecha sube el valor de la etapa activa");
        check(bar.activeStage() == stageBefore, "flecha derecha no cambia de etapa");

        sendKeyPress(&bar, Qt::Key_Left);
        check(bar.values()[1] == valueBefore, "flecha izquierda baja el valor");

        sendKeyPress(&bar, Qt::Key_Right, Qt::ShiftModifier);
        check(bar.values()[1] == valueBefore + 250, "con Shift el paso es grueso");

        sendKeyPress(&bar, Qt::Key_Down);
        check(bar.activeStage() == stageBefore + 1, "flecha abajo cambia de etapa");
        sendKeyPress(&bar, Qt::Key_Up);
        check(bar.activeStage() == stageBefore, "flecha arriba vuelve a la etapa anterior");
    }

    std::printf("--- Tab no se usa para cambiar de etapa ---\n");
    {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *bar = new DpiBarWidget;
        auto *edit = new QLineEdit;
        layout->addWidget(bar);
        layout->addWidget(edit);
        window.show();
        QTest::qWaitForWindowExposed(&window);
        bar->setActiveStage(1);

        const int stageBefore = bar->activeStage();
        sendKeyPress(bar, Qt::Key_Tab);
        check(bar->activeStage() == stageBefore,
              "Tab deja la etapa activa como estaba");

        const int stageBeforeBacktab = bar->activeStage();
        sendKeyPress(bar, Qt::Key_Backtab);
        check(bar->activeStage() == stageBeforeBacktab,
              "Backtab deja la etapa activa como estaba");

        // The bar must stay in the normal focus chain.
        bar->setFocus(Qt::OtherFocusReason);
        check(bar->hasFocus(), "la barra recibe el foco");
        QTest::keyClick(bar, Qt::Key_Tab);
        check(edit->hasFocus(), "Tab lleva el foco al control siguiente");
    }

    // ----------------------------------------------------------------- drag ---
    std::printf("--- un drag que pierde el foco se cancela ---\n");
    {
        DpiBarWidget bar;
        bar.resize(700, 90);
        bar.show();
        bar.setValues(kDefaults);
        QPoint handle;
        const bool found = findHandle(&bar, &handle);
        check(found, "se encontro un handle para arrastrar");

        if (found) {
            bar.setValues(kDefaults);
            sendMousePress(&bar, handle, Qt::LeftButton);
            sendMouseMove(&bar, handle + QPoint(50, 0), Qt::LeftButton);
            const int dragged = bar.values()[0];
            check(dragged != kDefaults[0], "el drag modifica el valor al mover");

            // The release never arrives, so the widget must let go of the drag
            // when focus goes away.
            QFocusEvent out(QEvent::FocusOut, Qt::OtherFocusReason);
            QApplication::sendEvent(&bar, &out);

            const int afterFocusOut = bar.values()[0];
            check(dragged == afterFocusOut, "el valor queda en el ultimo punto valido");
            check(bar.cursor().shape() == Qt::ArrowCursor,
                  "el cursor vuelve al normal al perder el foco");

            sendMouseMove(&bar, QPoint(bar.width() - 20, handle.y()), Qt::LeftButton);
            sendMouseMove(&bar, QPoint(20, handle.y()), Qt::LeftButton);
            check(bar.values()[0] == afterFocusOut,
                  "tras perder el foco el drag ya no sigue al puntero");
        }
    }

    std::printf("--- la ventana se desactiva durante el drag ---\n");
    {
        DpiBarWidget bar;
        bar.resize(700, 90);
        bar.show();
        bar.setValues(kDefaults);
        QPoint handle;
        if (findHandle(&bar, &handle)) {
            bar.setValues(kDefaults);
            sendMousePress(&bar, handle, Qt::LeftButton);
            sendMouseMove(&bar, handle + QPoint(50, 0), Qt::LeftButton);
            const int dragged = bar.values()[0];

            QEvent deactivate(QEvent::WindowDeactivate);
            QApplication::sendEvent(&bar, &deactivate);
            check(bar.cursor().shape() == Qt::ArrowCursor,
                  "desactivar la ventana suelta el drag");

            sendMouseMove(&bar, QPoint(20, handle.y()), Qt::LeftButton);
            check(bar.values()[0] == dragged,
                  "tras desactivar la ventana el drag ya no sigue al puntero");
        }
    }

    // --------------------------------------------------- press vs drag -------
    std::printf("--- un click simple no mueve el valor ---\n");
    {
        DpiBarWidget bar;
        bar.resize(700, 90);
        bar.show();
        bar.setValues(kDefaults);
        QPoint handle;
        if (findHandle(&bar, &handle)) {
            bar.setValues(kDefaults);
            const int reset = bar.values()[0];
            const QSignalSpy spy(&bar, &DpiBarWidget::valueChanged);
            // A press that does not travel must not change anything, otherwise a
            // double click on a handle nudges the value before the editor opens.
            sendMousePress(&bar, handle, Qt::LeftButton);
            sendMouseMove(&bar, handle, Qt::LeftButton);
            sendMouseRelease(&bar, handle, Qt::LeftButton);
            check(spy.count() == 0, "un click sin movimiento no emite valueChanged");
            check(bar.values()[0] == reset, "un click sin movimiento no cambia el valor");
        }
    }

    if (failures) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll dpi bar interaction tests PASSED\n");
    return 0;
}