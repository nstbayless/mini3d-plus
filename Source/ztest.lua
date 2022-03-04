
-- test z buffer by drawing a square rotating through another square

scene = lib3d.scene.new()
scene:setCameraOrigin(0, 0, 4)
scene:setLight(0.2, 0.8, 0.4)

n = scene:getRootNode()

square1 = lib3d.shape.new()

M = 10
square1:addFace(lib3d.point.new(-M,-M,0),
	lib3d.point.new(M,-M,0),
	lib3d.point.new(M,M,0),
	lib3d.point.new(-M,M,0), 0)

n1 = n:addChildNode()
n1:addShape(square1)

--[[
square2 = lib3d.shape.new()

square2:addFace(lib3d.point.new(-1,-1,0),
	lib3d.point.new(1,-1,0),
	lib3d.point.new(1,1,0),
	lib3d.point.new(-1,1,0), -0.5)

n2 = n:addChildNode()
n2:addShape(square2)
]]

local gfx = playdate.graphics

rot = lib3d.matrix.newRotation(1,0,1,0)
r = 0
a = 1

-- up/down arrows roll the scene forward and backwards
function playdate.upButtonDown() r -= 5 end
function playdate.downButtonDown() r += 5 end
function playdate.AButtonDown() a *= 0.5 end
function playdate.BButtonDown() a /= 0.5 end

function playdate.update()

	n = scene:getRootNode()
	scene:setCameraOrigin(0, 0, 4 * a)
	n:setTransform(lib3d.matrix.newRotation(r,1,0,0))

	--n2:addTransform(rot)
	
	gfx.clear(gfx.kColorBlack)
	scene:draw()
end


