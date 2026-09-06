local LetterBox = {}

function LetterBox.Start(actor, aspect_w, aspect_h)
   if not actor or not actor.StartLetterBoxing then
        warn("[LetterBox] StartLetterBoxing missing" )
        return false
   end 

   aspect_w = aspect_w or 21
   aspect_h = aspect_h or 9

   return actor:StartLetterBoxing(aspect_w, aspect_h)
end

function LetterBox.End(actor)
    if not actor or not actor.EndLetterBoxing then
        warn("[LetterBox] EndLetterBoxing missing" )
        return false
   end 

   return actor:EndLetterBoxing()
end

return LetterBox