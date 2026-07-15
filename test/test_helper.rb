require 'minitest/autorun'
require 'stringio'
require 'tmpdir'
require 'fileutils'
require 'zlib'

$LOAD_PATH.unshift(File.expand_path('../ext/rdmtx', __dir__))
require 'Rdmtx'

module PngHelpers
  PNG_SIGNATURE = "\x89PNG\r\n\x1a\n".b

  def png_dimensions(data)
    metadata = png_metadata(data)
    [metadata.fetch(:width), metadata.fetch(:height)]
  end

  def png_matrix(data)
    metadata = png_metadata(data)
    width = metadata.fetch(:width)
    height = metadata.fetch(:height)
    bytes_per_pixel = 3
    row_size = width * bytes_per_pixel
    raw = Zlib::Inflate.inflate(metadata.fetch(:idat))
    expected_size = (row_size + 1) * height

    raise 'Invalid PNG data size' unless raw.bytesize == expected_size

    rows = []
    previous = Array.new(row_size, 0)
    offset = 0

    height.times do
      filter = raw.getbyte(offset)
      offset += 1
      scanline = raw.byteslice(offset, row_size).bytes
      offset += row_size
      row = unfilter_scanline(filter, scanline, previous, bytes_per_pixel)
      rows << row.each_slice(bytes_per_pixel).map { |pixel| pixel.first < 128 ? '1' : '0' }.join
      previous = row
    end

    rows
  end

  private

  def png_metadata(data)
    io = StringIO.new(data)
    raise 'Invalid PNG signature' unless io.read(8) == PNG_SIGNATURE

    metadata = { idat: +''.b }
    saw_iend = false

    until saw_iend
      length_data = io.read(4)
      raise 'Missing PNG chunk length' unless length_data&.bytesize == 4

      length = length_data.unpack1('N')
      chunk_type = io.read(4)
      payload = io.read(length)
      crc = io.read(4)
      raise 'Invalid PNG chunk' unless chunk_type&.bytesize == 4 && payload&.bytesize == length && crc&.bytesize == 4

      case chunk_type
      when 'IHDR'
        raise 'Invalid IHDR length' unless length == 13

        metadata[:width], metadata[:height], metadata[:bit_depth], metadata[:color_type],
          metadata[:compression], metadata[:filter_method], metadata[:interlace] = payload.unpack('NNCCCCC')
      when 'IDAT'
        metadata[:idat] << payload
      when 'IEND'
        saw_iend = true
      end
    end

    raise 'Missing IHDR chunk' unless metadata.key?(:width)
    raise 'Unsupported PNG format' unless metadata.values_at(:bit_depth, :color_type, :compression, :filter_method, :interlace) == [8, 2, 0, 0, 0]

    metadata
  end

  def unfilter_scanline(filter, scanline, previous, bytes_per_pixel)
    row = Array.new(scanline.length, 0)

    scanline.each_index do |index|
      left = index >= bytes_per_pixel ? row[index - bytes_per_pixel] : 0
      above = previous[index]
      upper_left = index >= bytes_per_pixel ? previous[index - bytes_per_pixel] : 0
      predictor = case filter
                  when 0 then 0
                  when 1 then left
                  when 2 then above
                  when 3 then (left + above) / 2
                  when 4 then paeth_predictor(left, above, upper_left)
                  else raise "Unsupported PNG filter: #{filter}"
                  end
      row[index] = (scanline[index] + predictor) & 0xff
    end

    row
  end

  def paeth_predictor(left, above, upper_left)
    estimate = left + above - upper_left
    distance_left = (estimate - left).abs
    distance_above = (estimate - above).abs
    distance_upper_left = (estimate - upper_left).abs

    if distance_left <= distance_above && distance_left <= distance_upper_left
      left
    elsif distance_above <= distance_upper_left
      above
    else
      upper_left
    end
  end
end
