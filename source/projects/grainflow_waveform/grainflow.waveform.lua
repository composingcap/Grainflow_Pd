local Wave = pd.Class:new():register("grainflow.waveform")

-- =====================================
function Wave:initialize(sel, atoms)


    self.inlets = 1
    self.outlets = 1
    --canvas size (default values)
    self.width = 300
    self.height = 100
    
    -- flags
    self.canDraw = false
    self.mouseOn = false 
    self.drawRec = false
    
    --mouse position info
    self.mousePos = {0,0}
    
    -- grain arrays of data

    self.grainState = {}
    self.grainProgress = {}
    self.grainPosition = {}
    self.grainAmp = {}
    self.grainWindow = {}
    self.grainChannel = {}
    self.grainStream = {}
    self.readPosition = 0 --mouse position for reading 

    -- color values
    self.recordHeadC =  {r = 100,g = 100, b = 255,a = 1.0}
    self.waveformC = {r = 0,g = 0, b = 0,a = 1.0}
    self.grainC = {r = 121,g = 115, b = 150,a = .5}
    self.mouseC = {r = 100,g = 100, b = 100,a = 1.0}
    self.backgroundC = {r = 255,g = 255, b = 255,a = 1.0}
    self.borderC = {r = 100,g = 100, b = 100,a = 1.0}
    self.grainSize =  .1
    
    -- waveform array
    self.arrayAmount = 0
    self.arrayLength = 0
    self.startIndex = 0
    self.arrayName = {}
    self.sizeH  =  self.height /  self.arrayAmount

    --redraw speed
    self.delay_time = 20*2
    
    self:set_size(self.width, self.height)
    self:restore_state(atoms)
    return true
end
-- =====================================
function Wave:restore_state(atoms)

if atoms and #atoms >= 27 then
    self.width       =  atoms[1]
    self.height      =  atoms[2]
    self.recordHeadC = { r = atoms[3],  g = atoms[4],  b = atoms[5],  a = atoms[6] }
    self.waveformC   = { r = atoms[7],  g = atoms[8],  b = atoms[9],  a = atoms[10] }
    self.grainC      = { r = atoms[11],  g = atoms[12], b = atoms[13], a = atoms[14] }
    self.mouseC      = { r = atoms[15], g = atoms[16], b = atoms[17], a = atoms[18] }
    self.backgroundC = { r = atoms[19], g = atoms[20], b = atoms[21], a = atoms[22] }
    self.borderC     = { atoms[23], atoms[24], atoms[25], atoms[26] }
    self.grainSize   = atoms[27]
    
    self:repaint()  
