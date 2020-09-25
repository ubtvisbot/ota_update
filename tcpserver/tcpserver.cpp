#include "tcpserver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include "setting.h"
#include <QDataStream>
#include <QDir>
#include "log.h"
#include <QLoggingCategory>
#include <QFile>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <QTimer>
#include "aiboxdevice.h"

Q_LOGGING_CATEGORY(server, "tcpserver")

const static quint16 kPort = 57822;
const static QString kSavePath = "/opt/ota_package/ubtech";
const static QString kAvahiRegisterPath = "/usr/bin/avahi_register";
const static QString kKeyFilePath = "/tmp/ota_key";
extern const QString kPauseState = "pause";
extern const QString kNoPauseState = "no";
extern const QString kSaveData = "save";
extern const QString kClearData = "clear";
extern const QString kAiboxUpdating = "updating";
extern const QString kAiboxIdle = "idle";
const static int kCheckInterval = 1000 * 5; // ms
const static int kTimeOut = 60; // s
const static int kKeyValue = 456701;

static int s_headTimes = 0;
static int s_dataTimes = 0;

TcpServer::TcpServer(QObject *parent) : QObject(parent)
{
    m_pAvahi = new QProcess(this);
//    killAvahi();

    m_pSocket = nullptr;
    m_bIsConnected = false;
    m_eOtaState = emAiboxState::eAiboxIdle;

    m_pServer = new QTcpServer();
    m_pServer->listen(QHostAddress::Any, kPort);
    connect(m_pServer, SIGNAL(newConnection()), this, SLOT(onAcceptConnection()));

    m_pNetworkTimer = new QTimer(this);
    m_pNetworkTimer->setInterval(kCheckInterval);
    connect(m_pNetworkTimer, &QTimer::timeout, this, &TcpServer::onNetworkTimeOut);

    m_nPacketSize = -1;
    m_nHeaderSize = sizeof(PacketType) + sizeof(m_nPacketSize); // 头部大小等于指令类型长度加数据类型长度

    m_pOtaInfo = new OtaInfo;

    m_buff.clear();
    m_nReceivedBytes = 0;

    m_nNum = 0;

    if (!initOtaWindowsMsg())
    {
        exit(0);
    }

//    startAvahi();

    qCDebug(server()) << "wait for client to connect ...";
}

TcpServer::~TcpServer()
{
    qDebug() << __func__;
    delete m_pOtaInfo;
    m_pServer->close();
    delete m_pServer;

    if (m_file.isOpen())
    {
        m_file.close();
    }

//    stopAvahi();
    delete m_pAvahi;
}

void TcpServer::processPacket(QByteArray &data, PacketType type)
{
    switch (type)
    {
    case PacketType::Header : processHeaderPacket(data); break;
    case PacketType::Data : processDataPacket(data); break;
    case PacketType::Finish : processFinishPacket(); break;
    case PacketType::Restore : processRestorePacket(data); break;
    case PacketType::Stop : processStopPacket(); break;
    }
}

