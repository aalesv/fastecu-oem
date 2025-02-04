// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

// Copyright (C) 2019 Ford Motor Company
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "websocketiodevice.h"

template <typename NUMBER>
QByteArray toQByteArray(NUMBER number)
{
    QBuffer buf;
    buf.open(QBuffer::ReadWrite);
    QDataStream stream(&buf);

    stream<<number;
    QByteArray arr=buf.buffer();
    return arr;
}

WebSocketIoDevice::WebSocketIoDevice(QWebSocket *webSocket, QObject *parent)
    : QIODevice(parent)
    , m_socket(webSocket)
{
    open(QIODevice::ReadWrite);
    connect(webSocket, &QWebSocket::disconnected, this, &WebSocketIoDevice::disconnected);
    connect(webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketIoDevice::binaryMessageReceived);
    connect(webSocket, &QWebSocket::bytesWritten, this, &WebSocketIoDevice::bytesWritten);
}

qint64 WebSocketIoDevice::bytesAvailable() const
{
    return QIODevice::bytesAvailable() + m_buffer.size();
}

bool WebSocketIoDevice::isSequential() const
{
    return true;
}

void WebSocketIoDevice::close()
{
    if (m_socket)
        m_socket->close();
}

qint64 WebSocketIoDevice::readData(char *data, qint64 maxlen)
{
    auto sz = std::min(maxlen, qint64(m_buffer.size()));
    if (sz <= 0)
        return sz;
    memcpy(data, m_buffer.constData(), size_t(sz));
    m_buffer.remove(0, sz);
    return sz;
}

qint64 WebSocketIoDevice::writeData(const char *data, qint64 len)
{
    if (m_socket)
    {
        QByteArray message = QByteArray{data, int(len)};
        SEQ_TX++;
        prependHeader(message);
        return m_socket->sendBinaryMessage(message);
    }
    return -1;
}

void WebSocketIoDevice::binaryMessageReceived(const QByteArray &message)
{
    bool pass_message = true;
    QByteArray m = message;
    sequence_t seqRx = getSequenceNumber(message);
    //qDebug() << "SEQ_RX" << SEQ_RX << "seqRx" << seqRx;
    if (isHeaderValid(m))
    {
        if (!isSequenceValid(message))
        {
            qDebug() << "Last RX SEQ was" << SEQ_RX
                     << "but received" << seqRx;
            pass_message = false;
        }
        else
        {
            SEQ_RX = seqRx;
        }
        removeHeader(m);
    }
    else
        qDebug() << "WebSocketIoDevice: Invalid header";
    m_buffer.append(m);
    emit readyRead();
}

WebSocketIoDevice::checksum_t WebSocketIoDevice::calculateChecksum(const QByteArray &message)
{
    checksum_t checksum = 0;
    for(int i=0; i<message.size(); i++)
        checksum += (checksum_t)message[i];
    return ~checksum;
}

bool WebSocketIoDevice::isHeaderValid(const QByteArray &message)
{
    int len = message.size();
    if (len < headerLength)
        return false;
    QByteArray header = message.chopped(len-headerLength+1);
    checksum_t checksum_c = calculateChecksum(header);
    checksum_t checksum_p = (checksum_t)message[headerLength-1];
    if (checksum_c != checksum_p)
        return false;
    else
        return true;
}

bool WebSocketIoDevice::isSequenceValid(const QByteArray &message)
{
    bool r = true;
    sequence_t seqRx = getSequenceNumber(message);
    if (SEQ_RX == 0 || seqRx == 1)
    {
        //If we just started or remote side just started,
        //set sequnce number in any case
        r = true;
    }
    else
    {
        if (SEQ_RX >= seqRx)
        {
            r = false;
        }
    }
    return r;
}

bool WebSocketIoDevice::prependHeader(QByteArray &message)
{
    QByteArray header;
    header.append(toQByteArray(headerLength));
    header.append(toQByteArray(protocolVersion));
    header.append(toQByteArray(SEQ_TX));
    header.append(calculateChecksum(header));
    message.prepend(header);
    return true;
}

bool WebSocketIoDevice::removeHeader(QByteArray &message)
{
    int len = message.size();
    if (len < headerLength)
        return false;
    message = message.remove(0, headerLength);
    return true;
}

WebSocketIoDevice::sequence_t WebSocketIoDevice::getSequenceNumber(const QByteArray &message)
{
    if (message.size()<headerLength)
        return 0;
    //Remove length and version fields
    QByteArray b = message;
    b.remove(0, sizeof(int_t) + sizeof(int_t));
    b.remove(sizeof(sequence_t), b.size()-sizeof(sequence_t));
    QDataStream ds(b);
    sequence_t seq;
    ds >> seq;
    return seq;
}
