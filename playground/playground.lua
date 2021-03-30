test = {
	double_mem = 99.99,

	SingleReturn = function( val )
		return val
	end,

	MultiReturn = function( self, val )
		return self.double_mem, self.double_mem, val
	end,
}

print("全局注册测试--------------------")
print("add_ref(1, 1) => 3	2")
print( add_ref(1, 1) )
print()

print("print1(123) => 123")
print1(123)
print()

print("print2(123, 456) => 123 456")
print2(123, 456)
print()

print("print(fetch_window(pWnd)) => true, &g_wnd")
local tmp_wnd = Window()
print(tmp_wnd)
local suc, tmp_wnd = fetch_window(tmp_wnd)
print(suc, tmp_wnd)
print()

print("print(IntValue); IntValue = 321456; print(IntValue) => 999	21456" )
print(IntValue); IntValue = 321456; print(IntValue)
print()

print("print(ReadOnlyIntValue) => 321456")
print(ReadOnlyIntValue)
print()

print("ReadOnlyIntValue = 0 => property `ReadOnlyIntValue` is read-only")
local err_call = function() ReadOnlyIntValue = 0 end
print(pcall(err_call))
print()

print("print(kEnum0, kEnum1) => 0	1")
print(kEnum0, kEnum1)
print()

print("类注册测试--------------------")
print("local sw = Window(); print(sw) => &sw")
local sw = Window(); print(sw)
print()

print("sw:SetSize(123456); print(sw:GetSize();) => 321456.0")
sw:SetSize(123456); print(sw:GetSize())
print()

print("sw.Size = 654321; print(sw.Size) => 654321.0")
sw.Size = 654321; print(sw.Size)
print()

print("sw:SetTitle('script wnd'); print(sw:GetTitle()) => script wnd")
sw:SetTitle('script wnd'); print(sw:GetTitle())
print()

print("sw:SetTitle2('script wnd2'); print(sw:GetTitle()) => script wnd2")
sw:SetTitle2('script wnd2'); print(sw:GetTitle())
print()

print("sw.Title = 'title3'; print(sw.Title) => title3")
sw.Title = 'title3'; print(sw.Title)
print()

print("sw:ClearWndTitle(); print(sw.Title) => empty")
sw:ClearWndTitle(); print(sw.Title)
print()

print("脚本变量自动gc")
local pure_script_wnd = Window.TraceCreate()
pure_script_wnd = nil
collectgarbage()
print()

print("std::vector<Window*>测试")
local svs = _G["std::vector<Window*>"].new()
svs:resize(3)
for i = 0, 2 do
	svs:set(i, Window.TraceCreate())
	svs:get(i).Title = string.format("title_%s", i)
end
for k, v in svs:ipairs() do
	print( string.format( "svs[%d].Title=%s", k, v.Title ) )
end
svs:clear()
svs = nil
collectgarbage()
print()

print("类继承测试--------------------")
