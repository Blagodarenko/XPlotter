# XPlotter

CPU plotter for BURST

## Usage

```
@setlocal
@cd /d %~dp0 
XPlotter.exe -id 17559140197979902351 -sn 0 -n 20000 -t 2 -path F:\burst\plots -mem 5G
```

## Usage Breakdown

```
-id: Your BURST address numeric ID.
-sn: Nonce to Start at.
-n: Number of Nonces to Plot.
-t: Number of threads on your CPU you want to use.
-path: Path to your Plots.
-mem: Amount of RAM to use while plotting.
```

## Threads

You can find your number of threads with CPU-Z.

If you have Hyperthreading, You can double your thread count.

![Imgur](http://i.imgur.com/cv5pv7x.png)


## Resume Plotting

To resume plotting you need to match the config to the plotfile you are trying to resume, eg:

```
17559140197979902351_20001_20000_20000
```
```
XPlotter_avx.exe -id 17559140197979902351 -sn 20001 -n 20000 -t 6 -path F:\burst\plots -mem 5G
```
