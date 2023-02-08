local cos <const> = math.cos
local sin <const> = math.sin
local exp <const> = math.exp
local min <const> = math.min
local max <const> = math.max
local atan2 <const> = math.atan2
local abs <const> = math.abs
local dt = 0
local VMULT <const> = 20.0

local function clamp(x, a, b)
    if x < a then
        return a
    elseif x > b then
        return b
    end
    return x
end

scene = lib3d.scene.new()
scene:setCameraOrigin(0, 5, 6)
scene:setLight(0, 0, 1)

if lib3d.renderer.setRenderDistance then
    lib3d.renderer.setRenderDistance(100.0)
end

local rad_to_deg = 180 / math.pi

n = scene:getRootNode()
n2 = n:addChildNode()
terrain = lib3d.shape.new()
if terrain.setScanlining then
    terrain:setScanlining("even", 0xAAAAAAAA)
end
terrain:setClosed(1);

banner = lib3d.shape.new()
if lib3d.pattern then
    banner:setPattern(
        lib3d.pattern.new(
            -- we are using 7 patterns,
            -- but supports up to 33 patterns.
            
            0xD0, 0xF0, 0x70, 0xF0, 0x0D, 0x0F, 0x07, 0x0F, -- darker
            0xF0, 0xD0, 0xF0, 0xF0, 0x0F, 0x0F, 0x07, 0x0F, -- slightly darker
            0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, -- normal
            0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, -- normal
            0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, -- normal
            0xF0, 0xF2, 0xF0, 0xF0, 0x0F, 0x0F, 0x4F, 0x0F, -- slightly brighter
            0xF2, 0xF0, 0xF8, 0xF0, 0x2F, 0x0F, 0x8F, 0x0F -- brighter
        )
    )
end

if lib3d.texture then
    --banner:setTexture("assets/flag.png.u", true)
end
    
local function swap(a, i, j)
    local tmp = a[i]
    a[i] = a[j]
    a[j] = tmp
end

local function face_vertex(face, i)
    return lib3d.point.new(
        face[i][1],
        face[i][2],
        face[i][3]
    )
end

