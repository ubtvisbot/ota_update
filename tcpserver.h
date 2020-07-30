#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QtGlobal>
#include <QString>
#include <QFile>
#include <string>
#include <pthread.h>

using namespace std;

enum class PacketType : char
{
    Header = 0x01,
    Data,
    Finish,
    Restore,
    Stop
};

typedef struct _OtaInfo
{
    qint64 fileSize;
    QString fileName;
    QString version;
    bool isSaveData;
}OtaInfo;

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

    void start();
    friend void *readThread(void *arg);

private:
    void processPacket(QByteArray& data, PacketType type);
    void processHeaderPacket(QByteArray& data);
    void processDataPacket(QByteArray& data);
    void processFinishPacket();
    void processRestorePacket();
    void processStopPacket();

    void writePacket(qint32 packetDataSize, PacketType type, const QByteArray& data);

    qint32 waitForRead(QByteArray &data, const qint32 bytes);

signals:

public slots:
    void onAcceptConnection();
    void onReadFromClient();
    void onDisconnected();

private:
    QTcpServer *m_server;
    QTcpSocket *m_socket;

    QByteArray m_buff;
    int m_headerSize;
    qint32 m_packetSize;
    qint64 m_receivedBytes;

    OtaInfo *m_otaInfo;
    QFile m_file;

    pthread_t m_thread_id;

    bool m_isData;
    int m_num;
};

#endif // TCPSERVER_H
