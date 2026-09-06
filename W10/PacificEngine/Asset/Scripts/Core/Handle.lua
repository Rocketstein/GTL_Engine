local Handle = {}

function Handle.IsValid(H)
    return H ~= nil and H:IsValid()
end

return Handle
