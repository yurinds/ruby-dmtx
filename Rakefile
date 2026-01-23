require 'rake/testtask'

Rake::TestTask.new do |t|
  t.libs << 'test'
  t.pattern = 'test/test_*.rb'
  t.warning = true
end

task default: :test
