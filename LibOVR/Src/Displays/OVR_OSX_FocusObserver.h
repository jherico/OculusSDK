#ifndef OVR_OSX_FocusObserver_h
#define OVR_OSX_FocusObserver_h

#include "../Kernel/OVR_Threads.h"
#include "../Kernel/OVR_System.h"
#include "../Kernel/OVR_Lockless.h"

#include "../Service/Service_NetServer.h"

namespace OVR { namespace OSX{

    struct FocusReaderImpl;
    
class AppFocusObserver : public SystemSingletonBase<AppFocusObserver>    
{
    OVR_DECLARE_SINGLETON(AppFocusObserver);
    
public:
    Lock ListLock;
    Array<pid_t> AppList;
    Service::NetServerListener *listener;
    FocusReaderImpl* impl;
    
    void OnProcessFocus(pid_t pid);
    void SetListener(Service::NetServerListener *_listener);
    
    pid_t LastProcessId;
    pid_t ActiveProcessId;
    void AddProcess(pid_t pid);
    void nextProcess();
    void RemoveProcess(pid_t pid);
    
    
protected:
    void onAppFocus(pid_t pid);
    
    pid_t LastAppFocus;
    
};
    
    
 
}} // namespace OVR, OSX


#endif /* defined(__OVR_OSX_FocusReader__OVR_OSX_FocusObserver__) */

