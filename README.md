# Gravity
A high-performance software framework for rapid prototyping and development of complex, distributed systems

# Build Instructions
## Windows 32/64 bit build 
### 3rd party dependencies
The tools below are required to build the 32/64 bit Gravity libraries. Install them in the order listed. 
 - [.NET Framework 4](http://www.microsoft.com/en-us/download/details.aspx?id=17851) 
 - [Windows 7.1 SDK](http://www.microsoft.com/en-us/download/details.aspx?id=8279)
 - [Visual Studio 2012 Express for Windows Desktop](http://go.microsoft.com/?linkid=9816758)
    - Run windows update and install the recommended updates after this installation. 
 - [Google Protocol Buffers v2.5](http://code.google.com/p/protobuf/downloads/detail?name=protobuf-2.5.0.tar.bz2&can=2&q=)
    - Build libprotobuf libs 
      - Open the project file in the vsprojects directory (e.g. C:\protobuf-2.5.0\vsprojects\protobuf.sln) in Visual Studio 2012 Express 
      - Click ‘OK’ when prompted to upgrade to VS2012.
      - Right-click on the libprotobuf project in the “Solution Explorer” and select “Properties”. 
      - Select Configuration Properties->General and set (if not already set) the platform toolset to “Visual Studio 2012” for both the Debug and Release configurations.  
      - If building 64-bit version 
        - Open Configuration Manager 
        - Select <New…> from Active Solution platform pull-down selector 
        - For Type, select “x64”. 
        - Copy Settings from “Win32”. 
        - Select “Create new project platform” checkbox. 
        - Click OK button 
      - Select the desired build platform (Win32 or x64) 
      - Build the Release configuration 
      - If debug builds of Gravity are required, you also need to build the Debug configuration as follows: 
      - Select the Debug configuration 
      - Right-click on the libprotobuf project in the “Solution Explorer” and select “Properties”. 
      - Select Configuration Properties->General and change “Target Name” from “$(ProjectName?)” to “$(ProjectName?)_d” 
      - Build the Debug configuration 
    - Download [protoc.exe](http://code.google.com/p/protobuf/downloads/detail?name=protoc-2.5.0-win32.zip&can=2&q=) (version 2.5.0) and place in protobuf folder (e.g. C:\protobuf-2.5.0) 
      - Add the protoc.exe location to your path. 
 - [Java Development Kit (JDK)](http://www.oracle.com/technetwork/java/javase/downloads/jdk7-downloads-1880260.html) (32-bit or 64-bit depending on build requirements) 
 - [swigwin](http://www.swig.org/download.html)
 - [Windows Flex & Bison](http://sourceforge.net/projects/winflexbison/) 
 - [Boost](http://www.boost.org/)
 - [Maven](http://maven.apache.org/download.cgi)
 - ZeroMQ 
   - [32-bit](http://miru.hk/archive/ZeroMQ-3.2.3~miru2.3-x86.exe) 
   - [64-bit](http://miru.hk/archive/ZeroMQ-3.2.3~miru2.3-x64.exe)
   - Download and run the installer for the appropriate version of ZeroMQ (Get both if intending both 32-bit & 64-bit Gravity builds) 
   - Note that you’ll need to change the environmental variable (ZMQ_HOME) to toggle between the 32-bit and 64-bit versions. 
 - [PThreads](ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.zip)

### Environmental Variables
 - PROTOBUF_HOME - The root directory of the downloaded Protobuf package (e.g. C:\protobuf-2.5.0)
 - JAVA_HOME - The JDK root (e.g. C:\Program Files (x86)\Java\jdk1.7.0_xx) 
 - BOOST_HOME - The Boost root directory (e.g. C:\boost_1_57_0)
 - ZMQ_HOME - ZeroMQ root (e.g. C:\Program Files (x86)\ZeroMQ 3.2.3)
 - PTHREADS_HOME - PThreads pre built root (e.g. C:\pthreads-w32-2-9-1-release\Pre-built.2\) 
 - GRAVITY_HOME - Gravity root directory (e.g. C:\gravity)
 - LEX_CMD - win_flex.exe 
 - YACC_CMD - win_bison.exe 
 - PATH - Add the following, updating those in italics to fit your system:
   - %JAVA_HOME%\bin
   - %GRAVITY_HOME%\bin
   - %GRAVITY_HOME%\deps
   - *path\to\swigwin\directory*
   - *path\to\winflexbison\directory*
   - *path\to\maven\directory*

### Building Gravity
 - Clone from the git repository the Gravity source tree 
 - Update environmental variables as required for the desired build configuration 
   - Specifically, if changing between 32-bit and 64-bit builds, the following environment variables will need to be set appropriately: 
     - ZMQ_HOME 
     - JAVA_HOME 
 - Configure Dependencies 
   - Open a command prompt (as Administrator) 
   - Navigate to the GRAVITY_HOME directory 
   - 	Run the configDeps batch file 
     - This is will prompt you to specify the build configuration (VS2010/VS2012, 32-bit/64-bit) 
     - Enter the number for the desired configuration 
     - This will create a dependency directory used in later stages of the build 
 - Open Gravity solution file in GRAVITY_HOME\build\msvs\Gravity.sln in Visual Studio 2012 Express 
 - Select the desired build configuration (Release2010, Release2012, Debug2010, Debug2012) and platform (x86, x64) 
 - Build the solution 
The Gravity distribution will be located in %GRAVITY_HOME%\<Platform>\<Configuration> and will include bin, lib, deps, and include directories. 
Add the bin and deps directories to your system path. 
