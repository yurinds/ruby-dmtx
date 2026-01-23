This is a gem enabled version with some additional features of the ruby
wrapper for libdmtx that can be found at:
https://github.com/dmtx/dmtx-wrappers

The original README file is renamed as libdmtx-README.

[![CI](https://github.com/yurinds/ruby-dmtx/actions/workflows/ci.yml/badge.svg)](https://github.com/yurinds/ruby-dmtx/actions/workflows/ci.yml)

Installation
============

    gem install Rdmtx

Dependencies
============

Runtime system libraries:

* libdmtx (Data Matrix encoding)
* libpng (PNG writer for fast image output)

No ImageMagick/RMagick is required.

Alpine packages:

```
apk add --no-cache libdmtx libpng
```

Build dependencies (for native extension):

* libdmtx-dev
* libpng-dev

Alpine build packages:

```
apk add --no-cache build-base libdmtx-dev libpng-dev
```

Example
=======

    require 'Rdmtx'
    rdmtx = Rdmtx.new
    i = rdmtx.encode("Hello you !!", 5, 5)
    i.write("output.png")

API
===

### Create new Rdmtx object

    rdmtx.new

### Encoding

    rdmtx.encode(String, MarginSize, ModuleSize, SizeRequest)

Creates and returns an `Rdmtx::Image` object by encoding `String` with
(optional arguments) margin `MarginSize`, module `ModuleSize`, and
request a particular size of the output `SizeRequest`.

* `MarginSize` is the margin in number of pixels
* `ModuleSize` is the size of one module in number of pixels
* `SizeRequest` has to be one of pre-defined constants like:
  `DmtxSymbolSquareAuto`, `DmtxSymbol10x10`, etc..
  See [`ext/rdmtx/Rdmtx.c`](ext/rdmtx/Rdmtx.c) for a full listing of the
  possible values.



Differences from the original gem
=================================

* No RMagick/ImageMagick dependency.
* Encoding returns `Rdmtx::Image` (wrapper over PNG bytes) with `.write(path)`.
* Faster and lighter image generation: PNG is written directly from C via libpng.
* `decode` has been removed in this fork.


Development Environment
=======================

### Dependencies

```
sudo apt install software-properties-common
sudo apt-add-repository -y ppa:rael-gc/rvm
sudo apt update
sudo apt install rvm
sudo apt install libpng-dev libdmtx-dev
rvm install 2.7
```

### Build gem
```
rvm use 2.7@ruby-dmtx
gem build Rdmtx.gemspec
```

### Install and test local gem
```
gem install ./Rdmtx-0.5.2.gem
./test.rb
```

### Upload to rubygems
```
gem push Rdmtx-0.5.2.gem
```
