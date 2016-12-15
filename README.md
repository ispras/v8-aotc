# V8 JavaScript Engine with AOTC

This is a modified version of [V8 JavaScript Engine](https://developers.google.com/v8/). AOTC means "Ahead-Of-Time Compilation" and in this project it allows to save machine-dependent optimized Lithium IR of Crankshaft JIT compiler into a file. Saving is available through command line "d8" interface. Based on V8 version 3.31.

## Building

[V8 build instructions](https://github.com/v8/v8/wiki/Building%20with%20Gyp). Building process is the same, our implementation is only for x64 platform, basically build is done using "make x64.release" command.

## New command line options for d8

option | description
:----: | -----------
<code>--save-code&nbsp;\<file\></code> | Save Lithium IR into file
<code>--load-code&nbsp;\<file\></code> | Load Lithium IR from file
`--saveload-edge` | Set other d8 options which may break something when using saved Lithium IR. For example, concurrent JIT and concurrent on-stack-replacement must be disabled.

This implementation supports only a single JavaScript source file.

## Example

One of [arewefastyet.com](https://arewefastyet.com/) asm.js benchmarks
```
$ wget -q https://raw.githubusercontent.com/h4writer/arewefastyet/master/benchmarks/asmjs-apps/box2d.js
$ #three standard runs with different size
$ /usr/bin/time -f %U ./d8 --saveload-edge box2d.js -- 1
frame averages: 17.725 +- 42.323, range: 3.000 to 264.000 
0.68
$ /usr/bin/time -f %U ./d8 --saveload-edge box2d.js -- 2
frame averages: 6.347 +- 14.352, range: 3.000 to 178.000 
1.24
$ /usr/bin/time -f %U ./d8 --saveload-edge box2d.js -- 3
frame averages: 4.892 +- 10.184, range: 3.000 to 179.000 
1.98
```
Gaining up to ~0.2 second speedup using saved Lithium
```
$ #saving
$ /usr/bin/time -f %U ./d8 --saveload-edge --save-code box2d.lithium box2d.js -- 1
frame averages: 16.425 +- 32.401, range: 4.000 to 191.000 
0.69
$ #three runs with loading
$ /usr/bin/time -f %U ./d8 --saveload-edge --load-code box2d.lithium box2d.js -- 1
frame averages: 10.400 +- 13.926, range: 3.000 to 69.000 
0.43
$ /usr/bin/time -f %U ./d8 --saveload-edge --load-code box2d.lithium box2d.js -- 2
frame averages: 5.601 +- 7.078, range: 3.000 to 70.000 
1.08
$ /usr/bin/time -f %U ./d8 --saveload-edge --load-code box2d.lithium box2d.js -- 3
frame averages: 4.632 +- 5.095, range: 2.000 to 68.000 
1.82
```


## Resources

- Augmenting JavaScript JIT with Ahead-of-Time Compilation. Compiler, Architecture and Tools Conference 2015, Haifa, Israel. ([slides](https://software.intel.com/sites/default/files/managed/65/2f/aotc_haifa.pdf))
- Ahead-of-Time Compilation of JavaScript Programs. Programming and Computer Software, 2017, Vol. 43, No. 1, pp. 51–59, ISSN 0361-7688
- [Ahead of Time Optimization for JavaScript Programs. Trudy ISP RAN/Proc. ISP RAS, vol. 27, issue 6, 2015, pp. 67-86 (in Russian)](http://www.ispras.ru/proceedings/docs/2015/27/6/isp_27_2015_6_67.pdf)
