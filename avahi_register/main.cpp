#include "getinfo.h"
#include <QDebug>
#include <QString>
#include <QCoreApplication>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

//static const char kStateFilePath[] = "/etc/aibox_state";
static const char kVersionFilePath[] = "/etc/aibox_sw_version";

static AvahiEntryGroup *s_group = NULL;
static AvahiSimplePoll *s_simple_poll = NULL;
static char *s_name = NULL;
static string s_type = "_oneai._tcp";
static int s_portNumber = 57822;
static string s_version = "";
static string s_state = "0";

static void create_services(AvahiClient *c);

char *saveStr(const char *s)
{
    char *p;

    if ((p = (char *) malloc(strlen(s) + 1)) != NULL) {
        strcpy(p, s);
    }
    return (p);
}

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {
    assert(g == s_group || s_group == NULL);
    s_group = g;

    /* Called whenever the entry group state changes */
    qDebug() << __func__ << "enter";
    switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED :
        qDebug() << __func__ << "AVAHI_ENTRY_GROUP_ESTABLISHED";
        /* The entry group has been established successfully */
        fprintf(stderr, "Service '%s' successfully established.\n", s_name);
        break;

    case AVAHI_ENTRY_GROUP_COLLISION : {
        char *n;
        qDebug() << __func__ << "collision";
        /* A service name collision with a remote service
                                                * happened. Let's pick a new name */
        n = avahi_alternative_service_name(s_name);
        avahi_free(s_name);
        s_name = n;

        fprintf(stderr, "Service name collision, renaming service to '%s'\n", s_name);

        /* And recreate the services */
        create_services(avahi_entry_group_get_client(g));
        break;
    }

    case AVAHI_ENTRY_GROUP_FAILURE :
        qDebug() << __func__ << "AVAHI_ENTRY_GROUP_FAILURE";
        fprintf(stderr, "Entry group failure: %s\n", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

        /* Some kind of failure happened while we were registering our services */
        avahi_simple_poll_quit(s_simple_poll);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        qDebug() << __func__ << "AVAHI_ENTRY_GROUP_REGISTERING";
        ;
    }
    qDebug() << __func__ << "exit";
}

static void create_services(AvahiClient *c) {
    char *n;
    int ret;
    assert(c);
    qDebug() << __func__ << "enter";
    /* If this is the first time we're called, let's create a new
     * entry group if necessary */

    if (!s_group)
        if (!(s_group = avahi_entry_group_new(c, entry_group_callback, NULL))) {
            fprintf(stderr, "avahi_entry_group_new() failed: %s\n", avahi_strerror(avahi_client_errno(c)));
            goto fail;
        }

    /* If the group is empty (either because it was just created, or
     * because it was reset previously, add our entries.  */

    if (avahi_entry_group_is_empty(s_group)) {

        /* We will now add two services and one subtype to the entry
         * group. The two services have the same name, but differ in
         * the service type (IPP vs. BSD LPR). Only services with the
         * same name should be put in the same entry group. */

        if ((ret = avahi_entry_group_add_service(s_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, (AvahiPublishFlags)0, s_name, s_type.c_str(), NULL, NULL, s_portNumber, s_version.c_str(), s_state.c_str(), NULL)) < 0) {

            if (ret == AVAHI_ERR_COLLISION)
                goto collision;

            fprintf(stderr, "Failed to add _ipp._tcp service: %s\n", avahi_strerror(ret));
            goto fail;
        }

        /* Tell the server to register the service */
        if ((ret = avahi_entry_group_commit(s_group)) < 0) {
            fprintf(stderr, "Failed to commit entry group: %s\n", avahi_strerror(ret));
            goto fail;
        }
    }
    qDebug() << __func__ << "exit";
    return;

collision:

    /* A service name collision with a local service happened. Let's
     * pick a new name */
    n = avahi_alternative_service_name(s_name);
    avahi_free(s_name);
    s_name = n;

    fprintf(stderr, "Service name collision, renaming service to '%s'\n", s_name);

    avahi_entry_group_reset(s_group);

    create_services(c);
    qDebug() << __func__ << "collision exit";
    return;

fail:
    avahi_simple_poll_quit(s_simple_poll);
    qDebug() << __func__ << "fail exit";
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    assert(c);
    qDebug() << __func__ << "enter";
    /* Called whenever the client or server state changes */

    switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
        qDebug() << __func__ << "AVAHI_CLIENT_S_RUNNING";
        /* The server has startup successfully and registered its host
             * name on the network, so it's time to create our services */
        create_services(c);
        break;

    case AVAHI_CLIENT_FAILURE:
        qDebug() << __func__ << "AVAHI_CLIENT_FAILURE";
        fprintf(stderr, "Client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
        avahi_simple_poll_quit(s_simple_poll);

        break;

    case AVAHI_CLIENT_S_COLLISION:
        qDebug() << __func__ << "AVAHI_CLIENT_S_COLLISION";
        /* Let's drop our registered services. When the server is back
             * in AVAHI_SERVER_RUNNING state we will register them
             * again with the new host name. */

    case AVAHI_CLIENT_S_REGISTERING:
        qDebug() << __func__ << "AVAHI_CLIENT_S_REGISTERING";
        /* The server records are now being established. This
             * might be caused by a host name change. We need to wait
             * for our own records to register until the host name is
             * properly esatblished. */

        if (s_group)
            avahi_entry_group_reset(s_group);

        break;

    case AVAHI_CLIENT_CONNECTING:
        qDebug() << __func__ << "AVAHI_CLIENT_CONNECTING";
        ;
    }
    qDebug() << __func__ << "exit";
}

