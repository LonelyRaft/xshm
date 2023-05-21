 
#include "xshm.h"

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#endif // _WIN32

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <climits>
#endif // __linux__

#ifdef _WIN32
struct xShmAttr
{
    unsigned int size;
    unsigned int refcnt;
};
#endif // _WIN32

class xShmData
{
public:
    xShmData();
    explicit xShmData(
        const std::string& key);
    ~xShmData();

    bool shmsem_open();
    void shmsem_close();
    void shmsem_lock();
    void shmsem_unlock();

    unsigned int shm_size();

    bool shm_create(unsigned int size);
    bool shm_attach(xShm::AccessMode mode);
    bool shm_detach();
    inline bool shm_is_attached() const {return m_data != nullptr;}

    inline xShm::xShmError shm_error() const {return m_error;}

    bool shm_key(const std::string& key);
    inline const std::string& shm_key() const {return m_key;}

    void* shm_buf();
    inline const void* shm_const_buf() const {return m_data;}

    static const std::string xshm_errstr[];
private:
    std::string m_key;
    void* m_data;
    xShm::xShmError m_error;
    xShm::AccessMode m_mode;
#ifdef __linux__
    int m_handle;
    sem_t* m_sem;
#endif // __linux__
#ifdef _WIN32
    HANDLE m_handle;
    HANDLE m_sem;
    xShmAttr *m_attr;
#endif //  _WIN32
};

#ifdef __linux__
xShmData::xShmData()
{
    m_handle = 0;
    m_data = nullptr;
    m_sem = nullptr;
    m_error = xShm::xShmError::NoError;
    m_mode = xShm::AccessMode::ReadWrite;
}

bool xShmData::shmsem_open()
{
    if(m_key.empty())
        return false;
    std::string semName(m_key);
    semName.append("_xShmSem");
    m_sem =  sem_open(
        semName.c_str(),
        O_CREAT | O_EXCL,
        S_IRUSR | S_IWUSR , 1);
    if(m_sem == nullptr && errno == EEXIST)
        m_sem = sem_open(semName.c_str(), 0);
    return m_sem != nullptr;
}

void xShmData::shmsem_close()
{
    if(m_sem != nullptr)
    {
        sem_close(m_sem);
        std::string semName(m_key);
        semName.append("_xShmSem");
        sem_unlink(semName.c_str());
    }
}

void xShmData::shmsem_lock()
{
    if(m_sem == nullptr)
    {
        m_error = xShm::xShmError::Uninitialized;
        return;
    }
    sem_wait(m_sem);
}

void xShmData::shmsem_unlock()
{
    if(m_sem == nullptr)
    {
        m_error = xShm::xShmError::Uninitialized;
        return;
    }
    sem_post(m_sem);
}

unsigned int xShmData::shm_size()
{
    if(m_data == nullptr)
    {
        m_error = xShm::xShmError::Uninitialized;
        return  0;
    }
    struct stat fstatus = {0};
    fstat(m_handle, &fstatus);
    return fstatus.st_size;
}

bool xShmData::shm_create(unsigned int size)
{
    if(m_key.empty())
    {
        m_error = xShm::xShmError::InvalidKey;
        return false;
    }
    if(size == 0)
    {
        m_error = xShm::xShmError::InvalidSize;
        return false;
    }

    int handle = shm_open(
        m_key.c_str(),
        O_CREAT | O_EXCL | O_RDWR,
        S_IRUSR | S_IWUSR);
    if(handle < 0)
    {
        m_error = xShm::xShmError::ShmCreatFailed;
        if(errno == EEXIST)
            m_error = xShm::xShmError::ShmExists;
        return false;
    }

    if(ftruncate(handle, size) < 0)
    {
        shm_unlink(m_key.c_str());
        m_error = xShm::xShmError::SystemError;
        return false;
    }

    m_error = xShm::xShmError::NoError;
    return true;
}

