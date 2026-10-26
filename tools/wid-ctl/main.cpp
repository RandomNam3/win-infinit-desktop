/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    wid-ctl: sends one request to the running canvas service and prints the answer.

        wid-ctl state
        wid-ctl pan dx=-500 dy=0
        wid-ctl setCamera x=0 y=0 zoom=0.5 monitor=0
        wid-ctl --wait 10 version

    Values that look like numbers or true/false are sent as such. Exit code 0 on success,
    1 for an error answer, 2 when the service is not reachable.
*/
#include "ipcserver.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QThread>

#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList arguments = app.arguments().mid(1);
    int waitSeconds = 0;
    if (arguments.size() >= 2 && arguments.first() == QLatin1String("--wait")) {
        waitSeconds = arguments.at(1).toInt();
        arguments = arguments.mid(2);
    }
    if (arguments.isEmpty()) {
        std::fputs("usage: wid-ctl [--wait SECONDS] METHOD [key=value ...]   (key=@file: the value is read from a file)\n", stderr);
        return 1;
    }
    QJsonObject params;
    for (const QString &argument : arguments.mid(1)) {
        const int equals = argument.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            continue;
        }
        const QString key = argument.left(equals);
        QString value = argument.mid(equals + 1);
        if (value.startsWith(QLatin1Char('@'))) {
            // key=@file: the value is the file's text (JSON documents for setGroups, setAreas).
            QFile file(value.mid(1));
            if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) {
                std::fprintf(stderr, "wid-ctl: cannot read %s\n", qPrintable(value.mid(1)));
                return 1;
            }
            value = QString::fromUtf8(file.readAll()).trimmed();
            if (value.startsWith(QChar(0xfeff))) {
                value.remove(0, 1);
            }
            params.insert(key, value);
            continue;
        }
        bool isNumber = false;
        const double number = value.toDouble(&isNumber);
        if (isNumber) {
            params.insert(key, number);
        } else if (value == QLatin1String("true") || value == QLatin1String("false")) {
            params.insert(key, value == QLatin1String("true"));
        } else {
            params.insert(key, value);
        }
    }

    QLocalSocket socket;
    QElapsedTimer waited;
    waited.start();
    for (;;) {
        socket.connectToServer(Wid::ipcServerName());
        if (socket.waitForConnected(1000)) {
            break;
        }
        if (waited.elapsed() >= qint64(waitSeconds) * 1000) {
            std::fprintf(stderr, "wid-ctl: the canvas service is not running (%s)\n", qPrintable(socket.errorString()));
            return 2;
        }
        QThread::msleep(250);
    }
    const QJsonObject request{{QStringLiteral("id"), 1}, {QStringLiteral("method"), arguments.first()}, {QStringLiteral("params"), params}};
    socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    socket.flush();
    // Events (statusChanged) may come first; wait for the answer with our id.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        if (!socket.canReadLine() && !socket.waitForReadyRead(int(10000 - timer.elapsed()))) {
            break;
        }
        while (socket.canReadLine()) {
            const QJsonObject answer = QJsonDocument::fromJson(socket.readLine()).object();
            if (answer.value(QStringLiteral("id")).toInt() != 1) {
                continue;
            }
            if (answer.contains(QStringLiteral("error"))) {
                std::fprintf(stderr, "wid-ctl: %s\n", qPrintable(answer.value(QStringLiteral("error")).toString()));
                return 1;
            }
            const QJsonValue result = answer.value(QStringLiteral("result"));
            QByteArray text;
            if (result.isObject()) {
                text = QJsonDocument(result.toObject()).toJson(QJsonDocument::Indented);
            } else if (result.isArray()) {
                text = QJsonDocument(result.toArray()).toJson(QJsonDocument::Indented);
            } else {
                text = QJsonDocument(QJsonArray{result}).toJson(QJsonDocument::Compact);
                text = text.mid(1, text.size() - 2) + '\n';
            }
            std::fwrite(text.constData(), 1, std::size_t(text.size()), stdout);
            return 0;
        }
    }
    std::fputs("wid-ctl: no answer\n", stderr);
    return 2;
}
