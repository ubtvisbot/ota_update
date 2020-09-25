#ifndef AIBOXSTATE_H
#define AIBOXSTATE_H

enum emResultState
{
    Idle = 0,               // 空闲状态
    UpdateSuccess = 1,      // 升级成功
    RestoreSuccess = 2,     // 恢复成功
    UpdateImageError = 3,	// 升级镜像错误
    RestoreImageError = 4,  // 备份镜像错误
    Updating = 5,           // 升级中
};

//Q_DECLARE_METATYPE(emResultState)

enum emAiboxState
{
    eAiboxIdle= 0,              //空闲状态
    eAiboxToUpdate = 1,         //升级中
    eAiboxUpdateSuccess,        //升级成功
    eAiboxUpdateFail,           //升级失败
    eAiboxToRestore,            //恢复系统中
    eAiboxRestoreSuccess = 5,   //恢复系统成功
    eAiboxRestoreFail,          //恢复系统失败
    eAiboxFileSendingPause,     //文件发送暂停
    eAiboxFileSendingResume,    //文件发送恢复
    eAiboxFileSendingSuccess,   //文件发送成功
    eAiboxFileSendingFail = 10, //文件发送失败
};

#endif // AIBOXSTATE_H
