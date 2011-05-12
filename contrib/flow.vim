" Vim syntax file
" Language: Flow (Flow Control Language)
" Maintainer: Christian Parpart
" Latest Revision: 15 September 2010

if exists("b:current_syntax")
	finish
endif

" ---------------------------------------------------------------------------------
"
" comments
syn keyword flowTodo contained TODO FIXME XXX NOTE
syn match flowComment "#.*$" contains=flowTodo
syn match flowComment "//.*$" contains=flowTodo
syn region flowComment start="\/\*" end="\*\/" contains=flowTodo

" blocks
syn region flowBlock start="{" end="}" transparent fold

" keywords
syn keyword flowKeywords import from
syn keyword flowKeywords handler on do extern var if then else elseif elsif
syn keyword flowCoreHandler assert assert_fail finish
syn keyword flowCoreFunctions __print tonumber tostring tobool

" symbols
syn keyword flowOperator and or xor nand not shl shr in
syn match flowOperator '(\|)'
syn match flowOperator '{\|}'
syn match flowOperator '\[\|\]'
syn match flowOperator '[,;]'
syn match flowOperator '[^=<>~\$\^\/\*]\zs\(=>\|=\^\|=\$\|=\~\|==\|!=\|<=\|>=\|<\|>\|!\|=\|+\|-\|\*\*\|\*\)\ze[^=<>~\$\^\/\*]'
" FIXME the '/' operator is not mentioned in the operator list, currently, to
" properly highlight the /regex/ operator.

" regular expressions (/things.like.that/)
"syn match flowRegexpEscape "\\\[" contained display
syn match flowRegexpSpecial "\(\[\|?\|\]\|(\|)\|\.\|*\||\)" contained display
syn region flowRegexp matchgroup=Delimiter start="\(=\|=\~\|(\)\s*/"ms=e,me=e end="/" skip="\\/" oneline contains=flowRegexpEscape,flowRegexpSpecial
syn region flowRegexp matchgroup=Delimiter start="/"ms=e,me=e end="/" skip="\\/" oneline contains=flowRegexpEscape,flowRegexpSpecial

" types
syn keyword flowType void int string bool
syn match flowType '\.\.\.'

" numbers
syn match flowNumber '\d\+'
syn match flowNumber '\d\+.\d'
syn keyword flowNumber true false
syn keyword flowUnit bit kbit mbit gbit tbit
syn keyword flowUnit byte kbyte mbyte gbyte tbyte
syn keyword flowUnit sec min hours days weeks months years

" strings
syn match flowSpecial display contained "\\\(u\x\{4}\|U\x\{8}\)"
syn match flowFormat display contained "\\\(r\|n\|t\)"
syn match flowFormat display contained "%\(\d\+\$\)\=[-+' #0*]*\(\d*\|\*\|\*\d\+\$\)\(\.\(\d*\|\*\|\*\d\+\$\)\)\=\([hlL]\|ll\)\=\([bdiuoxXDOUfeEgGcCsSpn]\|\[\^\=.[^]]*\]\)"
syn region flowString start=+L\="+ skip=+\\\\\|\\"+ end=+"+ contains=flowSpecial,flowFormat
syn region flowRawString start="'" end="'"

if exists("flow_x0")
	" extend core with some x0 HTTP web server core features.
	syn keyword flowCoreFunctions listen pathinfo
	syn match flowCoreFunctions '\<\(mimetypes\.default\|mimetypes\)\>'
	syn match flowCoreFunctions '\<plugin\.\(directory\|load\)\>'
	syn match flowCoreFunctions '\<proxy\.\(reverse\)\>'
	syn match flowCoreFunctions '\<\(workers\)\>'
	syn match flowCoreFunctions '\<cgi\.\(mapping\|prefix\|exec\|map\)\>'
	syn match flowCoreFunctions '\<rrd\(\.\(filename\|step\)\)\?\>'
	syn match flowCoreFunctions '\<vhost\.\(mapping\|add\|map\)\>'
	syn match flowCoreFunctions '\<error\.\(handler\)\>'
	syn match flowCoreVar '\<sys\.\(env\|pid\|cwd\|now\|now_str\)\>'
	syn match flowCoreVar '\<req\.\(method\|host\|path\|url\|header\|remoteip\|remoteport\|localip\|localport\)\>'
	syn match flowCoreVar '\<phys\.\(path\|exists\|is_reg\|is_dir\|is_exe\|size\|mtime\|mimetype\)\>'
	syn match flowCoreVar '\<server\.\(advertise\|tags\)\>'
	syn match flowCoreVar '\<etag\.\(mtime\|size\|inode\)\>'
	syn match flowCoreVar '\<compress\.\(min\|max\|level\|types\)\>'
	syn match flowCoreVar '\<ssl\.\(listen\|context\|loglevel\)\>'
	syn match flowCoreVar '\<\(userdir.name\|userdir\)\>'
	syn match flowCoreVar '\<log\.\(level\|file\)\>'
	syn keyword flowCoreVar max_read_idle max_write_idle max_keepalive_idle max_connections max_files max_address_space max_core_size tcp_cork tcp_nodelay
	syn keyword flowCoreVar max_request_uri_size max_request_header_size max_request_header_count max_request_body_size
	syn keyword flowCoreFunctions header

	" core handlers
	syn keyword flowCoreHandler redirect respond

	" upstream plugin handlers
	syn keyword flowCoreHandler fastcgi staticfile dirlisting
	syn keyword flowCoreFunctions docroot alias
	syn match flowCoreHandler '\<access\.\(deny\|allow\)\>'
	syn keyword flowCoreFunctions accesslog autoindex
endif

" ---------------------------------------------------------------------------------

let b:current_syntax = "flow"

hi def link flowTodo          Todo
hi def link flowComment       Comment
hi def link flowString        Constant
hi def link flowRawString     Constant
hi def link flowNumber        Constant
hi def link flowRegexp        Constant
hi def link flowType          Type
hi def link flowBlock         Statement
hi def link flowKeywords      Keyword
hi def link flowOperator      Operator
hi def link flowCoreFunctions PreProc
hi def link flowCoreHandler   PreProc
hi def link flowCoreVar       PreProc
hi def link flowSpecial       Special
hi def link flowUnit          Special
hi def link flowFormat        Special
hi def link flowRegexpEscape  Special
hi def link flowRegexpSpecial Special

" possible link targets Todo, Comment, Constant, Type, Keyword, Statement, Special, PreProc
