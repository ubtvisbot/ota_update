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

Q_LOGGING_CATEGORY(server, "tcpserver")

const static quint16 kPort = 57822;
const static QString kSavePath = "/opt/ota_package/ubtech";
const static QString kRecoverPath = "/opt/ota_package/ubtech/recovery.sh";
const static QString kCompletePath = "/opt/ota_package/ubtech/complete.sh";
const static QString kStatePath = "/opt/ota_package/ubtech/state";
const static QString kPauseState = "pause";

static int headTimes = 0;
static int dataTimes = 0;

TcpServer::TcpServer(UpdateWindow *w, QObject *parent) : m_uw(w), QObject(parent)
{
    m_server = new QTcpServer();
    m_server->listen(QHostAddress::Any, kPort);
    connect(m_server, SIGNAL(newConnection()), this, SLOT(onAcceptConnection()));

    m_packetSize = -1;
    m_headerSize = sizeof(PacketType) + sizeof(m_packetSize); // 头部大小等于指令类型长度加数据类型长度

    m_otaInfo = new OtaInfo;

    m_buff.clear();
    m_receivedBytes = 0;

    m_num = 0;

    qCDebug(server()) << "wait for client to connect ...";
}

TcpServer::~TcpServer()
{
    delete m_otaInfo;
    m_server->close();

    if (m_file.isOpen())
    {
        m_file.close();
    }
}

QString TcpServer::getResultState()
{
    qCDebug(server()) << "enter " << __func__;
    QString state = "";

    system(kCompletePath.toStdString().data()); // 生成上次升级或者恢复后的结果状态

    QFile fp(kStatePath);

    if (!fp.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCCritical(server()) << "open " << kStatePath << " fail.";
    }
    else
    {
        char buff[8] = {0};
        fp.read(buff, sizeof(buff));

        state = buff;
        if (state.endsWith('\n'))
        {
            state = state.left(state.size() - 1);
        }
    }

    return state;
}

void TcpServer::processPacket(QByteArray &data, PacketType type)
{
    switch (type)
    {
    case PacketType::Header : processHeaderPacket(data); break;
    case PacketType::Data : processDataPacket(data); break;
    case PacketType::Finish : processFinishPacket(); break;
    case PacketType::Restore : processRestorePacket(); break;
    case PacketType::Stop : processStopPacket(); break;
    }
}

void TcpServer::processHeaderPacket(QByteArray &data)
{
    qCDebug(server()) << "enter " << __func__;

    m_uw->show();
    m_uw->startShowUpdate();

    setWakeup();

    if (!isSpaceEnough())
    {
        qWarning(server()) << "the free space is not enough!";
        sendFinishMsg(emAiboxState::eAiboxFileSendingPause);
        Setting::instance().setPauseState(kPauseState);
        startRestore();
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(data).object();

    QString localVersion = Setting::instance().getVersion(); // 获取本地保存的版本信息

    m_otaInfo->fileName = obj.value("FileName").toString();
    m_otaInfo->isSaveData = obj.value("IsSaveData").toBool();
    m_otaInfo->version = obj.value("Version").toString();
    m_otaInfo->fileSize = obj.value("FileSize").toVariant().value<qint64>();

    qCDebug(server()) << "fileName: " << m_otaInfo->fileName<< " ,m_otaInfo->isSaveData: " << m_otaInfo->isSaveData<< " ,m_otaInfo->version: "
             << m_otaInfo->version<< " ,m_otaInfo->fileSize: " << m_otaInfo->fileSize;

    if (m_otaInfo->fileName.isEmpty())
    {
        sendFinishMsg(emAiboxState::eAiboxFileSendingFail);
        return;
    }

    QDir dir(kSavePath);
    if (!dir.exists()) {
        dir.mkpath(kSavePath);
    }

    QString filePath =  kSavePath + "/" + m_otaInfo->fileName;
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists())
    {
        if (localVersion == m_otaInfo->version)
        {
            m_receivedBytes = fileInfo.size(); // 版本匹配，计算已接受文件包的大小
        }
        else
        {
            // 版本不匹配，删除原文件并重新保存版本号
            QString cmd = "rm -f " + filePath;
            qCDebug(server()) << cmd;
            system(cmd.toStdString().c_str());
            Setting::instance().setVersion(m_otaInfo->version);
            m_receivedBytes = 0;
        }
    }
    else
    {
        m_receivedBytes = 0;
        Setting::instance().setVersion(m_otaInfo->version);
    }

    QJsonObject objReplay(QJsonObject::fromVariantMap({
                                                          {"FileSize", m_receivedBytes}
                                                      }));

    QByteArray headerData(QJsonDocument(objReplay).toJson());
    PacketType type = PacketType::Header;
    qint32 packetSize = headerData.size();

    // 发送客户端已接收文件的大小
    writePacket(packetSize, type, headerData);
    qCDebug(server()) << "send to client m_receivedBytes: " << m_receivedBytes;
}

