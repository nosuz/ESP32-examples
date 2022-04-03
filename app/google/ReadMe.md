HTTP access to Google API consumes a lot of memory and sometimes raises stack overflow panic. To avoid this error, increase `Main task stack size` for example 5120 bytes in `Component config > ESP System Settings`.

Component config > ESP System Settings

* Panic handler behaviour
* Main task stack size
