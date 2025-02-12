local Wave = pd.Class:new():register("grainflow.waveform")

-- =====================================
function Wave:initialize(sel, atoms)


    self.inlets = 1
    self.outlets = 1

    self.width = 400
    self.height = 200
    
    -- flags
    self.canDraw = false
    self.mouseOn = false 
    self.drawRec = false
    


    --mouse info
    self.mousePos = {0,0}
    
    -- grain arrays of data

    self.grainState = {}
    self.grainProgress = {}
    self.grainPosition = {}
    self.grainAmp = {}
    self.grainWindow = {}
    self.grainChannel = {}
    self.grainStream = {}
    self.readPos = 0

    -- color values
    self.recHeadC =  {r = 100,g = 100, b = 255,a = 1.0}
    self.waveformC = {r = 0,g = 0, b = 0,a = 1.0}
    self.grainC = {r = 121,g = 115, b = 150,a = .5}
    self.mouseC = {r = 100,g = 100, b = 100,a = 1.0}
    self.backgroundC = {r = 255,g = 255, b = 255,a = 1.0}
    self.borderC = {100,100,100,1}
    self.grainsize =  .1
        -- waveform array
        self.arrayAmount = 0
        self.arrayLength = 0
        self.startIndex = 0
        self.arrayname = {}
        self.sizeH  =  self.height /  self.arrayAmount
   
    --redraw speed
    self.delay_time = 20*2
    self.redawAble = false

    self:set_size(self.width, self.height)
    self:restore_state(atoms)
    return true
end
-- =====================================
function Wave:restore_state(atoms)

if atoms and #atoms >= 27 then
    self.width       =  atoms[1]
    self.height      =  atoms[2]
    self.recHeadC    = { r = atoms[3],  g = atoms[4],  b = atoms[5],  a = atoms[6] }
    self.waveformC   = { r = atoms[7],  g = atoms[8],  b = atoms[9],  a = atoms[10] }
    self.grainC      = { r = atoms[11],  g = atoms[12], b = atoms[13], a = atoms[14] }
    self.mouseC      = { r = atoms[15], g = atoms[16], b = atoms[17], a = atoms[18] }
    self.backgroundC = { r = atoms[19], g = atoms[20], b = atoms[21], a = atoms[22] }
    self.borderC     = { atoms[23], atoms[24], atoms[25], atoms[26] }
    self.grainsize   = atoms[27]
    
    self:repaint()  
