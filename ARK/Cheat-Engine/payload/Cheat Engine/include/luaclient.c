#ifndef CELUACLIENT_c
#define CELUACLIENT_c

#include <cepipelib.c>

#ifdef _WIN32
#include <windowslite.h>
#endif

char CELUA_ServerName[255];
PPipeClient luaclientpipe=(void*)0;



#ifdef _WIN32
size_t cssize=sizeof(CRITICAL_SECTION);
CRITICAL_SECTION luaclientcs;
void *luaclientcs_initonce=(void*)0;

BOOL __stdcall InitOnceExecuteOnce(void *InitOnce, void* InitFn, void* Parameter,  void *Context);
    
#else       
  #ifdef __APPLE__
    #define _PTHREAD_MUTEX_SIG_init 0x32aaaba7
    #define PTHREAD_MUTEX_INITIALIZER {_PTHREAD_MUTEX_SIG_init,{0}} 
    
    #if __SIZEOF_POINTER__ == 8
    #define __PTHREAD_MUTEX_SIZE__ 56
    #else
    #define __PTHREAD_MUTEX_SIZE__ 40 
    #endif
    
    typedef struct {
      long __sig;
      char __opague[__PTHREAD_MUTEX_SIZE__];
    } pthread_mutex_t;
    
  #else
    #define PTHREAD_MUTEX_INITIALIZER {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
    #if __SIZEOF_POINTER__ == 8
    #define __PTHREAD_MUTEX_SIZE__ 40
    #else
    #define __PTHREAD_MUTEX_SIZE__ 32 
    #endif 
    
    //in case of doubt, increase this
    
    typedef struct {
      char __size[__SIZEOF_PTHREAD_MUTEX_T];   
    } pthread_mutex_t;    
    
  #endif   
  
int pthread_mutex_init(pthread_mutex_t *__mutex, void *__mutexattr);
int pthread_mutex_lock(pthread_mutex_t *__mutex);
int pthread_mutex_unlock(pthread_mutex_t *__mutex);   
    
pthread_mutex_t luaclientcs;      

void InitializeCriticalSection(pthread_mutex_t *cs)
{
  //potential race condition here
  pthread_mutex_init(cs,(void*)0);
}

void EnterCriticalSection(pthread_mutex_t *cs)
{
  pthread_mutex_lock(cs);  
}

void LeaveCriticalSection(pthread_mutex_t *cs)
{
  pthread_mutex_unlock(cs);
}
    
#endif

int luaclientinitialized=0;


void CELUA_Initialize();

unsigned long long CELUA_ExecuteFunctionByReference(int ref, int paramcount, void** AddressOfParameters, int async)
{
  typedef enum {ptNil=0, ptBoolean=1, ptInt64=2, ptInt32=3, ptNumber=4, ptString=5, ptTable=6, ptUnknown=255} ParamType;
  ParamType valtype;
  unsigned char command=3;
  unsigned char vtb;
  unsigned char returncount=1;
  unsigned long long result=0;
  uint16_t sl;
  
    
 // debug_log("CELUA_ExecuteFunctionByReference");
    
  
 // debug_log("CELUA_ServerName=%s",CELUA_ServerName);
    
    
  //debug_log("calling CELUA_Initialize");
  CELUA_Initialize();
  if (luaclientinitialized==0)
  {
      debug_log("luaclientinitialized==0 Exiting...");
      return 0;
  }
 
#if __SIZEOF_POINTER__ == 8
  valtype=ptInt64;
#else
  valtype=ptInt32;

#endif

  vtb=valtype;
    


  
  if (luaclientpipe)
  {
    
    PMemoryStream ms=ms_create(32+paramcount*(sizeof(size_t)+1));

    ms_writeByte(ms,command);
    ms_writeByte(ms,async);
    ms_writeDword(ms,ref);
    ms_writeByte(ms,paramcount);
    int i;
    for (i=0; i<paramcount; i++)
    {
      ms_writeByte(ms, vtb);
      ms_write(ms, &AddressOfParameters[i], sizeof(void*));
    }    
    ms_writeByte(ms,returncount); //returncount
    
    
    EnterCriticalSection(&luaclientcs);
    if (ps_isvalid(luaclientpipe)==0)
    {
      debug_log("luapipe was invalid");
      ms_destroy(ms);
      LeaveCriticalSection(&luaclientcs); 
      return 0;       
    }
    
    
    ps_writeMemStreamRaw(luaclientpipe,ms);
    ms_destroy(ms);    
    returncount=ps_readByte(luaclientpipe);
    
    //debug_log("returncount=%d", returncount);
    
    for (i=0; i<returncount; i++)
    {
        //debug_log("read return value %d",i);
        
      valtype=ps_readByte(luaclientpipe);
      switch (valtype)
      {
        case ptNil:
        case ptUnknown:
          continue;
          
        case ptBoolean:
          result=ps_readByte(luaclientpipe);
          continue;
          
        case ptInt32:
          result=ps_readDword(luaclientpipe);
          continue;              
          
        case ptInt64:
        case ptNumber:
          result=ps_readQword(luaclientpipe);
          continue;         

        case ptString:
        {
          char *buf;
          sl=ps_readWord(luaclientpipe);          
          buf=malloc(sl+1);
          
          if (buf)
          {            
            ps_read(luaclientpipe, buf, sl);
            buf[sl]=0;
            
           //debug_log("lua result %d was a string:%s", i, buf);
            
            free(buf);
          }
          else
            debug_log("Failed to allocate %d bytes", sl+1);
          continue;
        }
          
      } 
    }
      
     // debug_log("after reading return values");

    
    
    
      LeaveCriticalSection(&luaclientcs);
     // debug_log("released critical value");

    
  }
  
  return result;
}

BOOL __stdcall InitCriticalSectionOnce(void* InitOnce, void* Parameter, void* Context)
{
  InitializeCriticalSection(&luaclientcs);
  return TRUE;
}


void CELUA_Initialize()
{
    if (luaclientinitialized)
    {
        //debug_log("already initialized");
        if (ps_isvalid(luaclientpipe))
            return;
            
        debug_log("luapipe was invalid. Reconnecting");
        luaclientinitialized=0;
    }
    else
      InitOnceExecuteOnce(&luaclientcs_initonce,InitCriticalSectionOnce, NULL, NULL); 
    
    EnterCriticalSection(&luaclientcs);

      
    if (luaclientinitialized==0)
    {
        debug_log("CELUA_Initialize: luaclientinitialized==0");      
        if (luaclientpipe==(void*)0)
        {
            debug_log("luaclientpipe is NULL. Initializing it ( CELUA_ServerName=%s )", CELUA_ServerName);
            
            if (CELUA_ServerName[0]==0)
            {
                debug_log("CELUA_ServerName[0]==0");
            }
         
            luaclientpipe=CreateClientPipeConnection(CELUA_ServerName);
            debug_log("luaclientpipe created");
        }
        
      luaclientinitialized=1;
    }
    LeaveCriticalSection(&luaclientcs);
}


#endif //CELUACLIENT_c
