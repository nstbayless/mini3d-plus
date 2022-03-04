scene = lib3d.scene.new()
scene:setCameraOrigin(0, 5, 6)

local rad_to_deg = 180 / math.pi

n = scene:getRootNode()
n2 = n:addChildNode()
terrain = lib3d.shape.new()

local TSIZE = 8

terrain:addFace(
    lib3d.point.new(-TSIZE,-TSIZE,0),
	lib3d.point.new(TSIZE,-TSIZE,0),
	lib3d.point.new(TSIZE,TSIZE,0),
	lib3d.point.new(-TSIZE,TSIZE,0), 0.5)

n2:addShape(terrain)
    
local KSIZE = 2
kartshape = lib3d.shape.new()

kartshape:addFace(lib3d.point.new(-KSIZE/2,0,0),
    lib3d.point.new(KSIZE/2,0,0),
    lib3d.point.new(KSIZE/2,0,KSIZE),
    lib3d.point.new(-KSIZE/2,0,KSIZE), -0.5)
    
kartNode = n:addChildNode()
kartNode:addShape(kartshape)

local gfx = playdate.graphics

kart = {
    x = 0,
    y = 0,
    z = 0,
    
    -- facing
    fdx = 0,
    fdy = 1,
    
    -- velocity
    vx = 0,
    vy = 0.1,
    vz = 0,
    
    update = function(self)
        self.x += self.vx
        self.y += self.vy
        self.z += self.vz
    end,
    getTransform = function(self)
        local theta = math.atan2(self.fdy, self.fdx)
        local mat = lib3d.matrix.newRotation(theta * rad_to_deg - 90,0,0,1)
        lib3d.matrix.addTranslation(
            mat,
            self.x,
            self.y,
            self.z
        )
        return mat
    end,
    setShoulderCamera = function(self, scene)
        local radius = 10
        local attack = 0.6
        scene:setCameraOrigin(self.x- self.fdx * radius, self.y - self.fdy * radius, self.z + radius * attack)
        scene:setCameraTarget(self.x, self.y, self.z)
        scene:setCameraUp(0, 0, -1)
    end
}
rot = lib3d.matrix.newRotation(1,0,1,0)
r = 0
l = 0
t = 0
function playdate.upButtonDown() r -= 5 end
function playdate.downButtonDown() r += 5 end
function playdate.update()
    t += 1
    
    kart.fdx = math.sin(t / 20)
    kart.fdy = math.cos(t / 20)
    kart:update()
    kartNode:setTransform(
        kart:getTransform()
    )
    
    kart:setShoulderCamera(scene)
    scene:setLight(0.1, 0.2, 0.2)
	
	gfx.clear(gfx.kColorBlack)
	scene:draw()
end