void TcpServer::processHeaderPacket(QByteArray &data)
{
    qCDebug(server()) << "enter " << __func__;

    m_eOtaState = emAiboxState::eAiboxToUpdate;

    Setting::instance().setAiboxState(kAiboxUpdating);

    sendOtaWindowsMsg(Updating);

    AiboxDevice::instance().setWakeup();

    QJsonObject obj = QJsonDocument::fromJson(data).object();

    QString localVersion = Setting::instance().getVersion(); // 获取本地保存的版本信息

    m_pOtaInfo->fileName = obj.value("FileName").toString();
    m_pOtaInfo->isSaveData = obj.value("IsSaveData").toBool();
    m_pOtaInfo->version = obj.value("Version").toString();
    m_pOtaInfo->fileSize = obj.value("FileSize").toVariant().value<qint64>();

    qCDebug(server()) << "fileName: " << m_pOtaInfo->fileName<< " ,m_otaInfo->isSaveData: " << m_pOtaInfo->isSaveData<< " ,m_otaInfo->version: "
             << m_pOtaInfo->version<< " ,m_otaInfo->fileSize: " << m_pOtaInfo->fileSize;

    if (!AiboxDevice::instance().isSpaceEnough(m_pOtaInfo->fileSize))
    {
        qWarning(server()) << "the free space is not enough!";

        m_eOtaState = emAiboxState::eAiboxFileSendingPause;
        sendFinishMsg(m_eOtaState);
        Setting::instance().setPauseState(kPauseState);

        return;
    }
    else
    {
        Setting::instance().setPauseState(kNoPauseState);
    }

    if (m_pOtaInfo->fileName.isEmpty())
    {
        m_eOtaState = emAiboxState::eAiboxFileSendingFail;
        sendFinishMsg(m_eOtaState);
        sendOtaWindowsMsg(UpdateImageError);
        return;
    }

    if (m_pOtaInfo->isSaveData)
    {
        Setting::instance().setUserDataState(kSaveData);
    }
    else
    {
        Setting::instance().setUserDataState(kClearData);
    }

    QDir dir(kSavePath);
    if (!dir.exists()) {
        dir.mkpath(kSavePath);
    }

    QString filePath =  kSavePath + "/" + m_pOtaInfo->fileName;
    m_file.setFileName(filePath);

    QFileInfo fileInfo(filePath);
    if (fileInfo.exists())
    {
        if (localVersion == m_pOtaInfo->version)
        {
            m_nReceivedBytes = fileInfo.size(); // 版本匹配，计算已接受文件包的大小
        }
        else
        {
            // 版本不匹配，删除原文件并重新保存版本号
            QString cmd = "rm -f " + kSavePath + "/*.tar.gz";
            qCDebug(server()) << cmd;
            system(cmd.toStdString().c_str());
            Setting::instance().setVersion(m_pOtaInfo->version);
            m_nReceivedBytes = 0;
        }
    }
    else
    {
        m_nReceivedBytes = 0;
        Setting::instance().setVersion(m_pOtaInfo->version);
    }

    if (!m_file.open(QIODevice::Append))
    {
        qCCritical(server()) << "open " << filePath << " fail!";
        return;
    }


    QJsonObject objReplay(QJsonObject::fromVariantMap({
                                                          {"FileSize", m_nReceivedBytes}
                                                      }));

    QByteArray headerData(QJsonDocument(objReplay).toJson());
    PacketType type = PacketType::Header;
    qint32 packetSize = headerData.size();

    // 发送客户端已接收文件的大小
    writePacket(packetSize, type, headerData);
    qCDebug(server()) << "send to client m_receivedBytes: " << m_nReceivedBytes;
}

void TcpServer::processDataPacket(QByteArray &data)
{
    static int interval = 0;
//    qCDebug(server()) << "enter " << __func__;

    if (m_file.isOpen() && m_nReceivedBytes + data.size() <= m_pOtaInfo->fileSize)
    {
        QDataStream out(&m_file);
        out.writeRawData(data.constData(), data.size());

        m_file.flush();
        m_nReceivedBytes += data.size();
        m_nNum++;

        if (++interval >= 1000)
        {
            qCDebug(server()) << "enter " << __func__;
            qCDebug(server()) << "num: " << m_nNum << " received Bytes " << data.size();
            interval = 0;
        }
    }
}

