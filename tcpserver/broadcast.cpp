#include "broadcast.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <cassert>
#include <QDebug>
#include "aiboxdevice.h"
#include "setting.h"

const static int kPortSend = 45670;
const static int kPortRecive = kPortSend + 1;
const static QString kVersionFile = "/etc/aibox_sw_version";
const static QString kService = "_oneai._udp";
const static QString kCharging = "ADAPTER\n";

Broadcast::Broadcast(QObject *parent) : QObject(parent)
{
    m_pUdp = new QUdpSocket(this);
    connect(m_pUdp, &QUdpSocket::readyRead, this, &Broadcast::onReadData);

    m_pUdp->bind(kPortRecive, QUdpSocket::ShareAddress);

//    m_pUdpSend = new QUdpSocket(this);

//    connect(&m_timer, &QTimer::timeout, this, &Broadcast::sendBroadcast);
//    m_timer.start(1000);
}

Broadcast::~Broadcast()
{

}

void Broadcast::sendBroadcast()
{
//    qDebug() << "enter " << __func__;
    static int nInterval = 0;
    nInterval++;

    QString version = AiboxDevice::instance().getVersion(kVersionFile.toStdString().data());
    QString sn = AiboxDevice::instance().getSN();

    QString state = "0";

    emAiboxState eAiboxState = AiboxDevice::instance().getAiboxState();
    state = QString::number((int)eAiboxState);

    QString uuid = AiboxDevice::instance().getUuid();

    QJsonObject obj(QJsonObject::fromVariantMap({
                                                    {"Version", version},
                                                    {"State", state},
                                                    {"SN", sn},
                                                    {"Uuid", uuid},
                                                }));

    QByteArray data(QJsonDocument(obj).toJson(QJsonDocument::Compact));

//    QVector<QHostAddress> addresses = getBroadcastAddresses();
//    foreach (QHostAddress address, addresses)
//    {
//        m_pUdpSend->writeDatagram(data, address, kPort);
//    }

//    m_pUdpSend->writeDatagram(data, QHostAddress::Broadcast, kPort + 1);
    struct sockaddr_in addrServer;

    QList<QHostAddress> addresses = getIpAddresses();
    QList<QHostAddress> tmpAddresses;

    foreach (auto address, addresses)
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            tmpAddresses.push_back(address);
            int sockClient = createDestIpSendBrocast(address.toString().toStdString().data(), kPortSend, &addrServer);
            sendto(sockClient, data.constData(), data.size(), 0, (struct sockaddr*)&addrServer, sizeof(struct sockaddr_in));
            close(sockClient);
        }
    }

    if (nInterval >= 5)
    {
        qDebug() << "send data to " << tmpAddresses << ": " << version << sn << state << uuid;
        nInterval = 0;
    }
}

QVector<QHostAddress> Broadcast::getBroadcastAddresses()
{
    QVector<QHostAddress> addresses;
    foreach (QNetworkInterface iface, QNetworkInterface::allInterfaces())
    {
        if (iface.flags() & QNetworkInterface::CanBroadcast)
        {
            foreach (QNetworkAddressEntry addressEntry, iface.addressEntries())
            {
                if (!addressEntry.broadcast().isNull())
                {
                    addresses.push_back(addressEntry.broadcast());
                }
            }
        }
    }

    qDebug() << addresses;
    return addresses;
}

QList<QHostAddress> Broadcast::getIpAddresses()
{
    return QNetworkInterface::allAddresses();
}

int Broadcast::createDestIpSendBrocast(const char *ipaddress, int port, sockaddr_in *addrServer)
{
    if(ipaddress == NULL)
    {
        return -1;
    }

    int sockClient  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockClient < 0)
    {
        qDebug() << "socket create failed. ip=" << ipaddress;
        return  -1;
    }

    int nBroadcast = 1;

    if (0 != setsockopt(sockClient, SOL_SOCKET,SO_BROADCAST, (char*)&nBroadcast, sizeof(int)))
    {
        qDebug() << "setsockopt failed. ip=" << ipaddress;
        close(sockClient);
        return  -1;
    }

    struct sockaddr_in addrClient = {0};
    addrClient.sin_family = AF_INET;
    addrClient.sin_addr.s_addr = inet_addr(ipaddress);
    addrClient.sin_port = 0;    // 0 表示由系统自动分配端口号

    if (0 != bind(sockClient, (struct sockaddr*)&addrClient, sizeof(addrClient)))
    {
        qDebug() << "bind failed.ip=" << ipaddress;
        close(sockClient);
        return  -1;
    }

    addrServer->sin_family = AF_INET;
    addrServer->sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addrServer->sin_port = htons(port);
    return sockClient;
}

void Broadcast::onReadData()
{
    static int nInterval = 1;


//    qDebug() << "enter " << __func__;
    while (m_pUdp->hasPendingDatagrams())
    {
        nInterval++;
        QByteArray data;

        qint64 datagramSize = m_pUdp->pendingDatagramSize();
        assert(datagramSize <= std::numeric_limits<int>::max());

        data.resize(static_cast<int>(datagramSize));

        QHostAddress sender;

        m_pUdp->readDatagram(data.data(), data.size(), &sender);

        QJsonObject obj = QJsonDocument::fromJson(data).object();

        if (nInterval >= 5)
        {
            qDebug() << "recive data from " << sender << ", size is " << data.size() << ", data is: " << data << ",Json is: " << obj;
            nInterval = 0;
        }

        if (obj.value("Service").toString() == kService
                && AiboxDevice::instance().getChargeType() == kCharging
                && AiboxDevice::instance().getAiboxState() != emAiboxState::eAiboxToUpdate)
        {
            sendBroadcast();
        }
    }
}
