local hotfix = require "hotfixtest/hotfix"
local test =  require "hotfixtest/test"
local test_hot = require "hotfixtest/test_hot"

print("before hotfix")
for i = 1, 5 do
    test.print_index() -- 热更前，调用 print_index，打印 index 的值
end


hotfix.update(test.print_index, test_hot) -- 收集旧函数的上值，用于新函数的引用，这个对应之前说的归纳第2小点
test.print_index = test_hot -- 新函数替换旧的函数，对应之前说的归纳第1小点


print("after hotfix")
for i = 1, 5 do
    test.print_index() -- 打印更新后的 index 值
end

test.show() -- show 函数没有被热更，但它获取到的 index 值应该是 最新的，即 index = 5。