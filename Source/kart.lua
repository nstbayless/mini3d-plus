scene = lib3d.scene.new()
scene:setCameraOrigin(0, 5, 6)

local rad_to_deg = 180 / math.pi

n = scene:getRootNode()
n2 = n:addChildNode()
terrain = lib3d.shape.new()

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
    for _, face in ipairs(j["faces"]) do
        terrain:addFace(
            table.unpack(face_vertices(face))
            --face_vertex(face, 1),
            --face_vertex(face, 3),
            --face_vertex(face, 4)
        )
    end
end

n2:addShape(terrain)
    
local KSIZE = 2
kartshape = lib3d.shape.new()

kartshape:addFace(lib3d.point.new(-KSIZE/2,0,0),
    lib3d.point.new(KSIZE/2,0,0),
    lib3d.point.new(KSIZE/2,0,KSIZE),
    lib3d.point.new(-KSIZE/2,0,KSIZE), -0.1)
    
kartNode = n:addChildNode()
kartNode:addShape(kartshape)

local gfx = playdate.graphics

kart = {
    -- position and size
    pos = lib3d.point.new(),
    r = 1,
    
    -- facing
    f = lib3d.point.new(),
    
    -- velocity
    v = lib3d.point.new(),
    
    gravity = -0.05,
    
    input = function(self)
        local theta = math.atan2(self.f.y, self.f.x)
        if playdate.buttonIsPressed(playdate.kButtonLeft) then
            theta -= 0.05
        end
        if playdate.buttonIsPressed(playdate.kButtonRight) then
            theta += 0.05
        end
        self.f.x = math.cos(theta)
        self.f.y = math.sin(theta)
    end,
    apply_collision = function(self, normal)
       self.v -= normal * (normal:dot(self.v) + 0.01)
    end,
    update = function(self)
        local p = 0.97
        self.v.x = self.v.x * p + self.f.x * (1 - p)
        self.v.y = self.v.y * p + self.f.y * (1 - p)
        self.v.z += self.gravity
        
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
                    self.v.z = -0.2
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
        
        self.pos = self.pos + self.v
    end,
    getTransform = function(self)
        local theta = math.atan2(self.f.y, self.f.x)
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
        local attack = 0.4
        scene:setCameraOrigin(
            self.pos.x- self.f.x * radius,
            self.pos.y - self.f.y * radius,
            self.pos.z + radius * attack)
        scene:setCameraTarget(self.pos.x, self.pos.y, self.pos.z)
        scene:setCameraUp(0, 0, -1)
    end
}
rot = lib3d.matrix.newRotation(1,0,1,0)
r = 0
l = 0
t = 0

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
end