void TcpServer::processFinishPacket()
{
    qCDebug(server()) << "enter " << __func__;
    qCDebug(server()) << "tcpserver received Bytes " << m_nReceivedBytes;
    qCDebug(server()) << "client file size " << m_pOtaInfo->fileSize;

    if (m_file.isOpen())
    {
        m_file.close();
    }

    int pos = m_pOtaInfo->fileName.indexOf(".");
    QString md5sumOrignal = m_pOtaInfo->fileName.mid(0, pos);
    qCDebug(server()) << "orignal md5sum: " << md5sumOrignal;

    QString fullPath = kSavePath + "/" + m_pOtaInfo->fileName;
    QString cmd = "md5sum " + fullPath;

    qCDebug(server()) << "cmd " << cmd;
    FILE *fp = popen(cmd.toStdString().data(), "r");
    if (fp)
    {
        char md5sumBuf[128] = {0};
        fread(md5sumBuf, 1, sizeof(md5sumBuf), fp);

        pclose(fp);

        QString md5sumNew = QString(md5sumBuf);
        int pos = md5sumNew.indexOf(" ");
        md5sumNew = md5sumNew.mid(0, pos);

        qCDebug(server()) << "new md5sum: " << md5sumNew;

        if (!md5sumOrignal.compare(QString(md5sumNew)))
        {
            qCDebug(server()) << "md5sum success";

            m_eOtaState = emAiboxState::eAiboxFileSendingSuccess;

            // 校验成功
            sendFinishMsg(m_eOtaState);
//            startUpdate();
//            sendOtaWindowsMsg(eAiboxUpdateSuccess);
        }
        else
        {
            qCWarning(server()) << "md5sum fail";

            m_eOtaState = emAiboxState::eAiboxFileSendingFail; // 校验失败

            // 校验失败，删除原文件
            QString cmd = "rm -f " + kSavePath + "/*.tar.gz";
            qCDebug(server()) << cmd;
            system(cmd.toStdString().c_str());

            sendFinishMsg(m_eOtaState);
            sendOtaWindowsMsg(UpdateImageError);
        }
    }
    else
    {
        qCCritical(server()) << "exec command md5sum fail";
        m_eOtaState = emAiboxState::eAiboxFileSendingFail;
        sendFinishMsg(m_eOtaState);
        sendOtaWindowsMsg(UpdateImageError);
    }

    Setting::instance().setAiboxState(kAiboxIdle);
}

void TcpServer::processRestorePacket(QByteArray &data)
{
    qCDebug(server()) << "enter " << __func__;
    m_eOtaState = emAiboxState::eAiboxToRestore;

    QJsonObject obj = QJsonDocument::fromJson(data).object();

    bool isSaveData = obj.value("IsSaveData").toBool();

    qCDebug(server()) << obj << ", isSaveData: " << isSaveData;

    if (isSaveData)
    {
        Setting::instance().setUserDataState(kSaveData);
    }
    else
    {
        Setting::instance().setUserDataState(kClearData);
    }

    QByteArray headerData = {0};
    headerData.resize(0);
    PacketType type = PacketType::Restore;
    qint32 packetSize = headerData.size();

    writePacket(packetSize, type, headerData);
}

void TcpServer::processStopPacket()
{
    qCDebug(server()) << "enter " << __func__;

    QByteArray headerData = {0};
    headerData.resize(0);
    PacketType type = PacketType::Stop;
    qint32 packetSize = headerData.size();

    writePacket(packetSize, type, headerData);

    m_pSocket->disconnectFromHost();
}

void TcpServer::sendFinishMsg(int result)
{
    qCDebug(server()) << "enter " << __func__;

    QJsonObject objReplay(QJsonObject::fromVariantMap({
                                                          {"Result", result}
                                                      }));

    QByteArray headerData(QJsonDocument(objReplay).toJson());
    PacketType type = PacketType::Finish;
    qint32 packetSize = headerData.size();

    writePacket(packetSize, type, headerData);

    qCDebug(server()) << "finish, Result: " << result;
}

void TcpServer::writePacket(qint32 packetDataSize, PacketType type, const QByteArray &data)
{
    qCDebug(server()) << "enter " << __func__;
    if (m_pSocket)
    {
        m_pSocket->write(reinterpret_cast<const char*>(&packetDataSize), sizeof(packetDataSize));
        m_pSocket->write(reinterpret_cast<const char*>(&type), sizeof(type));
        m_pSocket->write(data);
    }
}

