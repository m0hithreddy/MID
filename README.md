# MID

**MID - Multiple Interface Downloader**

## Overview

* MID is a HTTP[S] download accelerator.
 
* Traditional HTTP[S] download accelerators use HTTP range requests to achieve this. 

* MID also uses HTTP range requests to achieve the download acceleration but it takes it to the next level by involving multiple network-interfaces in the download (hence bandwidth boost)
 
## Dependencies

The list of dependencies required for the compiling, running and building from the sources of MID are given along with the possible installation steps (OS dependent)

##### MID dependencies

* [```libssl-dev```](https://github.com/openssl/openssl) [```$ sudo apt install libssl-dev```]
* [```zlib1g-dev```](https://github.com/madler/zlib) [```$ sudo apt install zlib1g-dev```]

##### Build and Install dependencies

* [```autoconf```](https://github.com/autotools-mirror/autoconf) [```$ sudo apt install autoconf```]
* [```automake```](https://github.com/autotools-mirror/automake) [```$ sudo apt install automake```]
* [```libtool```](https://github.com/autotools-mirror/libtool) [```$ sudo apt install libtool```]
* [```txt2man```](https://github.com/mvertes/txt2man) [```$ sudo apt install txt2man```]

## Downloading

Obtain the latest stable MID sources by cloning from GitHub mirror

	$ git clone https://github.com/m0hithreddy/MID.git && cd MID

Also you can fetch the archive of master branch or particular relase as follows

* Master Branch

		$ wget https://github.com/m0hithreddy/MID/archive/master.zip && unzip master.zip && cd MID-master
	
* Zip archive of particular release
		
		$ wget https://github.com/m0hithreddy/MID/archive/vx.y.z.zip && unzip vx.y.z.zip && cd MID-x.y.z

* Gzipped Tar archive of particular release
		
		$ wget https://github.com/m0hithreddy/MID/archive/vx.y.z.tar.gz && tar -xvf vx.y.z.tar.gz && cd MID-x.y.z

## Build and Install

After installing dependencies, obtaining MID sources and changing to the source directory...

* Quick and Dirty Installation

		$ ./configure
		$ make all
		$ make install

* Clean Installation

		$ autoreconf -vfi
		$ ./configure
		$ make all
		$ make uninstall
		$ make install

## Usage

Verify the installation with ```$ {MID | mid} -V```. Now you can download the file from URL as follows
			
	$ sudo {MID | mid} --url URL [--detailed-progress | -Pd] [--exclude-interfaces lo | -ni lo]

Check ```$ {MID | mid} --help``` for more options and ```/usr/local/etc/MID.conf``` can be used to make the settings persistent

## Reporting

As a user you can be helpful to the project by reporting any undefined, undesired and unexpected behavior. Please, see [Reporting.md](https://github.com/m0hithreddy/MID/blob/master/Reporting.md)

## Contributing

[MID](https://github.com/m0hithreddy/MID) works! But MID requires many feature additions, improvements and surveillance. If you are a budding developer like me, it is a high time
you can get into some serious development by contributing to MID. Please, see [Contributing.md](https://github.com/m0hithreddy/MID/blob/master/Contributing.md)

## License
[GNU GPLv3](https://choosealicense.com/licenses/gpl-3.0/)
