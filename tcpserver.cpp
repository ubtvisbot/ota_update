#include "tcpserver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include "setting.h"
#include <QDataStream>

const static quint16 kPort = 57822;
const static QString kSavePath = "/home/oneai/oneai";

TcpServer::TcpServer(QObject *parent) : QObject(parent)
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

    qDebug() << "wait for client to connect";
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
    qDebug() << __func__;

    QJsonObject obj = QJsonDocument::fromJson(data).object();

    QString localVersion = Setting::instance().getVersion();

    m_otaInfo->fileName = obj.value("FileName").toString();
    m_otaInfo->isSaveData = obj.value("IsSaveData").toBool();
    m_otaInfo->version = obj.value("Version").toString();
    m_otaInfo->fileSize = obj.value("FileSize").toVariant().value<qint64>();

    qDebug() << "fileName: " << m_otaInfo->fileName<< " ,m_otaInfo->isSaveData: " << m_otaInfo->isSaveData<< " ,m_otaInfo->version: "
             << m_otaInfo->version<< " ,m_otaInfo->fileSize: " << m_otaInfo->fileSize;

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
            qDebug() << cmd;
            system(cmd.toStdString().c_str());
            Setting::instance().setVersion(m_otaInfo->version);
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
    qDebug() << "m_receivedBytes: " << m_receivedBytes;
}

void TcpServer::processDataPacket(QByteArray &data)
{
    qDebug() << __func__;

    if (!m_file.isOpen())
    {
        QString filePath =  kSavePath + "/" +  m_otaInfo->fileName; // "test.log"; //m_otaInfo->fileName;

        m_file.setFileName(filePath);

        if (!m_file.open(QIODevice::Append))
        {
            qDebug() << "open " << filePath << " fail!";
            exit(-1);
        }
    }

    if (m_file.isOpen() && m_receivedBytes + data.size() <= m_otaInfo->fileSize)
    {
        QDataStream out(&m_file);
        out.writeRawData(data.constData(), data.size());
//        m_file.write(data);
        m_file.flush();
        m_receivedBytes += data.size();
        m_num++;
        qDebug() << "num " << m_num << " received Bytes " << data.size();
    }
        qDebug() << "m_receivedBytes " << m_receivedBytes;
}

void TcpServer::processFinishPacket()
{
    qDebug() << __func__;
    qDebug() << "m_receivedBytes " << m_receivedBytes;
    qDebug() << "m_otaInfo->fileSize " << m_otaInfo->fileSize;

    if (m_file.isOpen())
    {
        m_file.close();
    }

    int pos = m_otaInfo->fileName.indexOf(".");
    QString md5sumOrignal = m_otaInfo->fileName.mid(0, pos);
    qDebug() << "md5sumOrignal" << md5sumOrignal;

    QString fullPath = kSavePath + "/" + m_otaInfo->fileName;
    QString cmd = "md5sum " + fullPath;

    qDebug() << "cmd " << cmd;
    FILE *fp = popen(cmd.toStdString().data(), "r");
    if (fp)
    {
        char md5sumBuf[128];
        fread(md5sumBuf, 1, sizeof(md5sumBuf), fp);

        pclose(fp);

        QString md5sumNew = QString(md5sumBuf);
        int pos = md5sumNew.indexOf(" ");
        md5sumNew = md5sumNew.mid(0, pos);

        qDebug() << "md5sumNew " << md5sumNew;

        int result = -1;

        if (!md5sumOrignal.compare(QString(md5sumNew)))
        {
            result = 1; // 校验成功
            qDebug() << "md5sum success";
        }
        else
        {
            result = 0; // 校验失败
            qDebug() << "md5sum fail";
        }

        QJsonObject obj(QJsonObject::fromVariantMap({
                                                        {"Result", result}
                                                    }));

        QByteArray headerData(QJsonDocument(obj).toJson());
        PacketType type = PacketType::Finish;
        qint32 packetSize = headerData.size();

        writePacket(packetSize, type, headerData);
    }
    else
    {
        qDebug() << "exec command md5sum fail";
        exit(-1);
    }
}

void TcpServer::processRestorePacket()
{
    qDebug() << __func__;
}

void TcpServer::processStopPacket()
{
    qDebug() << __func__;
}

void TcpServer::writePacket(qint32 packetDataSize, PacketType type, const QByteArray &data)
{
    qDebug() << __func__;
    if (m_socket)
    {
        m_socket->write(reinterpret_cast<const char*>(&packetDataSize), sizeof(packetDataSize));
        m_socket->write(reinterpret_cast<const char*>(&type), sizeof(type));
        m_socket->write(data);
    }
}

void TcpServer::onAcceptConnection()
{
    m_socket = m_server->nextPendingConnection();

    QString ip =  m_socket->peerAddress().toString();
    quint16 port = m_socket->peerPort();
    QString log = QString("[%1:%2] connect sucess").arg(ip).arg(port);
    qDebug() << log;

    connect(m_socket, SIGNAL(readyRead()), this, SLOT(onReadFromClient()));
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);
}

void TcpServer::onReadFromClient()
{
    qint64 allSize = m_socket->bytesAvailable();
    m_buff.append(m_socket->readAll());

    qDebug() << "read all bytes available: " << allSize;

    PacketType type;
    QByteArray data;

    while (m_buff.size() >= m_headerSize)
    {
        if (m_packetSize < 0)
        {
            memcpy(&m_packetSize, m_buff.constData(), sizeof(m_packetSize));
            m_buff.remove(0, sizeof(m_packetSize));

            type = static_cast<PacketType>(m_buff.at(0));
            m_buff.remove(0, sizeof(type));
            qDebug() << "m_packetSize " <<m_packetSize <<", type " << (int)type << ",m_buff size " << m_buff.size();
        }

        if (m_buff.size() >= m_packetSize)
        {
            data = m_buff.mid(0, m_packetSize);

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
    qDebug() << "Disconnected address " << m_socket->peerAddress();
    qDebug() << "Disconnected port" <<m_socket->peerPort();
    m_buff.clear();
}
