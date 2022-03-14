
-- first, we compute the vertices of an icosohedron:

local p = (math.sqrt(5) - 1) / 2

local x1 = lib3d.point.new(0, -p, 1)
local x2 = lib3d.point.new(0, p, 1)
local x3 = lib3d.point.new(0, p, -1)
local x4 = lib3d.point.new(0, -p, -1)

local y1 = lib3d.point.new(1, 0, p)
local y2 = lib3d.point.new(1, 0, -p)
local y3 = lib3d.point.new(-1, 0, -p)
local y4 = lib3d.point.new(-1, 0, p)

local z1 = lib3d.point.new(-p, 1, 0)
local z2 = lib3d.point.new(p, 1, 0)
local z3 = lib3d.point.new(p, -1, 0)
local z4 = lib3d.point.new(-p, -1, 0)

-- now we add the faces to our prototype shape, with vertices in the
-- correct order so the faces point outward (using right hand rule)

shape = lib3d.shape.new()

shape:addFace(z1, y3, y4)
shape:addFace(z1, x3, y3)
shape:addFace(z1, z2, x3)
shape:addFace(z1, x2, z2)
shape:addFace(z1, y4, x2)

shape:addFace(y4, y3, z4)
shape:addFace(z4, y3, x4)
shape:addFace(y3, x3, x4)
shape:addFace(x4, x3, y2)
shape:addFace(x3, z2, y2)
shape:addFace(y2, z2, y1)
shape:addFace(z2, x2, y1)
shape:addFace(y1, x2, x1)
shape:addFace(x2, y4, x1)
shape:addFace(x1, y4, z4)

shape:addFace(z3, y2, y1)
shape:addFace(z3, y1, x1)
shape:addFace(z3, x1, z4)
shape:addFace(z3, z4, x4)
shape:addFace(z3, x4, y2)

shape:setClosed(true)

-- the scene object does the drawing

scene = lib3d.scene.new()
scene:setCameraOrigin(0, 0, -4)
scene:setLight(0.2, 0.8, 0.4)


-- add six copies of the shape to the scene

n = scene:getRootNode()

n1 = n:addChildNode()
n1:addShape(shape, 2, 0, 0)

n2 = n:addChildNode()
n2:addShape(shape, -2, 0, 0)
n2:setColorBias(0.8)

n3 = n:addChildNode()
n3:addShape(shape, 0, 2, 0)
n3:setWireframeMode(2)
n3:setWireframeColor(1)
n3:setFilled(false)

n4 = n:addChildNode()
n4:addShape(shape, 0, -2, 0)
n4:setWireframeMode(1)
n4:setWireframeColor(1)
n4:setFilled(false)

n5 = n:addChildNode()
n5:addShape(shape, 0, 0, 2)
n5:setColorBias(0.8)
n5:setWireframeMode(2)
n5:setWireframeColor(0)

n6 = n:addChildNode()
n6:addShape(shape, 0, 0, -2)
n6:setColorBias(-0.8)
n6:setWireframeMode(1)
n6:setWireframeColor(1)


-- and we're ready to go!

local gfx = playdate.graphics
local x = 0
local z = -4
local dx = 0
local dz = 0

--playdate.startAccelerometer()

rot1 = lib3d.matrix.newRotation(5,0,0,1)
rot2 = lib3d.matrix.newRotation(3,0,1,0)

function playdate.update()

	if dx ~= 0 or dz ~= 0 then
		x += dx / 10
		z += dz / 10
		scene:setCameraOrigin(-x, 0, z)
	end

	--local x, y = playdate.readAccelerometer()
	
	--n:setTransform(lib3d.matrix.newRotation(y*100,1,0,0))
	--n:addTransform(lib3d.matrix.newRotation(x*100,0,1,0))

	n:addTransform(rot2)
	n:addTransform(rot1)
	
	gfx.clear(gfx.kColorBlack)
	scene:draw()
end

function playdate.cranked()
	local a = playdate.getCrankPosition() * math.pi / 180
	scene:setCameraUp(math.sin(a), math.cos(a), 0)
end

function playdate.upButtonDown()	dz += 1		end
function playdate.upButtonUp()		dz -= 1		end
function playdate.downButtonDown()	dz -= 1		end
function playdate.downButtonUp()	dz += 1		end
function playdate.leftButtonDown()	dx -= 1		end
function playdate.leftButtonUp()	dx += 1		end
function playdate.rightButtonDown()	dx += 1		end
function playdate.rightButtonUp()	dx -= 1		end
