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
#include "mainwindow.h"

using namespace std;

enum class PacketType : char
{
    Header = 0x01,
    Data,
    Finish,
    Restore,
    Stop
};

enum emAiboxState
{
    eAiboxIdle= 0,              //空闲状态
    eAiboxToUpdate = 1,         //升级中
    eAiboxUpdateSuccess,        //升级成功
    eAiboxUpdateFail,           //升级失败
    eAiboxToRestore,            //恢复系统中
    eAiboxRestoreSuccess,       //恢复系统成功
    eAiboxRestoreFail,          //恢复系统失败
    eAiboxFileSendingPause,     //文件发送暂停
    eAiboxFileSendingResume,    //文件发送恢复
    eAiboxFileSendingSuccess,   //文件发送成功
    eAiboxFileSendingFail,      //文件发送失败
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
    explicit TcpServer(MainWindow *w, QObject *parent = nullptr);
    ~TcpServer();

private:
    void processPacket(QByteArray& data, PacketType type);
    void processHeaderPacket(QByteArray& data);
    void processDataPacket(QByteArray& data);
    void processFinishPacket();
    void processRestorePacket();
    void processStopPacket();

    void sendFinishMsg(int result);
    void writePacket(qint32 packetDataSize, PacketType type, const QByteArray& data);

    void setWakeup();
public:
    bool isSpaceEnough();
    void startUpdate();
    void startRestore();

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

    int m_num;

    MainWindow *m_w;
};

#endif // TCPSERVER_H
