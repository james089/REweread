-- Deployed download helpers use LuaJIT and the firmware's existing Expat.
-- No external DTD/entity loader is installed and no diagnostics expose content.
local ffi=require('ffi')
ffi.cdef[[
typedef void* wr_epub_xml_parser;
wr_epub_xml_parser XML_ParserCreateNS(const char*,char);
int XML_Parse(wr_epub_xml_parser,const char*,int,int);
void XML_ParserFree(wr_epub_xml_parser);
]]
local xml=ffi.load('libexpat.so.1')
local M={}
function M.validate(text)
 assert(type(text)=='string' and #text<=8*1024*1024,'Invalid EPUB XHTML input')
 -- Download caches may contain a body fragment; validate it in the same
 -- namespace context used by the final EPUB writer.
 -- A chapter cache may concatenate several fetched HTML pages. The EPUB
 -- writer joins their bodies; discarded page headers are not book content.
 local bodies={}
 for body in text:gmatch('<body[^>]*>(.-)</body%s*>')do bodies[#bodies+1]=body end
 text=#bodies>0 and table.concat(bodies,'\n') or text:gsub('<%?xml.-%?>','')
 text='<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><body>'..text..'</body></html>'
 local parser=assert(xml.XML_ParserCreateNS(nil,31),'EPUB XML parser unavailable')
 local ok=xml.XML_Parse(parser,text,#text,1)==1
 xml.XML_ParserFree(parser)
 assert(ok,'EPUB XHTML is not well-formed XML')
 return true
end
return M
