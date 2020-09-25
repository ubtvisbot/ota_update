#include "getinfo.h"
#include <QFileInfo>
#include <stdio.h>
#include <QDebug>

GetInfo::GetInfo()
{

}

string GetInfo::getSN()
{
    string sn = "";
    char buff[64] ={0};
    FILE *fp = popen("/usr/local/UBTTools/ubtsn -r", "r");

    if (fp)
    {
        fread(buff, 1, sizeof(buff), fp);

        sn = buff;

        string::size_type pos =sn.find_first_of("=");
        if (pos == string::npos)
        {
            pclose(fp);
            qDebug() << "can not find separator =";
            return "";
        }

        sn = sn.substr(pos + 1);

        // delete all of the last '\n'
        while (sn[sn.size() - 1] == '\n')
        {
            sn.erase(sn.size() - 1);
        }

        pclose(fp);
    }
    else
    {
        qDebug() << "exec command ubtsn -r fail";
    }

    return sn;
}

string GetInfo::getVersion(const char *path)
{
    string version = "";
    FILE *fp = fopen(path, "r");

    if (fp == nullptr)
    {
        qDebug() << "open file " << path << " fail";
    }
    else
    {
        char buff[64] = {0};

        fgets(buff, sizeof(buff), fp);

        version = buff;

        string::size_type pos = version.find_first_of("_");
        if (pos == string::npos)
        {
            fclose(fp);
            qDebug() << "can not find separator _";
            return "";
        }

        version = version.substr(pos + 1);

        // delete all of the last '\n'
        while (version[version.size() - 1] == '\n')
        {
            version.erase(version.size() - 1);
        }

        fclose(fp);
    }

    return version;
}

string GetInfo::getState(const char *path)
{
    string state = "";
    FILE *fp = fopen(path, "r");

    if (fp == nullptr)
    {
        qDebug() << "open file " << path << " fail";
    }
    else
    {
        char buff[3] = {0};

        fgets(buff, sizeof(buff), fp);
        state = buff;

        // delete all of the last '\n'
        while (state[state.size() - 1] == '\n')
        {
            state.erase(state.size() - 1);
        }

        fclose(fp);
    }

    return state;
}
