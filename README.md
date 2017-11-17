# GPMF Introduction

The GPMF structured storage format was originally proposed to store high-frequency periodic sensor data within a video file like an MP4. Action cameras, like that from GoPro, have limited computing resources beyond that needed to store video and audio, so any telemetry storage needed to be lightweight in computation, memory usage and storage bandwidth. While JSON and XML systems where initially considered, the burden on the embedded camera system was too great, so something simpler was needed. While the proposed GPMF structure could be used stand-alone, our intended implementation uses an additional time-indexed track with an MP4, and with an application marker within JPEG images. GPMF share a Key, Length, Value structure (KLV), similar to QuickTime atoms or Interchange File Format (IFF), but the new KLV system is better for describing sensor data. Problems solved:

* The contents of new Keys can be parsed without prior knowledge.
* Nested structures can be defined without &#39;Key&#39; dictionary.
* Structure prevents naming collisions between multiple sources.
* Nested structures allows for the communication of metadata for telemetry, such as scale, units, and data ranges etc.
* Somewhat human (engineer) readable (i.e. hex-editor friendly.)
* Timing and indexing for use existing methods stored within the wrapping MP4 of similar container format.

GPMF -- GoPro Metadata Format or General Purpose Metadata Format -- is a modified Key, Length, Value solution, with a 32-bit aligned payload, that is both compact, full extensible and somewhat human readable in a hex editor. GPMF allows for dependent creation of new FourCC tags, without requiring central registration to define the contents and whether the data is in a nested structure. GPMF is optimized as a time of capture storage format for the collection of sensor data as it happens. 

## GPMF-writer

Most developers using GPMF will likely only need the [GPMF-parser](https://github.com/gopro/gpmf-parser) for extracting and processing existing camera telemetry data.  For developers creating new GPMF data, please read on.  The GPMF-parser readme contains an explanation of the GPMF structure.

# Included Within This Repository

* The complete source to an GPMF writer library
* Demo code for using the GPMF writer with GPMF-parser components used to verify written data.
* CMake support for building the demo project.
* Tested on:
  - macOS High Sierra with XCode v8 & v9
  - Windows 10 with Visual Studio 2015 & 2017
  - Ubuntu 16.04 with gcc v5.4

## Not-included

* MP4/MOV multiplexing or writing.

# License Terms

GPMF-write is licensed under either:

* Apache License, Version 2.0, (LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0)
* MIT license (LICENSE-MIT or http://opensource.org/licenses/MIT)

at your option.

## Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.

### Quick Start for Developers

#### Setup

Clone the project from Github (git clone https://github.com/gopro/gpmf-write).

