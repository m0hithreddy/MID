# MID

**MID - Multiple Interface Downloader**

## Overview

* MID is a HTTP download accelerator.
 
* Traditional HTTP download accelerators use HTTP range requests to achieve this. 

* MID also uses HTTP range requests to achieve download acceleration but it takes it to the next level by involving multiple network-interfaces in the download (hence bandwidth boost)
 
## Dependencies

The list of dependencies required for the compiling, running and building from the sources of MID are given along with the possible installation steps (OS dependent)

##### MID dependencies

* [```libssl-dev```](https://github.com/openssl/openssl) [```$ apt install libssl-dev```]

##### Build and Install dependencies

* [```autoconf```](https://github.com/autotools-mirror/autoconf) [```$ apt install autoconf```]
* [```automake```](https://github.com/autotools-mirror/automake) [```$ apt install automake```]
* [```libtool```](https://github.com/autotools-mirror/libtool) [```$ apt install libtool```]
* [```txt2man```](https://github.com/mvertes/txt2man) [```$ apt install txt2man```]

## Downloading

Obtain the latest stable MID sources by cloning from GitHub mirror

	$ git clone https://github.com/m0hithreddy/MID.git

Also you can fetch the archive of master branch or particular relase as follows

* Master Branch

		$ wget https://github.com/m0hithreddy/MID/archive/master.zip && unzip MID-master.zip && cd MID-master
	
* Zip archive of particular release
		
		$ wget https://github.com/m0hithreddy/MID/archive/vx.y.z.zip && unzip MID-x.y.z.zip && cd MID-x.y.z

* Gzipped Tar archive of particular release
		
		$ wget https://github.com/m0hithreddy/MID/archive/vx.y.z.tar.gz && tar -xvf MID-x.y.z.tar.gz && cd MID-x.y.z

## Build and Install

After installing dependencies, obtaining MID sources and changing to the source directory...

* Quick and Dirty Installation

		$ ./configure
		$ make
		$ sudo make install

* Clean Installation

		$ autoreconf -vfi
		$ ./configure
		$ make
		$ sudo make install

## Usage

Verify the installation with ```$ MID -v```. Now you can download the file from URL as follows
			
	$ sudo MID --url URL [ --detailed-progress | -Pd ]

Check ```$ MID --help``` for more options and ```/etc/MID/MID.conf``` can be used to make the settings persistent

## Reporting

As a user you can be helpful to the project by reporting any undefined, undesired and unexpected behavior. Please, see [Reporting.md](https://github.com/m0hithreddy/MID/blob/master/Reporting.md)

## Contributing

[MID](https://github.com/m0hithreddy/MID) works! But MID requires many feature additions, improvements and survilance. If you are a budding developer like me, it is a high time
you can get into some serious development by contributing to MID. Please, see [Contributing.md](https://github.com/m0hithreddy/MID/blob/master/Contributing.md)

## License
[GNU GPLv3](https://choosealicense.com/licenses/gpl-3.0/)
