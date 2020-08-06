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

Q_LOGGING_CATEGORY(server, "tcpserver")

const static quint16 kPort = 57822;
const static QString kSavePath = QDir::homePath() + "/oneai";
const static QString kRecoverPath = "/opt/ota_package/ubtech/recovery.sh";

static int headTimes = 0;
static int dataTimes = 0;

TcpServer::TcpServer(MainWindow *w, QObject *parent) : m_w(w), QObject(parent)
{
    m_server = new QTcpServer();
    m_server->listen(QHostAddress::Any, kPort);
    connect(m_server, SIGNAL(newConnection()), this, SLOT(onAcceptConnection()));

    m_packetSize = -1;
    m_headerSize = sizeof(PacketType) + sizeof(m_packetSize);

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

    m_w->show();

    setWakeup();

    if (!isSpaceEnough())
    {
        qWarning(server()) << "the free space is not enough!";
        sendFinishMsg(emAiboxState::eAiboxFileSendingPause);
        startRestore();
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(data).object();

    QString localVersion = Setting::instance().getVersion();

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
            m_receivedBytes = fileInfo.size();
        }
        else
        {
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
    QString cmd = "xset dpms 0 0 0";
    system(cmd.toStdString().data());
}

bool TcpServer::isSpaceEnough()
{
    const QString cmd = "df / -l --output=avail | tail -1";

    FILE *fp = popen(cmd.toStdString().data(), "r");
    if (!fp)
    {
        qCCritical(server()) << "popen " << cmd << " fail!";
        return false;
    }

    const qint64 needSpace = (qint64)1000*1024*1024*1024;

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
    QString update = kRecoverPath + "update";
    system(update.toStdString().data());
}

void TcpServer::startRestore()
{
    QString restore = kRecoverPath + "restore";
    system(restore.toStdString().data());
}

void TcpServer::onAcceptConnection()
{
    m_socket = m_server->nextPendingConnection();

    QString ip =  m_socket->peerAddress().toString();
    quint16 port = m_socket->peerPort();
    QString log = QString("[%1:%2] connect sucess").arg(ip).arg(port);
    qCDebug(server()) << log;

    connect(m_socket, SIGNAL(readyRead()), this, SLOT(onReadFromClient()));
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);
}

void TcpServer::onReadFromClient()
{
    m_buff.append(m_socket->readAll());

    QByteArray data;

    while (m_buff.size() >= m_headerSize)
    {
        if (m_packetSize < 0)
        {
            memcpy(&m_packetSize, m_buff.constData(), sizeof(m_packetSize));
            m_buff.remove(0, sizeof(m_packetSize));

            headTimes++;
            qCDebug(server()) << "head number: " << headTimes << ",m_packetSize " << m_packetSize << ", remain m_buff size " << m_buff.size();
        }

        if (m_buff.size() > m_packetSize)
        {
            PacketType type;
            type = static_cast<PacketType>(m_buff.at(0));
            m_buff.remove(0, sizeof(type));
            if (type == PacketType::Data)
            {
                dataTimes++;
            }

            data = m_buff.mid(0, m_packetSize);
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
    qCDebug(server()) << "Disconnected address " << m_socket->peerAddress();
    qCDebug(server()) << "Disconnected port" <<m_socket->peerPort();

    m_w->hide();
    if (m_file.isOpen())
    {
        m_file.close();
    }
    m_buff.clear();
    dataTimes = 0;
    headTimes = 0;
    m_num = 0;
}
