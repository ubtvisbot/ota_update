#ifndef BROADCAST_H
#define BROADCAST_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

class Broadcast : public QObject
{
    Q_OBJECT
public:
    explicit Broadcast(QObject *parent = nullptr);
    ~Broadcast();

    void sendBroadcast();

    QVector<QHostAddress> getBroadcastAddresses();
    QList<QHostAddress> getIpAddresses();

    int createDestIpSendBrocast(const char* ipaddress, int port, struct sockaddr_in *addrServer);

signals:

public slots:
    void onReadData();

private:
    QUdpSocket* m_pUdp;
//    QUdpSocket* m_pUdpSend;

    QTimer m_timer;
};

#endif // BROADCAST_H