void TcpServer::startAvahi()
{
    qCDebug(server()) << "enter " << __func__;
    QStringList argv;

    QString state = "0";

    // 首先判断上次是否为暂停文件发送，如果是给客户端发送“文件发送恢复”命令
    if (Setting::instance().getPauseState() == kPauseState)
    {
        state = QString::number((int)eAiboxFileSendingResume); // 文件发送恢复

        // 重置状态
        Setting::instance().setPauseState(kNoPauseState);
    }
    else if (Setting::instance().getAiboxState() == kAiboxUpdating)
    {
        state = QString::number((int)eAiboxUpdateFail); // 升级失败
    }
    else
    {
        emResultState resultState = AiboxDevice::instance().getResultState();
        emAiboxState eState = AiboxDevice::instance().switchResultStateToAiboxState(resultState);
        state = QString::number((int)eState);
    }

    // 发送aibox的状态给客户端
    argv << "-s" << state;
    qDebug() << "state is "<< state << ", cmd is " << argv;
    m_pAvahi->start(kAvahiRegisterPath, argv);
    m_pAvahi->waitForStarted(1000);
}

void TcpServer::stopAvahi()
{
    qCDebug(server()) << "enter " << __func__;
    m_pAvahi->close();
    m_pAvahi->waitForFinished(1000);
}

void TcpServer::killAvahi()
{
    qCDebug(server()) << "enter" << __func__;
    QProcess kill_avahi;
    kill_avahi.start("sh", QStringList() << "-c" << "ps ax | grep avahi_register | awk '{print $1}' |xargs kill -9 &>/dev/null");
    kill_avahi.waitForStarted(1000);
    kill_avahi.waitForFinished(1000);
    usleep(1000 * 200);
}

bool TcpServer::initOtaWindowsMsg()
{
    qCDebug(server()) << "enter " << __func__;
    QFile keyFile(kKeyFilePath);

    if (!keyFile.exists())
    {
        if (!keyFile.open(QIODevice::ReadWrite))
        {
            qCCritical(server()) << "open " << kKeyFilePath << " fail";
            return false;
        }
        else
        {
            keyFile.close();
        }
    }

    QString cmd = "chmod 0666 " + kKeyFilePath;
    system(cmd.toStdString().data());

    key_t key = ftok(kKeyFilePath.toStdString().data(), 6);
    qDebug() << "ftok key: " << key;
    if (key < 0)
    {
        key = (key_t)kKeyValue;
    }

    m_nMsgId = msgget(key, IPC_CREAT | 0666);
    qCDebug(server()) << "msgget result, m_nMsgId is: " << m_nMsgId;
    if (m_nMsgId < 0)
    {
        qCDebug(server()) << "msgget error " << m_nMsgId;
        return false;
    }

    return true;
}

void TcpServer::sendOtaWindowsMsg(emResultState eState)
{
    qCDebug(server()) << "enter " << __func__;
    OtaMessage msg;
    msg.eState = eState;
    msg.type = 1;

    int re = msgsnd(m_nMsgId, &msg,  sizeof(msg) - sizeof(msg.type), 0);
    qCDebug(server()) << "msgsnd result " << re << ", msg state " << eState;
}

void TcpServer::onAcceptConnection()
{
    qCDebug(server()) << "enter " << __func__;

    AiboxDevice::instance().setAiboxState(emAiboxState::eAiboxToUpdate);
    AiboxDevice::instance().setUuid();


    QTcpSocket* pNewSocket = m_pServer->nextPendingConnection();

    if (!m_bIsConnected)
    {
        m_pSocket = pNewSocket;
        m_bIsConnected = true;
    }
    else
    {
        qCDebug(server()) << "aibox is connected, it will disconnected from " << pNewSocket->peerAddress() << pNewSocket->peerPort();
        pNewSocket->disconnectFromHost();

        return;
    }


    QString ip =  m_pSocket->peerAddress().toString();
    quint16 port = m_pSocket->peerPort();
    QString log = QString("[%1:%2] connect sucess").arg(ip).arg(port);
    qCDebug(server()) << log;

    connect(m_pSocket, SIGNAL(readyRead()), this, SLOT(onReadFromClient()));
    connect(m_pSocket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);

    m_pNetworkTimer->start();
    m_lastTime = QDateTime::currentDateTime();
}

