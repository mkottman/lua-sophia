local sophia = require "sophia"

assert(sophia.env, "The library is missing the 'env' method")

local env = assert(sophia.env())

os.execute("rm -rf db")

assert(env:ctl("dir", "db"))
local db = assert(env:open())

-- basic store and retrieve
assert(db:set("hello", "world"))
local val = assert(db:get("hello"))
assert(val == "world", "The stored and retrieved values differ")

db:set("ahoj", "svet")
-- iterator
for k, v in db:cursor() do
	print(k, "->", v)
end

-- delete the key
assert(db:set("hello"))
local val, err = db:get("hello")
assert(not err, err)
assert(not val, "The deleted value is still there")

-- for loop
local N = 1000
for i=1,N do
	assert(db:set(tostring(i), "Value: "..i))
end
for i=1,N do
	local val = assert(db:get(tostring(i)))
	assert(val == "Value: "..i, "Error at "..i.." : "..val)
end

-- performance
local N = 1000000
local start = os.clock()
for i=1,N do
	local v = tostring(i)
	db:set(v, v)
end
print('Writing '..N..' items: ', N/(os.clock() - start), 'ops')
local start = os.clock()
for i=1,N do
	local v = tostring(i)
	db:get(v)
end
print('Reading '..N..' items: ', N/(os.clock() - start), 'ops')
