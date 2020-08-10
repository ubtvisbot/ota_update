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
#include "updatewindow.h"

using namespace std;

enum class PacketType : char
{
    Header = 0x01,
    Data,
    Finish,
    Restore,
    Stop
};

enum emResultState
{
    Idle = 0,               // 空闲状态
    UpdateSuccess = 1,      // 升级成功
    RestoreSuccess = 2,     // 恢复成功
    UpdateImageError = 3,	// 升级镜像错误
    RestoreImageError = 4,  // 备份镜像错误
};

enum emAiboxState
{
    eAiboxIdle= 0,              //空闲状态
    eAiboxToUpdate = 1,         //升级中
    eAiboxUpdateSuccess,        //升级成功
    eAiboxUpdateFail,           //升级失败
    eAiboxToRestore,            //恢复系统中
    eAiboxRestoreSuccess = 5,   //恢复系统成功
    eAiboxRestoreFail,          //恢复系统失败
    eAiboxFileSendingPause,     //文件发送暂停
    eAiboxFileSendingResume,    //文件发送恢复
    eAiboxFileSendingSuccess,   //文件发送成功
    eAiboxFileSendingFail = 10, //文件发送失败
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
    explicit TcpServer(UpdateWindow *w, QObject *parent = nullptr);
    ~TcpServer();

    /*
     * 获取上次升级或者恢复的状态
     */
    QString getResultState();

    /*
     * 给客户端发送结束指令，数据为结果的状态
     */
    void sendFinishMsg(int result);

    /*
     * 把从系统返回的状态转换为客户端所需要的状态
     */
    emAiboxState switchResultStateToAiboxState(const int &state);

private:
    void processPacket(QByteArray& data, PacketType type);
    void processHeaderPacket(QByteArray& data);
    void processDataPacket(QByteArray& data);
    void processFinishPacket();
    void processRestorePacket();
    void processStopPacket();

    void writePacket(qint32 packetDataSize, PacketType type, const QByteArray& data);

    /*
     * 设置电脑不休眠
     */
    void setWakeup();


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
    int m_headerSize; // 头部的字节长度
    qint32 m_packetSize; // 数据长度
    qint64 m_receivedBytes; // 接收到的文件大小

    OtaInfo *m_otaInfo; // 保存客户端发过来的文件信息
    QFile m_file;

    int m_num;

    UpdateWindow *m_uw;
};

#endif // TCPSERVER_H
