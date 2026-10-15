# Third-Party Notices

## gps-sdr-sim (MIT License)

The navigation-message functions in `navmsg-server/extracted_funcs.c` and parts of
`navmsg-server/navmsg_tcp_server.c` are derived from `gpssim.c` of
[gps-sdr-sim](https://github.com/osqzss/gps-sdr-sim) by Takuji Ebinuma.

```
MIT License

Copyright (c) 2015-2025 Takuji Ebinuma

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## RINEX navigation data

`navmsg-server/data/brdc1000.26n` is an IGS broadcast ephemeris file produced via
NASA CDDIS (see the file header) and is included as sample input.

## NI GPS Data Streaming VI

`labview/third_party/niGPS Direct Streaming.vi` is the National Instruments
GPS streaming implementation used for the final integration (thesis Section 3.8).
It is not the authors' original work and remains subject to National Instruments'
license terms. NI, LabVIEW and USRP are trademarks of their respective owners.
