#!/usr/bin/env ruby
require 'Rdmtx'

rdmtx = Rdmtx.new
i = rdmtx.encode("Hello you !!", 5, 5, DmtxSymbolSquareAuto)
if !i
  puts "Encode failed"
  exit 1
end
i.write("output.png")
puts "Written output.png"
