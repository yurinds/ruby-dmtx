Gem::Specification.new do |s|
  s.name        = 'Rdmtx'
  s.version     = '0.5.2'
  s.date        = '2025-01-03'
  s.summary     = 'Ruby libdmtx wrapper'

  s.description = 'This is a ruby wrapper for libdmtx, which is a open source software for reading and writing Data Matrix barcodes.'
  s.authors     = ['Srijan Choudhary']
  s.email       = 'srijan4@gmail.com'
  s.homepage    = 'https://github.com/srijan/ruby-dmtx'
  s.license     = 'LGPL-2.1'

  s.files       = Dir.glob('ext/**/*.{c,h,rb}') + Dir.glob('test/**/*.rb') + ['README.md']
  s.extensions  = ['ext/rdmtx/extconf.rb']

  s.requirements << 'libdmtx'
  s.requirements << 'libpng'

  s.add_development_dependency 'minitest', '>= 5'
  s.add_development_dependency 'rake', '~> 13'
end
