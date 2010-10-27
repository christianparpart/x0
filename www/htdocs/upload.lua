#! /usr/bin/lua

-- testing only
-- do not distribute

function parse_headers(h)
	local t = {}
	for k,v in h:gmatch("([^:\r\n]+): *([^\r\n]+)") do
		t[k:lower()] = v
	end
	return t
end

function split(s, sep)
   	sep = sep or '\n'
   	local t = {}
   	local i = 1
   	while true do
		local b,e = string.find(s, sep, i)
		if not b then table.insert(t, string.sub(s, i)) break end
		table.insert(t, string.sub(s, i, b-1))
		i = e + 1
   	end
   	return t
end

function trim(s)
  	return (s:gsub("^%s*(.-)%s*$", "%1"))
end

function get_disposition_names(disp)
	local t = {}
	if disp then 
		string.gsub(disp, ';%s*([^%s=]+)="(.-)"', function(k,v) t[k] = v end)
  	else
		error("Error processing multipart/form-data." ..  "\nMissing content-disposition header")
  	end
  	return t.name, t.filename
end

function cgi_request()
	local m = os.getenv'REQUEST_METHOD'
	if  m == 'POST' then
		local s 
		local len = tonumber(os.getenv'CONTENT_LENGTH')
		if len then 
			s = io.read(len) 
		else 
			s = ''
		end
		return m, s
	else
		return m, os.getenv'QUERY_STRING'
	end
end

function decode_form(s, t)
	local ct = os.getenv'CONTENT_TYPE'
  	local _,_,boundary = ct:find('boundary%="?(.-)"?$')
	if boundary then 
		local t   = t or {}
		local tp  = {}
		local b   = "--" .. boundary
		b = b:gsub("%-", "%%-")
		if s:find(b) then
			local tb = split(s, b)
			for _,v in ipairs(tb) do 
				local tmp = trim(v)
				if tmp ~= "" and tmp ~= "--" then
					local i    = string.find(v, "\r\n\r\n")
					local hraw = string.sub(v, 1, i-1)
					local val  = string.sub(v,i+4)
					local hdr  = parse_headers(hraw)
					local name, filename = get_disposition_names(hdr["content-disposition"])
					if filename then 
						t[name]  = filename 
						tp[name] = val
					else 
						t[name] = trim(val) 
					end
				end
			end
		end
		return t, tp
	else
		-- normal form 
		-- not supported
	end
end

function newbuf()
	local buf = {
		_buf    = {},
		clear   = function (self) self._buf = {}; return self end,
		content = function (self) return table.concat(self._buf) end,
		append  = function (self, s)
					self._buf[#(self._buf) + 1] = s
					return self
				end,
		set     = function (self, s) self._buf = {s}; return self end,
	}
  	return buf
end

function page(s, t, mimetype)
	local t   = t or {}
	local buf = newbuf()
	--
	mimetype = mimetype or 'text/html; charset="UTF-8"'
	buf:append('Content-Type: ' .. mimetype .. '\r\n')
	--
	t[#t+1]  = 'Content-Length: ' .. tostring(s:len())
	for _,h in ipairs(t) do 
		buf:append(h .. '\r\n') 
	end
	buf:append'\r\n'
	io.write(buf:content(), s)
	io.flush()
end

---
-- program

local FORM = [[
<html>
<title>upload test</title>
<body>
<form method="post" action="/upload.lua" enctype="multipart/form-data">
<input type="file" name="myfile" />
<input type="submit" value="Upload" />
</form>
</body>
</html>
]]


local method, params = cgi_request()
if method == 'POST' then
	local t, tp    = decode_form(params)
	local filename = t["myfile"]
	local raw      = tp["myfile"]
	local name     = filename:gsub(".+/","") 
	local file     = io.open('./' .. name, 'w+')
	file:write(raw) 
	file:close()

	page("<html><body>uploaded file: " .. filename .. "</body></html>")
else
	-- default 
	page( FORM )

end

