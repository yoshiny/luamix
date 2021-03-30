print( test.str )

local sw = Window.new();

print(sw)
sw.Text = "created in script"
print(sw:GetText() )

sw = nil

collectgarbage()

print( "after collectgarbage()" )


print(sw)

print( Window.add( 1, 2) )

print("--------------")




print( test.pWnd )

test.pWnd.Text = "a btn"

print( test.pWnd:GetText() )

print( test.pBtn.Text )

print("--------------")

print( test.vi:size() )
-- test.vi:reserve(10)
print( test.vi:capacity() )

-- test.vi:resize_with(10, 999)

-- test.vi:insert(9, 1000)

test.vi:set(0, 88888)



print( test.vi:size() )
print( test.vi:capacity() )

--for k, v in test.vi:ipairs() do
	--print( string.format("%s = %s", k, v) )
--end

print("--------------")

test.vw:resize(10)

for i = 0, 9 do
	test.vw:set(i, Window.new())
end

for k, v in test.vw:ipairs() do
	v.Text = string.format( "title<%s>", k )
end

for k, v in test.vw:ipairs() do
	print( string.format( "window_%s:%s", k, v.Text ) )
end

print("--------------")

local svi = _G["std::vector<int>"].new()

svi:resize_with(10, 999)

for k, v in svi:ipairs() do
	print( string.format("%s = %s", k, v) )
end

print("--------------")

for k, v in pairs( LuaMix ) do
	print( k, v )
end

print("--------------")