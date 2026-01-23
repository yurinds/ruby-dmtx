require_relative 'test_helper'

class RdmtxTest < Minitest::Test
  include PngHelpers

  def setup
    skip 'Rdmtx extension not available' unless defined?(Rdmtx)
    @rdmtx = Rdmtx.new
  end

  def test_encode_returns_image_object
    image = @rdmtx.encode('hello', 5, 5, DmtxSymbolSquareAuto)
    refute_nil image
    assert_instance_of Rdmtx::Image, image
  end

  def test_image_data_is_png
    image = @rdmtx.encode('hello', 5, 5, DmtxSymbolSquareAuto)
    data = image.data
    assert_kind_of String, data
    assert data.start_with?(PngHelpers::PNG_SIGNATURE)
    width, height = png_dimensions(data)
    assert_operator width, :>, 0
    assert_operator height, :>, 0
    assert_equal width, height
  end

  def test_write_creates_png_file
    image = @rdmtx.encode('hello', 5, 5, DmtxSymbolSquareAuto)
    Dir.mktmpdir('rdmtx') do |dir|
      path = File.join(dir, 'output.png')
      image.write(path)
      assert File.exist?(path)
      data = File.binread(path)
      assert data.start_with?(PngHelpers::PNG_SIGNATURE)
      assert_operator data.bytesize, :>, 0
    end
  end

  def test_module_size_changes_dimensions
    image_small = @rdmtx.encode('hello', 2, 2, DmtxSymbolSquareAuto)
    image_large = @rdmtx.encode('hello', 2, 4, DmtxSymbolSquareAuto)
    refute_nil image_small
    refute_nil image_large
    width_small, _height_small = png_dimensions(image_small.data)
    width_large, _height_large = png_dimensions(image_large.data)
    assert_operator width_large, :>, width_small
  end
end
