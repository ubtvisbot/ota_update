#ifndef GETINFO_H
#define GETINFO_H

#include <string>

using namespace std;

class GetInfo
{
public:
    GetInfo();

    string getSN();
    string getVersion(const char *path);
    string getState(const char *path);
};

#endif // GETINFO_H