bool xShmData::shm_attach(xShm::AccessMode mode)
{
    if(m_key.empty())
    {
        m_error = xShm::xShmError::InvalidKey;
        return false;
    }
    if(mode != xShm::AccessMode::ReadOnly &&
        mode != xShm::AccessMode::ReadWrite)
    {
        m_error = xShm::xShmError::InvalidMode;
        return false;
    }
    if(m_data != nullptr)
    {
        m_error = xShm::xShmError::ShmAttached;
        return false;
    }

    int handle = -1;
    void* addr = nullptr;
    unsigned int shmsz = 0;
    do
    {
        int acsmode = O_RDONLY;
        if(mode == xShm::AccessMode::ReadWrite)
            acsmode = O_RDWR;
        handle = shm_open(
            m_key.c_str(),
            acsmode , S_IRUSR | S_IWUSR);
        if(handle < 0)
        {
            m_error = xShm::xShmError::ShmOpenFailed;
            break;
        }

        struct stat fstatus = {0};
        if(fstat(handle, &fstatus) < 0)
        {
            m_error = xShm::xShmError::SystemError;
            break;
        }
        shmsz = fstatus.st_size;

        if(ftruncate(handle, shmsz) < 0)
        {
            m_error = xShm::xShmError::SystemError;
            break;
        }

        acsmode = PROT_READ;
        if(mode == xShm::AccessMode::ReadWrite)
            acsmode |= PROT_WRITE;
        addr = mmap(
            nullptr, shmsz, acsmode,
            MAP_SHARED, handle, 0);
        if(addr == MAP_FAILED)
        {
            m_error = xShm::xShmError::ShmLostAddr;
            break;
        }

        if(!shmsem_open())
        {
            m_error = xShm::xShmError::LockOpenFailed;
            break;
        }

        m_handle = handle;
        m_data = addr;
        m_mode = mode;
        m_error = xShm::xShmError::NoError;
        return true;
    }while(0);

    if(addr != nullptr)
        munmap(addr, shmsz);
    if(handle > 0)
        close(handle);
    return false;
}

bool xShmData::shm_detach()
{
    if(m_data != nullptr)
    {
        struct stat fstatus = {0};
        fstat(m_handle, &fstatus);
        munmap(m_data, fstatus.st_size);
        m_data = nullptr;
        close(m_handle);
        m_handle = -1;
        shmsem_close();
        if(fstatus.st_nlink == 1)
            shm_unlink(m_key.c_str());
    }
    return true;
}
#endif // __linux__

#ifdef _WIN32
xShmData::xShmData()
{
    m_handle = 0;
    m_data = nullptr;
    m_sem = nullptr;
    m_attr = nullptr;
    m_error = xShm::xShmError::NoError;
    m_mode = xShm::AccessMode::ReadWrite;
}

bool xShmData::shmsem_open()
{
    if(m_key.empty())
        return false;
    std::string semName(m_key);
    semName.append("_xShmSem");
    m_sem = CreateSemaphore(
        nullptr, 1, 1, semName.c_str());
    return m_sem != nullptr;
}

void xShmData::shmsem_close()
{
    if(m_sem != nullptr)
        CloseHandle(m_sem);
}

void xShmData::shmsem_lock()
{
    if(m_sem == nullptr)
    {
        m_error = xShm::xShmError::xShmUninitialized;
        return;
    }
    WaitForSingleObject(m_sem, INFINITE);
}

void xShmData::shmsem_unlock()
{
    if(m_sem == nullptr)
    {
        m_error = xShm::xShmError::xShmUninitialized;
        return;
    }
    ReleaseSemaphore(m_sem, 1, nullptr);
}

bool xShmData::shm_create(unsigned int size)
{
    if(m_key.empty())
    {
        m_error = xShm::xShmError::InvalidKey;
        return false;
    }
    if(size == 0 || size > (UINT_MAX - sizeof(xShmAttr)))
    {
        m_error = xShm::xShmError::InvalidSize;
        return false;
    }

    HANDLE handle = CreateFileMapping(
        INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, 0, size + sizeof(xShmAttr),
        m_key.c_str());
    if(handle == nullptr)
    {
        m_error = xShm::xShmError::xShmCreatFailed;
        return false;
    }
    if(GetLastError() ==
        ERROR_ALREADY_EXISTS)
    {
        m_error = xShm::xShmError::ShmExists;
        return false;
    }

    xShmAttr *attr = (xShmAttr*)MapViewOfFile(
        handle, FILE_MAP_ALL_ACCESS,
        0, 0, size);
    if(attr == nullptr)
    {
        CloseHandle(handle);
        m_error = xShm::xShmError::xShmNoAddr;
        return false;
    }

    attr->refcnt = 0;
    attr->size = size;

    UnmapViewOfFile(attr);

    m_error = xShm::xShmError::NoError;
    return true;
}

