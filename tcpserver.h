#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>

enum class PacketType : char
{
    Header = 0x01,
    Data,
    Finish,
    Restore,
    Stop
};

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);

private:
    void processPacket(QByteArray& data, PacketType type);
    void processHeaderPacket(QByteArray& data);
    void processDataPacket(QByteArray& data);
    void processFinishPacket(QByteArray& data);
    void processRestorePacket(QByteArray& data);
    void processStopPacket(QByteArray& data);

    void writePacket(qint32 packetDataSize, PacketType type, const QByteArray& data);

signals:

public slots:
    void onAcceptConnection();
    void onReadFromClient();

private:
    QTcpServer *m_server;
    QTcpSocket *m_client;
};

#endif // TCPSERVER_H
