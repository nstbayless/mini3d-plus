
-- makeTriShape(knot, 64, 12, 0.3)
-- z buffer: 22 FPS
-- ordering table: 26 FPS


local sin = math.sin
local cos = math.cos

local function knot(t)
	t *= 2*math.pi
	local x = (sin(t) + 2*sin(2*t))/3
	local y = (cos(t) - 2*cos(2*t))/3
	local z = sin(3*t)/2
	return x,y,z
end

local function torus(t)
	t *= 2*math.pi
	local x = sin(t)
	local y = cos(t)
	return x,y,0
end

local function makeShape(func, steps, spokes, thickness, colorfunc, quad)

	local shape = lib3d.shape.new()
	local d = 0.01

	local function makeRing(step)
		local t = step/steps
		local x,y,z = func(t)

		-- compute vector at point
		local xm,ym,zm = func(t-d)
		local xp,yp,zp = func(t+d)
		local vx,vy,vz = xp-xm, yp-ym, zp-zm

		-- normalize
		local s = math.sqrt(vx*vx+vy*vy+vz*vz)
		vx /= s
		vy /= s
		vz /= s

		local r = math.sqrt(vx*vx+vy*vy)
		local ring = {}

		-- build ring of points around the curve point, perpendicular to v
		for i=0,spokes-1 do
			local p = i
			if not quad then p += (step%2)/2 end
			local st = 2*math.pi*p/spokes
			local rx = x - thickness * (sin(st)*vy + cos(st)*vx*vz) / r
			local ry = y + thickness * (sin(st)*vx - cos(st)*vy*vz) / r
			local rz = z + thickness * cos(st)*r
			ring[i+1] = lib3d.point.new(rx, ry, rz)
		end
		
		return ring
	end

	local function addRing(points1, points2, s)
		local n = #points1
		for i=1,n do
			local j = (i%n)+1
			if quad then
				local c = 0
				if colorfunc ~= nil then c = colorfunc(s,i) end
				shape:addFace(points1[i],points2[i],points2[j],points1[j],c)
			else
				local c1 = 0
				local c2 = 0
				if colorfunc ~= nil then
					c1 = colorfunc(2*s,i)
					c2 = colorfunc(2*s+1,i)
				end
				if s%2 == 0 then
					shape:addFace(points1[i],points2[i],points1[j],c1)
					shape:addFace(points2[i],points2[j],points1[j],c2)
				else
					shape:addFace(points1[i],points2[i],points2[j],c2)
					shape:addFace(points1[i],points2[j],points1[j],c1)
				end
			end
		end
	end

	local ring0 = makeRing(0)
	local ring = ring0

	for i=0,steps-2 do
		local next = makeRing(i+1)
		addRing(ring,next,i)
		ring = next
	end

	addRing(ring,ring0,steps-1)

	shape:setClosed(true)

	return shape
end

local function makeTriShape(f,st,sp,t,c) return makeShape(f,st,sp,t,c,false) end
local function makeQuadShape(f,st,sp,t,c) return makeShape(f,st,sp,t,c,true) end


------

scene = lib3d.scene.new()
scene:setCameraOrigin(0, 0, -4)
scene:setLight(0.2, 0.8, 0.4)

local n1 = scene:getRootNode():addChildNode()
local s1 = makeTriShape(knot, 48, 12, 0.45) --, function(i,j) return 0.2-(i%2) end)
n1:addShape(s1)
n1:setWireframeMode(1)
n1:setWireframeColor(1)
n1:setColorBias(-1)

local n2 = scene:getRootNode():addChildNode()
local s2 = makeQuadShape(torus, 16, 16, 0.5, function(i,j) return (i+j)%2 - 0.5 end)
n2:addShape(s2)

local gfx = playdate.graphics

rot1 = lib3d.matrix.newRotation(2,0,1,1)
rot2 = lib3d.matrix.newRotation(1,1,1,0)

playdate.display.setRefreshRate(0)

function playdate.update()
	gfx.clear(gfx.kColorBlack)
	playdate.drawFPS(0,0)

	n1:addTransform(rot1)
	n2:addTransform(rot2)

	scene:draw()
end
