#include "tcpserver.h"
#include <QtGlobal>

const static quint16 kPort= 17116;

TcpServer::TcpServer(QObject *parent) : QObject(parent)
{
    m_server = new QTcpServer();
    m_server->listen(QHostAddress::Any, kPort);
    connect(m_server, SIGNAL(newConnection()), this, SLOT(onAcceptConnection()));
}

void TcpServer::processPacket(QByteArray &data, PacketType type)
{
    switch (type)
    {
    case PacketType::Header : processHeaderPacket(data); break;
    case PacketType::Data : processDataPacket(data); break;
    case PacketType::Finish : processFinishPacket(data); break;
    case PacketType::Restore : processRestorePacket(data); break;
    case PacketType::Stop : processStopPacket(data); break;
    }

}

void TcpServer::processHeaderPacket(QByteArray &data)
{

}

void TcpServer::processDataPacket(QByteArray &data)
{

}

void TcpServer::processFinishPacket(QByteArray &data)
{

}

void TcpServer::processRestorePacket(QByteArray &data)
{

}

void TcpServer::processPausePacket(QByteArray &data)
{

}

void TcpServer::processStopPacket(QByteArray &data)
{

}

void TcpServer::writePacket(qint32 packetDataSize, PacketType type, const QByteArray &data)
{
    if (m_client)
    {
        m_client->write(reinterpret_cast<const char*>(&packetDataSize), sizeof(packetDataSize));
        m_client->write(reinterpret_cast<const char*>(&type), sizeof(type));
        m_client->write(data);
    }
}

void TcpServer::onAcceptConnection()
{
    m_client = m_server->nextPendingConnection();
    connect(m_client, SIGNAL(readyRead()), this, SLOT(onReadFromClient()));
}

void TcpServer::onReadFromClient()
{
    if (mInfo->getState() == TransferState::Cancelled)
        return;

    mBuff.append(mSocket->readAll());

    while (mBuff.size() >= mHeaderSize) {
        if (mPacketSize < 0) {
            memcpy(&mPacketSize, mBuff.constData(), sizeof(mPacketSize));
            mBuff.remove(0, sizeof(mPacketSize));
        }

        if (mBuff.size() > mPacketSize) {
            PacketType type = static_cast<PacketType>(mBuff.at(0));
            QByteArray data = mBuff.mid(1, mPacketSize);

            processPacket(data, type);
            mBuff.remove(0, mPacketSize + 1);

            mPacketSize = -1;
        }
        else {
            break;
        }
    }

}
