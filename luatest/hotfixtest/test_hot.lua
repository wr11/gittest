local index -- 这个 index 必须声明，不用赋值，才能够引用到 test 模块中的局部变量 index

return function ()  -- 返回一个闭包函数，这个就是要更新替换后的原型
    index = index + 1
    print(index)
end