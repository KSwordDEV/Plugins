
local function internalGetZipFileTableOfContents(s)
--[[
Reads the table of contents from a zip file
s is a table that contains:
  ms : MemoryStream - an empty memory stream
  read(self, offsetToRead, size): bytesRead - A function you provide to fill the memory stream with data. Return the number of bytes actually read

--]]

  local function bufferStream(offset,size)
    return s:read(offset, size)
  end
  if s==nil then
    return nil,'parameter 1 is nil'
  end

  if type(s)~='table' then
    return nil,'parameter is not a table'
  end

  if s.ms==nil then
    return nil,'missing the memorystream (ms)'
  end

  if s.read==nil then
    return nil,'read is missing'
  end

  local f=s.ms
  local result={}
  local fileoffset=0

  while bufferStream(fileoffset,512)>0 do
    local r={}
    local filename
    local header=f.readDword()

    if header~=0x4034b50 then
      return result
    end

    f.position=f.position+14
    local compressedSize=f.readDword()
    local size=f.readDword()

    local filenamelength=f.readWord()
    local extra=f.readWord()

    if (filenamelength>512-64) then --actually just 30, but adding some extra overhead
      --really long filename...
      local oldpos=f.Position
      bufferStream(fileoffset,64+filenamelength)

      f.Position=oldpos
    end


    filename=f.readString(filenamelength)

    r.filename=filename
    r.fileoffset=fileoffset+f.Position+extra
    r.compressedSize=compressedSize
    r.size=size
    table.insert(result, r)

    fileoffset=fileoffset+f.Position+extra+compressedSize

  end

  return result
end

function GetNetworkZipFileTableOfContents(path)
  local c=getCEServerInterface()
  local sh={
    ms=createMemoryStream(),
    read=function(self, offset,size)
      self.ms.size=0
     
      c.getFilePart(path,self.ms,offset,size)
      self.ms.position=0
      return self.ms.size
    end
  }

  return internalGetZipFileTableOfContents(sh)
end

function GetZipFileTableOfContents(path)
  local f=createFileStream(path, fmOpenRead | fmShareDenyNone)  --lazy assing. While an memorystream is expected, a filestream will work as well as no memorystream specifics are used and lua doesn't care

  if f==nil then
    return nil,'Failure opening '..path
  end

  local sh={
    ms=createMemoryStream(),

    read=function(self, offset,size)
      f.position=offset
      self.ms.clear()
      self.ms.copyFrom(f,size)
      self.ms.position=0
      return self.ms.size
    end
  }
  local r,r2=internalGetZipFileTableOfContents(sh)

  f.destroy()
  sh.ms.destroy()
  return r,r2
end

