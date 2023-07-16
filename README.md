
<p align="center">
  <img height="192" src="doc/logo.jpg"/>
</p>

[![build & test (clang, gcc, MSVC)](https://github.com/kamchatka-volcano/stone_skipper/actions/workflows/build_and_test.yml/badge.svg?branch=master)](https://github.com/kamchatka-volcano/stone_skipper/actions/workflows/build_and_test.yml)

**stone_skipper** is a process-launching server that runs the commands registered in the configuration file for the corresponding incoming HTTP requests. It works asynchronously and can initiate multiple processes without blocking while waiting for their completion, even when running with a single thread.

### Usage
**stone_skipper** is implemented as a FastCGI service, so you'll require a compatible HTTP server to use it. NGINX can be used with a following configuration:
```
server {
	listen 8088;
	server_name localhost;
	index /~;	
	location / {
		try_files $uri $uri/ @fcgi;
	}
	
	location @fcgi {	
		fastcgi_pass  unix:/tmp/stone_skipper.sock;		
		#or using a TCP socket
		#fastcgi_pass localhost:9000;
		include fastcgi_params;		
		fastcgi_keep_conn off;	    
	}
}
```

After enabling this configuration, **stone_skipper** can be launched using the following command:

```
stone_skipper -fcgiAddress=/tmp/stone_skipper.sock
```

#### Configuration
`stone_skipper` creates an empty default configuration file `stone_skipper/stone_skipper.cfg` in your system config directory. The path to another config file can be set in the command line using the following format:
```
stone_skipper -fcgiAddress=stone_skipper.sock -config=/home/user/stone_skipper.cfg
```

The configuration file is written in [shoal](shoal.eelnet.org) format:

```
#tasks:
###
  route = /greet/{{name}}
  command = echo "Hello {{name}}"
###
  route = /shutdown
  process = off.bat
```

`tasks` is a list of process launching tasks, where each task consists of a `route` parameter that specifies the incoming request's path triggering the action, and either a `command` or `process` parameter that specifies the command to launch. When using `command`, the command is executed in the shell, while `process` launches it directly. 

The `route` parameter can define a wildcard parameter that matches one or more characters. The result of a match can be used in the `command` or `process` parameters. For example, with the earlier provided configuration, a request to `/greet/world` would launch the `echo "Hello world"` command.

#### Command line options

|                           |                                                                                |
|---------------------------|--------------------------------------------------------------------------------| 
| **Parameters:**           |                                                                                |    
| `-fcgiAddress=<fcgiHost>` | socket for FastCGI connection (either a file path, or 'ipAddress:port' string) |
| `-log=<path>`             | log file path (optional)                                                       |
| `-config=<path>`          | config file path (optional)                                                    |
| `-shell=<string> `        | shell command (optional)                                                       |
| `-threads=<int> `         | number of threads (optional)                                                   |
| **Flags:**                |                                                                                | 
| `--help`                  | show usage info and exit                                                       |
| `--version`               | show version info eand exit                                                    |


### Build instructions

To build `stone_skipper`, you will need a C++20-compliant compiler, CMake, and the Boost.Process library installed on your
system.

```
git clone https://github.com/kamchatka-volcano/stone_skipper.git
cd stone_skipper
cmake -S . -B build
cmake --build build
```

Boost dependencies can be resolved using [`vcpkg`](https://vcpkg.io/en/getting-started.html) by running the build with
this command:

```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg path>/scripts/buildsystems/vcpkg.cmake
```

### Running unit tests

```
cd stone_skipper
cmake -S . -B build -DENABLE_TESTS=ON
cmake --build build 
cd build/tests && ctest
```

## Running functional tests

Download [`lunchtoast`](https://github.com/kamchatka-volcano/lunchtoast/releases) executable, build `stone_skipper` and start NGINX with `functional_tests/nginx_*.conf` config file.
Launch tests with the following command:

* Linux command:

```
lunchtoast functional_tests
```

* Windows command:

```
lunchtoast.exe functional_tests -shell="msys2 -c"
```

To run functional tests on Windows, it's recommended to use the bash shell from the `msys2` project. After installing
it, add the following script `msys2.cmd` to your system `PATH`:

```bat
@echo off
setlocal
IF NOT DEFINED MSYS2_PATH_TYPE set MSYS2_PATH_TYPE=inherit
set CHERE_INVOKING=1
C:\\msys64\\usr\\bin\\bash.exe -leo pipefail %*
```

### License

**stone_skipper** is licensed under the [MS-PL license](/LICENSE.md)  