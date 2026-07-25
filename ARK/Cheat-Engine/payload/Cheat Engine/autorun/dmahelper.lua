require("autorun\\newdefaulttemplates")

--DMA helper library (an example script showing lua and C interoperability)
--config (you are encouraged to change this)
local configname='cedma.txt'

--todo: in case of 100% physical access and no windows to run from, scan for the mz/pe header of ntoskernel and go from there
--also currently assuming 64-bit windows

dma={}
dma.openedHandles={}

local ExportScannerThread

function RtlGetVersion()
  local params=createMemoryStream()
  local size=4+4+4+4+4+128*2
  params.writeDword(size)
  params.Size=size

  local r=executeCodeLocalEx('ntdll.RtlGetVersion', params.Memory)
  if r==0 then
    r={}
    params.Position=4
    r.source=0
    r.dwMajorVersion=params.readDword()
    r.dwMinorVersion=params.readDword()
    r.dwBuildNumber=params.readDword()
    return r
  end
end

function GetNTDLLVersion()
  if getOpenedProcessID()==0 or readByte(process)==nil then
    openProcess(getCheatEngineProcessID())
  end
  
  local r=getFileVersion(enumModules()[2].PathToFile)  
  if r then
    local result
    result.source=1
    result.dwMajorVersion=r.major
    result.dwMinorVersion=r.minor
    result.dwBuildNumber=r.build
    return result    
  end
end

function dma.loadData()
  assert(dma.datapath,'dma: no datapath set')
  local sl=createStringList()
  sl.loadFromFile(dma.datapath) 
  local r,err=pcall(loadstring(sl.text))
  
  sl.destroy()
  
  r,err=pcall(function()  
    local currentKernelVersion
    if dma.kernelversion.source==0 then
      currentKernelVersion=RtlGetVersion()
      if currentKernelVersion==nil then error('RtlGetVersion() failed and the source was from RtlGetVersion()') end
    else
      local currentKernelVersion=GetNTDLLVersion()
      if currentKernelVersion==nil then error('GetNTDLLVersion() failed and the source was from GetNTDLLVersion()') end
    end
    
    if currentKernelVersion.dwMajorVersion~=dma.kernelversion.dwMajorVersion or
       currentKernelVersion.dwMinorVersion~=dma.kernelversion.dwMinorVersion or
       currentKernelVersion.dwBuildNumber~=dma.kernelversion.dwBuildNumber then error('Different version') end
  end)
  
  if not r then
    --if inMainThread() then
    --  if messageDialog('dmahelper','The version of '..configname..' doesn\'t match your system. Load anyhow?', mtCondirmation,mbYes,mbNo)==mrYes then
    --    return true      
    --  end
    --end
    dma.offsets=nil
    dma.kernelversion=nil
  end
  
  return r,err
end

function dma.saveData()
  assert(dma.datapath,'dma: no datapath set')
  assert(dma.offsets, 'dma.saveData called with no offsets found')
    
  local sl=createStringList()
  local currentKernelVersion=RtlGetVersion()
  if currentKernelVersion==nil then 
    currentKernelVersion=GetNTDLLVersion()
  end
  
  sl.add('dma.kernelversion={}')
  for buildinfo,value in pairs(currentKernelVersion) do
    sl.add('dma.kernelversion.'..buildinfo..'='..value)
  end  
  
  sl.add('dma.offsets={}')
  for offsetname,offset in pairs(dma.offsets) do
    sl.add('dma.offsets.'..offsetname..'='..offset)
  end
  
  sl.saveToFile(dma.datapath)
  sl.destroy()
end

