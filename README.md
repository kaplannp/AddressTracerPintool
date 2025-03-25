# AddressTracer
## Overview
This is a program for generating per thread traces from a multithreaded program.
The output is roughly:
```
BeginRoi
num instruction <number>
W: <address>
num instruction <number>
R: <address>
EndRoi
...
```
The num instructions represent the number of instruction after the last memory
operation *inlcuding this memory operation*. e.g. `add, add, load` prints 
`num 3; R: <addr>`.

Note also that some x86 instructions will read and write in the same
instruciton, in this case the instruction still just counts as 1 for the num
instructions, but it will print two memory lines after the num instructions.

## ROI (region of interest)
We use the old ROI method of instrumenting every instruciton, but having
conditional callbacks based on being in the ROI.

To create an ROI in source, include the `BEGIN_PIN_ROI` and `END_PIN_ROI`
macros before/after your ROI. NOTE that these macros will not be included in the
instruction count. Trace collection begins immediately after/before the macros.

