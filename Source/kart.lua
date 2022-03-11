local cos <const> = math.cos
local sin <const> = math.sin
local exp <const> = math.exp
local min <const> = math.min
local max <const> = math.max
local atan2 <const> = math.atan2
local abs <const> = math.abs


scene = lib3d.scene.new()
scene:setCameraOrigin(0, 5, 6)
scene:setLight(0, 0, 1)

local rad_to_deg = 180 / math.pi

n = scene:getRootNode()
n2 = n:addChildNode()
terrain = lib3d.shape.new()
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
    
local function swap(a, i, j)
    local tmp = a[i]
    a[i] = a[j]
    a[j] = tmp
end

local function face_vertex(face, i)
    return lib3d.point.new(
        face[i][2],
        face[i][1],
        face[i][3]
    )
end

local function face_vertices(face)
    verts = {}
    for i=1,#face do
        verts[#verts+1]  = face_vertex(face, i)
    end
    return verts
end

-- track
j = json.decodeFile("assets/track.json")
if j then
    for _, face in ipairs(j["course"]) do
        f_idx = terrain:addFace(
            table.unpack(face_vertices(face))
        )
        if lib3d.texture then
            terrain:setFaceTextureMap(
                f_idx,
                0, 0,
                0, 1,
                1, 1,
                1, 0
            )
        end
    end
    for _, face in ipairs(j["banner"]) do
        f_idx = banner:addFace(
            table.unpack(face_vertices(face))
        )
    end
end
j = nil -- clean up

n2:addShape(terrain)
n2:addShape(banner)

local KSIZE = 4.5
local sink = 0.4
kartshape = lib3d.imposter.new()

-- load kart textures
if lib3d.texture then
    KART_ANGLES = 40
    local kart_texture = {}
    for i = 0,KART_ANGLES-1 do
        local f = "assets/kart/img-"..tostring(i)..".png.u"
        kart_texture[i] = lib3d.texture.new(f, true)
    end
end

kartshape:setPosition(lib3d.point.new(0, 0, 0))
kartshape:setRectangle(-KSIZE /2, -KSIZE * (1-sink), KSIZE /2, KSIZE * sink)
kartshape:setZOffsets(0, 0, -2, -2) -- helps imposter appear above the floor

if lib3d.texture then
    kartshape:setTexture(lib3d.texture.new("assets/kart/img-0.png.u", true))
    terrain:setTexture("assets/texture.png.u", true)
end

kartNode = n:addChildNode()
kartNode:addImposter(kartshape)

local gfx = playdate.graphics

kart = {
    -- position and size
    pos = lib3d.point.new(),
    r = 2,
    
    -- facing
    f = lib3d.point.new(-1, -1, 0),
    
    -- velocity
    v = lib3d.point.new(),
    
    -- camera angle
    cam = lib3d.point.new(-1, -1, 0),
    
    gravity = lib3d.point.new(0, 0, -0.06),
    
    -- not the *literal* maximum speed.
    TOP_SPEED = 4,
    
    input = function(self)
        local theta = atan2(self.f.y, self.f.x)
        local theta_mult = exp(-self.v:length() * 2 / self.TOP_SPEED)
        if playdate.buttonIsPressed(playdate.kButtonLeft) then
            theta -= 0.05 * theta_mult
        end
        if playdate.buttonIsPressed(playdate.kButtonRight) then
            theta += 0.05 * theta_mult
        end
        self.f.x = cos(theta)
        self.f.y = sin(theta)
    end,
    apply_collision = function(self, normal)
       self.v -= normal * (normal:dot(self.v) + 0.01)
    end,
    update = function(self)
        local p = 0.95
        local qspeed = min(self.TOP_SPEED, max(0.5, self.f:dot(self.v)) * 1.15)
        self.v.x = self.v.x * p + self.f.x * (1 - p) * qspeed
        self.v.y = self.v.y * p + self.f.y * (1 - p) * qspeed
        self.v += self.gravity
        
        for i = 0,5 do
            collision, normal = terrain:collidesSphere(
                lib3d.point.new(
                    self.pos.x + self.v.x,
                    self.pos.y + self.v.y,
                    self.pos.z + self.v.z + self.r
                ),
                self.r
            )
            if (collision) then
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
                    self:apply_collision(normal)
                end
            end
        end
        
        self.cam += self.f * 0.1 + self.v * 0.01
        self.cam.z = 0
        if self.cam:length() < 0.1 then
            self.cam = self.f
        else
            self.cam:normalize()
        end
        
        self.pos = self.pos + self.v
    end,
    getTransform = function(self)
        local theta = atan2(self.f.y, self.f.x)
        local mat = lib3d.matrix.newRotation(theta * rad_to_deg - 90,0,0,1)
        lib3d.matrix.addTranslation(
            mat,
            self.pos.x,
            self.pos.y,
            self.pos.z
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
        scene:setCameraUp(0, 0, -1)
    
        -- set texture
        if lib3d.texture then
            local cam_reltheta = atan2(fv.y, fv.x) - atan2(self.f.y, self.f.x)
            local kart_frame = math.floor(-cam_reltheta * KART_ANGLES / 2 / math.pi + 0.5) % KART_ANGLES
            kartshape:setTexture(kart_texture[kart_frame])
        end
    end,
}
rot = lib3d.matrix.newRotation(1,0,1,0)
r = 0
l = 0
t = 0

playdate.display.setRefreshRate(20)

function playdate.update()
    
    kart:input()
    
    kart:update()
    kartNode:setTransform(
        kart:getTransform()
    )
    
    kart:setShoulderCamera(scene)
	
	gfx.clear(gfx.kColorBlack)
	scene:draw()
    --scene:drawZBuff()
    playdate.drawFPS(0,0)
end
