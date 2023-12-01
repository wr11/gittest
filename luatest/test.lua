function hasChineseCharacters(str)  
    -- 使用正则表达式匹配中文字符  
    local pattern = [[[\x{4e00}-\x{9fa5}]]]
    -- 在字符串中查找匹配项  
    local match = str:match(pattern)  
    -- 如果找到匹配项，则返回 true，否则返回 false  
    if match then  
        return true  
    else  
        return false  
    end  
end