bool xShmData::shm_attach(xShm::AccessMode mode)
{
    if(m_key.empty())
    {
        m_error = xShm::xShmError::InvalidKey;
        return false;
    }
    if(m_data != nullptr)
    {
        m_error = xShm::xShmError::ShmAttached;
        return false;
    }
    if(mode != xShm::AccessMode::ReadOnly &&
        mode != xShm::AccessMode::ReadWrite)
    {
        m_error = xShm::xShmError::InvalidMode;
        return false;
    }

    HANDLE handle = OpenFileMapping(
        FILE_MAP_ALL_ACCESS, false,
        m_key.c_str());
    if (handle == nullptr)
    {
        m_error = xShm::xShmError::ShmOpenFailed;
        return false;
    }
    xShmAttr* attr = (xShmAttr*)MapViewOfFile(
        handle, FILE_MAP_ALL_ACCESS,
        0, 0, sizeof(xShmAttr));
    if(attr == nullptr)
    {
        m_error = xShm::xShmError::xShmNoAddr;
        return false;
    }

    if(!shmsem_open())
    {
        m_error = xShm::xShmError::mLockCreatFailed;
        return false;
    }

    attr->refcnt++;
    m_handle = handle;
    m_mode = mode;
    m_attr = attr;
    m_data = attr + 1;
    m_error = xShm::xShmError::NoError;
    return true;
}

bool xShmData::shm_detach()
{
    if(m_data != nullptr)
    {
        m_attr->refcnt--;
        unsigned int refcnt =
            m_attr->refcnt;
        UnmapViewOfFile(m_attr);
        m_attr = nullptr;
        m_data = nullptr;
        if(refcnt == 0)
            CloseHandle(m_handle);
        m_handle = nullptr;
    }
    return true;
}

unsigned int xShmData::shm_size()
{
    if(m_data != nullptr)
    {
        m_error = xShm::xShmError::xShmUninitialized;
        return 0;
    }
    return m_attr->size;
}
#endif // _WIN32

xShmData::xShmData(
    const std::string& key)
    :xShmData()
{
    m_key = key;
}

xShmData::~xShmData()
{
    shm_detach();
}

xShm::xShm()
{
    mp = new xShmData;
}

xShm::xShm(const std::string &key)
{
    mp = new xShmData(key);
}

xShm::~xShm()
{
    if(mp != nullptr)
    {
        delete mp;
        mp = nullptr;
    }
}

bool xShmData::shm_key(const std::string &key)
{
    if(m_data != nullptr)
    {
        m_error =  xShm::xShmError::ShmAttached;
        return false;
    }
    if(key.empty())
    {
        m_error = xShm::xShmError::InvalidKey;
        return false;
    }
    m_key = key;
    return true;
}

void *xShmData::shm_buf()
{
    if(m_mode != xShm::AccessMode::ReadWrite)
    {
        m_error = xShm::xShmError::InvalidMode;
        return nullptr;
    }
    return m_data;
}

bool xShm::create(unsigned int size, AccessMode mode)
{
    if(!mp->shm_create(size) &&
        mp->shm_error() != xShmError::ShmExists)
        return false;
    return mp->shm_attach(mode);
}

bool xShm::attach(AccessMode mode)
{
    return mp->shm_attach(mode);
}

bool xShm::detach()
{
    return mp->shm_detach();
}

bool xShm::isAttached()
{
    return mp->shm_is_attached();
}

unsigned int xShm::size() const
{
    return mp->shm_size();
}

void xShm::lock()
{
    mp->shmsem_lock();
}

void xShm::unlock()
{
    mp->shmsem_unlock();
}

xShm::xShmError xShm::error()
{
    return mp->shm_error();
}

std::string xShm::errorString()
{
    unsigned int idx =
        static_cast<unsigned int>(
        mp->shm_error());
    return xShmData::xshm_errstr[idx];
}

void *xShm::data() const
{
    return mp->shm_buf();
}

const void *xShm::constData() const
{
    return mp->shm_const_buf();
}

std::string xShm::key() const
{
    return mp->shm_key();
}

bool xShm::setKey(const std::string &key)
{
    return mp->shm_key(key);
}

const std::string xShmData::xshm_errstr[]={
    std::string("NoError"),
    std::string("InvalidKey"),
    std::string("InvalidSize"),
    std::string("InvalidMode"),
    std::string("ShmExists"),
    std::string("ShmCreatFailed"),
    std::string("ShmOpenFailed"),
    std::string("ShmLostAddr"),
    std::string("ShmAttached"),
    std::string("LockOpenFailed"),
    std::string("Uninitialized"),
    std::string("SystemError"),
};