void usage() {
    printf("\nNAME\n\tregister-service\n\n");
    printf("SYNOPSIS\n\tregister-service [OPTION]\n\n");
    printf("DESCRIPTION\n\tRegister a program with avahi/mDNS and then start it.\n\n");
    printf("\t-n <name>\tAvahi name.\n");
    printf("\t-p <port>\tPort Number.\n");
    printf("\t-s <state>\tState state.\n");
    printf("\t-t <type>\tAvahi type.\n");
    printf("\t-v\t\tVerbose.\n");
    printf("\t-h|?\t\tHelp.\n");
    printf("\n");

    exit(0);
}



int main(int argc, char *argv[])
{
//    QCoreApplication a(argc, argv);

    GetInfo getInfo;

    string version = getInfo.getVersion(kVersionFilePath);
    qDebug()<< "version " << QString::fromStdString(version);
    if (version.empty())
    {
        qDebug() << "get version fail";
        return -1;
    }
    s_version = version;

    string sn = getInfo.getSN();
    qDebug()<< "SN " << QString::fromStdString(sn);
    if (sn.empty())
    {
        qDebug() << "get SN fail";
        return -1;
    }

    AvahiClient *client = NULL;
    int error;

    extern char *optarg;
    string localName = "oneai_" + sn;

    int ch = 0;
    int verbose = 0;

    if (optarg != NULL)
    {
        printf("00 %s\n", optarg);
    }

    while ((ch = getopt (argc, argv, "?hn:p:s:t:v")) != -1) {
        switch(ch) {
        case 'h':
        case '?':
            usage();
            break;
        case 'n':
            localName = optarg;
            break;
        case 'p':
            s_portNumber = atoi(optarg);
            break;
        case 's':
            s_state = optarg;
            break;
        case 't':
            s_type = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            break;
        }
    }

    if(verbose) {
        printf("Name  : %s\n", localName.c_str());
        printf("Type  : %s\n", s_type.c_str());
        printf("Port  : %d\n", s_portNumber);
        printf("State  : %s\n", s_state.c_str());
    }

    /* Allocate main loop object */
    if (!(s_simple_poll = avahi_simple_poll_new())) {
        fprintf(stderr, "Failed to create simple poll object.\n");
        goto fail;
    }

    s_name = avahi_strdup(localName.c_str());

    /* Allocate a new client */
    client = avahi_client_new(avahi_simple_poll_get(s_simple_poll), (AvahiClientFlags)0, client_callback, NULL, &error);

    /* Check wether creating the client object succeeded */
    if (!client) {
        fprintf(stderr, "Failed to create client: %s\n", avahi_strerror(error));
        goto fail;
    }

    /* Run the main loop */
    avahi_simple_poll_loop(s_simple_poll);

    return 0;
    //    return a.exec();

fail:

    /* Cleanup things */

    if (client)
        avahi_client_free(client);

    if (s_simple_poll)
        avahi_simple_poll_free(s_simple_poll);

    avahi_free(s_name);

    return 1;
}
