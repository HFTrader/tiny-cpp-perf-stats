# tiny-cpp-perf-stats

## Description

This project intends to provide an easy framework for modelling algorithm behavior. It grew organically from the necessity of tapping into the internal CPU performance counters and then generating some statistics on it.

The project is composed of one very simple class/API for encapsulation of the stats collection and on simple OLS solver for model selection.

Once data is collected, several statistical models are fitted. The best fit is then printed.

The statistics model the number of cycles spent by the processor as dependent of:
- one Big-O: constant, Log(N), N, N.Log(N) and N^2
- several permutations of performance counters: cache misses and branch misses

A total of 28 models is generated and fitted. The best model is calculated using the Akaike Information Criterion (AIC). Other metrics are also displayed (R^2, F-test, BIC).

A test was created that uses the scheduler problem implemented in two naive ways:
- std::multimap
- std::priority_queue


## Dependencies:

- Boost math/statistics/container

If you boost is 

- Armadillo C++ Algebra

## Instructions:

- Build libtinyperfstats (see CMakeLists.txt)

- Create a test binary

Include _Snapshot.h_ and create one Snapshot object. Typically your problem will be dependent on some **N**, being the number of objects for example.

Call snap.start() at the beginning of every section you want to measure.

Call snap.stop() passing a label for the test (like "Lookup" or "Insert") and also pass your **N** and the number of times (loops) each operation has been performed.

After all data collection is done, call snap.summary() passing one header string and a FILE pointer object (defaults to stdin). The best models will be computed and displayed.

- Build the test linking against the library

When the test runs, the library will take care of starting/stopping performance counters and running some statistical model selection at the end.

## Example

```
$ ./schedule   (the test app)

PriorityQueue, Event:ReCheck, Cyc/Ins:0.80 Cyc/Bch:531.83 Points:6 Rsq: 1.00 F:0.000000 LL:-6.916004 aic:3.638668 bic:3.499841
   Term: Constant      p:0.00000 coef:136.678
   Term: CacheMisses   p:0.00000 coef:132.729
   Term: BranchMisses  p:0.00000 coef:21.0578
   Term: N*Log(N)      p:0.00000 coef:5.71472e-05
PriorityQueue, Event:Refill, Cyc/Ins:0.59 Cyc/Bch:521.00 Points:6 Rsq: 1.00 F:0.000000 LL:-4.949521 aic:2.316507 bic:2.247093
   Term: Constant      p:0.00000 coef:515.005
   Term: Log(N)        p:0.03915 coef:0.45287
PriorityQueue, Event:Check, Cyc/Ins:0.88 Cyc/Bch:496.12 Points:6 Rsq: 1.00 F:0.000000 LL:-23.438262 aic:8.812754 bic:8.708634
   Term: Constant      p:0.00000 coef:88.5013
   Term: CacheMisses   p:0.00000 coef:165.089
   Term: N*Log(N)      p:0.00000 coef:2.80993e-05
PriorityQueue, Event:FillUp, Cyc/Ins:0.57 Cyc/Bch:613.50 Points:6 Rsq: 1.00 F:0.000000 LL:-23.438262 aic:8.812754 bic:8.708634
(model did not converge)
MultiMap, Event:ReCheck, Cyc/Ins:1.48 Cyc/Bch:561.91 Points:6 Rsq: 1.00 F:0.000000 LL:-19.854808 aic:7.618269 bic:7.514149
   Term: Constant      p:0.00000 coef:57.1476
   Term: CacheMisses   p:0.00000 coef:39.7849
   Term: N*Log(N)      p:0.00000 coef:2.47464e-05
MultiMap, Event:Refill, Cyc/Ins:1.61 Cyc/Bch:320.19 Points:6 Rsq: 0.99 F:0.000011 LL:-42.323127 aic:14.774376 bic:14.704962
   Term: CacheMisses   p:0.00147 coef:207
   Term: Log(N)        p:0.00030 coef:180.901
MultiMap, Event:Check, Cyc/Ins:1.57 Cyc/Bch:836.50 Points:6 Rsq: 1.00 F:0.000000 LL:-7.157631 aic:3.052544 bic:2.983130
   Term: Constant      p:0.00000 coef:20.1331
   Term: N*Log(N)      p:0.00000 coef:7.16537e-06
MultiMap, Event:FillUp, Cyc/Ins:1.33 Cyc/Bch:271.45 Points:6 Rsq: 0.99 F:0.000014 LL:-41.569846 aic:14.523282 bic:14.453869
   Term: CacheMisses   p:0.00337 coef:170.837
   Term: Log(N)        p:0.00014 coef:178.206
```

## Questions:

1) Could I have just used "perf stat"? No, because in this case you will be counting initialization and destruction phases of the process as well. Most of the time it is just a tiny section in the middle that you want to measure.

2) Could I have just generated a CSV with the performance counters and then used R or python to generate the results? Yes, but there is a convenience factor to have everything in a single binary - less moving parts for installation. This way we do not depend on any particular python version or library installed.


Vitorian LLC

We are a C++/FinTech startup. Please support our work!

web: www.vitorian.com

substack: lucisqr.substack.com

email: henry@vitorian.com

linkedin: www.linkedin.com/in/hftrader

LICENSE: MIT
