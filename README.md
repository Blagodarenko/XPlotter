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
