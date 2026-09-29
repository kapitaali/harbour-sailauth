#include "clipboardhelper.h"

ClipboardHelper::ClipboardHelper(QObject *parent) : QObject(parent)
{
}

void ClipboardHelper::setText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

QString ClipboardHelper::text()
{
    return QGuiApplication::clipboard()->text();
}
