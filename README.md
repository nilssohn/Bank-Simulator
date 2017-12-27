# Bank Simulator
Simulates and gathers metrics for a day in a bank. It's a multithreaded process using signals and timers.

# Usage
It was implemented for QNX environment, but it was also tested locally on a POSIX environment. Normal gcc command doesn't
link some libraries like pthread, so it has to be specified. The run.sh script compiles the existing executable file AND
runs the program.
