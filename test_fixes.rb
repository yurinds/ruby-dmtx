#!/usr/bin/env ruby
# Test script to verify fixes

require_relative 'ext/rdmtx/Rdmtx'
require 'benchmark'

puts "=" * 60
puts "TESTING RDMTX FIXES"
puts "=" * 60

rdmtx = Rdmtx.new

# Test 1: Проверка валидации параметров
puts "\n[TEST 1] Parameter validation"
puts "-" * 60

begin
  rdmtx.encode("test", -1, 5)
  puts "❌ FAILED: Should reject negative margin"
rescue ArgumentError => e
  puts "✅ PASSED: Correctly rejected negative margin"
  puts "   Error: #{e.message}"
end

begin
  rdmtx.encode("test", 99999, 5)
  puts "❌ FAILED: Should reject too large margin"
rescue ArgumentError => e
  puts "✅ PASSED: Correctly rejected large margin"
  puts "   Error: #{e.message}"
end

begin
  rdmtx.encode("test", 5, -1)
  puts "❌ FAILED: Should reject negative module"
rescue ArgumentError => e
  puts "✅ PASSED: Correctly rejected negative module"
  puts "   Error: #{e.message}"
end

begin
  rdmtx.encode("test", 5, 99999)
  puts "❌ FAILED: Should reject too large module"
rescue ArgumentError => e
  puts "✅ PASSED: Correctly rejected large module"
  puts "   Error: #{e.message}"
end

begin
  rdmtx.encode("", 5, 5)
  puts "❌ FAILED: Should reject empty string"
rescue ArgumentError => e
  puts "✅ PASSED: Correctly rejected empty string"
  puts "   Error: #{e.message}"
end

# Test 2: Проверка обработки исключений (не должно быть утечек)
puts "\n[TEST 2] Exception handling (memory leak prevention)"
puts "-" * 60

begin
  1000.times do
    begin
      rdmtx.encode("test", "invalid_type", 5)
    rescue TypeError
      # Ожидаемое исключение
    end
  end
  puts "✅ PASSED: 1000 TypeError exceptions handled without crash"
rescue => e
  puts "❌ FAILED: #{e.message}"
end

# Test 3: Нормальная работа
puts "\n[TEST 3] Normal operation"
puts "-" * 60

begin
  image = rdmtx.encode("Hello World!", 5, 5)
  if image.is_a?(Rdmtx::Image)
    puts "✅ PASSED: encode() returns Rdmtx::Image"
  else
    puts "❌ FAILED: encode() returned #{image.class}"
  end

  data = image.data
  if data.is_a?(String) && data.bytesize > 0
    puts "✅ PASSED: image.data returns non-empty string (#{data.bytesize} bytes)"
  else
    puts "❌ FAILED: image.data invalid"
  end

  # Проверка PNG signature
  png_sig = "\x89PNG\r\n\x1a\n".b
  if data.start_with?(png_sig)
    puts "✅ PASSED: PNG signature valid"
  else
    puts "❌ FAILED: PNG signature invalid"
  end

rescue => e
  puts "❌ FAILED: #{e.message}"
  puts e.backtrace.first(5)
end

# Test 4: Производительность
puts "\n[TEST 4] Performance benchmark"
puts "-" * 60

iterations = 1000
result = Benchmark.measure do
  iterations.times do
    rdmtx.encode("Hello World!", 5, 5)
  end
end

avg_time = (result.real * 1000 / iterations).round(3)
puts "✅ #{iterations} iterations completed"
puts "   Total time: #{(result.real * 1000).round(1)}ms"
puts "   Average time per encode: #{avg_time}ms"

if avg_time < 10
  puts "   Performance: EXCELLENT"
elsif avg_time < 20
  puts "   Performance: GOOD"
else
  puts "   Performance: ACCEPTABLE"
end

# Test 5: Разные размеры
puts "\n[TEST 5] Different sizes and parameters"
puts "-" * 60

test_cases = [
  { text: "Hi", margin: 2, module: 2, desc: "Small (2x2)" },
  { text: "Hello World!", margin: 5, module: 5, desc: "Medium (5x5)" },
  { text: "Hello World!", margin: 10, module: 10, desc: "Large (10x10)" },
  { text: "A" * 100, margin: 5, module: 3, desc: "Long text (100 chars)" }
]

test_cases.each do |tc|
  begin
    image = rdmtx.encode(tc[:text], tc[:margin], tc[:module])
    size = image.data.bytesize
    puts "✅ #{tc[:desc]}: #{size} bytes"
  rescue => e
    puts "❌ #{tc[:desc]}: #{e.message}"
  end
end

# Test 6: Write to file
puts "\n[TEST 6] Write to file"
puts "-" * 60

begin
  require 'tmpdir'
  Dir.mktmpdir('rdmtx_test') do |dir|
    image = rdmtx.encode("Test Image", 5, 5)
    path = File.join(dir, 'test_output.png')
    image.write(path)

    if File.exist?(path)
      size = File.size(path)
      puts "✅ PASSED: File written successfully (#{size} bytes)"

      # Проверка что файл валидный PNG
      content = File.binread(path)
      if content.start_with?("\x89PNG\r\n\x1a\n".b)
        puts "✅ PASSED: Written file is valid PNG"
      else
        puts "❌ FAILED: Written file is not valid PNG"
      end
    else
      puts "❌ FAILED: File not created"
    end
  end
rescue => e
  puts "❌ FAILED: #{e.message}"
end

puts "\n" + "=" * 60
puts "TESTING COMPLETE"
puts "=" * 60
