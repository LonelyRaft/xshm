
#ifndef XSHM_H
#define XSHM_H

#include <string>

class xShmData;
class xShm
{
public:
    xShm();
    explicit xShm(
        const std::string& key);
    virtual ~xShm();

    enum class AccessMode:
        unsigned int {
        ReadOnly = 0,
        ReadWrite = 1,
    };

    enum class xShmError
    {
        NoError = 0,
        InvalidKey = 1,
        InvalidSize = 2,
        InvalidMode = 3,
        ShmExists = 4,
        ShmCreatFailed = 5,
        ShmOpenFailed  =6,
        ShmLostAddr = 7,
        ShmAttached = 8,
        LockOpenFailed = 9,
        Uninitialized = 10,
        SystemError = 11,
    };

    bool create(
        unsigned int size,
        AccessMode mode =
        AccessMode::ReadWrite);
    bool attach(
        AccessMode mode =
        AccessMode::ReadWrite);
    bool detach();
    bool isAttached();
    xShmError error();
    std::string errorString();
    void* data() const;
    const void* constData() const;
    std::string key() const;
    bool setKey(const std::string& key);
    unsigned int size() const;
    void lock();
    void unlock();

private:
    xShmData *mp;
};

#endif // XSHM_H
