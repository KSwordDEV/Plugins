androidNativeBridge={}
--support for android 9 for loading

function initAndroidNativeBridge()
  androidNativeBridge={}
  waitForPDB()

  local a=getAddressSafe('libnb.NativeBridgeItf')
  if a==nil then
    a=getAddressSafe('NativeBridgeItf')
  end
  if a then
    local pz=getPointerSize()
    androidNativeBridge.version=readInteger(a)
    androidNativeBridge.initialize=readPointer(a+pz)
    androidNativeBridge.loadLibrary=readPointer(a+pz*2)
    androidNativeBridge.getTrampoline=readPointer(a+pz*3)
    androidNativeBridge.isSupported=readPointer(a+pz*4)
    androidNativeBridge.getAppEnv=readPointer(a+pz*5)

    if androidNativeBridge.version>=2 then
      androidNativeBridge.isCompatibleWith=readPointer(a+pz*6)
      androidNativeBridge.getSignalHandler=readPointer(a+pz*7)

      if androidNativeBridge.version>=3 then
        androidNativeBridge.unloadLibrary=readPointer(a+pz*8)
        androidNativeBridge.getError=readPointer(a+pz*9)
        androidNativeBridge.isPathSupported=readPointer(a+pz*10)
        androidNativeBridge.initAnonymousNamespace=readPointer(a+pz*11)
        androidNativeBridge.createNamespace=readPointer(a+pz*12)
        androidNativeBridge.linkNamespaces=readPointer(a+pz*13)
        androidNativeBridge.loadLibraryExt=readPointer(a+pz*14)
        androidNativeBridge.getVendorNamespace=readPointer(a+pz*15)
      end
    end

    registerSymbol('anb_initialize', androidNativeBridge.initialize, true)
    registerSymbol('anb_iscompatiblewith', androidNativeBridge.isCompatibleWith, true)
    registerSymbol('anb_isSupported', androidNativeBridge.isSupported, true)
    registerSymbol('anb_loadLibrary', androidNativeBridge.loadLibrary, true)
    registerSymbol('anb_getError', androidNativeBridge.getError, true)
    registerSymbol('anb_loadLibraryExt', androidNativeBridge.loadLibraryExt, true)
    registerSymbol('anb_isPathSupported', androidNativeBridge.isPathSupported, true)
    registerSymbol('anb_createNamespace', androidNativeBridge.createNamespace, true)
    registerSymbol('anb_initAnonymousNamespace', androidNativeBridge.initAnonymousNamespace, true)
    registerSymbol('anb_getVendorNamespace', androidNativeBridge.getVendorNamespace, true)

    return androidNativeBridge
  end

end