void TcpServer::processDataPacket(QByteArray &data)
{
    qCDebug(server()) << "enter " << __func__;

    if (!m_file.isOpen())
    {
        QString filePath =  kSavePath + "/" +  m_otaInfo->fileName; // "test.log"; //m_otaInfo->fileName;

        m_file.setFileName(filePath);

        if (!m_file.open(QIODevice::Append))
        {
            qCCritical(server()) << "open " << filePath << " fail!";
            return;
        }
    }

    if (m_file.isOpen() && m_receivedBytes + data.size() <= m_otaInfo->fileSize)
    {
        QDataStream out(&m_file);
        out.writeRawData(data.constData(), data.size());

        m_file.flush();
        m_receivedBytes += data.size();
        m_num++;
        qCDebug(server()) << "num: " << m_num << " received Bytes " << data.size();
    }
}

void TcpServer::processFinishPacket()
{
    qCDebug(server()) << "enter " << __func__;
    qCDebug(server()) << "tcpserver received Bytes " << m_receivedBytes;
    qCDebug(server()) << "client file size " << m_otaInfo->fileSize;

    if (m_file.isOpen())
    {
        m_file.close();
    }

    int pos = m_otaInfo->fileName.indexOf(".");
    QString md5sumOrignal = m_otaInfo->fileName.mid(0, pos);
    qCDebug(server()) << "orignal md5sum: " << md5sumOrignal;

    QString fullPath = kSavePath + "/" + m_otaInfo->fileName;
    QString cmd = "md5sum " + fullPath;

    qCDebug(server()) << "cmd " << cmd;
    FILE *fp = popen(cmd.toStdString().data(), "r");
    if (fp)
    {
        char md5sumBuf[128];
        fread(md5sumBuf, 1, sizeof(md5sumBuf), fp);

        pclose(fp);

        QString md5sumNew = QString(md5sumBuf);
        int pos = md5sumNew.indexOf(" ");
        md5sumNew = md5sumNew.mid(0, pos);

        qCDebug(server()) << "new md5sum: " << md5sumNew;

        emAiboxState result = emAiboxState::eAiboxIdle;

        if (!md5sumOrignal.compare(QString(md5sumNew)))
        {
            result = emAiboxState::eAiboxFileSendingSuccess; // 校验成功
            qCDebug(server()) << "md5sum success";
            sendFinishMsg(result);
            startUpdate();
        }
        else
        {
            result = emAiboxState::eAiboxFileSendingFail; // 校验失败
            qCWarning(server()) << "md5sum fail";
            sendFinishMsg(result);
        }
    }
    else
    {
        qCCritical(server()) << "exec command md5sum fail";
        sendFinishMsg(emAiboxState::eAiboxFileSendingFail);
    }
}

void TcpServer::processRestorePacket()
{
    qCDebug(server()) << "enter " << __func__;
    startRestore();
}

void TcpServer::processStopPacket()
{
    qCDebug(server()) << "enter " << __func__;

    QByteArray headerData = {0};
    headerData.resize(0);
    PacketType type = PacketType::Stop;
    qint32 packetSize = headerData.size();

    writePacket(packetSize, type, headerData);
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
}

emAiboxState TcpServer::switchResultStateToAiboxState(const int &state)
{
    emResultState eState = (emResultState)state;
    switch (eState) {
    case emResultState::RestoreSuccess:
        return emAiboxState::eAiboxRestoreSuccess;

    case emResultState::UpdateSuccess:
        return emAiboxState::eAiboxUpdateSuccess;

    case emResultState::RestoreImageError:
        return emAiboxState::eAiboxRestoreFail;

    case emResultState::UpdateImageError:
        return emAiboxState::eAiboxUpdateFail;

    default:
        return emAiboxState::eAiboxIdle;
    }
}

