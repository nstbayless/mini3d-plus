local function asserteq(
    a, b
)
    print (a, b)
    assert(a == b)
end

asserteq(lib3d.math.collide_sphere_triangle(
    lib3d.point.new(0, 0, 0),
    0.6,
    lib3d.point.new(1, 0, 0),
    lib3d.point.new(0, 1, 0),
    lib3d.point.new(0, 0, 1)
), true)--]]

asserteq(lib3d.math.collide_sphere_triangle(
    lib3d.point.new(0, 0, 0),
    0.5,
    lib3d.point.new(1, 0, 0),
    lib3d.point.new(0, 1, 0),
    lib3d.point.new(0, 0, 1)
), false)--]]

asserteq(lib3d.math.collide_sphere_triangle(
    lib3d.point.new(1, 1, 0),
    0.7,
    lib3d.point.new(1, 0, 0),
    lib3d.point.new(0, 1, 0),
    lib3d.point.new(0, 0, 0)
), false)--]]

asserteq(lib3d.math.collide_sphere_triangle(
    lib3d.point.new(1, 1, 0),
    0.75,
    lib3d.point.new(1, 0, 0),
    lib3d.point.new(0, 1, 0),
    lib3d.point.new(0, 0, 0)
), true)--]]

asserteq(lib3d.math.collide_sphere_triangle(
    lib3d.point.new(1, 1, 0.5),
    0.7,
    lib3d.point.new(1, 0, 0),
    lib3d.point.new(0, 1, 0),
    lib3d.point.new(0, 0, 0)
), false)--]]

asserteq(lib3d.math.collide_sphere_triangle(
    lib3d.point.new(1, 1, 0.5),
    1.5,
    lib3d.point.new(1, 0, 0),
    lib3d.point.new(0, 1, 0),
    lib3d.point.new(0, 0, 0)
), true)--]]

asserteq(lib3d.math.collide_sphere_triangle(
    lib3d.point.new(2, 0, 0),
    0.99,
    lib3d.point.new(1, 0, 0),
    lib3d.point.new(0, 1, 0),
    lib3d.point.new(0, 0, 0)
), false)--]]

asserteq(lib3d.math.collide_sphere_triangle(
    lib3d.point.new(2, 0, 0),
    1.1,
    lib3d.point.new(1, 0, 0),
    lib3d.point.new(0, 1, 0),
    lib3d.point.new(0, 0, 0)
), true)--]]