else
    pd.post("No valid state to restore; received " .. (atoms and #atoms or 0) .. " values.")
end
end
-- =====================================
function Wave:save_state()
    function Wave:save_state()
        -- Ensure that self.borderC exists and has 4 elements.
        if self.borderC and #self.borderC >= 4 then
         local state = { self.width, self.height, 
                self.recHeadC.r, self.recHeadC.g, self.recHeadC.b, self.recHeadC.a,
                self.waveformC.r, self.waveformC.g, self.waveformC.b, self.waveformC.a,
                self.grainC.r,    self.grainC.g,    self.grainC.b,    self.grainC.a,
                self.mouseC.r,    self.mouseC.g,    self.mouseC.b,    self.mouseC.a,
                self.backgroundC.r, self.backgroundC.g, self.backgroundC.b, self.backgroundC.a,
                self.borderC[1],  self.borderC[2],  self.borderC[3],  self.borderC[4],
                self.grainsize
            }
            self:set_args(state)
            pd.post("State saved.")
        else
            pd.post("No valid borderC state to save.")
        end
    end
    
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
   -- self:repaint(7) 
    self.clock:delay(self.delay_time)

end

-- =====================================
function Wave:mouse_down(x, y)
    if self.mouseOn == false  and x >= 0 and y >= 0  and x <= self.width and y <= self.height then
        self.mousePos[1] = normalize(x,0,self.width)
        self.mousePos[2] = normalize(y,self.height,0)
        self.readPos = self.mousePos[1]
        self:outlet(1,"list" , self.mousePos)
        self:outlet(1,"list" , self.mousePos)
        self.mouseOn = true
    end
end
-- =====================================
function Wave:mouse_drag(x, y)
    if self.mouseOn  and x >= 0 and y >= 0  and x <= self.width and y <= self.height then
        self.mousePos[1] = normalize(x,0,self.width)
        self.mousePos[2] = normalize(y,self.height,0)
        self.readPos = self.mousePos[1]
        self:outlet(1,"list" , self.mousePos)
    end
end
-- =====================================
function Wave:mouse_up(x, y)
    if self.mouseOn  and x >= 0 and y >= 0  and x <= self.width and y <= self.height then
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
    
    if type(funct) == "function" then  
        local args = {}
        for i = 2, size do
            table.insert(args, atoms[i])
            
        end
        funct(self,args)
    else
        local nameN = sel
        local nameF =  Wave[nameN]
        if type(nameF) == "function" then 
            local argsu = atoms
            nameF(self,argsu)
        end
    end
end

-- =====================================
function Wave:buffername(atoms)

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
self.arrayname = atoms
self.sizeH =  self.height /  self.arrayAmount
end

-- =====================================
function Wave:readPosition(atoms)

end
-- =====================================
function Wave:recordHead(atoms)

    self.readPos = atoms[1]  * self.width
    self.drawRec = true
end
-- =====================================
function Wave:recordHeadSamps(atoms)

end
-- =====================================
function Wave:backgroundColor(atoms)
    self.backgroundC = { r = atoms[1], g = atoms[2], b = atoms[3], a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:borderColor(atoms)
    
    self.borderC     = { atoms[1], atoms[2], atoms[3], atoms[4] }
    self:repaint(7)
    self:save_state()
end
-- =====================================
function Wave:waveformColor(atoms)
    self.waveformC   = { r = atoms[1],  g = atoms[2],  b = atoms[3],  a = atoms[4] }
    self:save_state()

end
-- =====================================
function Wave:grainColor(atoms)
    self.grainC      = { r = atoms[1],  g = atoms[2], b = atoms[3], a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:pointerColor(atoms)
    self.mouseC      = { r = atoms[1], g = atoms[2], b = atoms[3], a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:recordingColor(atoms)
    self.recHeadC    = { r = atoms[1],  g = atoms[2],  b = atoms[3],  a = atoms[4] }
    self:save_state()
end
-- =====================================
function Wave:setsize(atoms)

    self.width = atoms[1]   
    self.height = atoms[2]
    self:set_size(self.width, self.height)
    self:save_state()
    self.sizeH  =  self.height /  self.arrayAmount
end
-- =====================================
function Wave:grainsize(atoms)

    self.grainsize   = atoms[1]
    self:save_state()

end
-- ===========Paint functions ==========
-- =====================================
function Wave:paint(g)
    local bckc = self.backgroundC
    g:set_color(bckc.r,bckc.g,bckc.b,bckc.a)
    g:fill_rect(0, 0, self.width, self.height)

end
-- ///////////////////////////////////
-- ======================================
function Wave:paint_layer_2(g)

    local colorW = self.waveformC
    if self.arrayAmount > 0  then
    local amount = self.arrayAmount
    local sizeH =  self.height / amount
    local wavehight = sizeH /2
    
    for k = 1, amount do
        local array = pd.table(self.arrayname[k])
        if array then
            local size = array:length()
            local middleH = sizeH * (k-1) + (sizeH/2)
            local chunck = math.floor(size / self.width)
            local startx = 0
            local starty = (array:get(0) * wavehight) + middleH

            g:set_color(colorW.r,colorW.g,colorW.b,colorW.a)
            local W = Path(startx,starty )
            for i = 1, self.width - 1 do
                local val = array:get(chunck * i)
                local x = i
                local y =  (val * wavehight) +  middleH
                W:line_to(x, y)
            end
            g:stroke_path(W, 1)
        end
    end
else 
        -- Draw an invisible rectangle so the layer is not empty
        g:set_color(0,0,0,0)
        g:fill_rect(0, 0, 1, 1)

end

end
function Wave:paint_layer_3(g)
if    self.canDraw   then
    local colorW = self.waveformC
    if self.arrayAmount > 0  then
        local amount = self.arrayAmount
        local sizeH =  self.height / amount
        local wavehight = sizeH /2
    
        for k = 1, amount do
            local array = pd.table(self.arrayname[k])
            if array then
                local size = array:length()
                local middleH = sizeH * (k-1) + (sizeH/2)
                local chunck = math.floor(size / self.width)


                g:set_color(colorW[1],colorW[2],colorW[3],1) 
                for i = 1, self.width - 1 do
                    local val = array:get(chunck * i)
                    local x = i
                    local y =  (val * wavehight) +  middleH
                    g:draw_line(x,y ,x, middleH ,1)
                end
            
            end
        end
    else 
        g:set_color(colorW.r,colorW.g,colorW.b,1)
        local W = Path(0,self.height /2 )
        W:line_to(self.width, self.height /2 )
        g:stroke_path(W, 1)
    end
else
        -- Draw an invisible rectangle so the layer is not empty
        g:set_color(0,0,0,0)
        g:fill_rect(0, 0, 1, 1)

end
end
-- ======================================================

function Wave:paint_layer_4(g)
    
    if self.drawRec  then
    local rc = self.recHeadC
    g:set_color(rc.r,rc.b,rc.g,rc.a)
    g:draw_line(self.readPos,0,self.readPos, self.height,2)
    self.drawRec = false
    else   
        -- Draw an invisible rectangle so the layer is not empty
        g:set_color(0,0,0,0)
        g:fill_rect(0, 0, 1, 1)
        self.drawRec = false
    end

end
-- ======================================================
function Wave:paint_layer_5(g)

    if self.mouseOn and self.drawRec == false then
    local mc = self.mouseC
    local x = self.mousePos[1] * self.width
    local y = (1 - self.mousePos[2]) * self.height
    g:set_color(mc.r,mc.b , mc.g, mc.a)
    g:draw_line(x, 0, x, self.height, 2)
    else
                -- Draw an invisible rectangle so the layer is not empty
                g:set_color(0,0,0,0)
                g:fill_rect(0, 0, 1, 1)
    end
end
-- ======================================================
function Wave:paint_layer_6(g)
  local gc = self.grainC
    local amount = # (self.grainState)
    if amount > 0  then
        for i = 1, amount  do
    
            if self.grainState[i] == 1 and self.grainPosition[i]  and self.grainAmp[i] and self.grainChannel[i] and self.grainProgress[i] then

                local radius = ( self.sizeH * .1) *  (1 - 2 * (self.grainProgress[i] - 0.5)^2)
        
                local x = self.grainPosition[i] * self.width
                local y = (((self.grainChannel[i] - 1) * self.sizeH) + ((1 - self.grainAmp[i]) * self.sizeH) + ( self.sizeH * .1) + 4) * .8

                local brighF = 1 - (self.grainProgress[i] * 1)
                
                g:set_color( gc.r *brighF, gc.g *brighF, gc.b, gc.a)
              
                g:fill_ellipse(x- (radius/2),y- (radius/2), radius,radius)
                
              --  g:set_color( gc.r *brighF, gc.g *brighF, gc.b, gc.a)
    
              --  g:stroke_ellipse(x- (radius/2),y- (radius/2), radius,radius,1)
            end
        end
    else 
    
        -- Draw an invisible rectangle so the layer is not empty
        g:set_color(0,0,0,0)
        g:fill_rect(0, 0, 1, 1)
    end

end   

  
function Wave:paint_layer_7(g)
   
    local bc = self.borderC
    g:set_color(bc[1], bc[2], bc[3], bc[4])
    g:stroke_rounded_rect(1, 1, self.width , self.height , 9, 2)

end   
--==============================================
function normalize(val,min,max)
    local normalized = (val- min)/(max- min)
    return normalized
end
