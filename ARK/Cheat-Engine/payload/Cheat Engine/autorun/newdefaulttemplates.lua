--handy helper function
function forEachAndFutureForm(classname, func)
  local i
  for i=0,getFormCount()-1 do
    local f
    f=getForm(i)
    if f.ClassName==classname then
      func(f)
    end
  end

  registerFormAddNotification(function(f)
    if classname==f.ClassName then
      f.registerFirstShowCallback(function()
        func(f)
      end)
    end
  end)
end


forEachAndFutureForm('TfrmAutoInject',function(f)
  f.registerCreateCallback(function(f)
    if f.ScriptMode=='smAutoAssembler' then
      f.menuAOBInjection.OnClick=function(s)
        local address=getNameFromAddress(getMemoryViewForm().DisassemblerView.SelectedAddress)
        address=inputQuery('Code inject template+', 'On what address do you want the jump?', address)
        if address==nil then return end

        local name='INJECT'                
        local counter=1
        while f.assemblescreen.Lines.Text:find(name..':') do
          counter=counter+1
          name='INJECT'..counter
        end
        name=inputQuery('Code inject template+', 'What do you want to name the symbol for the injection point?', name)
        if name==nil then return end

        local radius=10
        radius=tonumber(inputQuery('Code inject template+', 'Comment radius?', radius));
        if radius==nil then radius=10 end
        

        generateAOBInjectionScript(f.assemblescreen.Lines, name, address, radius)
        
      end

      f.menuFullInjection.OnClick=function(s)
        local address=getNameFromAddress(getMemoryViewForm().DisassemblerView.SelectedAddress)
        address=inputQuery('Code inject template+', 'On what address do you want the jump?', address)
        if address==nil then return end

        local radius=10
        radius=tonumber(inputQuery('Code inject template+', 'Comment radius?', radius));
        if radius==nil then radius=10 end        

        generateFullInjectionScript(f.assemblescreen.Lines, address, radius)        
      end
    end
  end)
end)