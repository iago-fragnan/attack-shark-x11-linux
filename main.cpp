#include "atsx11.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    atsx11 w;
    w.show();
    return a.exec();
}