function dma.init()
  if not (dbk_initialized() or dbvm_initialized()) then
    dbk_useKernelmodeProcessMemoryAccess()
  end

  if dma.initialized then return true end
  if getOpenedProcessID()==0 or readByte(process)==nil then
    openProcess(getCheatEngineProcessID())
  end
    
  
  dma.datapath=getTempFolder()..configname
  enableKernelSymbols()
  
  if PsInitialSystemProcess==nil then

    
    if ExportScannerThread==nil then
      ExportScannerThread=createThread(function(t)
        t.freeOnTerminate(false)
        
        while not t.Terminated and PsInitialSystemProcess==nil do
          PsInitialSystemProcess=getAddressSafe('PsInitialSystemProcess')
          sleep(10)
        end
      end)
    end
  end
  
  
  if dma.offsets==nil then
    if fileExists(dma.datapath) then

      local r,err=dma.loadData(dma.datapath)
      if r then 
        outputDebugString("loadData successful")
      end
    end
    
    if dma.offsets==nil then
      OutputDebugString("Obtaining offsets")
      --first try with debug symbols from microsoft

      enableWindowsSymbols()    
      searchPDBWhileLoading(true)

      local f=createForm(false)
      f.Caption='Loading symbols'
      local t=createLabel(f)
      t.Caption=[[First time init. Please wait for the symbols to load
  (Close this window to try the brute force method)
      ]]
      t.align='alClient'

      t.AutoSize=true
      f.AutoSize=true
      f.OnClose=function()
        f=nil
        return caFree
      end

      --f.OnCloseQuery=function() return false end
      f.Position='poScreenCenter'

      local struct_EPROCESS
      local struct_KPROCESS
      local struct_PEB
      local struct_PEB_LDR_DATA
      local struct_LDR_DATA_TABLE_ENTRY
      local struct_USER_PROCESS_PARAMETERS
      createThread(function(t)
        while struct_EPROCESS==nil do
          struct_EPROCESS=getStructureElementsFromName('_EPROCESS')
          if symbolsDoneLoading() then break end

          if struct_EPROCESS==nil then sleep(25) end
        end

        while struct_KPROCESS==nil do
          struct_KPROCESS=getStructureElementsFromName('_KPROCESS')
          if symbolsDoneLoading() then break end

          if struct_KPROCESS==nil then sleep(25) end
        end


        while struct_PEB==nil do
          struct_PEB=getStructureElementsFromName('_PEB')
          if symbolsDoneLoading() then break end

          if struct_PEB==nil then sleep(25) end
        end

        while struct_PEB_LDR_DATA==nil do
          struct_PEB_LDR_DATA=getStructureElementsFromName('_PEB_LDR_DATA')
          if symbolsDoneLoading() then break end

          if struct_PEB_LDR_DATA==nil then sleep(25) end
        end

        while struct_LDR_DATA_TABLE_ENTRY==nil do
          struct_LDR_DATA_TABLE_ENTRY=getStructureElementsFromName('_LDR_DATA_TABLE_ENTRY')
          if symbolsDoneLoading() then break end
          
          if struct_LDR_DATA_TABLE_ENTRY==nil then sleep(25) end
        end
        
        while struct_USER_PROCESS_PARAMETERS==nil do
          struct_USER_PROCESS_PARAMETERS=getStructureElementsFromName('_RTL_USER_PROCESS_PARAMETERS')
          if symbolsDoneLoading() then break end
          
          if struct_USER_PROCESS_PARAMETERS==nil then sleep(25) end        
        end

        while struct_USER_PROCESS_PARAMETERS==nil do
          struct_USER_PROCESS_PARAMETERS=getStructureElementsFromName('_RTL_USER_PROCESS_PARAMETERS')
          if symbolsDoneLoading() then break end
          
          if struct_USER_PROCESS_PARAMETERS==nil then sleep(25) end        
        end
        


        --waitForPDB()
        synchronize(function()
          --f.OnCloseQuery=nil
          if f then
            f.modalResult=mrYes
          end
        end)
      end)

      f.showModal()
      

      if struct_EPROCESS and struct_PEB then
        local ep={}
        for i=1,#struct_EPROCESS do
          ep[struct_EPROCESS[i].name]=struct_EPROCESS[i].offset
        end

        local kp={}
        for i=1,#struct_KPROCESS do
          kp[struct_KPROCESS[i].name]=struct_KPROCESS[i].offset
        end

        local peb={}
        for i=1,#struct_PEB do
          peb[struct_PEB[i].name]=struct_PEB[i].offset
        end

        local peb_ldr_data={}
        for i=1,#struct_PEB_LDR_DATA do
          peb_ldr_data[struct_PEB_LDR_DATA[i].name]=struct_PEB_LDR_DATA[i].offset
        end

        local ldr_data_table_entry={}
        for i=1,#struct_LDR_DATA_TABLE_ENTRY do
          ldr_data_table_entry[struct_LDR_DATA_TABLE_ENTRY[i].name]=struct_LDR_DATA_TABLE_ENTRY[i].offset
        end
        
        local user_process_parameters={}
        for i=1,#struct_USER_PROCESS_PARAMETERS do
          user_process_parameters[struct_USER_PROCESS_PARAMETERS[i].name]=struct_USER_PROCESS_PARAMETERS[i].offset
        end        

        dma.offsets={}

        dma.offsets.KProcess_DirectoryTableBase=kp.DirectoryTableBase
        dma.offsets.KProcess_UserDirectoryTableBase=kp.UserDirectoryTableBase      
        dma.offsets.EProcess_UniqueProcessId=ep.UniqueProcessId
        dma.offsets.EProcess_ActiveProcessLinks=ep.ActiveProcessLinks
        dma.offsets.EProcess_Peb=ep.Peb
        dma.offsets.EProcess_WoW64Process=ep.WoW64Process
        dma.offsets.Peb_Ldr=peb.Ldr
        dma.offsets.Peb_ProcessParameters=peb.ProcessParameters
        dma.offsets.ProcessParameters_ImagePathName=user_process_parameters.ImagePathName
        dma.offsets.Peb_Ldr_data_InLoadOrderModuleList=peb_ldr_data.InLoadOrderModuleList --likely 10
        dma.offsets.Ldr_data_table_entry_DllBase=ldr_data_table_entry.DllBase
        dma.offsets.Ldr_data_table_entry_SizeOfImage=ldr_data_table_entry.SizeOfImage
        dma.offsets.Ldr_data_table_entry_FullDllName=ldr_data_table_entry.FullDllName
        dma.offsets.Ldr_data_table_entry_BaseDllName=ldr_data_table_entry.BaseDllName

        dma.offsets.UnicodeBuffer=8
        
        dma.saveData()
      end
    end


    if dma.offsets==nil then
      --pdb lookup canceled or failed, try the brute force way
      print("manual offset lookup not yet implemented. Sorry")
      return false

    end

  end
  
  --setup an openProcess override as the processlist will be returning the CR3
  --issue: OpenProcess only takes dword input so the pid's can not be the CR3 value (unless you're on a 32-bit system) so go for pid, but set bit 1 of the pid to 1 if the KProcess_UserDirectoryTableBase is requested
 -- print("assembling CR3OpenProcess") 
  
  local aaresult, diorerr, warnings=autoAssemble([[
//This makes Kernel32_CloseHandle available to the C compiler
label(Kernel32_CloseHandle)
label(Kernel32_IsWow64Process)

Kernel32.CloseHandle:
Kernel32_CloseHandle:

Kernel32.IsWow64Process:
Kernel32_IsWow64Process:

{$c}
#include <lua.h>

typedef size_t HANDLE;

lua_State *GetLuaState();

size_t getCR3FromPID(int pid) //use lua for this
{
  lua_State *L=GetLuaState(); //gets the lua state for the current thread. If it's a thread with no lua state yet, it will be created. On thread termination it will be autofreed

  lua_getglobal(L,"dma");
  if (lua_istable(L,-1))
  {
    lua_pushstring(L,"getCR3FromPID");
    lua_gettable(L,-2); //push dma.getCR3FromPID on the stack
    lua_pushinteger(L,pid); //push the pid on the stackl
    if (lua_pcall(L, 1,1,0)==0)
    {
      size_t result=lua_tointeger(L,-1);
      lua_pop(L,1); //result of pcall
      
      lua_pushstring(L,"openedHandles");
      lua_gettable(L,-2); //returns dma.openedHandles (for handling closeHandle)
      lua_pushinteger(L,result);
      lua_pushboolean(L,1);
      lua_settable(L,-3); //set the returned cr3 to true in dma.openedHandles
      
      lua_pop(L,2); //1=pops the openedHandles tables, 2=pops the dma table
      
      return result;    
    }
    else
      lua_pop(L,1); //pushed error message
  }

  lua_pop(L,1); //whatever was on the stack
    
  return 0;
}

__stdcall size_t CR3OpenProcess(unsigned int dwDesiredAccess, int bInheritHandle, unsigned int dwProcessID)
{
    size_t r=getCR3FromPID(dwProcessID);
    r=r & ~(0xfff); //remove any unnecesary bits
    return r;
}  


__stdcall size_t Kernel32_CloseHandle(size_t handle);

int isCR3Handle(size_t handle)
{
  int result=0;
  lua_State *L=GetLuaState();
  
  lua_getglobal(L,"dma");
  if (lua_istable(L,-1))
  {
    lua_pushstring(L,"openedHandles");
    lua_gettable(L,-2); //get dma.openedHandles
    
    if (lua_istable(L,-1))
    {
      lua_pushinteger(L,handle);
      lua_gettable(L,-2); //get dma.openedHandles[handle]
      //nil will be returned as false
      result=lua_toboolean(L,-1); 
      lua_pop(L,1); //result of dma.openedHandles[handle]      
    }
    lua_pop(L,1); //result of dma.openedHandles
  }
  lua_pop(L,1); //getglobal result
  
  return result;
}

__stdcall int CR3CloseHandle(size_t handle)
{
    if ((handle & 0xfff)==0) //don't even bother checking if it's not xxx000
    {
      //maybe handle it, check dma.openedHandles
      if (isCR3Handle(handle))
        return 1;
    }
    
    //still here, call the original handler (SetApiPointer is not a hook, just a pointer, so doesn't affect the code)
    return Kernel32_CloseHandle(handle);
}  

__stdcall int Kernel32_IsWow64Process(size_t hProcess, int *Wow64Process);

__stdcall int CR3IsWow64Process(size_t hProcess, int *Wow64Process)
{
  if (((hProcess & 0xfff)==0) && (isCR3Handle(hProcess)) ) 
  {
    if (Wow64Process==(int*)0)
      return 0;
      
    lua_State *L=GetLuaState();
    
    lua_getglobal(L, "dma");
    if (lua_istable(L,-1))
    {
      lua_pushstring(L,"IsWow64Process"); //dma.IsWow64Process
      lua_gettable(L,-2);
      
      if (lua_isfunction(L,-1))
      {
        lua_pushinteger(L,hProcess);
        if (lua_pcall(L,1,1,0)==0)
        {          
          *Wow64Process=lua_toboolean(L,-1);
          lua_pop(L,1);
          
        }
        else
          lua_pop(L,1);
      }
      else
        lua_pop(L,1);
    }
    
    lua_pop(L,1);
    
    return 1; 
  }
  else
  {
    return Kernel32_IsWow64Process(hProcess, Wow64Process);
  }
}

size_t CR3CreateToolhelp32Snapshot(unsigned int dwFlags, unsigned int th32ProcessID)
{
  return -1; //I could implement the modulelist here, but i'm going for a seperate symbollist method (faster)
  
  //feel free to implement it though. There are a few functions that use this and not the callback
}

{$asm}

kernel32.CreateToolhelp32Snapshot:
mov rax,ffffffffffffffff
ret

kernelbase.EnumProcessModulesEx:
xor rax,rax
ret


]],true)
  
  if not aaresult then
    MessageDialog('CR3Override error: '..diorerr, mtError)
    return  
  end 
  
  if warnings then
    print("warnings="..warnings)
  end
  
  diorerr.ccodesymbols.name='DMA Script C-part'
  dma.scriptresult=diorerr
 
  --get the initial CR3 to use (Needed for kernelmode EProcess access)
  if ExportScannerThread then
    ExportScannerThread.waitfor()
    ExportScannerThread.destroy()
    ExportScannerThread=nil
  end
  
  if PsInitialSystemProcess==nil then
    MessageDialog('PsInitialSystemProcess is nil', mtError)
    return
  end
  
  if dbk_initialized() then
    dbk_useKernelmodeProcessMemoryAccess()
    
    local InitialEProcess=readQword(PsInitialSystemProcess)
    if InitialEProcess==nil then
      OpenProcess(getCheatEngineProcessID()) --maybe the driver was loaded but no kernelmode openprocess yet. try again
      InitialEProcess=readQword(PsInitialSystemProcess)
    end
    
    if InitialEProcess==nil then
      messageDialog("PsInitialSystemProcess is unreadable", mtError)
      return false
    end
    
    dma.SystemCR3=readQword(InitialEProcess+dma.offsets.KProcess_DirectoryTableBase)
  elseif dbvm_initialized() then
    --dbk is not loaded.  use dbvm
    dbvm_log_cr3_start()
    sleep(1)
    readQword(process)
    sleep(1)
    local cr3list=dbvm_log_cr3_stop()
    table.sort(cr3list,function(v1,v2) return v1<v2 end)
    
    for i=1,#cr3list do
      --find a cr3 that has access to PsInitialSystemProcess
      --(sure, I could use any of these in the list. But looking for one that won't close after a while. And if page protection is enabled then dbvm_getcr3 would return the usermode cr3 which can not access kernelmode)
      local btInitialEProcess=readProcessMemoryCR3(cr3list[i], PsInitialSystemProcess, 8)
      if btInitialEProcess and #btInitialEProcess==8 then
        local InitialEProcess=byteTableToQword(btInitialEProcess)
        local btInitialCR3=readProcessMemoryCR3(cr3list[i], InitialEProcess+dma.offsets.KProcess_DirectoryTableBase, 8)
        if btInitialCR3 and #btInitialCR3==8 then
          dma.SystemCR3=byteTableToQword(btInitialCR3)
          break
        end
      end      
    end
  else
    --maybe you have a dma card. In which case you may have physical memory access.  This still needs a routine to reference a virtual address back to the physical address
    --and stuff like PsInitialSystemProcess may be different from your lookup system and the target system so needs adjusting as well   
    
    messageDialog("No cr3 lookup memory access function available. If you do have one, adjust dmahelper.lua", mtError)
    return false
  end
  
 -- printf("SystemCR3=%x",dma.SystemCR3)
  
  if dma.SystemCR3==nil then
    messageDialog("SystemCR3 is nil", mtError)
    return false
  end
  
  
  CR3OpenProcess=getAddressSafe("CR3OpenProcess",true)
  CR3CloseHandle=getAddressSafe("CR3CloseHandle",true)
  CR3IsWow64Process=getAddressSafe("CR3IsWow64Process",true)
  
  if CR3OpenProcess==nil then
    messageDialog("CR3OpenProcess could not be found", mtError)
    return false  
  end
  
  if CR3CloseHandle==nil then
    messageDialog("CR3CloseHandle could not be found", mtError)
    return false  
  end  
  
  if CR3IsWow64Process==nil then
    messageDialog("CR3IsWow64Process could not be found", mtError)
    return false  
  end
  

  dma.SetApiPointer=function()
    setAPIPointer(0, CR3OpenProcess) --next openprocess will set cr3 as handle
    setAPIPointer(1, getAddressSafe("ReadProcessMemoryCR3",true))
    setAPIPointer(2, getAddressSafe("WriteProcessMemoryCR3",true))  
    setAPIPointer(3, getAddressSafe("VirtualQueryExCR3",true)) 
    setAPIPointer(4, CR3CloseHandle)      
    setAPIPointer(5, CR3IsWow64Process)  
    setAPIPointer(6, getAddressSafe("CR3CreateToolhelp32Snapshot",true)) 
    setAPIPointer(7, getAddressSafe("VirtualToPhysicalCR3", true))
  end  

  onAPIPointerChange(dma.SetApiPointer)
  dma.SetApiPointer()
  OpenProcess(4) --opens the system process for kernel access (just an initial init. not required. just handy to check stuff)
  
  registerProcessListCallback(dma.getProcessList)
  
  registerModuleListCallback(dma.enumModules)
  
  
  dma.initialized=true 
  return true
end

function dma.getCR3FromPID(pid)
  --printf("getCR3FromPID(%d)", pid)
  if pid==4 then
    return dma.SystemCR3
  else
    if dma.lastProcessList==nil then
      dma.getProcessList()
    end
    local r=dma.lastProcessList.pidmap[pid]
    if r then
      return r.CR3
    end
  end
end

function dma.KernelQword(address, cr3)
  local r=readProcessMemoryCR3(cr3 or dma.SystemCR3, address,8)
  if r and #r==8 then
    return byteTableToQword(r)
  end
end

function dma.KernelDword(address, cr3)
  local r=readProcessMemoryCR3(cr3 or dma.SystemCR3, address,4)
  if r and #r==4 then
    return byteTableToDword(r)
  end
end

function dma.KernelWord(address, cr3)
  local r=readProcessMemoryCR3(cr3 or dma.SystemCR3, address,2)
  if r and #r==2 then
    return byteTableToWord(r)
  end
end


function dma.KernelUnicodeString(address, cr3)
  local length=dma.KernelWord(address, cr3)
  if length and length~=0 then
    local ntStrPtr=dma.KernelQword(address+8, cr3)
    local ntStr=readProcessMemoryCR3(cr3 or dma.SystemCR3, ntStrPtr,length)
    if ntStr then
      return byteTableToWideString(ntStr)
    end
  end
end

function dma.UnicodeString(address)
  local length=readSmallInteger(address)
  if length and length~=0 then
    local ntStrPtr=readPointer(address+8)
    local ntStr=readBytes(ntStrPtr,length,true)
    if ntStr then
      return byteTableToWideString(ntStr)
    end
  end
end

function dma.IsWow64Process(handle)
 
  if handle==nil then
    handle=getOpenedProcessHandle()
  end
  local Wow64ProcessPointer
  local e=dma.lastProcessList.cr3map[handle]  
  if e then
    Wow64ProcessPointer=dma.KernelQword(e.EProcess+dma.offsets.EProcess_WoW64Process)
  end
  
  return Wow64ProcessPointer and Wow64ProcessPointer~=0  
end


function dma.getProcessList()
  if not dma.initialized then return nil,'dma not initialized' end

  local seen={}
  local list
  list={} --indexed list (for the processlist)
  list.pidmap={} --fill it as pid=entry (for pid to cr3 lookups)
  list.eprocessmap={}
  list.cr3map={}

  local entry --pid, cr3, name, path

  --use dma.SystemCR3 to get the processlist

  currentEProcess=dma.KernelQword(PsInitialSystemProcess)

  while currentEProcess do
    if seen[currentEProcess] then 
      --printf("seen")
      break 
    end
    seen[currentEProcess]=true

    local e={}
    e.Pid=dma.KernelDword(currentEProcess+dma.offsets.EProcess_UniqueProcessId)
    e.EProcess=currentEProcess
    e.CR3=dma.KernelQword(currentEProcess+dma.offsets.KProcess_DirectoryTableBase)

    
    local peb=dma.KernelQword(currentEProcess+dma.offsets.EProcess_Peb, e.CR3)
    if peb and peb~=0 then
      --printf("peb=%x", peb)
      local ProcessParameters=dma.KernelQword(peb+dma.offsets.Peb_ProcessParameters, e.CR3)

      if ProcessParameters and ProcessParameters~=0 then
        local ImageNamePath=ProcessParameters+dma.offsets.ProcessParameters_ImagePathName

        local name=dma.KernelUnicodeString(ImageNamePath, e.CR3)

        if name then
          e.PathToFile=name
          e.Name=extractFileName(name)
          table.insert(list,e)
          list.pidmap[e.Pid]=e
          list.eprocessmap[e.EProcess]=e
          list.cr3map[e.CR3]=e
        end

      end
    end



    local userpagedir=dma.KernelQword(currentEProcess+dma.offsets.KProcess_DirectoryTableBase);
    if e.Name and userpagedir and userpagedir~=0 and userpagedir~=e.CR3 then
      local e2={}
      e2.Name=e.Name
      e2.PathToFile=e.PathToFile
      e2.CR3=userpagedir
      e2.EProcess=e.EProcess
      e2.Pid=e.Pid+1 --special marker so show it's the usermode pagetable only

      table.insert(list,e2)
      list.pidmap[e.Pid]=e2
      list.eprocessmap[e.EProcess]=e2
      list.cr3map[e.CR3]=e2
    end

    currentEProcess=dma.KernelQword(currentEProcess+dma.offsets.EProcess_ActiveProcessLinks)-dma.offsets.EProcess_ActiveProcessLinks
  end

  dma.lastProcessList=list --use this for pid lookups

  return list

end

function dma.enumModules(pid)
  if dma.lastProcessList==nil then
    dma.getProcessList()
  end

  local pe
  if pid and pid~='' then
    pe=dma.lastProcessList.pidmap[pid]

    if pe==nil then
      local list=dma.getProcessList()
      pe=list.pidmap[pid]
    end
  else
    --no pid given, use the processhandle which is a cr3 value
    pe=dma.lastProcessList.cr3map[getOpenedProcessHandle()]
  end

  if pe==nil then
    return nil,'Process not found'
  end

  local eprocess=pe.EProcess
  
  local iswow64=dma.IsWow64Process(pe.CR3)

  local pebaddress=dma.KernelQword(eprocess+dma.offsets.EProcess_Peb)
  if pebaddress==nil or pebaddress==0 then
    return nil,'no peb found'
  end

  local ldr=readPointer(pebaddress+dma.offsets.Peb_Ldr)
  if ldr==nil or ldr==0 then
    return nil,'peb.ldr==nil'
  end

  --printf("count=%d",readInteger(ldr))
  local InLoadOrderModuleList=readPointer(ldr+dma.offsets.Peb_Ldr_data_InLoadOrderModuleList)
  if InLoadOrderModuleList==nil or InLoadOrderModuleList==0 then
    return nil,'InLoadOrderModuleList==nil'
  end

  local seen={}

  local results={}

  while InLoadOrderModuleList do
    if seen[InLoadOrderModuleList] then break end
    seen[InLoadOrderModuleList]=true

    local e={}
    e.Name=dma.UnicodeString(InLoadOrderModuleList+dma.offsets.Ldr_data_table_entry_BaseDllName)    
    e.Address=readPointer(InLoadOrderModuleList+dma.offsets.Ldr_data_table_entry_DllBase)
    e.Size=readPointer(InLoadOrderModuleList+dma.offsets.Ldr_data_table_entry_SizeOfImage)
    e.PathToFile=dma.UnicodeString(InLoadOrderModuleList+dma.offsets.Ldr_data_table_entry_FullDllName) --if you're using a DMA device, I recommend changing the path here to a netwrork path with the file
    
    e.Is64Bit=(iswow64==false) or (e.base>=0x100000000)
    if (e.Address~=0) and (e.Size~=0) and (e.Name) and (e.PathToFile) and (e.Name~='') and (e.PathToFile~='') then
      table.insert(results,e)
    end
    InLoadOrderModuleList=readPointer(InLoadOrderModuleList)
  end

  return results
end


if getOperatingSystem()==0 then
  forEachAndFutureForm("TProcessWindow", function(f)
    --print("weee")
    _G.fff=f
    if f.miDMA==nil then
      local miDMA=createMenuItem(f)
      miDMA.Caption='DMA'
      miDMA.Name='miDMA'
      f.Menu.Items.add(miDMA)

      local miLocalDMA=createMenuItem(f)
      miLocalDMA.Caption='Local DMA'
      miLocalDMA.Name='miLocalDMA'

      miDMA.add(miLocalDMA)
      miLocalDMA.OnClick=function()
        if messageDialog(translate('Switch to DMA mode? You have to restart CE to undo this'), mtWarning, mbYes,mbNo) ~= mrYes then return end
        
        if dma.init() then          
          for i=0, f.TabHeader.PageCount-1 do
            f.TabHeader.Page[i].TabVisible=false        
          end
          f.TabHeader.TabIndex=-1 --processlist as well
          
          getProcessList() --fixes a small issue
          f.miRefresh.doClick()        

        end
  
      end
    end      
    
    local oldOnShow=f.OnShow
    f.OnShow=function(f)
      if f.miDMA then
        f.miDMA.visible=dbk_initialized() or dbvm_initialized()
      end
      oldOnShow(f)
    end

  end)  
  
  

end