require 'minitest/autorun'
require 'stringio'
require 'tmpdir'
require 'fileutils'

begin
  $LOAD_PATH.unshift(File.expand_path('../ext/rdmtx', __dir__))
  require 'Rdmtx'
rescue LoadError => e
  warn "Rdmtx extension not built: #{e.message}"
end

module PngHelpers
  PNG_SIGNATURE = "\x89PNG\r\n\x1a\n".b

  def png_dimensions(data)
    io = StringIO.new(data)
    signature = io.read(8)
    raise 'Invalid PNG signature' unless signature == PNG_SIGNATURE

    length = io.read(4).unpack1('N')
    chunk_type = io.read(4)
    raise 'Missing IHDR chunk' unless chunk_type == 'IHDR'
    raise 'Invalid IHDR length' unless length == 13

    ihdr = io.read(length)
    width = ihdr[0, 4].unpack1('N')
    height = ihdr[4, 4].unpack1('N')

    [width, height]
  end
end
