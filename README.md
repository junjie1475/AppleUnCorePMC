# AppleUnCorePMC
A kext and demo tool to read and config apple silicon's uncore performance counter

## Hardware
This documentation can be applied to CPUs after Firestorm/Icestorm(As far as I can tell).
### Registers
The below registers are per-cluster based(i.e there is one per every cluster) and without specific specify they are all 64bits wide.

`UPMCR0` Uncore performance mointor control register. Control interrupts on which counters are enabled and how interrupts are generated for overflows. Bits[36:21] control whether interrupts are enabled on each counter. Bits[19:17] control how interrupts are generated for overflows.  	
  INTGEN_OFF = 0,
	INTGEN_AIC = 2,
	INTGEN_HALT = 3,
	INTGEN_FIQ = 4.
 
`UMPC[0-15]` Uncore performance counting register. You can asscess the current cluster's UMPCs(i.e cluster that your code runs on) via msr instrunctions, otherwise they are asscessed through MMIO(which has a fixed range for each cluster). Unlike Core PMCs all the counters are configurable.

`UPMECM[01]` Event configuration register. There is a 8bits event selector for each counter. 

`UPMECM[01234]` CPU mask register. Control whether or not that counter counts events generated by a cpu. There is a 16bits mask for each counter. Note: The bits are based off the start of the cluster -- e.g. even if a core has a CPU ID of 4, it might be the first CPU in a cluster.  Shift the registers right by the ID of the first CPU in the cluster.

### Events
Unfortunately, the events are largely undocumented. TODO: Find the event by Reverse engineering
```c
0x2 Reference cycle
0x3 Timebase - same as the one you get from mach_absolute_time()
```