void TcpServer::writePacket(qint32 packetDataSize, PacketType type, const QByteArray &data)
{
    qCDebug(server()) << "enter " << __func__;
    if (m_socket)
    {
        m_socket->write(reinterpret_cast<const char*>(&packetDataSize), sizeof(packetDataSize));
        m_socket->write(reinterpret_cast<const char*>(&type), sizeof(type));
        m_socket->write(data);
    }
}

void TcpServer::setWakeup()
{
    qCDebug(server()) << "enter " << __func__;
    QString cmd = "xset dpms 0 0 0";
    system(cmd.toStdString().data());
}

bool TcpServer::isSpaceEnough()
{
    qCDebug(server()) << "enter " << __func__;
    const QString cmd = "df / -l --output=avail | tail -1";

    FILE *fp = popen(cmd.toStdString().data(), "r");
    if (!fp)
    {
        qCCritical(server()) << "popen " << cmd << " fail!";
        return false;
    }

    const qint64 needSpace = (qint64)1024*1024;

    char result[128] = {0};
    fread(result, 1, sizeof(result), fp);
    pclose(fp);

    QString freeSize = result;

    qint64 freeSpace = freeSize.toLongLong();

    qCDebug(server()) << " free space is " << freeSpace << ", need space is " << needSpace;

    if (freeSpace <= needSpace)
    {
        return false;
    }

    return true;
}

void TcpServer::startUpdate()
{
    qCDebug(server()) << "enter " << __func__;
    QString update = kRecoverPath + "update";
    system(update.toStdString().data());
}

void TcpServer::startRestore()
{
    qCDebug(server()) << "enter " << __func__;
    QString restore = kRecoverPath + "restore";
    system(restore.toStdString().data());
}

void TcpServer::onAcceptConnection()
{
    qCDebug(server()) << "enter " << __func__;
    m_socket = m_server->nextPendingConnection();

    QString ip =  m_socket->peerAddress().toString();
    quint16 port = m_socket->peerPort();
    QString log = QString("[%1:%2] connect sucess").arg(ip).arg(port);
    qCDebug(server()) << log;

    connect(m_socket, SIGNAL(readyRead()), this, SLOT(onReadFromClient()));
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);

    // 首先判断上次是否为暂停文件发送，如果是给客户端发送“文件发送恢复”命令
    if (Setting::instance().getPauseState() == kPauseState)
    {
        sendFinishMsg(emAiboxState::eAiboxFileSendingResume);
    }
    else // 发送aibox的状态给客户端
    {
        QString resultState = getResultState();

        emAiboxState state = switchResultStateToAiboxState(resultState.toInt());

        sendFinishMsg(state);
    }
}

void TcpServer::onReadFromClient()
{
    qCDebug(server()) << "enter " << __func__;
    m_buff.append(m_socket->readAll());

    QByteArray data;

    while (m_buff.size() >= m_headerSize)
    {
        if (m_packetSize < 0) // m_packetSize为数据大小
        {
            memcpy(&m_packetSize, m_buff.constData(), sizeof(m_packetSize));
            m_buff.remove(0, sizeof(m_packetSize));

            headTimes++;
            qCDebug(server()) << "head number: " << headTimes << ",m_packetSize " << m_packetSize << ", remain m_buff size " << m_buff.size();
        }

        if (m_buff.size() > m_packetSize)
        {
            PacketType type;
            type = static_cast<PacketType>(m_buff.at(0)); // 取出指令
            m_buff.remove(0, sizeof(type));
            if (type == PacketType::Data)
            {
                dataTimes++;
            }

            data = m_buff.mid(0, m_packetSize); // 取出数据
            qCDebug(server()) << "data number: " << dataTimes << ", type " << (int)type << ",m_packetSize " << m_packetSize << ", remain m_buff size " << m_buff.size();

            processPacket(data, type);

            m_buff.remove(0, data.size());
            m_packetSize = -1;
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
    qCDebug(server()) << "Disconnected address " << m_socket->peerAddress();
    qCDebug(server()) << "Disconnected port" <<m_socket->peerPort();

    m_uw->hide();
    if (m_file.isOpen())
    {
        m_file.close();
    }
    m_buff.clear();
    dataTimes = 0;
    headTimes = 0;
    m_num = 0;
}
