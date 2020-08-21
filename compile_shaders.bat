echo off
mkdir build
for %%x in (preSegment.vert preSegment.frag preFan.vert preFan.frag post.vert post.frag) do tools\glslangValidator.exe -V res\shaders\%%x -o build\%%x.spv"
pause