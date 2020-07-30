#include "tcpserver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include "setting.h"
#include <QDataStream>

const static quint16 kPort = 17116;
const static QString kSavePath = "/home/qlf/oneai";

void *readThread(void *arg)
{
    TcpServer *ts = (TcpServer *)arg;
    PacketType type;
    QByteArray data;

    while (1)
    {
        if (ts->waitForRead(ts->m_buff, sizeof(ts->m_packetSize)) != sizeof(ts->m_packetSize))
        {
            qDebug() << "read m_packetSize error";
            return (void *)0;
        }
        memcpy(&ts->m_packetSize, ts->m_buff.constData(), sizeof(ts->m_packetSize));
        ts->m_buff.clear();

        if (ts->waitForRead(ts->m_buff, sizeof(type)) != sizeof(type))
        {
            qDebug() << "read type error";
            return (void *)0;
        }
        memcpy(&type, ts->m_buff.constData(), sizeof(type));
        ts->m_buff.clear();

        if (ts->waitForRead(ts->m_buff, ts->m_packetSize) != ts->m_packetSize)
        {
            qDebug() << "read data error";
            return (void *)0;
        }
        ts->processPacket(ts->m_buff, type);
        ts->m_buff.clear();
    }

    return (void *)0;
}

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

    m_isData = false;

    m_num = 0;

    //    m_otaInfo->fileName = "1234.tar.gz";

    //    int pos = m_otaInfo->fileName.indexOf(".");
    //    qDebug()<< pos;

    //    QString md5sum = m_otaInfo->fileName.mid(0, pos);
    //    qDebug()<< md5sum;

    //    QFileInfo fileinfo("/home/qlf/ota.conf.bak");
    //    //文件名
    //    QString file_name = fileinfo.fileName();
    //    //文件后缀
    //    QString file_suffix = fileinfo.suffix();
    //    //绝对路径
    //    QString file_path = fileinfo.absolutePath();

    //    qDebug() << file_name << " " << file_suffix << " " << file_path;
}

TcpServer::~TcpServer()
{
    delete m_otaInfo;
    if (m_file.isOpen())
    {
        m_file.close();
    }
}

void TcpServer::start()
{
    int32_t ret = pthread_create(&m_thread_id, NULL, readThread, (void *)this);
    if(0 != ret)
    {
        qDebug("pthread_create return :%d", ret);
    }
    else
    {
        // 线程退出后，自己释放资源
        pthread_detach(m_thread_id);
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

    m_otaInfo->fileName = obj.value("name").toString();
    m_otaInfo->isSaveData = obj.value("save").toBool();
    m_otaInfo->version = obj.value("version").toString();
    m_otaInfo->fileSize = obj.value("size").toVariant().value<qint64>();

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
                                                          {"size", m_receivedBytes}
                                                      }));

    QByteArray headerData(QJsonDocument(objReplay).toJson());
    PacketType type = PacketType::Header;
    qint32 packetSize = headerData.size();

    writePacket(packetSize, type, headerData);
    m_receivedBytes = 0;
}

void TcpServer::processDataPacket(QByteArray &data)
{
    qDebug() << __func__;
    if (!m_file.isOpen())
    {
        QString filePath =  kSavePath + "/" +  m_otaInfo->fileName; // "test.log"; //m_otaInfo->fileName;

        m_file.setFileName(filePath);

        if (m_file.exists())
        {
            m_file.remove();
        }

        if (!m_file.open(QIODevice::Append))
        {
            qDebug() << "open " << filePath << " fail!";
            exit(-1);
        }
    }

    QDataStream out(&m_file);
    if (m_receivedBytes + data.size() <= m_otaInfo->fileSize)
    {
        //        m_file.write(data);
        out.writeRawData(data.constData(), data.size());
        m_file.flush();
        m_receivedBytes += data.size();
        m_num++;
        qDebug() << "num " << m_num << " m_receivedBytes " << data.size();
    }
    //    qDebug() << "m_receivedBytes " << m_receivedBytes;
    //    qDebug() << "m_otaInfo->fileSize " << m_otaInfo->fileSize;
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
        }
        else
        {
            result = 0; // 校验失败
        }

        QJsonObject obj(QJsonObject::fromVariantMap({
                                                        {"result", result}
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

qint32 TcpServer::waitForRead(QByteArray &data, const qint32 bytes)
{
    while (m_socket->bytesAvailable() < bytes)
    {
        if (!m_socket->waitForReadyRead())
        {
            qDebug() << "time out~~~~~~~~~~~~~~~~~~~~~~~";
            qDebug() << "m_socket->bytesAvailable(): " << m_socket->bytesAvailable();
            return -1;
        }
    }

    data = m_socket->read(bytes);
    return data.size();
}

void TcpServer::onAcceptConnection()
{
    m_socket = m_server->nextPendingConnection();

    QString ip =  m_socket->peerAddress().toString();
    quint16 port = m_socket->peerPort();
    QString log = QString("[%1:%2] connect sucess").arg(ip).arg(port);
    qDebug() << log;

    //    connect(m_socket, SIGNAL(readyRead()), this, SLOT(onReadFromClient()));
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);

    start();
}

void TcpServer::onReadFromClient()
{
    m_buff.append(m_socket->readAll());
    //    processDataPacket(m_buff);
    //    m_buff.clear();

    qint64 allSize = m_socket->bytesAvailable();
    qDebug() << m_socket->bytesAvailable();

    while (allSize >= m_headerSize)
    {

    }
#if 1
    PacketType type;
    QByteArray data;

    //    char buff[1024 * 200];
    //    m_buff = m_socket->read(m_headerSize);
    //    memcpy(&m_packetSize, m_buff.constData(), sizeof(m_packetSize));
    //    m_buff.remove(0, sizeof(m_packetSize));

    //    type = static_cast<PacketType>(m_buff.at(0));
    //    m_buff.remove(0, sizeof(type));

    //    m_buff = m_socket->read(buff, m_headerSize);
    //    memcpy(&m_packetSize, buff, sizeof(m_packetSize));
    //    type = static_cast<PacketType>(buff[4]);


    qDebug() << "m_packetSize " <<m_packetSize <<", type " << (int)type;


    m_buff = m_socket->read(m_packetSize);
    qDebug() << "m_buff size " << m_buff.size();
    processPacket(m_buff, type);
    m_buff.clear();

    //    if (m_buff.size() >= m_packetSize) {

    //        data = m_buff.mid(0, m_packetSize);
    //        //            qDebug() << "data " << data;

    //        m_buff.remove(0, data.size());
    //        processPacket(data, type);
    //        m_isData = false;
    //    }
#endif

#if 0
    while (m_buff.size() >= m_headerSize)
    {
        if (!m_isData)
        {
            memcpy(&m_packetSize, m_buff.constData(), sizeof(m_packetSize));
            m_buff.remove(0, sizeof(m_packetSize));

            type = static_cast<PacketType>(m_buff.at(0));
            m_buff.remove(0, sizeof(type));
            qDebug() << "m_packetSize " <<m_packetSize <<", type " << (int)type << ",m_buff size " << m_buff.size();
        }

        if (m_buff.size() >= m_packetSize) {

            data = m_buff.mid(0, m_packetSize);
            //            qDebug() << "data " << data;

            m_buff.remove(0, data.size());
            processPacket(data, type);
            m_isData = false;
        }
        else
        {
            m_isData = true;
            qDebug() << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1";
            break;
        }
    }
#endif
}

void TcpServer::onDisconnected()
{
    qDebug() << "address " << m_socket->peerAddress();
    qDebug() << "port" <<m_socket->peerPort();
}