else
    pd.post("No valid state to restore; received " .. (atoms and #atoms or 0) .. " values.")
end
end
-- =====================================
function Wave:save_state()


    local state = { 
    self.width, self.height, 
    self.recordHeadC.r, self.recordHeadC.g, self.recordHeadC.b, self.recordHeadC.a,
    self.waveformC.r, self.waveformC.g, self.waveformC.b, self.waveformC.a,
    self.grainC.r,    self.grainC.g,    self.grainC.b,    self.grainC.a,
    self.mouseC.r,    self.mouseC.g,    self.mouseC.b,    self.mouseC.a,
    self.backgroundC.r, self.backgroundC.g, self.backgroundC.b, self.backgroundC.a,
    self.borderC.r,  self.borderC.g,  self.borderC.b,  self.borderC.a,
    self.grainSize
    }
    self:set_args(state)
    pd.post("State saved.")
end
-- =====================================
function Wave:postinitialize()
    self.clock = pd.Clock:new():register(self, "tick")
    self.clock:delay(self.delay_time)
end
-- =====================================
function Wave:finalize()
    self.clock:destruct()
end
-- =====================================
function Wave:tick()

    self:repaint(2)
    self:repaint(4)  
    self:repaint(5)  
    self:repaint(6) 
    self:repaint(7) 
    self.clock:delay(self.delay_time)

end

-- =====================================
function Wave:mouse_down(x, y)
    local w, h = self:get_size()
    if self.mouseOn == false  and x >= 0 and y >= 0  and x <= w and y <= h then
        self.mousePos[1] = normalize(x,0,w)
        self.mousePos[2] = normalize(y,h,0)
        self.readPosition = self.mousePos[1]
        self:outlet(1,"list" , self.mousePos)
        self:outlet(1,"list" , self.mousePos)
        self.mouseOn = true
    end
end
-- =====================================
function Wave:mouse_drag(x, y)
    local w, h = self:get_size()
    if self.mouseOn  and x >= 0 and y >= 0  and x <= w and y <= h then
        self.mousePos[1] = normalize(x,0,w)
        self.mousePos[2] = normalize(y,h,0)
        self.readPosition = self.mousePos[1]
        self:outlet(1,"list" , self.mousePos)
    end
end
-- =====================================
function Wave:mouse_up(x, y)
    local w, h = self:get_size()
    if self.mouseOn  and x >= 0 and y >= 0  and x <= w and y <= h then
        self.mouseOn = false
    end
end
-- =====================================
function Wave:in_1_bang()

end
-- =====================================
function Wave:in_1(sel, atoms)
-- parses all inputs and sends to proper functions 
    local name = atoms[1]
    local size =  #(atoms)
    local funct =  Wave[name]
    -- need to make sure its a function to avoid calling nil crashs 
    if type(funct) == "function" then  -- works when the word list is called first
        local args = {}
        for i = 2, size do
            table.insert(args, atoms[i])
        end
        funct(self,args)
    else -- if you send message without prepend list (not how you shoud in pd then this will process the function )
        local nameN = sel
        local nameF =  Wave[nameN]
        if type(nameF) == "function" then 
            local argsu = atoms
            nameF(self,argsu)
        end
    end
end

-- =====================================
function Wave:bufferName(atoms)
--not needed
end

-- =====================================
function Wave:grainState(atoms)
    self.grainState = atoms    
end
-- =====================================
function Wave:grainProgress(atoms)
    self.grainProgress = atoms
end
-- =====================================
function Wave:grainPosition(atoms)
    self.grainPosition = atoms
end
-- =====================================
function Wave:grainAmp(atoms)
    self.grainAmp = atoms
end
-- =====================================
function Wave:grainWindow(atoms)
    self.grainWindow = atoms
end

-- =====================================
function Wave:grainChannel(atoms)
    self.grainChannel = atoms
end
-- =====================================
function Wave:grainStream(atoms)
    self.grainStream = atoms
end
-- =====================================
function Wave:bufferList(atoms)
--self.redawAble = false
self.arrayAmount = # (atoms)
self.arrayName = atoms
self.sizeH =  self.height /  self.arrayAmount
end

-- =====================================
function Wave:readPosition(atoms)
--not needed
end
-- =====================================
function Wave:recordHead(atoms)

    self.readPosition = atoms[1]  * self.width
    self.drawRec = true
end
-- =====================================
function Wave:recordHeadSamps(atoms)

end
-- =====================================
function Wave:backgroundColor(atoms)
-- store and change background color
    self.backgroundC = { r = atoms[1], g = atoms[2], b = atoms[3], a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:borderColor(atoms)
-- store and change boarder color
    self.borderC     = { r = atoms[1],  g = atoms[2],  b = atoms[3],  a = atoms[4]  }
    self:repaint(7)
    self:save_state()
end
-- =====================================
function Wave:waveformColor(atoms)
-- store and change waveform color
    self.waveformC   = { r = atoms[1],  g = atoms[2],  b = atoms[3],  a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:grainColor(atoms)
-- store and change grain color
    self.grainC      = { r = atoms[1],  g = atoms[2], b = atoms[3], a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:pointerColor(atoms)
-- store and change mouse pointer bar color   
    self.mouseC      = { r = atoms[1], g = atoms[2], b = atoms[3], a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:recordingColor(atoms)
-- store and change recording  (for gflowLive) pointer bar color   
    self.recordHeadC    = { r = atoms[1],  g = atoms[2],  b = atoms[3],  a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:setSize(atoms)
-- store changes to size of object with setSize message
    self.width = atoms[1]   
    self.height = atoms[2]
    self:set_size(self.width, self.height)
    self:save_state()
    self.sizeH  =  self.height /  self.arrayAmount
end
-- =====================================
function Wave:grainSize(atoms)
-- store changes to size of grain circles with grainSize message
    self.grainSize   = atoms[1]
    self:save_state()

end
-- ===========Paint functions ==========
-- =====================================
function Wave:paint(g)
 -- paint only the background canvas layer   
    local w, h = self:get_size()
    g:set_color(self.backgroundC.r,self.backgroundC.g,self.backgroundC.b,self.backgroundC.a)
    g:fill_rect(0, 0, w, h)
end
-- ///////////////////////////////////
-- ======================================
function Wave:paint_layer_2(g)
-- draw the waveform 
    local colorW = self.waveformC
    local w, h = self:get_size()

    if self.arrayAmount <= 0  then
        -- Draw an invisible rectangle so the layer is not empty
        g:set_color(0,0,0,0)
        g:fill_rect(0, 0, 1, 1)
        return
    end    

    local amount = self.arrayAmount
    local sizeH =  h / amount
    local waveHeight = sizeH /2
    g:set_color(colorW.r,colorW.g,colorW.b,colorW.a)
    
    for k = 1, amount do 
        local array = pd.table(self.arrayName[k])
        if array then -- currently drawing only the first sample of each chunk 
            local size = array:length()
            local middleH = sizeH * (k-1) + (sizeH/2)
            local chunk = math.floor(size /w)
            local startY = (array:get(0) * waveHeight) + middleH
            local W = Path(0,startY )
            
            for i = 1, w - 1 do
                local val = array:get(chunk * i)
                local y =  (val * waveHeight) +  middleH
                W:line_to(i, y)
            end
            g:stroke_path(W, 1)
        end
    end
end
-- ======================================

-- ======================================================

function Wave:paint_layer_4(g)
    -- Cache the current state and clear it immediately
    local drawRec = self.drawRec
    self.drawRec = false
    if not drawRec then
        g:set_color(0, 0, 0, 0)
        g:fill_rect(0, 0, 1, 1)
        return
    end

    local w, h = self:get_size()
    local rc = self.recordHeadC
    local readPos = self.readPosition
    g:set_color(rc.r, rc.b, rc.g, rc.a)
    g:draw_line(readPos, 0, readPos, h, 2)
    

end
-- ======================================================
function Wave:paint_layer_5(g)
    local mouseOn = self.mouseOn
    local drawRec = self.drawRec

    if not (mouseOn and not drawRec) then
        g:set_color(0, 0, 0, 0)
        g:fill_rect(0, 0, 1, 1)
        return
    end
    local w, h = self:get_size()
    local mc       = self.mouseC
    local mousePos = self.mousePos

    local x = mousePos[1] * w
    local y = (1 - mousePos[2]) * h  -- Note: 'y' is computed but not used in drawing

    g:set_color(mc.r, mc.b, mc.g, mc.a)
    g:draw_line(x, 0, x, h, 2)
end


-- ======================================================
function Wave:paint_layer_6(g)

    local amount = # (self.grainState)
    
    if amount == 0 then
        g:set_color(0, 0, 0, 0)
        g:fill_rect(0, 0, 1, 1)
        return
    end
    local w, h = self:get_size()
    local grainState    = self.grainState
    local grainPosition = self.grainPosition
    local grainAmp      = self.grainAmp
    local grainChannel  = self.grainChannel
    local grainProgress = self.grainProgress
    local sizeH         = self.sizeH
    local gc            = self.grainC


    for i = 1, amount  do
    
        if not (grainState[i] == 1 and grainPosition[i]  and grainAmp[i] and grainChannel[i] and grainProgress[i]) then
            goto continue
        end    
    
        local radius = ( sizeH * .1) *  (1 - 2 * (grainProgress[i] - 0.5)^2)
        
        local x = grainPosition[i] * w
        local y = (((grainChannel[i] - 1) * sizeH) + ((1 - grainAmp[i]) * sizeH) + (sizeH * .1) + 4) * .8

        local bright = 1 - (grainProgress[i] * 1)
                
        g:set_color( gc.r *bright, gc.g *bright, gc.b, gc.a)

        g:fill_ellipse(x- (radius/2),y- (radius/2), radius,radius)
                
         --  g:set_color( gc.r *brighF, gc.g *brighF, gc.b, gc.a)
    
        --  g:stroke_ellipse(x- (radius/2),y- (radius/2), radius,radius,1)
    ::continue::
    end
end   


function Wave:paint_layer_7(g)
    local w, h = self:get_size()
    local bc = self.borderC
    g:set_color(bc.r,bc.g,bc.b,bc.a)
    g:stroke_rounded_rect(1, 1, w , h , 9, 2)

end   
--==============================================
function normalize(val,min,max)
    local normalized = (val- min)/(max- min)
    return normalized
end