void TcpServer::onReadFromClient()
{
//    qCDebug(server()) << "enter " << __func__;
//    qCDebug(server()) << "bytesAvailable " << m_pSocket->bytesAvailable();

    if (!m_pNetworkTimer->isActive())
    {
        m_pNetworkTimer->start();
    }

    // 有数据到来，重置心跳时间
    m_lastTime = QDateTime::currentDateTime();


    m_buff.append(m_pSocket->readAll());

//    qCDebug(server()) << "m_buff size " << m_buff.size() << ", m_nHeaderSize " << m_nHeaderSize << ", m_nPacketSize " << m_nPacketSize;
    QByteArray data;

    while (m_buff.size() >= m_nHeaderSize)
    {
        if (m_nPacketSize < 0) // m_packetSize为数据大小
        {
            memcpy(&m_nPacketSize, m_buff.constData(), sizeof(m_nPacketSize));
            m_buff.remove(0, sizeof(m_nPacketSize));

            s_headTimes++;
//            qCDebug(server()) << "head number: " << s_headTimes << ",m_packetSize " << m_nPacketSize << ", remain m_buff size " << m_buff.size();
        }

        if (m_buff.size() > m_nPacketSize)
        {
            PacketType type;
            type = static_cast<PacketType>(m_buff.at(0)); // 取出指令
            m_buff.remove(0, sizeof(type));
            if (type == PacketType::Data)
            {
                s_dataTimes++;
            }

            data = m_buff.mid(0, m_nPacketSize); // 取出数据
//            qCDebug(server()) << "data number: " << s_dataTimes << ", type " << (int)type << ",m_packetSize " << m_nPacketSize << ", remain m_buff size " << m_buff.size();

            processPacket(data, type);

            m_buff.remove(0, data.size());
            m_nPacketSize = -1;
        }
        else
        {
            break;
        }
    }
}

void TcpServer::onDisconnected()
{
    qCDebug(server()) << "enter " << __func__;
    qCDebug(server()) << "Disconnected address " << m_pSocket->peerAddress();
    qCDebug(server()) << "Disconnected port" << m_pSocket->peerPort();

    if (m_file.isOpen())
    {
        m_file.close();
    }
    m_buff.clear();
    s_dataTimes = 0;
    s_headTimes = 0;
    m_nNum = 0;
    m_nPacketSize = -1;
    m_bIsConnected = false;

//    m_pSocket = nullptr;

    if (m_pNetworkTimer->isActive())
    {
        m_pNetworkTimer->stop();
    }

    Setting::instance().setAiboxState(kAiboxIdle);

    if (m_eOtaState == emAiboxState::eAiboxToUpdate)
    {
        sendOtaWindowsMsg(UpdateImageError);
        m_eOtaState = emAiboxState::eAiboxUpdateFail;
    }
    else if (m_eOtaState == emAiboxState::eAiboxFileSendingSuccess)
    {
//        sendOtaWindowsMsg(eAiboxUpdateSuccess);
        m_eOtaState = emAiboxState::eAiboxIdle;
        AiboxDevice::instance().startUpdate();
        return;
    }
    else if (m_eOtaState == emAiboxState::eAiboxToRestore || m_eOtaState ==  eAiboxFileSendingPause)
    {
        m_eOtaState = emAiboxState::eAiboxIdle;
        AiboxDevice::instance().startRestore();
        return;
    }

    AiboxDevice::instance().setAiboxState(m_eOtaState);
    m_eOtaState = emAiboxState::eAiboxIdle;

//    killAvahi();
//    stopAvahi();
//    startAvahi();
}

void TcpServer::onNetworkTimeOut()
{
//    qCDebug(server()) << "enter " << __func__;

    if (m_lastTime.secsTo(QDateTime::currentDateTime()) >= kTimeOut)
    {
        qCDebug(server()) << "network time out, it will disconnect from client ...";
        m_pNetworkTimer->stop();
        if (m_pSocket)
        {
            m_pSocket->disconnectFromHost();
        }
    }
}
