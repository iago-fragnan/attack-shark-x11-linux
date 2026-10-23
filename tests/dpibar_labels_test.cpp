#include "../dpibar.h"
#include "../dpiscale.h"

#include <QApplication>
#include <cstdio>

static int g_failures = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_failures; } \
} while(0)

struct Case {
    const char *name;
    int values[6];
};

// Every set from the report and from the shapes that make the stages bunch up on
// the logarithmic scale.
static const Case kCases[] = {
    { "reported 1200..22000", { 1200, 1600, 2400, 3400, 5000, 22000 } },
    { "all in the bottom decade", { 400, 450, 500, 550, 600, 800 } },
    { "bunched in the middle", { 800, 1200, 1600, 2000, 2400, 3200 } },
    { "widest range", { 50, 400, 1200, 4000, 12000, 26000 } },
    { "only two distinct values", { 800, 800, 800, 800, 800, 26000 } },
};

static bool overlaps(const QRect &a, const QRect &b)
{
    return a.intersects(b);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // The width the bar has inside the window, and a couple of others so the
    // placement is not only correct at one size.
    const int widths[] = { 320, 620, 900 };

    for (const Case &c : kCases) {
        for (int width : widths) {
            DpiBarWidget bar;
            bar.setValues(c.values);
            bar.setActiveStage(4);
            bar.resize(width, bar.sizeHint().height());

            const QVector<DpiBarWidget::StageLabel> labels = bar.stageLabelLayout();

            std::printf("--- %s @ %dpx -> %lld label(s) ---\n",
                        c.name, width, static_cast<long long>(labels.size()));

            EXPECT(!labels.isEmpty(), "se dibuja al menos una etiqueta");

            // The invariant the report is about: no label may print over another.
            bool collided = false;
            for (int i = 0; i < labels.size(); ++i) {
                for (int j = i + 1; j < labels.size(); ++j) {
                    if (overlaps(labels[i].rect, labels[j].rect)) {
                        std::printf("  overlap: stage %d %dx%d at %d,%d  vs  stage %d %dx%d at %d,%d\n",
                                    labels[i].stage + 1, labels[i].rect.width(), labels[i].rect.height(),
                                    labels[i].rect.x(), labels[i].rect.y(),
                                    labels[j].stage + 1, labels[j].rect.width(), labels[j].rect.height(),
                                    labels[j].rect.x(), labels[j].rect.y());
                        collided = true;
                    }
                }
            }
            EXPECT(!collided, "ninguna etiqueta se superpone con otra");

            // The active stage is the one the value readout and the Apply button
            // talk about, so it must always be on screen.
            bool activePresent = false;
            for (const auto &label : labels)
                if (label.active)
                    activePresent = true;
            EXPECT(activePresent, "la etapa activa siempre tiene etiqueta");

            // Nothing may be painted outside the widget.
            for (const auto &label : labels) {
                EXPECT(label.rect.left() >= 0 && label.rect.right() < bar.width(),
                       "la etiqueta entra en el ancho del widget");
                EXPECT(label.rect.top() >= 0 && label.rect.bottom() < bar.height(),
                       "la etiqueta entra en el alto del widget");
            }

            // Each stage is labelled at most once.
            for (int i = 0; i < labels.size(); ++i)
                for (int j = i + 1; j < labels.size(); ++j)
                    EXPECT(labels[i].stage != labels[j].stage,
                           "una etapa no se etiqueta dos veces");
        }
    }

    if (g_failures) {
        std::printf("\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll stage label tests PASSED\n");
    return 0;
}