local function face_vertices(face, ...)
    verts = {}
    for i=1,#face do
        verts[#verts+1]  = face_vertex(face, i)
    end
    vargs = {...}
    for i = 1,#vargs do
        verts[#verts+1] = vargs[i]
    end
    return verts
end

-- track
path = {}
j = json.decodeFile("assets/track.json")
if j then
    for _, face in ipairs(j["course"]) do
        f_idx = terrain:addFace(
            table.unpack(face_vertices(face, 0.2))
        )
        if lib3d.texture then
            if (f_idx ~= 0) then
            terrain:setFaceTextureMap(
                f_idx,
                0, 0,
                1, 0,
                1, 1,
                0, 1
            ) end
        end
    end
    for _, face in ipairs(j["banner"]) do
        f_idx = banner:addFace(
            table.unpack(face_vertices(face))
        )
        banner:setFaceTextureMap(
            f_idx,
            0, 0,
            1, 0,
            1, 1,
            0, 1
        )
    end
    for i, e in ipairs(j["path"]) do
        if e.x1 then
            path[i] = {
                p1=lib3d.point.new(e.x1, e.y1, e.z1),
                p2=lib3d.point.new(e.x2, e.y2, e.e2),
                next=e.next
            }
        end
    end
end
j = nil -- clean up

n2:addShape(terrain)
n2:addShape(banner)

local KSIZE = 4
local sink = 0.4

-- load kart textures
if lib3d.texture then
    KART_ANGLES = 96
    kart_texture = {}
    for i = 0,KART_ANGLES-1 do
        local f = "assets/kart/img-"..tostring(i)..".png.u"
        kart_texture[i] = lib3d.texture.new(f, true)
    end
end

local function randelt(a)
    return a[math.random(1, #a)]
end

if lib3d.texture then
    terrain:setTexture("assets/texture.png.u", true)
end

local gfx = playdate.graphics

local function distanceToPathNode(p, idx)
    local node = path[idx] or path[1]
    local d = (node.p2 - node.p1)
    local dp = (p - node.p1)
    d:normalize()
    return -d:cross(dp).z
end

local function getPathNodeCentre(idx, t)
    t = (t or 0.5) + 0.0
    return path[idx].p1 * t + path[idx].p2 * (1 - t)
end

local function getPathNodeNormal(idx)
    local node = path[idx] or path[1]
    local d = (node.p2 - node.p1)
    d = d:cross(lib3d.point.new(0, 0, -1))
    d:normalize()
    return d
end

local function angleDifference(a, b)
    if b - a < -math.pi then
        return b - a + 2*math.pi
    elseif b - a >= math.pi then
        return b - a - 2*math.pi
    end
    return b - a
end

function makeKart(is_player) return {
    -- position and size
    pos = lib3d.point.new(),
    r = KSIZE * 0.35,
    rrest = KSIZE * 0.35,
    
    -- facing
    f = lib3d.point.new(-1, 0, 0),
    
    -- velocity
    v = lib3d.point.new(),
    
    -- camera angle
    cam = lib3d.point.new(-1, -1, 0),
    
    gravity = lib3d.point.new(0, 0, -1.1),
    
    maxgravity = 0.25 * VMULT,
    
    -- not the *literal* maximum speed.
    TOP_SPEED = 4,
    kartNode = nil,
    is_player = is_player,
    is_player_control = false,
    nextPathNode = 1,
    preferred_t = 0.5,
    add = function(self)
        self.kartNode = n:addChildNode()
        
        self.kartshape = lib3d.imposter.new()
        self.kartshape:setPosition(lib3d.point.new(0, 0, 0))
        self.kartshape:setRectangle(-KSIZE /2, -KSIZE * (1-sink), KSIZE /2, KSIZE * sink)
        self.kartshape:setZOffsets(0, 0, -4, -4) -- helps imposter appear above the floor
        if self.kartshape.setTexture then
            self.kartshape:setTexture(lib3d.texture.new("assets/kart/img-0.png.u", true))
        end
        self.kartNode:addImposter(self.kartshape)
    end,
    remove = function(self)
        if self.kartNode then
            -- TODO: remove
        end
    end,
    input = function(self)
        local theta = atan2(self.f.y, self.f.x)
        local theta_mult = exp(-self.v:length() / self.TOP_SPEED)
        if self.is_player then
            if playdate.buttonIsPressed(playdate.kButtonLeft) or playdate.buttonIsPressed(playdate.kButtonRight) then
                self.is_player_control = true
            end
        end
        if self.is_player_control then
            -- player steers manually
            if playdate.buttonIsPressed(playdate.kButtonLeft) then
                theta -= theta_mult * dt
            end
            if playdate.buttonIsPressed(playdate.kButtonRight) then
                theta += theta_mult * dt
            end
        else
            -- automatic steering
            if distanceToPathNode(self.pos, self.nextPathNode) <= 0 then
                self.nextPathNode = randelt(path[self.nextPathNode].next)
            end
            
            -- destination drift
            self.preferred_t += (math.random(1) - math.random(1)) * 0.05 * dt
            self.preferred_t = clamp(self.preferred_t, 0.1, 0.9)
            
            desPos = getPathNodeCentre(self.nextPathNode, self.preferred_t) + getPathNodeNormal(self.nextPathNode) * 2.5
            desF = desPos - self.pos
            desTheta = atan2(desF.y, desF.x)
            thetaError = angleDifference(theta, desTheta)
            theta += clamp(thetaError, -theta_mult * dt, theta_mult * dt)
            
        end
        
        self.f.x = cos(theta)
        self.f.y = sin(theta)
    end,
    apply_collision = function(self, normal, dist, face_idx)
        self.v -= normal * (normal:dot(self.v) + 0.001)
    end,
    update = function(self)
        self:input()
        local p = math.exp(-dt)
        local qspeed = min(self.TOP_SPEED, max(0.5, self.f:dot(self.v)) * 1.15)
        self.v.x = self.v.x * p + self.f.x * (1 - p) * qspeed
        self.v.y = self.v.y * p + self.f.y * (1 - p) * qspeed
        
        if (self.v:dot(self.gravity) < self.maxgravity) then
            self.v += self.gravity * dt
        end
        
        local collision_occurred = false
        for i = -1,5 do
            collision, normal, distance, face_idx = terrain:collidesSphere(
                lib3d.point.new(
                    self.pos.x + self.v.x * dt * VMULT,
                    self.pos.y + self.v.y * dt * VMULT,
                    self.pos.z + self.v.z * dt * VMULT + self.r
                ),
                self.r
            )
            if (collision) then
                collision_occurred = true
                if i == 5 then
                    -- give up
                    print("GIVE UP")
                    self.v.x = -self.f.x / 10
                    self.v.y = -self.f.y / 10
                    self.v.z = 0.2
                elseif i == 3 then
                    -- add a bit of the normal directly to position
                    self.v -= normal * 0.1
                    self.pos -= normal * 0.1
                elseif i == 4 then
                    -- move upward a little
                    self.pos.z -= 0.1
                else
                    self:apply_collision(normal, distance, face_idx)
                end
            else
                break
            end
        end
        if not collision_occurred then
            -- gradually expand collision free in the air to
            -- improve chance of hitting walls
            self.r += dt * 0.2
        else
            self.r = self.rrest
        end
        
        self.cam += (self.f * 2.0 + self.v * 0.2) * dt
        self.cam.z = 0
        if self.cam:length() < 0.1 then
            self.cam = self.f
        else
            self.cam:normalize()
        end
        
        self.pos = self.pos + (self.v * dt) * VMULT
        
        -- bounds
        if self.pos.z < -100 then
            self.pos = lib3d.point.new(0, 0, 160)
            self.cam = lib3d.point.new(-1, 0, 0)
            self.f = lib3d.point.new(-1, 0, 0)
            self.v = lib3d.point.new(0.2, 0, -3.5)
            self.nextPathNode = 1
        end
        
        self.kartNode:setTransform(
            self:getTransform()
        )
    end,
    getTransform = function(self)
        local theta = atan2(self.f.y, self.f.x)
        local mat = lib3d.matrix.newRotation(theta * rad_to_deg - 90,0,0,1)
        lib3d.matrix.addTranslation(
            mat,
            self.pos.x,
            self.pos.y,
            self.pos.z + 0.75
        )
        return mat
    end,
    setShoulderCamera = function(self, scene)
        local radius = 10
        local attack = 0.5
        local fv = self.cam
        scene:setCameraOrigin(
            self.pos.x - fv.x * radius,
            self.pos.y - fv.y * radius,
            self.pos.z + radius * attack)
        scene:setCameraTarget(self.pos.x, self.pos.y, self.pos.z + 4)
        scene:setCameraUp(0, 0, 1)
    end,
    setImage = function(self, scene)
        -- set texture
        local fv = lib3d.point.new(scene:getCameraTarget()) - lib3d.point.new(scene:getCameraOrigin())
        if self.kartshape.setTexture then
            local cam_reltheta = atan2(fv.y, fv.x) - atan2(self.f.y, self.f.x)
            local kart_frame = math.floor(-cam_reltheta * KART_ANGLES / 2 / math.pi + 0.5) % KART_ANGLES
            self.kartshape:setTexture(kart_texture[kart_frame])
        end
    end
}
end

rot = lib3d.matrix.newRotation(1,0,1,0)
r = 0
l = 0
t = 0

playdate.display.setRefreshRate(40)

function playdate.AButtonDown()
    if lib3d.renderer.setInterlaceEnabled then
        lib3d.renderer.setInterlaceEnabled(not lib3d.renderer.getInterlaceEnabled())
    end
end

local kart = makeKart(true)
kart:add()

local npckarts = {}

-- kart logic is very inefficient, so we can't actually afford to run multiple karts
-- on the device at once. (TODO: OPTIMIZE KARTS!)
if playdate.simulator then
    npckarts = {makeKart(), makeKart(), makeKart(), makeKart()}
end

for i, kart in ipairs(npckarts) do
    kart.pos.x -= i * 5
    local offset = sin(i * 1.8)
    kart.pos.y -= 5 * offset
    kart.preferred_t = 0.5 + offset / 2.2
    kart.TOP_SPEED *= (1.0 + 0.3 * cos(i * 2))
    kart:add()
end

function playdate.update()
    dt = max(0.02, min(0.15, playdate.getElapsedTime()))
    playdate.resetElapsedTime()
    kart:update()
    for _, npckart in ipairs(npckarts) do
        npckart:update()
    end
    
    if kart.is_player then
        kart:setShoulderCamera(scene)
    end
    
    kart:setImage(scene)
    for _, npckart in ipairs(npckarts) do
        npckart:setImage(scene)
    end
	
    scene:prefetchZBuff();
	if not lib3d.renderer.getInterlaceEnabled then -- [sic]
        -- if interlace is enabled, the scene clears to black automatically,
        -- so we don't need this.
        gfx.clear(gfx.kColorBlack)
    end
	scene:draw()
    playdate.drawFPS(0,0)
end

playdate.resetElapsedTime()