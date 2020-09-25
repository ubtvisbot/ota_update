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
#include <QProcess>
#include "batterymonitor.h"
#include <QMetaType>
#include <QTimer>
#include <QDateTime>
#include "aiboxstate.h"

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

typedef struct _OtaMessage
{
    long type;
    emResultState eState;
}OtaMessage;

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

    /*
     * 给客户端发送结束指令，数据为结果的状态
     */
    void sendFinishMsg(int result);

    bool initOtaWindowsMsg();
    void sendOtaWindowsMsg(emResultState eState);

private:
    void processPacket(QByteArray& data, PacketType type);
    void processHeaderPacket(QByteArray& data);
    void processDataPacket(QByteArray& data);
    void processFinishPacket();
    void processRestorePacket(QByteArray& data);
    void processStopPacket();

    void writePacket(qint32 packetDataSize, PacketType type, const QByteArray& data);

    void startAvahi();
    void stopAvahi();

    /*
     * 强行杀掉avahi的注册进程
     */
    void killAvahi();

signals:

public slots:
    void onAcceptConnection();
    void onReadFromClient();
    void onDisconnected();
    void onNetworkTimeOut();

private:
    QTcpServer *m_pServer;
    QTcpSocket *m_pSocket;

    bool m_bIsConnected;

    QByteArray m_buff;
    int m_nHeaderSize; // 头部的字节长度
    qint32 m_nPacketSize; // 数据长度
    qint64 m_nReceivedBytes; // 接收到的文件大小

    OtaInfo *m_pOtaInfo; // 保存客户端发过来的文件信息
    QFile m_file;

    emAiboxState m_eOtaState;

    int m_nNum;

    QProcess *m_pAvahi;

    int m_nMsgId;

    QTimer* m_pNetworkTimer;
    QDateTime m_lastTime;

};

#endif // TCPSERVER_H
