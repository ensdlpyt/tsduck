# Building TSDuck   {#building}
[TOC]

TSDuck can be built on Windows, Linux and macOS.

Support for Dektec devices and DVB tuners is implemented only on Windows and Linux.
macOS can only support files and IP for TS input and output.


# Pre-requisites: build tools {#buildreq}

## Windows {#reqwindows}

- Visual Studio 2017 Community Edition. This is the free version of Visual Studio.
  If can be downloaded [here](https://www.visualstudio.com/downloads/). This link
  allows the download of the latest version of Visual Studio but offers no way to
  download a specific previous version.

- CppUnit binary libraries. Download [here](https://sourceforge.net/projects/cppunit-msvc/files/).

- Doxygen for Windows. Download [here](http://www.doxygen.org/download.html).

- Graphviz for Windows (used by Doxygen to generate graphs and diagrams).
  Download [here](http://www.graphviz.org/Download_windows.php).

- NSIS, the NullSoft Scriptable Install System.
  Download [here](http://nsis.sourceforge.net/Download).
  Note that TSDuck is usually built with
  [NSIS Version 2.46](https://sourceforge.net/projects/nsis/files/NSIS%202/2.46/nsis-2.46-setup.exe/download).

- Optional Dektec Windows SDK (DTAPI): Execute the PowerShell script `dektec\Download-Install-Dtapi.ps1`.
  It downloads and installs the Dektec Windows SDK from the Dektec site.
  Alternatively, you may download it [here](http://www.dektec.com/products/SDK/DTAPI/Downloads/WinSDK.zip).
  TSDuck project files will detect DTAPI automatically. See the Visual Studio property
  file `build\msvc2017\msvc-use-dtapi.props` for details.

## Fedora {#reqfedora}

- Setup for a TSDuck native build:
~~~~
dnf install gcc-c++ doxygen graphviz curl pcsc-tools pcsc-lite-devel cppunit-devel libcurl libcurl-devel
~~~~

- Setup to build 32-bit TSDuck on 64-bit system (command `make m32`):
~~~~
dnf install glibc-devel.i686 libstdc++-devel.i686 pcsc-lite-devel.i686 cppunit-devel.i686 libcurl-devel.i686
~~~~

## Red Hat Entreprise Linux, CentOS {#reqrhel}

- Setup for a TSDuck native build:
~~~~
yum install gcc-c++ doxygen graphviz curl pcsc-tools pcsc-lite-devel cppunit-devel libcurl libcurl-devel
~~~~

- Setup to build 32-bit TSDuck on 64-bit system (command `make m32`):
~~~~
yum install glibc-devel.i686 libstdc++-devel.i686 pcsc-lite-devel.i686 cppunit-devel.i686 libcurl-devel.i686
~~~~

## Ubuntu {#requbuntu}

- Setup for a TSDuck native build:
~~~~
apt-get install g++ doxygen graphviz curl pcscd libpcsclite-dev libcppunit-dev libcurl3 libcurl3-dev
~~~~

- It is not possible to build 32-bit TSDuck on 64-bit Ubuntu system (command `make m32`) because
  there is no 32-bit cross-compiled package for pcsc and cppunit on Ubuntu 64-bit.

## All Linux distros {#reqlinux}

- Optional Dektec DTAPI: The command `make` at the top level will automatically
  download the LinuxSDK from the Dektec site. See `dektec/Makefile` for details.

## macOS {#reqmac}

- Install the Xcode command line utilities (in other words, the _clang_ compiler suite):
~~~~
xcode-select --install
~~~~

- Install the [Homebrew](https://brew.sh/) package manager:
~~~~
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
~~~~

- Install common open source tools using Homebrew:
~~~~
brew install pcsc-lite cppunit doxygen graphviz gnu-sed grep
~~~~

# Building the TSDuck binaries {#buildbin}

## Windows {#buildwindows}

Execute the PowerShell script `build\Build.ps1`. The TSDuck binaries, executables and
DLL's, are built in directories `msvc2017\Release-Win32` and `msvc2017\Release-x64`
for 32-bit and 64-bit platforms respectively.

## Linux and macOS {#buildlinux}

Execute the command `make` at top level. The TSDuck binaries, executables and shared
objects (`.so`), are built in the `src` directory tree in subdirectories `release-i386`
and `release-x86_64` for 32-bit and 64-bit platforms respectively.

To build a 32-bit version of TSDuck on a 64-bit system, execute the command `make m32`.

# Building the TSDuck installers {#buildinst}

There is no need to build the TSDuck binaries before building the installers.
Building the binaries, when necessary, is part of the installer build.

All installation packages are dropped into the subdirectory `installers`.
The packages are not deleted by the cleanup procedures. They are not pushed
into the git repository either.

## Windows {#instwindows}

Execute the PowerShell script `build\Build-Installer.ps1`.
Two installers are built, for 32-bit and 64-bit systems respectively.

## Fedora, CentOS, Red Hat Entreprise Linux {#instrhel}

Execute the command `make rpm` at top level to build a `.rpm` package for the same
architecture as the build system. On 64-bit systems, execute the command `make rpm32`
to build a 32-bit package.

## Ubuntu {#instubuntu}

Execute the command `make deb` at top level to build a `.deb` package for the same
architecture as the build system.

## Installer files {#instfiles}

The following table summarizes the packages which are built and dropped
into the `installers` directory, through a few examples, assuming that the
current version of TSDuck is 3.7-512.

| File name                            | Description
| ------------------------------------ | -------------------------
| TSDduck-3.7-512-src.zip              | Source archive on Windows
| tsduck-3.7-512.tgz                   | Source archive on Linux and macOS
| tsduck_3.7-512_amd64.deb             | Binary package for 64-bit Ubuntu
| tsduck-3.7-512.el7.i386.rpm          | Binary package for 32-bit Red Hat or CentOS 7.x
| tsduck-3.7-512.el7.x86_64.rpm        | Binary package for 64-bit Red Hat or CentOS 7.x
| tsduck-3.7-512.el7.src.rpm           | Source package for Red Hat or CentOS 7.x
| tsduck-3.7-512.fc25.i386.rpm         | Binary package for 32-bit Fedora 25
| tsduck-3.7-512.fc25.x86_64.rpm       | Binary package for 64-bit Fedora 25
| tsduck-3.7-512.fc25.src.rpm          | Source package for Fedora 25
| tsduck-dev_3.7-512_amd64.deb         | Development package for 64-bit Ubuntu
| tsduck-devel-3.7-512.el7.i386.rpm    | Development package for 32-bit Red Hat or CentOS 7.x
| tsduck-devel-3.7-512.el7.x86_64.rpm  | Development package for 64-bit Red Hat or CentOS 7.x
| tsduck-devel-3.7-512.fc25.i386.rpm   | Development package for 32-bit Fedora 25
| tsduck-devel-3.7-512.fc25.x86_64.rpm | Development package for 64-bit Fedora 25
| TSDuck-Win32-3.7-512.exe             | Binary package for 32-bit Windows
| TSDuck-Win64-3.7-512.exe             | Binary package for 64-bit Windows

On Linux systems, there are two different packages. The package `tsduck` contains
the tools and plugins. This is the only required package if you just need to use
TSDuck. The package named `tsduck-devel` (or `tsduck-dev` on Ubuntu) contains the
development environment. It is useful only for third-party applications which use
the TSDuck library.

On Windows systems, there is only one binary installer which contains the tools,
plugins, documentation and development environment. The user can select which
components shall be installed. The development environment is unselected by default.
