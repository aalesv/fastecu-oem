// Copyright (C) 2019 Ford Motor Company
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef WEBSOCKETIODEVICE_H
#define WEBSOCKETIODEVICE_H

#include <QBuffer>
#include <QIODevice>
#include <QPointer>
#include <QtWebSockets/QtWebSockets>

class WebSocketIoDevice : public QIODevice
{
    using sequence_t = quint64;
    using checksum_t = uint8_t;
    using int_t = uint8_t;
    Q_OBJECT
public:
    WebSocketIoDevice(QWebSocket *webSocket, QObject *parent = nullptr);

signals:
    void disconnected();

    // QIODevice interface
public:
    qint64 bytesAvailable() const override;
    bool isSequential() const override;
    void close() override;

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    void binaryMessageReceived(const QByteArray &message);

private:
    //Protocol structure
    //[Length with checksum][Version][Header][Checksum]
    //For version 1 header is [sequence number]
    const int_t protocolVersion = 1;
    const int_t headerLength =
        sizeof(int_t) + sizeof(int_t) + sizeof(sequence_t) + sizeof(checksum_t);
    sequence_t SEQ_TX = 0;
    sequence_t SEQ_RX = 0;
    bool prependHeader(QByteArray &message);
    bool removeHeader(QByteArray &message);
    checksum_t calculateChecksum(const QByteArray &message);
    bool isHeaderValid(const QByteArray &message);
    bool isSequenceValid(const QByteArray &message);
    sequence_t getSequenceNumber(const QByteArray &message);
    QPointer<QWebSocket> m_socket;
    QByteArray m_buffer;
};

#endif // WEBSOCKETIODEVICE_H
